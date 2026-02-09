/// @file thinkinginfopresenter.cpp
/// @brief 思考情報GUI表示Presenterクラスの実装

#include "thinkinginfopresenter.h"
#include "usi.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"

#include <QTimer>
#include <memory>

ThinkingInfoPresenter::ThinkingInfoPresenter(QObject* parent)
    : QObject(parent)
    , m_locale(QLocale::English)
{
}

// ============================================================
// 依存関係設定
// ============================================================

void ThinkingInfoPresenter::setGameController(ShogiGameController* controller)
{
    m_gameController = controller;
}

// ============================================================
// 状態設定
// ============================================================

void ThinkingInfoPresenter::setAnalysisMode(bool mode)
{
    m_analysisMode = mode;
}

void ThinkingInfoPresenter::setPreviousMove(int fileTo, int rankTo)
{
    m_previousFileTo = fileTo;
    m_previousRankTo = rankTo;
}

void ThinkingInfoPresenter::setClonedBoardData(const QVector<QChar>& boardData)
{
    m_clonedBoardData = boardData;
}

void ThinkingInfoPresenter::setPonderEnabled(bool enabled)
{
    m_ponderEnabled = enabled;
}

void ThinkingInfoPresenter::setBaseSfen(const QString& sfen)
{
    m_baseSfen = sfen;
    
    // SFENから手番を抽出（形式: "盤面 手番 駒台 手数"）
    // 例: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() >= 2) {
        const QString turn = parts.at(1);
        // b=先手(Player1), w=後手(Player2)
        m_thinkingStartPlayerIsP1 = (turn == QStringLiteral("b"));
        qCDebug(lcEngine) << "setBaseSfen: turn=" << turn
                          << "isP1=" << m_thinkingStartPlayerIsP1;
    } else {
        qCWarning(lcEngine) << "setBaseSfen: SFENから手番をパースできず:" << sfen.left(50);
    }
}

QString ThinkingInfoPresenter::baseSfen() const
{
    return m_baseSfen;
}

// ============================================================
// info処理
// ============================================================

void ThinkingInfoPresenter::onInfoReceived(const QString& line)
{
    m_infoBuffer.append(line);

    if (!m_flushScheduled) {
        m_flushScheduled = true;
        QTimer::singleShot(50, this, &ThinkingInfoPresenter::flushInfoBuffer);
    }
}

void ThinkingInfoPresenter::flushInfoBuffer()
{
    m_flushScheduled = false;

    QStringList batch;
    batch.swap(m_infoBuffer);

    for (const QString& line : batch) {
        if (!line.isEmpty()) {
            processInfoLineInternal(line);
        }
    }
}

void ThinkingInfoPresenter::processInfoLine(const QString& line)
{
    processInfoLineInternal(line);
}

void ThinkingInfoPresenter::processInfoLineInternal(const QString& line)
{
    // 処理フロー:
    // 1. ShogiEngineInfoParserを生成し、前回指し手・手番を設定
    // 2. info行をパースして盤面情報と合わせて解析
    // 3. 探索手・深さ・ノード数・NPS・ハッシュ使用率のシグナルを発行
    // 4. 評価値/詰み手数を更新
    // 5. 有効な情報があれば思考タブ向けの統合シグナルを発行

    qCDebug(lcEngine) << "processInfoLine: boardSize=" << m_clonedBoardData.size()
                      << "isP1=" << m_thinkingStartPlayerIsP1
                      << "prevFile=" << m_previousFileTo << "prevRank=" << m_previousRankTo;

    int scoreInt = 0;

    // スマートポインタでメモリ管理
    auto info = std::make_unique<ShogiEngineInfoParser>();

    info->setPreviousFileTo(m_previousFileTo);
    info->setPreviousRankTo(m_previousRankTo);
    
    // 思考開始時の手番を設定（bestmove後の局面更新の影響を受けないようにする）
    ShogiGameController::Player startPlayer = m_thinkingStartPlayerIsP1 
        ? ShogiGameController::Player1 
        : ShogiGameController::Player2;
    info->setThinkingStartPlayer(startPlayer);

    // const_castを避けるためlineをコピー
    QString lineCopy = line;
    info->parseEngineOutputAndUpdateState(
        lineCopy,
        m_gameController,
        m_clonedBoardData,
        m_ponderEnabled
    );

    // シグナル経由でGUI項目を更新
    emitSearchedHand(info.get());
    emitDepth(info.get());
    emitNodes(info.get());
    emitNps(info.get());
    emitHashfull(info.get());

    // 評価値/詰み手数の更新
    updateEvaluationInfo(info.get(), scoreInt);

    // 思考タブへ追記するシグナルを発行
    if (!info->time().isEmpty() || !info->depth().isEmpty() ||
        !info->nodes().isEmpty() || !info->score().isEmpty() ||
        !info->pvKanjiStr().isEmpty()) {

        // multipv値を取得（空の場合は1とみなす）
        int multipv = 1;
        if (!info->multipv().isEmpty()) {
            multipv = info->multipv().toInt();
            if (multipv < 1) multipv = 1;
        }

        emit thinkingInfoUpdated(info->time(), info->depth(),
                                 info->nodes(), info->score(),
                                 info->pvKanjiStr(), info->pvUsiStr(),
                                 m_baseSfen, multipv, scoreInt);
    }
}

