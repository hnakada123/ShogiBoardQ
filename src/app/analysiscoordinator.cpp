#include "analysiscoordinator.h"
#include "engineanalysistab.h"

#include <QDebug>
#include <QRegularExpression>
#include <QGlobalStatic>
#include <limits>

// Q_GLOBAL_STATICを使用してexit-time-destructorを回避
Q_GLOBAL_STATIC_WITH_ARGS(QRegularExpression, reDepth, (QStringLiteral(R"((?:^|\s)depth\s+(\d+))")))
Q_GLOBAL_STATIC_WITH_ARGS(QRegularExpression, reSel, (QStringLiteral(R"((?:^|\s)seldepth\s+(\d+))")))
Q_GLOBAL_STATIC_WITH_ARGS(QRegularExpression, reScoreCp, (QStringLiteral(R"((?:^|\s)score\s+cp\s+(-?\d+))")))
Q_GLOBAL_STATIC_WITH_ARGS(QRegularExpression, reScoreMate, (QStringLiteral(R"((?:^|\s)score\s+mate\s+(-?\d+))")))
Q_GLOBAL_STATIC_WITH_ARGS(QRegularExpression, rePv, (QStringLiteral(R"((?:^|\s)pv\s+(.+)$)")))

AnalysisCoordinator::AnalysisCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_deps(d)
{
}

void AnalysisCoordinator::setDeps(const Deps& d)
{
    m_deps = d;
}

void AnalysisCoordinator::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
}

void AnalysisCoordinator::setOptions(const Options& opt)
{
    m_opt = opt;
}

void AnalysisCoordinator::startAnalyzeRange()
{
    if (!m_deps.sfenRecord || m_deps.sfenRecord->isEmpty()) {
        qWarning() << "[ANA] startAnalyzeRange: sfenRecord not ready";
        return;
    }
    if (m_running) stop();

    m_mode = RangePositions;
    m_running = true;

    // endPly 未指定なら末尾まで
    const int last = static_cast<int>(m_deps.sfenRecord->size()) - 1;
    if (m_opt.endPly < 0 || m_opt.endPly > last) {
        m_opt.endPly = last;
    }
    if (m_opt.startPly < 0) m_opt.startPly = 0;
    if (m_opt.startPly > m_opt.endPly) m_opt.startPly = m_opt.endPly;

    emit analysisStarted(m_opt.startPly, m_opt.endPly, m_mode);

    // MultiPV は setoption で設定（対応エンジンのみ有効）
    if (m_opt.multiPV > 1) {
        send_(QStringLiteral("setoption name MultiPV value %1").arg(m_opt.multiPV));
    }
    // 念のため isready → readyok を待ちたければ、呼び出し側で同期を取ること
    // ここでは簡易化して直ちに開始
    startRange_();
}

void AnalysisCoordinator::startAnalyzeSingle(int ply)
{
    if (!m_deps.sfenRecord || m_deps.sfenRecord->isEmpty()) {
        qWarning() << "[ANA] startAnalyzeSingle: sfenRecord not ready";
        return;
    }
    if (m_running) stop();

    m_mode = SinglePosition;
    m_running = true;

    m_opt.startPly = qMax(0, ply);
    m_opt.endPly   = m_opt.startPly;

    emit analysisStarted(m_opt.startPly, m_opt.endPly, m_mode);

    if (m_opt.multiPV > 1) {
        send_(QStringLiteral("setoption name MultiPV value %1").arg(m_opt.multiPV));
    }
    startSingle_(m_opt.startPly);
}

void AnalysisCoordinator::stop()
{
    if (!m_running) return;
    // USI の明示停止
    send_(QStringLiteral("stop"));

    m_running = false;
    m_mode = Idle;
    m_currentPly = -1;

    emit analysisFinished(Idle);
}

void AnalysisCoordinator::startRange_()
{
    m_currentPly = m_opt.startPly;
    sendAnalyzeForPly_(m_currentPly);
}

void AnalysisCoordinator::startSingle_(int ply)
{
    m_currentPly = ply;
    sendAnalyzeForPly_(m_currentPly);
}

void AnalysisCoordinator::nextPlyOrFinish_()
{
    if (m_mode != RangePositions) {
        // SinglePosition の場合はここで終わる
        m_running = false;
        m_mode = Idle;
        m_currentPly = -1;
        emit analysisFinished(Idle);
        return;
    }

    if (m_currentPly < 0) m_currentPly = m_opt.startPly;

    if (m_currentPly >= m_opt.endPly) {
        // すべて完了
        m_running = false;
        m_mode = Idle;
        m_currentPly = -1;
        emit analysisFinished(RangePositions);
        return;
    }

    // 次の局面へ
    m_currentPly += 1;
    sendAnalyzeForPly_(m_currentPly);
}

