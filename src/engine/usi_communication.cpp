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
                                           int byoyomiMilliSec, const QString& btime,
                                           const QString& wtime, QStringList& positionStrList,
                                           int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                           bool useByoyomi)
{
    // 対局時は検討タブ用モデルをクリア
    m_considerationModel = nullptr;

    m_matchHandler->handleHumanVsEngineCommunication(positionStr, positionPonderStr, outFrom, outTo,
                                                     byoyomiMilliSec, btime, wtime, positionStrList,
                                                     addEachMoveMilliSec1, addEachMoveMilliSec2,
                                                     useByoyomi);
}

void Usi::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                        QString& positionPonderStr,
                                                        QPoint& outFrom, QPoint& outTo,
                                                        int byoyomiMilliSec, const QString& btime,
                                                        const QString& wtime,
                                                        int addEachMoveMilliSec1,
                                                        int addEachMoveMilliSec2, bool useByoyomi)
{
    // 対局時は検討タブ用モデルをクリア
    m_considerationModel = nullptr;

    m_matchHandler->handleEngineVsHumanOrEngineMatchCommunication(positionStr, positionPonderStr,
                                                                  outFrom, outTo, byoyomiMilliSec,
                                                                  btime, wtime, addEachMoveMilliSec1,
                                                                  addEachMoveMilliSec2, useByoyomi);
}

// ============================================================
// 棋譜解析
// ============================================================

void Usi::executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    // 処理フロー:
    // 1. 既存の停止タイマーをキャンセル
    // 2. 局面SFENを保存（読み筋表示用）
    // 3. 盤面クローン + position送信
    // 4. MultiPV設定送信
    // 5. go infinite送信
    // 6. タイムアウト設定（秒読みありの場合）またはbestmove待機

    // 既存の停止タイマーをキャンセル
    if (m_analysisStopTimer) {
        m_analysisStopTimer->stop();
        m_analysisStopTimer->deleteLater();
        m_analysisStopTimer = nullptr;
    }

    // 思考開始時の局面SFENを保存（読み筋表示用）
    {
        QString baseSfen = m_matchHandler->computeBaseSfenFromBoard();
        if (!baseSfen.isEmpty()) {
            m_presenter->setBaseSfen(baseSfen);
        }
    }

    m_matchHandler->cloneCurrentBoardData();
    m_protocolHandler->sendPosition(positionStr);

    // MultiPV を設定（常に送信して、前回の設定をリセット）
    m_protocolHandler->sendRaw(QStringLiteral("setoption name MultiPV value %1").arg(multiPV));

    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendRaw("go infinite");

    if (byoyomiMilliSec <= 0) {
        (void)m_protocolHandler->keepWaitingForBestMove();
    } else {
        // タイムアウト後にstop送信（メンバータイマーを使用）
        m_analysisStopTimer = new QTimer(this);
        m_analysisStopTimer->setSingleShot(true);
        connect(m_analysisStopTimer, &QTimer::timeout, this, &Usi::onAnalysisStopTimeout);
        m_analysisStopTimer->start(byoyomiMilliSec);

        static constexpr int kPostStopGraceMs = 4000;
        const int waitBudget = qMax(byoyomiMilliSec + kPostStopGraceMs, 2500);
        (void)m_protocolHandler->waitForBestMove(waitBudget);
    }
}

void Usi::sendAnalysisCommands(const QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    qCDebug(lcEngine) << "sendAnalysisCommands:"
                      << "positionStr=" << positionStr
                      << "byoyomiMilliSec=" << byoyomiMilliSec
                      << "multiPV=" << multiPV;

    // 既存の停止タイマーをキャンセル（前回の検討のタイマーが残っている可能性がある）
    if (m_analysisStopTimer) {
        m_analysisStopTimer->stop();
        m_analysisStopTimer->deleteLater();
        m_analysisStopTimer = nullptr;
    }

    // 思考開始時の局面SFENを保存（読み筋表示用）
    {
        QString baseSfen = m_matchHandler->computeBaseSfenFromBoard();
        if (!baseSfen.isEmpty()) {
            m_presenter->setBaseSfen(baseSfen);
        }
    }

    m_matchHandler->cloneCurrentBoardData();
    m_protocolHandler->sendPosition(positionStr);

    // MultiPV を設定（常に送信して、前回の設定をリセット）
    m_protocolHandler->sendRaw(QStringLiteral("setoption name MultiPV value %1").arg(multiPV));

    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendRaw("go infinite");

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
