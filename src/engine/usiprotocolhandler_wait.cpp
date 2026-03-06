/// @file usiprotocolhandler_wait.cpp
/// @brief USIプロトコル応答待機メソッドの実装

#include "usiprotocolhandler.h"
#include "engineprocessmanager.h"
#include "logcategories.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <functional>

namespace {
constexpr int kWakeCheckIntervalMs = 50;
const QEventLoop::ProcessEventsFlags kPumpFlags =
    QEventLoop::ExcludeSocketNotifiers;

using ConditionFn = std::function<bool()>;

void pumpEventsSlice(int maxMs)
{
    QCoreApplication::processEvents(kPumpFlags, maxMs);
}

bool waitUntil(const ConditionFn& isDone,
               const ConditionFn& shouldAbort,
               int timeoutMs,
               EngineProcessManager* processManager)
{
    if (isDone()) {
        return true;
    }
    if (timeoutMs <= 0) {
        return false;
    }

    if (processManager != nullptr) {
        processManager->pumpPendingOutput();
    }

    if (isDone() || shouldAbort()) {
        return isDone() && !shouldAbort();
    }

    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired() && !isDone() && !shouldAbort()) {
        if (processManager != nullptr) {
            processManager->pumpPendingOutput();
        }
        pumpEventsSlice(1);
        if (isDone() || shouldAbort()) {
            break;
        }

        const qint64 remainingMs = deadline.remainingTime();
        const int waitMs = (remainingMs > 0)
                               ? qMax(1, qMin(kWakeCheckIntervalMs, static_cast<int>(remainingMs)))
                               : 1;
        if (processManager != nullptr) {
            (void)processManager->waitForReadyReadAndPump(waitMs);
        } else {
            pumpEventsSlice(waitMs);
        }
    }

    if (processManager != nullptr) {
        // タイムアウト時でも stderr / 端数出力を取りこぼさない
        processManager->pumpPendingOutput();
    }
    pumpEventsSlice(1);

    return !deadline.hasExpired() && !shouldAbort() && isDone();
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
    Q_UNUSED(signal)

    // waitUntil() が QTimer + QEventLoop でイベント駆動待機し、フラグ更新を待機する。
    flag = false;
    const quint64 expectedId = m_seq;
    const auto isDone = [&flag] { return flag; };
    const auto shouldAbort = [this, expectedId] {
        return m_seq != expectedId || shouldAbortWait();
    };

    if (m_processManager != nullptr) {
        m_processManager->pumpPendingOutput();
    }

    return waitUntil(isDone,
                     shouldAbort,
                     timeoutMs,
                     m_processManager) && flag;
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
                              m_processManager);
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
                     m_processManager);
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
                              m_processManager);
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
                    m_processManager);

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
