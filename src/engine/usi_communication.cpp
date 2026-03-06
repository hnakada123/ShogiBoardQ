/// @file usi_communication.cpp
/// @brief Usi クラスの通信処理実装（詰将棋・対局・解析）

#include "usi.h"
#include "usimatchhandler.h"

#include <QTimer>

// ============================================================
// 詰将棋関連
// ============================================================

void Usi::executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec)
{
    m_matchHandler->cloneCurrentBoardData();
    sendPositionAndGoMateCommands(mateLimitMilliSec, positionStr);
}

void Usi::sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr)
{
    m_protocolHandler->sendPosition(positionStr);
    m_protocolHandler->sendGoMate(mateLimitMilliSec);
}

// ============================================================
// 対局通信処理（UsiMatchHandlerへ委譲）
// ============================================================

void Usi::handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                           QPoint& outFrom, QPoint& outTo,
                                           const UsiTimingParams& timing,
                                           QStringList& positionStrList)
{
    // 対局時は検討タブ用モデルをクリア
    m_considerationModel = nullptr;

    m_matchHandler->handleHumanVsEngineCommunication(positionStr, positionPonderStr, outFrom, outTo,
                                                     timing, positionStrList);
}

void Usi::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                        QString& positionPonderStr,
                                                        QPoint& outFrom, QPoint& outTo,
                                                        const UsiTimingParams& timing)
{
    // 対局時は検討タブ用モデルをクリア
    m_considerationModel = nullptr;

    m_matchHandler->handleEngineVsHumanOrEngineMatchCommunication(positionStr, positionPonderStr,
                                                                  outFrom, outTo, timing);
}

// ============================================================
// 棋譜解析
// ============================================================

void Usi::resetAnalysisStopTimer()
{
    if (!m_analysisStopTimer) {
        return;
    }
    m_analysisStopTimer->stop();
    m_analysisStopTimer->deleteLater();
    m_analysisStopTimer = nullptr;
}

void Usi::prepareAnalysisSession(const QString& positionStr, int multiPV)
{
    resetAnalysisStopTimer();

    // 思考開始時の局面SFENを保存（読み筋表示用）
    const QString baseSfen = m_matchHandler->computeBaseSfenFromBoard();
    if (!baseSfen.isEmpty()) {
        m_presenter->setBaseSfen(baseSfen);
    }

    m_matchHandler->cloneCurrentBoardData();
    m_protocolHandler->sendPosition(positionStr);

    // MultiPV を設定（常に送信して、前回の設定をリセット）
    m_protocolHandler->sendRaw(QStringLiteral("setoption name MultiPV value %1").arg(multiPV));

    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendRaw("go infinite");
}

void Usi::executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    // 後方互換API。現在はUIブロック回避のため非ブロッキングで処理する。
    sendAnalysisCommands(positionStr, byoyomiMilliSec, multiPV);
}

void Usi::sendAnalysisCommands(const QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    qCDebug(lcEngine) << "sendAnalysisCommands:"
                      << "positionStr=" << positionStr
                      << "byoyomiMilliSec=" << byoyomiMilliSec
                      << "multiPV=" << multiPV;

    prepareAnalysisSession(positionStr, multiPV);

    // タイムアウト後にstop送信（byoyomiMilliSec > 0 の場合のみ）
    // メンバータイマーを使用して、次回の sendAnalysisCommands でキャンセル可能にする
    if (byoyomiMilliSec > 0) {
        m_analysisStopTimer = new QTimer(this);
        m_analysisStopTimer->setSingleShot(true);
        connect(m_analysisStopTimer, &QTimer::timeout, this, &Usi::onConsiderationStopTimeout);
        m_analysisStopTimer->start(byoyomiMilliSec);
    }

    // waitForBestMove は呼ばない（非ブロッキング）
    // bestmove はシグナル経由で通知される
}