void ThinkingInfoPresenter::requestClearThinkingInfo()
{
    qCDebug(lcEngine) << "requestClearThinkingInfo";
    m_infoBuffer.clear();
    emit clearThinkingInfoRequested();
}

// ============================================================
// 通信ログ
// ============================================================

/// プレフィックス文字列（"[E1]..."等）からエンジン番号タグを抽出する
static QString extractEngineTag(const QString& prefix)
{
    // プレフィックスは "[E1]..." や "[E2]..." の形式
    // E1 または E2 を抽出
    if (prefix.contains(QStringLiteral("[E1"))) {
        return QStringLiteral("E1");
    } else if (prefix.contains(QStringLiteral("[E2"))) {
        return QStringLiteral("E2");
    }
    return QStringLiteral("E?");
}

void ThinkingInfoPresenter::logSentCommand(const QString& prefix, const QString& command)
{
    // エンジンタグ付きフォーマットで送信ログを通知
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("▶ ") + tag + QStringLiteral(": ") + command);
}

void ThinkingInfoPresenter::logReceivedData(const QString& prefix, const QString& data)
{
    // エンジンタグ付きフォーマットで受信ログを通知
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("◀ ") + tag + QStringLiteral(": ") + data);
}

void ThinkingInfoPresenter::logStderrData(const QString& prefix, const QString& data)
{
    // エンジンタグ付きフォーマットで標準エラーログを通知
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("⚠ ") + tag + QStringLiteral(": ") + data);
}

// ============================================================
// シグナル発行ヘルパメソッド
// ============================================================

void ThinkingInfoPresenter::emitSearchedHand(const ShogiEngineInfoParser* info)
{
    if (!info->searchedHand().isEmpty()) {
        emit searchedMoveUpdated(info->searchedHand());
    }
}

void ThinkingInfoPresenter::emitDepth(const ShogiEngineInfoParser* info)
{
    if (info->depth().isEmpty()) return;

    QString depthStr;
    if (info->seldepth().isEmpty()) {
        depthStr = info->depth();
    } else {
        depthStr = info->depth() + "/" + info->seldepth();
    }
    emit searchDepthUpdated(depthStr);
}

void ThinkingInfoPresenter::emitNodes(const ShogiEngineInfoParser* info)
{
    if (info->nodes().isEmpty()) return;

    unsigned long long nodes = info->nodes().toULongLong();
    emit nodeCountUpdated(m_locale.toString(nodes));
}

void ThinkingInfoPresenter::emitNps(const ShogiEngineInfoParser* info)
{
    if (info->nps().isEmpty()) return;

    unsigned long long nps = info->nps().toULongLong();
    emit npsUpdated(m_locale.toString(nps));
}

void ThinkingInfoPresenter::emitHashfull(const ShogiEngineInfoParser* info)
{
    if (info->hashfull().isEmpty()) {
        emit hashUsageUpdated("");
    } else {
        unsigned long long hash = info->hashfull().toULongLong() / 10;
        emit hashUsageUpdated(QString::number(hash) + "%");
    }
}

// ============================================================
// 評価値計算
// ============================================================

// やねうら王系エンジンの優等局面スコア（VALUE_SUPERIOR）に合わせた詰み評価値
// VALUE_SUPERIOR == 28000, PawnValue == 90, centi-pawn換算で100/90倍 → 約31111
static constexpr int SCORE_MATE_VALUE = 31111;

int ThinkingInfoPresenter::calculateScoreInt(const ShogiEngineInfoParser* info) const
{
    int scoreInt = 0;

    // score mate の値がプラス（勝ち）または "+" の場合
    if ((info->scoreMate().toLongLong() > 0) || (info->scoreMate() == "+")) {
        scoreInt = SCORE_MATE_VALUE;
    }
    // score mate の値がマイナス（負け）または "-" の場合
    else if ((info->scoreMate().toLongLong() < 0) || (info->scoreMate() == "-")) {
        scoreInt = -SCORE_MATE_VALUE;
    }

    return scoreInt;
}

