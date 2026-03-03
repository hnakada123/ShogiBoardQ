/// @file usiprotocolhandler_wait.cpp
/// @brief USIプロトコル応答待機メソッドの実装

#include "usiprotocolhandler.h"
#include "engineprocessmanager.h"
#include "logcategories.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

// ============================================================
// 待機メソッド
// ============================================================

bool UsiProtocolHandler::waitForResponseFlag(bool& flag,
                                             void(UsiProtocolHandler::*signal)(),
                                             int timeoutMs)
{
    if (flag) return true;
    if (timeoutMs <= 0) return false;
    flag = false;

    const quint64 expectedId = m_seq;
    QElapsedTimer timer;
    timer.start();

    static constexpr int kSliceMs = 10;

    while (!flag) {
        if (timer.elapsed() >= timeoutMs) {
            return false;
        }
        if (m_seq != expectedId || shouldAbortWait()) {
            return false;
        }

        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(kSliceMs);
        connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        connect(this, signal, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);
    }

    return flag;
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

    QElapsedTimer timer;
    timer.start();

    static constexpr int kSliceMs = 10;

    while (!m_bestMoveReceived) {
        if (timer.elapsed() >= timeoutMs) {
            return false;
        }
        if (m_seq != expectedId || shouldAbortWait()) {
            return false;
        }

        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(kSliceMs);
        QObject::connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);
    }

    return true;
}

bool UsiProtocolHandler::waitForBestMoveWithGrace(int budgetMs, int graceMs)
{
    QElapsedTimer t;
    t.start();
    const qint64 hard = budgetMs + qMax(0, graceMs);

    m_bestMoveReceived = false;
    const quint64 expectedId = m_seq;

    static constexpr int kSliceMs = 10;

    while (t.elapsed() < hard) {
        if (m_seq != expectedId || shouldAbortWait()) return false;
        if (m_bestMoveReceived) return true;

        const qint64 remaining = hard - t.elapsed();
        if (remaining <= 0) break;
        const int slice = static_cast<int>(qMin<qint64>(kSliceMs, remaining));

        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(slice);
        QObject::connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);
    }
    return m_bestMoveReceived;
}

bool UsiProtocolHandler::keepWaitingForBestMove(int timeoutMs)
{
    if (timeoutMs <= 0) return false;

    m_bestMoveReceived = false;
    const quint64 expectedId = m_seq;
    QElapsedTimer timer;
    timer.start();

    while (!m_bestMoveReceived) {
        if (timer.elapsed() >= timeoutMs) {
            qCWarning(lcEngine) << "keepWaitingForBestMove: hard-timeout" << timeoutMs << "ms";
            return false;
        }
        if (m_seq != expectedId || shouldAbortWait()) {
            return false;
        }

        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(10);
        QObject::connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);

        if (shouldAbortWait()) {
            return false;
        }
    }

    return true;
}

void UsiProtocolHandler::waitForStopOrPonderhit()
{
    if (m_stopOrPonderhitPending) {
        m_stopOrPonderhitPending = false;
        return;
    }

    const quint64 expectedId = m_seq;
    QElapsedTimer timer;
    timer.start();

    static constexpr int kSliceMs = 10;
    static constexpr int kStopWaitTimeoutMs = 2000;

    while (!m_stopOrPonderhitPending) {
        if (timer.elapsed() >= kStopWaitTimeoutMs) {
            return;
        }
        if (m_seq != expectedId || shouldAbortWait()) {
            return;
        }

        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(kSliceMs);
        connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        connect(this, &UsiProtocolHandler::stopOrPonderhitSent, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);
    }

    m_stopOrPonderhitPending = false;
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
