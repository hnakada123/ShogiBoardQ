/// @file usiprotocolhandler_wait.cpp
/// @brief USIプロトコル応答待機メソッドの実装

#include "usiprotocolhandler.h"
#include "engineprocessmanager.h"
#include "logcategories.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <functional>

namespace {
constexpr int kPollIntervalMs = 10;
constexpr auto kWaitEventFlags = QEventLoop::ExcludeUserInputEvents;

using ConditionFn = std::function<bool()>;
using WakeConnectorFn = std::function<QMetaObject::Connection(QEventLoop&)>;

bool waitUntil(const ConditionFn& isDone,
               const ConditionFn& shouldAbort,
               int timeoutMs,
               const WakeConnectorFn& connectWake = {})
{
    if (isDone()) {
        return true;
    }
    if (timeoutMs <= 0) {
        return false;
    }

    QElapsedTimer elapsed;
    elapsed.start();

    while (!isDone()) {
        if (shouldAbort()) {
            return false;
        }

        const qint64 remaining = static_cast<qint64>(timeoutMs) - elapsed.elapsed();
        if (remaining <= 0) {
            return false;
        }

        QEventLoop loop;
        QTimer sliceTimer;
        sliceTimer.setSingleShot(true);

        QMetaObject::Connection wakeConn;
        const bool hasWakeConnector = static_cast<bool>(connectWake);
        if (hasWakeConnector) {
            wakeConn = connectWake(loop);
        }
        const QMetaObject::Connection sliceConn = QObject::connect(
            &sliceTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

        const int sliceMs = static_cast<int>(qMin<qint64>(kPollIntervalMs, remaining));
        sliceTimer.start(sliceMs);
        loop.exec(kWaitEventFlags);

        if (hasWakeConnector) {
            QObject::disconnect(wakeConn);
        }
        QObject::disconnect(sliceConn);
    }

    return true;
}

WakeConnectorFn makeWakeConnector(UsiProtocolHandler* self,
                                  void (UsiProtocolHandler::*signal)())
{
    return [self, signal](QEventLoop& loop) {
        return QObject::connect(self, signal, &loop, &QEventLoop::quit,
                                Qt::QueuedConnection);
    };
}
} // namespace

// ============================================================
// 待機メソッド
// ============================================================

bool UsiProtocolHandler::waitForResponseFlag(bool& flag,
                                             void(UsiProtocolHandler::*signal)(),
                                             int timeoutMs)
{
    if (flag) return true;
    if (timeoutMs <= 0) return false;

    // QEventLoop は waitUntil() 側で駆動する。
    flag = false;
    const quint64 expectedId = m_seq;
    const auto isDone = [&flag] { return flag; };
    const auto shouldAbort = [this, expectedId] {
        return m_seq != expectedId || shouldAbortWait();
    };

    return waitUntil(isDone,
                     shouldAbort,
                     timeoutMs,
                     makeWakeConnector(this, signal)) && flag;
}

bool UsiProtocolHandler::waitForUsiOk(int timeoutMs)
{
    return waitForResponseFlag(m_usiOkReceived,
                               &UsiProtocolHandler::usiOkReceived, timeoutMs);
}

bool UsiProtocolHandler::waitForReadyOk(int timeoutMs)
{
    return waitForResponseFlag(m_readyOkReceived,
                               &UsiProtocolHandler::readyOkReceived, timeoutMs);
}

bool UsiProtocolHandler::waitForBestMove(int timeoutMs)
{
    if (m_bestMoveReceived) return true;
    if (timeoutMs <= 0) return false;

    m_bestMoveReceived = false;
    const quint64 expectedId = m_seq;
    const auto isDone = [this] { return m_bestMoveReceived; };
    const auto shouldAbort = [this, expectedId] {
        return m_seq != expectedId || shouldAbortWait();
    };

    // waitUntil() は elapsed 時間を timeoutMs と比較して false を返す。
    const bool ok = waitUntil(isDone,
                              shouldAbort,
                              timeoutMs,
                              makeWakeConnector(this, &UsiProtocolHandler::bestMoveReceived));
    if (!ok) {
        return false;
    }
    return true;
}

bool UsiProtocolHandler::waitForBestMoveWithGrace(int budgetMs, int graceMs)
{
    const int hard = budgetMs + qMax(0, graceMs);
    if (hard <= 0) return false;

    m_bestMoveReceived = false;
    const quint64 expectedId = m_seq;
    const auto isDone = [this] { return m_bestMoveReceived; };
    const auto shouldAbort = [this, expectedId] {
        return m_seq != expectedId || shouldAbortWait();
    };

    return waitUntil(isDone,
                     shouldAbort,
                     hard,
                     makeWakeConnector(this, &UsiProtocolHandler::bestMoveReceived));
}

bool UsiProtocolHandler::keepWaitingForBestMove(int timeoutMs)
{
    if (timeoutMs <= 0) return false;

    m_bestMoveReceived = false;
    const quint64 expectedId = m_seq;
    const auto isDone = [this] { return m_bestMoveReceived; };
    const auto shouldAbort = [this, expectedId] {
        return m_seq != expectedId || shouldAbortWait();
    };

    const bool ok = waitUntil(isDone,
                              shouldAbort,
                              timeoutMs,
                              makeWakeConnector(this, &UsiProtocolHandler::bestMoveReceived));
    if (!ok && !shouldAbort()) {
        qCWarning(lcEngine) << "keepWaitingForBestMove: hard-timeout" << timeoutMs << "ms";
    }
    return ok;
}

void UsiProtocolHandler::waitForStopOrPonderhit()
{
    if (m_stopOrPonderhitPending) {
        m_stopOrPonderhitPending = false;
        return;
    }

    const quint64 expectedId = m_seq;
    static constexpr int kStopWaitTimeoutMs = 2000;
    const auto isDone = [this] { return m_stopOrPonderhitPending; };
    const auto shouldAbort = [this, expectedId] {
        return m_seq != expectedId || shouldAbortWait();
    };

    (void)waitUntil(isDone,
                    shouldAbort,
                    kStopWaitTimeoutMs,
                    makeWakeConnector(this, &UsiProtocolHandler::stopOrPonderhitSent));

    if (m_stopOrPonderhitPending) {
        m_stopOrPonderhitPending = false;
    }
}

bool UsiProtocolHandler::shouldAbortWait() const
{
    if (m_timeoutDeclared) return true;

    if (m_processManager) {
        if (m_processManager->shutdownState() !=
            EngineProcessManager::ShutdownState::Running) {
            return true;
        }
        if (!m_processManager->isRunning()) {
            return true;
        }
    }

    return false;
}
