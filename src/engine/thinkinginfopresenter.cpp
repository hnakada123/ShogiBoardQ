/**
 * @file thinkinginfopresenter.cpp
 * @brief 思考情報GUI表示Presenterクラスの実装
 */

#include "thinkinginfopresenter.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "shogiinforecord.h"

#include <QTimer>
#include <QDebug>
#include <memory>

ThinkingInfoPresenter::ThinkingInfoPresenter(QObject* parent)
    : QObject(parent)
    , m_locale(QLocale::English)
{
}

// === モデル設定 ===

void ThinkingInfoPresenter::setCommLogModel(UsiCommLogModel* model)
{
    m_commLogModel = model;
}

void ThinkingInfoPresenter::setThinkingModel(ShogiEngineThinkingModel* model)
{
    m_thinkingModel = model;
}

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

    // const_castを避けるためlineをコピー
    QString lineCopy = line;
    info->parseEngineOutputAndUpdateState(
        lineCopy,
        m_gameController,
        m_clonedBoardData,
        m_ponderEnabled
    );

    // GUI項目の更新
    updateSearchedHand(info.get());
    updateDepth(info.get());
    updateNodes(info.get());
    updateNps(info.get());
    updateHashfull(info.get());

    // 評価値/詰み手数の更新
    updateEvaluationInfo(info.get(), scoreInt);

    // 思考タブへ追記
    if (!info->time().isEmpty() || !info->depth().isEmpty() ||
        !info->nodes().isEmpty() || !info->score().isEmpty() ||
        !info->pvKanjiStr().isEmpty()) {

        if (m_thinkingModel) {
            m_thinkingModel->prependItem(
                new ShogiInfoRecord(info->time(),
                                    info->depth(),
                                    info->nodes(),
                                    info->score(),
                                    info->pvKanjiStr()));
        }

        // シグナルを発行
        emit thinkingInfoUpdated(info->time(), info->depth(),
                                 info->nodes(), info->score(),
                                 info->pvKanjiStr());
    }
}

void ThinkingInfoPresenter::clearThinkingInfo()
{
    // QPointerがnullになっていれば、モデルは既に破棄されている
    if (m_thinkingModel) {
        m_thinkingModel->clearAllItems();
    }
    m_infoBuffer.clear();
}

// === 通信ログ ===

void ThinkingInfoPresenter::logSentCommand(const QString& prefix, const QString& command)
{
    if (m_commLogModel) {
        m_commLogModel->appendUsiCommLog(prefix + " > " + command);
    }
}

void ThinkingInfoPresenter::logReceivedData(const QString& prefix, const QString& data)
{
    if (m_commLogModel) {
        m_commLogModel->appendUsiCommLog(prefix + " < " + data);
    }
}

void ThinkingInfoPresenter::logStderrData(const QString& prefix, const QString& data)
{
    if (m_commLogModel) {
        m_commLogModel->appendUsiCommLog(prefix + " <stderr> " + data);
    }
}

// === GUI更新ヘルパメソッド ===

void ThinkingInfoPresenter::updateSearchedHand(const ShogiEngineInfoParser* info)
{
    if (m_commLogModel) {
        m_commLogModel->setSearchedMove(info->searchedHand());
    }
}

void ThinkingInfoPresenter::updateDepth(const ShogiEngineInfoParser* info)
{
    if (!m_commLogModel) return;

    if (info->seldepth().isEmpty()) {
        m_commLogModel->setSearchDepth(info->depth());
    } else {
        m_commLogModel->setSearchDepth(info->depth() + "/" + info->seldepth());
    }
}

void ThinkingInfoPresenter::updateNodes(const ShogiEngineInfoParser* info)
{
    if (!m_commLogModel) return;

    unsigned long long nodes = info->nodes().toULongLong();
    m_commLogModel->setNodeCount(m_locale.toString(nodes));
}

void ThinkingInfoPresenter::updateNps(const ShogiEngineInfoParser* info)
{
    if (!m_commLogModel) return;

    unsigned long long nps = info->nps().toULongLong();
    m_commLogModel->setNodesPerSecond(m_locale.toString(nps));
}

void ThinkingInfoPresenter::updateHashfull(const ShogiEngineInfoParser* info)
{
    if (!m_commLogModel) return;

    if (info->hashfull().isEmpty()) {
        m_commLogModel->setHashUsage("");
    } else {
        unsigned long long hash = info->hashfull().toULongLong() / 10;
        m_commLogModel->setHashUsage(QString::number(hash) + "%");
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
