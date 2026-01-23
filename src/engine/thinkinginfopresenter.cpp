/**
 * @file thinkinginfopresenter.cpp
 * @brief 思考情報GUI表示Presenterクラスの実装
 *
 * View層（モデル）への直接依存を排除し、
 * 全ての更新をシグナル経由で行う疎結合な設計。
 */

#include "thinkinginfopresenter.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"

#include <QTimer>
#include <QDebug>
#include <memory>

ThinkingInfoPresenter::ThinkingInfoPresenter(QObject* parent)
    : QObject(parent)
    , m_locale(QLocale::English)
{
}

// === 依存関係設定 ===

void ThinkingInfoPresenter::setGameController(ShogiGameController* controller)
{
    m_gameController = controller;
}

// === 状態設定 ===

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
        qDebug().noquote() << "[ThinkingInfoPresenter::setBaseSfen] turn=" << turn 
                           << "m_thinkingStartPlayerIsP1=" << m_thinkingStartPlayerIsP1;
    } else {
        qDebug().noquote() << "[ThinkingInfoPresenter::setBaseSfen] WARNING: could not parse turn from sfen=" << sfen.left(50);
    }
}

QString ThinkingInfoPresenter::baseSfen() const
{
    return m_baseSfen;
}

// === info処理 ===

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
    qDebug().noquote() << "[ThinkingInfoPresenter::processInfoLineInternal] m_clonedBoardData.size()=" << m_clonedBoardData.size();
    qDebug().noquote() << "[ThinkingInfoPresenter::processInfoLineInternal] m_gameController=" << m_gameController;
    qDebug().noquote() << "[ThinkingInfoPresenter::processInfoLineInternal] m_thinkingStartPlayerIsP1=" << m_thinkingStartPlayerIsP1;
    
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
    qDebug().noquote() << "[ThinkingInfoPresenter::processInfoLineInternal] startPlayer=" 
                       << (m_thinkingStartPlayerIsP1 ? "P1(sente)" : "P2(gote)");

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

        emit thinkingInfoUpdated(info->time(), info->depth(),
                                 info->nodes(), info->score(),
                                 info->pvKanjiStr(), info->pvUsiStr(),
                                 m_baseSfen);
    }
}

void ThinkingInfoPresenter::requestClearThinkingInfo()
{
    qDebug().noquote() << "[ThinkingInfoPresenter::requestClearThinkingInfo] called";
    m_infoBuffer.clear();
    emit clearThinkingInfoRequested();
}

// === 通信ログ ===

// ★ 追加: プレフィックスからエンジン番号を抽出するヘルパー関数
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
    // ★ 変更: 新しいフォーマット ▶ E1: command
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("▶ ") + tag + QStringLiteral(": ") + command);
}

void ThinkingInfoPresenter::logReceivedData(const QString& prefix, const QString& data)
{
    // ★ 変更: 新しいフォーマット ◀ E1: response
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("◀ ") + tag + QStringLiteral(": ") + data);
}

void ThinkingInfoPresenter::logStderrData(const QString& prefix, const QString& data)
{
    // ★ 変更: 新しいフォーマット ⚠ E1: stderr
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("⚠ ") + tag + QStringLiteral(": ") + data);
}

// === シグナル発行ヘルパメソッド ===

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

// === 評価値計算 ===

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
    qDebug() << "[TIP] updateLastScore: scoreInt=" << scoreInt << "m_lastScoreCp(before)=" << m_lastScoreCp;
    
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
    
    qDebug() << "[TIP] updateLastScore: m_lastScoreCp(after)=" << m_lastScoreCp;
    emit scoreUpdated(m_lastScoreCp, m_scoreStr);
}

void ThinkingInfoPresenter::updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreInt)
{
    // multipv 1（1行目）の場合のみ評価値グラフを更新
    // multipvが空の場合も更新（単一PVモードの場合）
    const QString multipv = info->multipv();
    const QString scoreCp = info->scoreCp();
    
    qDebug() << "[TIP] updateEvaluationInfo: multipv=" << multipv 
             << "scoreCp=" << scoreCp 
             << "m_lastScoreCp(before)=" << m_lastScoreCp;
    
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
            qDebug() << "[TIP] UPDATING score: scoreInt=" << scoreInt << "m_scoreStr=" << m_scoreStr;
            updateLastScore(scoreInt);
        } else {
            qDebug() << "[TIP] SKIPPING graph update for multipv=" << multipv << "(score set for display only)";
        }
    }
    
    qDebug() << "[TIP] updateEvaluationInfo done: m_lastScoreCp(after)=" << m_lastScoreCp;
}
