/// @file usi_communication.cpp
/// @brief Usi クラスの通信処理実装（詰将棋・対局・解析）

#include "usi.h"
#include "shogiboard.h"
#include "usimatchhandler.h"
#include "sfenpositiontracer.h"

#include <QTimer>

// ============================================================
// 詰将棋関連
// ============================================================

void Usi::executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec)
{
    if (deferUntilReady([this, positionStr, mateLimitMilliSec]() mutable {
        executeTsumeCommunication(positionStr, mateLimitMilliSec);
    })) return;
    const QString baseSfen = SfenPositionTracer::sfenFromPositionCommand(positionStr);
    if (!baseSfen.isEmpty()) {
        m_presenter->setBaseSfen(baseSfen);
    } else {
        const QString fallbackSfen = currentBoardSfen();
        if (!fallbackSfen.isEmpty()) {
            m_presenter->setBaseSfen(fallbackSfen);
        }
    }
    m_presenter->requestClearThinkingInfo();
    sendPositionAndGoMateCommands(mateLimitMilliSec, positionStr);
}

void Usi::sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr)
{
    if (deferUntilReady([this, positionStr, mateLimitMilliSec]() mutable {
        sendPositionAndGoMateCommands(mateLimitMilliSec, positionStr);
    })) return;
    m_protocolHandler->sendPosition(positionStr);
    m_protocolHandler->sendGoMate(mateLimitMilliSec);
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
    const QString baseSfen = SfenPositionTracer::sfenFromPositionCommand(positionStr);
    m_presenter->setBaseSfen(!baseSfen.isEmpty() ? baseSfen : currentBoardSfen());
    m_considerationMaxMultiPV = qBound(1, multiPV, 10);
    m_protocolHandler->sendPosition(positionStr);

    // MultiPV を設定（常に送信して、前回の設定をリセット）
    m_protocolHandler->sendSetOption(QStringLiteral("MultiPV"), QString::number(multiPV));

    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendGoInfinite();
}

void Usi::sendPositionAndGoInfinite(const QString& positionStr)
{
    if (deferUntilReady([this, positionStr]() { sendPositionAndGoInfinite(positionStr); })) return;
    m_protocolHandler->sendPosition(positionStr);
    m_protocolHandler->sendGoInfinite();
}

void Usi::sendAnalysisCommands(const QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    if (deferUntilReady([this, positionStr, byoyomiMilliSec, multiPV]() {
        sendAnalysisCommands(positionStr, byoyomiMilliSec, multiPV);
    })) return;
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

    // 応答はシグナルで受信する。
    // bestmove はシグナル経由で通知される
}

QString Usi::currentBoardSfen() const
{
    if (!m_gameController || !m_gameController->board()) return {};
    auto* board = m_gameController->board();
    return board->convertBoardToSfen() + QLatin1Char(' ') + turnToSfen(board->currentPlayer())
        + QLatin1Char(' ') + board->convertStandToSfen() + QStringLiteral(" 1");
}
