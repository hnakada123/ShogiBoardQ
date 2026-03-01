/// @file usiprotocolhandler_wait.cpp
/// @brief USIプロトコル応答待機メソッドの実装

#include "usiprotocolhandler.h"
#include "engineprocessmanager.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QThread>

// ============================================================
// 待機メソッド
// ============================================================

bool UsiProtocolHandler::waitForResponseFlag(bool& flag,
                                             void(UsiProtocolHandler::*signal)(),
                                             int timeoutMs)
{
    if (flag) return true;
    flag = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, signal, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

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

    QElapsedTimer timer;
    timer.start();

    static constexpr int kSliceMs = 10;

    while (!m_bestMoveReceived) {
        if (timer.elapsed() >= timeoutMs) {
            return false;
        }
        if (shouldAbortWait()) {
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

    while (t.elapsed() < hard) {
        if (m_seq != expectedId || shouldAbortWait()) return false;

        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (m_bestMoveReceived) return true;

        QThread::msleep(1);
    }
    return m_bestMoveReceived;
}

bool UsiProtocolHandler::keepWaitingForBestMove()
{
    m_bestMoveReceived = false;

    while (!m_bestMoveReceived) {
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
    QEventLoop loop;
    connect(this, &UsiProtocolHandler::stopOrPonderhitSent, &loop, &QEventLoop::quit);
    loop.exec();
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