void AnalysisCoordinator::sendAnalyzeForPly_(int ply)
{
    if (!m_running) return;
    if (!m_deps.sfenRecord) return;
    if (ply < 0 || ply >= m_deps.sfenRecord->size()) {
        qWarning() << "[ANA] sendAnalyzeForPly_: index out of range" << ply;
        nextPlyOrFinish_();
        return;
    }

    const QString sfen = m_deps.sfenRecord->at(ply);
    
    // 1) position sfen ... の形式でコマンドを準備
    //    startpos の場合はそのまま、それ以外は "position sfen ..." で送る
    if (sfen == QStringLiteral("startpos") || sfen.startsWith(QStringLiteral("position "))) {
        m_pendingPosCmd = sfen;
    } else {
        m_pendingPosCmd = QStringLiteral("position sfen %1").arg(sfen);
    }
    qDebug().noquote() << "[ANA] sendAnalyzeForPly_: ply=" << ply << "posCmd=" << m_pendingPosCmd;
    
    // 2) positionPreparedシグナルを発行（GUI更新用）
    //    sendGoCommand()が呼ばれるまでgoコマンドは送信しない
    emit positionPrepared(ply, sfen);
}

void AnalysisCoordinator::sendGoCommand()
{
    if (!m_running) return;
    if (m_pendingPosCmd.isEmpty()) return;
    
    qDebug().noquote() << "[ANA] sendGoCommand: sending position and go commands";
    
    // positionコマンドを送信
    send_(m_pendingPosCmd);
    m_pendingPosCmd.clear();
    
    // goコマンドを送信
    send_(QStringLiteral("go movetime %1").arg(m_opt.movetimeMs));

    // 分析進行に応じたツリーハイライトなどが必要ならここで
    if (m_analysisTab && m_opt.centerTree) {
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, m_currentPly, /*centerOn=*/true);
    }
}


void AnalysisCoordinator::onEngineInfoLine(const QString& line)
{
    qDebug().noquote() << "[ANA::onEngineInfoLine] m_running=" << m_running << "m_currentPly=" << m_currentPly;
    if (!m_running) return;
    if (m_currentPly < 0) return;

    // "info ..." 以外は無視
    if (!line.startsWith(QStringLiteral("info"))) return;

    ParsedInfo p;
    if (!parseInfoUSI_(line, &p)) {
        // パース不能でも raw を進捗として流しておくと UI 側で全文表示に使える
        qDebug().noquote() << "[ANA::onEngineInfoLine] parse failed, emitting raw";
        emit analysisProgress(m_currentPly, -1, -1,
                              std::numeric_limits<int>::min(), 0,
                              QString(), line);
        return;
    }

    qDebug().noquote() << "[ANA::onEngineInfoLine] emitting analysisProgress: ply=" << m_currentPly << "scoreCp=" << p.scoreCp << "pv=" << p.pv.left(30);
    emit analysisProgress(m_currentPly, p.depth, p.seldepth,
                          p.scoreCp, p.mate, p.pv, line);
}

void AnalysisCoordinator::onEngineBestmoveReceived(const QString& /*line*/)
{
    if (!m_running) return;

    // 定跡の場合（info行なしでbestmoveが来た場合）でも結果を通知
    // m_currentPly は sendAnalyzeForPly_ で設定済み
    // 通常の解析では onEngineInfoLine でanalysisProgressが発行済みだが、
    // 定跡の場合は発行されていないので、空の結果を通知
    // （AnalysisFlowController側で定跡かどうかを判断できるようにする）
    
    // "bestmove ..." を受けたら次へ
    nextPlyOrFinish_();
}

bool AnalysisCoordinator::parseInfoUSI_(const QString& line, ParsedInfo* out)
{
    // ざっくりとした USI info 行のパース：
    // 例）info depth 20 seldepth 34 score cp 23 pv 7g7f 3c3d ...
    //     info depth 25 score mate 3 pv ...
    // 必要十分ではないが実用的な範囲で抽出
    if (!out) return false;

    int depth = -1, seldepth = -1, scoreCp = std::numeric_limits<int>::min(), mate = 0;
    QString pv;

    // depth
    {
        auto m = reDepth->match(line);
        if (m.hasMatch()) depth = m.captured(1).toInt();
    }
    // seldepth
    {
        auto m = reSel->match(line);
        if (m.hasMatch()) seldepth = m.captured(1).toInt();
    }
    // score
    {
        auto mCp   = reScoreCp->match(line);
        auto mMate = reScoreMate->match(line);
        if (mCp.hasMatch()) {
            scoreCp = mCp.captured(1).toInt();
        } else if (mMate.hasMatch()) {
            mate = mMate.captured(1).toInt(); // 詰みまでの手数（正負で先後）
        }
    }
    // pv
    {
        auto m = rePv->match(line);
        if (m.hasMatch()) pv = m.captured(1).trimmed();
    }

    if (depth < 0 && seldepth < 0 && scoreCp == std::numeric_limits<int>::min() && mate == 0 && pv.isEmpty()) {
        return false;
    }
    out->depth    = depth;
    out->seldepth = seldepth;
    out->scoreCp  = scoreCp;
    out->mate     = mate;
    out->pv       = pv;
    return true;
}

void AnalysisCoordinator::send_(const QString& line)
{
    emit requestSendUsiCommand(line);
}
