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

        emit thinkingInfoUpdated(info->time(), info->depth(),
                                 info->nodes(), info->score(),
                                 info->pvKanjiStr(), info->pvUsiStr(),
                                 m_baseSfen);
    }
}

void ThinkingInfoPresenter::requestClearThinkingInfo()
{
    m_infoBuffer.clear();
    emit clearThinkingInfoRequested();
}

// === 通信ログ ===

void ThinkingInfoPresenter::logSentCommand(const QString& prefix, const QString& command)
{
    emit commLogAppended(prefix + " > " + command);
}

void ThinkingInfoPresenter::logReceivedData(const QString& prefix, const QString& data)
{
    emit commLogAppended(prefix + " < " + data);
}

void ThinkingInfoPresenter::logStderrData(const QString& prefix, const QString& data)
{
    emit commLogAppended(prefix + " <stderr> " + data);
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

int ThinkingInfoPresenter::calculateScoreInt(const ShogiEngineInfoParser* info) const
{
    int scoreInt = 0;

    if ((info->scoreMate().toLongLong() > 0) || (info->scoreMate() == "+")) {
        scoreInt = 2000;
    } else if ((info->scoreMate().toLongLong() < 0) || (info->scoreMate() == "-")) {
        scoreInt = -2000;
    }

    return scoreInt;
}

void ThinkingInfoPresenter::updateScoreMateAndLastScore(ShogiEngineInfoParser* info, int& scoreInt)
{
    if (info->scoreMate().isEmpty()) {
        m_lastScoreCp = 0;
    } else {
        scoreInt = calculateScoreInt(info);

        QString scoreMate = info->scoreMate();

        if ((scoreMate == "+") || (scoreMate == "-")) {
            scoreMate = "詰";
        } else {
            scoreMate += "手詰";
        }

        info->setScore(scoreMate);
        m_scoreStr = info->scoreMate();
        m_lastScoreCp = scoreInt;
        
        emit scoreUpdated(m_lastScoreCp, m_scoreStr);
    }
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
    if (scoreInt > 2000) {
        m_lastScoreCp = 2000;
    } else if (scoreInt < -2000) {
        m_lastScoreCp = -2000;
    } else {
        m_lastScoreCp = scoreInt;
    }
    
    emit scoreUpdated(m_lastScoreCp, m_scoreStr);
}

void ThinkingInfoPresenter::updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreInt)
{
    if (info->scoreCp().isEmpty()) {
        updateScoreMateAndLastScore(info, scoreInt);
    } else {
        updateAnalysisModeAndScore(info, scoreInt);
        info->setScore(m_scoreStr);
        m_pvKanjiStr = info->pvKanjiStr();
        updateLastScore(scoreInt);
    }
}
