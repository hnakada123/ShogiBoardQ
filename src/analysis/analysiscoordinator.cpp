/// @file analysiscoordinator.cpp
/// @brief 局面解析コーディネータクラスの実装

#include "analysiscoordinator.h"
#include "logcategories.h"

#include <limits>
#include <utility>

AnalysisCoordinator::AnalysisCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_deps(d)
{
    m_stopTimer.setSingleShot(true);
    connect(&m_stopTimer, &QTimer::timeout,
            this, &AnalysisCoordinator::onStopTimerTimeout);
}

void AnalysisCoordinator::setDeps(const Deps& d)
{
    m_deps = d;
}

void AnalysisCoordinator::setOptions(const Options& opt)
{
    m_opt = opt;
}

void AnalysisCoordinator::startAnalyzeRange()
{
    if (!m_deps.sfenRecord || m_deps.sfenRecord->isEmpty()) {
        qCWarning(lcAnalysis) << "startAnalyzeRange: sfenRecord not ready";
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

    // MultiPV は setoption で設定（対応エンジンのみ有効）
    if (m_opt.multiPV > 1) {
        emit requestSetOption(QStringLiteral("MultiPV"), QString::number(m_opt.multiPV));
    }
    // 念のため isready → readyok を待ちたければ、呼び出し側で同期を取ること
    // ここでは簡易化して直ちに開始
    startRange();
}

void AnalysisCoordinator::startAnalyzeSingle(int ply)
{
    if (!m_deps.sfenRecord || m_deps.sfenRecord->isEmpty()) {
        qCWarning(lcAnalysis) << "startAnalyzeSingle: sfenRecord not ready";
        return;
    }
    if (m_running) stop();

    m_mode = SinglePosition;
    m_running = true;

    m_opt.startPly = qMax(0, ply);
    m_opt.endPly   = m_opt.startPly;

    if (m_opt.multiPV > 1) {
        emit requestSetOption(QStringLiteral("MultiPV"), QString::number(m_opt.multiPV));
    }
    startSingle(m_opt.startPly);
}

void AnalysisCoordinator::stop()
{
    if (!m_running) return;

    m_stopTimer.stop();

    // USI の明示停止
    emit requestStop();

    m_running = false;
    m_mode = Idle;
    m_currentPly = -1;

    emit analysisFinished(Idle);
}

void AnalysisCoordinator::startRange()
{
    m_currentPly = m_opt.startPly;
    sendAnalyzeForPly(m_currentPly);
}

void AnalysisCoordinator::startSingle(int ply)
{
    m_currentPly = ply;
    sendAnalyzeForPly(m_currentPly);
}

void AnalysisCoordinator::nextPlyOrFinish()
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
    sendAnalyzeForPly(m_currentPly);
}

void AnalysisCoordinator::sendAnalyzeForPly(int ply)
{
    if (!m_running) return;
    if (!m_deps.sfenRecord) return;
    if (ply < 0 || ply >= m_deps.sfenRecord->size()) {
        qCWarning(lcAnalysis) << "sendAnalyzeForPly: index out of range" << ply;
        nextPlyOrFinish();
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
    qCDebug(lcAnalysis).noquote() << "sendAnalyzeForPly: ply=" << ply << "posCmd=" << m_pendingPosCmd;

    emit positionPrepared(ply, sfen);
    sendGoCommand();
}

void AnalysisCoordinator::sendGoCommand()
{
    if (!m_running) return;
    if (m_pendingPosCmd.isEmpty()) return;

    qCDebug(lcAnalysis).noquote() << "sendGoCommand: sending position and go infinite commands";

    const QString positionCommand = std::exchange(m_pendingPosCmd, QString());
    m_stopTimer.start(m_opt.movetimeMs);
    emit requestSearch(positionCommand);
}


void AnalysisCoordinator::onEngineInfo(const UsiInfo& info)
{
    qCDebug(lcAnalysis).noquote() << "onEngineInfo: m_running=" << m_running << "m_currentPly=" << m_currentPly;
    if (!m_running) return;
    if (m_currentPly < 0) return;

    qCDebug(lcAnalysis).noquote() << "emitting analysisProgress: ply=" << m_currentPly
                                  << "depth=" << info.depth << "pv=" << info.pv.size();
    emit analysisProgress(m_currentPly, info);
}

void AnalysisCoordinator::onEngineBestmoveReceived(const QString& /*line*/)
{
    if (!m_running) return;

    // 定跡の場合（info行なしでbestmoveが来た場合）でも結果を通知
    // m_currentPly は sendAnalyzeForPly_ で設定済み
    // 通常の解析では onEngineInfo でanalysisProgressが発行済みだが、
    // 定跡の場合は発行されていないので、空の結果を通知
    // （AnalysisFlowController側で定跡かどうかを判断できるようにする）

    // 次の局面へ進む前に前局面の停止タイマーを解除する。
    m_stopTimer.stop();
    nextPlyOrFinish();
}

void AnalysisCoordinator::onStopTimerTimeout()
{
    if (!m_running) return;

    qCDebug(lcAnalysis).noquote() << "onStopTimerTimeout: sending stop command after"
                                  << m_opt.movetimeMs << "ms";

    // stopコマンドを送信（bestmoveが返ってきたらonEngineBestmoveReceived_で処理される）
    emit requestStop();
}
