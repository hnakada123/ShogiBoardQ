/// @file usi_async.cpp
/// @brief USI初期化と対局要求の非同期制御
#include "usi.h"
#include "usimatchhandler.h"
#include <utility>

bool Usi::startAndInitializeEngineAsync(const QString& engineFile, const QString& engineName)
{
    cancelCurrentOperation();
    QString error;
    if (!m_session->startAsync(engineFile, engineName, &error)) {
        emit errorOccurred(error);
        return false;
    }
    return true;
}

void Usi::onEngineInitialized()
{
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

bool Usi::deferUntilReady(std::function<void()> action) const
{
    if (!isInitializing()) return false;
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

void Usi::requestHumanReply(QString& position, const QString& ponder, const UsiMove& humanMove,
                            const UsiTimingParams& timing, QStringList& history)
{
    if (!position.contains(QStringLiteral(" moves"))) position += QStringLiteral(" moves");
    position += QLatin1Char(' ') + humanMove.toString();
    history.append(position);
    requestMatchMove(position, ponder, timing);
}
