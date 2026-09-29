/// @file usi_async.cpp
/// @brief USI初期化と対局要求の非同期制御
#include "usi.h"
#include "usimatchhandler.h"
#include <utility>

bool Usi::startAndInitializeEngineAsync(const QString& engineFile, const QString& engineName)
{
    cancelCurrentOperation();
    m_protocolHandler->loadEngineOptions(engineName);
    m_initializing = true;
    m_startTimer.start(5000);
    if (!m_processManager->startProcessAsync(engineFile)) {
        cancelCurrentOperation();
        return false;
    }
    return true;
}

void Usi::onProcessStarted()
{
    if (!m_initializing) return;
    m_startTimer.stop();
    m_protocolHandler->initializeEngineAsync();
}

void Usi::onEngineInitialized(bool success)
{
    if (!m_initializing) return;
    m_initializing = false;
    if (!success) { cleanupEngineProcessAndThread(); return; }
    auto actions = std::exchange(m_pendingActions, {});
    const QPointer<Usi> guard(this);
    const auto generation = m_asyncGeneration;
    for (auto& action : actions) {
        action();
        if (!guard || generation != m_asyncGeneration) return;
    }
    emit engineInitialized();
}

void Usi::onProtocolError(const QString& message)
{
    cleanupEngineProcessAndThread();
    emit errorOccurred(message);
}

void Usi::onProcessExited()
{
    onProtocolError(tr("Engine exited unexpectedly."));
}

void Usi::onStartTimeout()
{
    const QString message = tr("Failed to start engine: %1").arg(currentEnginePath());
    cleanupEngineProcessAndThread();
    emit errorOccurred(message);
}

bool Usi::deferUntilReady(std::function<void()> action) const
{
    if (!m_initializing) return false;
    m_pendingActions.push_back(std::move(action));
    return true;
}

void Usi::requestMatchMove(const QString& position, const QString& ponder, const UsiTimingParams& timing)
{
    if (deferUntilReady([this, position, ponder, timing]() { requestMatchMove(position, ponder, timing); })) return;
    m_considerationModel = nullptr;
    m_matchHandler->setClock(m_matchClock);
    m_matchHandler->requestMove(position, ponder, timing);
}

void Usi::requestHumanReply(QString& position, const QString& ponder, const QPoint& from, const QPoint& to,
                            const UsiTimingParams& timing, QStringList& history)
{
    const auto move = m_matchHandler->convertHumanMoveToUsiFormat(from, to, m_gameController->promote());
    m_gameController->setPromote(false);
    if (!position.contains(QStringLiteral(" moves"))) position += QStringLiteral(" moves");
    position += QLatin1Char(' ') + move;
    history.append(position);
    requestMatchMove(position, ponder, timing);
}