void ThinkingInfoPresenter::updateAnalysisModeAndScore(const ShogiEngineInfoParser* info, int& scoreInt)
{
    if (!m_gameController) {
        m_scoreStr = info->scoreCp();
        scoreInt = m_scoreStr.toInt();
        return;
    }

    if (m_analysisMode) {
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            scoreInt = info->scoreCp().toInt();
            m_scoreStr = info->scoreCp();
        } else if (m_gameController->currentPlayer() == ShogiGameController::Player2) {
            scoreInt = -info->scoreCp().toInt();
            m_scoreStr = QString::number(scoreInt);
        }
    } else {
        m_scoreStr = info->scoreCp();
        scoreInt = m_scoreStr.toInt();

        if (info->evaluationBound() == ShogiEngineInfoParser::EvaluationBound::LowerBound) {
            m_scoreStr += "++";
        } else if (info->evaluationBound() == ShogiEngineInfoParser::EvaluationBound::UpperBound) {
            m_scoreStr += "--";
        }
    }
}

void ThinkingInfoPresenter::updateLastScore(int scoreInt)
{
    qCDebug(lcEngine) << "updateLastScore: scoreInt=" << scoreInt << "before=" << m_lastScoreCp;
    
    // 評価値グラフの表示上限に合わせてクリッピング
    // 詰み評価値（±31111）を考慮した上限を設定
    static constexpr int SCORE_CLIP_MAX = 32000;
    
    if (scoreInt > SCORE_CLIP_MAX) {
        m_lastScoreCp = SCORE_CLIP_MAX;
    } else if (scoreInt < -SCORE_CLIP_MAX) {
        m_lastScoreCp = -SCORE_CLIP_MAX;
    } else {
        m_lastScoreCp = scoreInt;
    }
    
    qCDebug(lcEngine) << "updateLastScore: after=" << m_lastScoreCp;
    emit scoreUpdated(m_lastScoreCp, m_scoreStr);
}

void ThinkingInfoPresenter::updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreInt)
{
    // 処理フロー:
    // 1. multipv番号を確認し、1（または空）のみグラフ更新対象とする
    // 2. scoreCpが空の場合 → scoreMateを確認し詰み評価値を計算
    //    - scoreMateも空なら何もしない
    //    - 詰み表示文字列を生成し、multipv1ならグラフ更新
    // 3. scoreCpがある場合 → 解析モードに応じて符号反転し評価値を算出
    //    - multipv1ならグラフ・PV文字列を更新

    // multipv 1（1行目）の場合のみ評価値グラフを更新
    // multipvが空の場合も更新（単一PVモードの場合）
    const QString multipv = info->multipv();
    const QString scoreCp = info->scoreCp();
    
    qCDebug(lcEngine) << "updateEvaluationInfo: multipv=" << multipv
                      << "scoreCp=" << scoreCp
                      << "before=" << m_lastScoreCp;
    
    // multipv 2以降の場合は、思考タブの表示用にscoreをセットするが、
    // 評価値グラフ（m_lastScoreCp）は更新しない
    const bool isMultipv1 = multipv.isEmpty() || multipv == "1";
    
    if (info->scoreCp().isEmpty()) {
        // score mate の場合
        if (info->scoreMate().isEmpty()) {
            // スコア情報がない場合は何もしない
            return;
        }
        
        scoreInt = calculateScoreInt(info);
        
        QString scoreMate = info->scoreMate();
        if ((scoreMate == "+") || (scoreMate == "-")) {
            scoreMate = "詰";
        } else {
            scoreMate += "手詰";
        }
        
        info->setScore(scoreMate);
        
        // multipv 1 の場合のみ評価値グラフを更新
        if (isMultipv1) {
            m_scoreStr = info->scoreMate();
            m_lastScoreCp = scoreInt;
            emit scoreUpdated(m_lastScoreCp, m_scoreStr);
        }
    } else {
        // score cp の場合
        updateAnalysisModeAndScore(info, scoreInt);
        info->setScore(m_scoreStr);
        
        // multipv 1 の場合のみ評価値グラフを更新
        if (isMultipv1) {
            m_pvKanjiStr = info->pvKanjiStr();
            qCDebug(lcEngine) << "評価値更新: scoreInt=" << scoreInt << "scoreStr=" << m_scoreStr;
            updateLastScore(scoreInt);
        } else {
            qCDebug(lcEngine) << "multipv=" << multipv << "グラフ更新スキップ（表示用のみ）";
        }
    }
    
    qCDebug(lcEngine) << "updateEvaluationInfo完了: lastScoreCp=" << m_lastScoreCp;
}
