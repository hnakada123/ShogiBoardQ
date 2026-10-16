/// @file usienginesession.cpp
/// @brief USI エンジン1台への接続の実装

#include "usienginesession.h"

#include <utility>

#include <QCoreApplication>
#include <QPointer>

#include "engineprocessmanager.h"
#include "engineprofilestore.h"
#include "logcategories.h"
#include "usiprotocolhandler.h"

UsiEngineSession::UsiEngineSession(QObject* parent)
    : QObject(parent)
    , m_process(new EngineProcessManager(this))
    , m_handler(new UsiProtocolHandler(m_process, this))
{
    m_startTimer.setSingleShot(true);
    m_stopTimer.setSingleShot(true);
    m_replyTimer.setSingleShot(true);
    connect(&m_startTimer, &QTimer::timeout, this, &UsiEngineSession::onStartTimeout);
    connect(&m_stopTimer, &QTimer::timeout, this, &UsiEngineSession::onStopTimer);
    connect(&m_replyTimer, &QTimer::timeout, this, &UsiEngineSession::searchTimedOut);
    connect(m_process, &EngineProcessManager::processStarted, this, &UsiEngineSession::onProcessStarted);
    connect(m_process, &EngineProcessManager::processExited, this, &UsiEngineSession::onProcessExited);
    connect(m_process, &EngineProcessManager::processError, this, &UsiEngineSession::onProcessError);
    connect(m_handler, &UsiProtocolHandler::initializationFinished, this, &UsiEngineSession::onInitialized);
    connect(m_handler, &UsiProtocolHandler::errorOccurred, this, &UsiEngineSession::onHandlerError);
    connect(m_handler, &UsiProtocolHandler::bestMoveReceived, this, &UsiEngineSession::onSearchFinished);
    connect(m_handler, &UsiProtocolHandler::checkmateReceived, this, &UsiEngineSession::onSearchFinished);
}

UsiEngineSession::~UsiEngineSession()
{
    quit();
}

// ============================================================
// 起動と終了
// ============================================================

void UsiEngineSession::startAsync(const QString& enginePath, const QString& engineName)
{
    quit();
    m_probeOnly = false;
    m_handler->setLaunchOptions(EngineProfileStore::launchOptions(engineName));
    launch(enginePath);
}

void UsiEngineSession::probeAsync(const QString& enginePath)
{
    quit();
    m_probeOnly = true;
    launch(enginePath);
}

void UsiEngineSession::launch(const QString& enginePath)
{
    m_lastError.clear();
    if (enginePath.isEmpty()) {
        m_lastError = QCoreApplication::translate("Usi", "Engine file path is empty.");
        failLaunch(enginePath);
        return;
    }
    m_handler->beginStartup();
    m_startTimer.start(m_initializationTimeoutMs);
    m_launching = true;
    const bool accepted = m_process->startProcessAsync(enginePath);
    m_launching = false;
    if (!accepted || !m_lastError.isEmpty()) failLaunch(enginePath);
}

void UsiEngineSession::failLaunch(const QString& enginePath)
{
    quit();
    if (m_lastError.isEmpty())
        m_lastError = QCoreApplication::translate("Usi", "Failed to start engine: %1").arg(enginePath);
    m_launchFailurePending = true;
    QMetaObject::invokeMethod(this, &UsiEngineSession::reportLaunchFailure, Qt::QueuedConnection);
}

void UsiEngineSession::reportLaunchFailure()
{
    if (!std::exchange(m_launchFailurePending, false)) return;
    emit errorOccurred(m_lastError);
}

void UsiEngineSession::onProcessStarted()
{
    if (m_handler->state() != UsiProtocolHandler::State::Starting) return;
    m_startTimer.stop();
    m_handler->initializeEngineAsync(m_initializationTimeoutMs, m_probeOnly);
}

void UsiEngineSession::onInitialized(bool success)
{
    if (!success) {
        quit();
        return;
    }
    emit ready();
    // 初期化の間に頼まれたことを順に行う。途中で取り消されたら（m_pending が空になる）残りはやめる
    const QPointer<UsiEngineSession> guard(this);
    while (guard && !m_pending.empty() && !isInitializing()) {
        auto action = std::move(m_pending.front());
        m_pending.erase(m_pending.begin());
        action();
    }
}

void UsiEngineSession::onStartTimeout()
{
    onHandlerError(QCoreApplication::translate("Usi", "Failed to start engine: %1")
                       .arg(m_process->currentEnginePath()));
}

void UsiEngineSession::onProcessExited()
{
    onHandlerError(QCoreApplication::translate("Usi", "Engine exited unexpectedly."));
}

void UsiEngineSession::quit()
{
    m_launchFailurePending = false;
    m_startTimer.stop();
    clearSearchTimers();
    m_pending.clear();
    m_handler->markStopped();
    m_process->stopProcessAsync();
}

bool UsiEngineSession::isRunning() const
{
    return m_process->isRunning();
}

bool UsiEngineSession::isInitializing() const
{
    return m_handler->isInitializing();
}

void UsiEngineSession::onProcessError(QProcess::ProcessError error, const QString& message)
{
    Q_UNUSED(error)
    onHandlerError(message);
}

void UsiEngineSession::onHandlerError(const QString& message)
{
    qCWarning(lcEngine) << "UsiEngineSession error:" << message;
    m_lastError = message;
    if (m_launching) return;
    quit();
    emit errorOccurred(message);
}

// ============================================================
// 探索
// ============================================================

void UsiEngineSession::whenReady(std::function<void()> action)
{
    if (isInitializing()) {
        m_pending.push_back(std::move(action));
        return;
    }
    action();
}

void UsiEngineSession::search(const UsiSearchRequest& request)
{
    // 初期化の後に始める
    if (isInitializing()) {
        m_pending.push_back([this, request]() { startRequestedSearch(request); });
        return;
    }
    startRequestedSearch(request);
}

void UsiEngineSession::startRequestedSearch(const UsiSearchRequest& request)
{
    clearSearchTimers();
    m_replyTimeoutMs = request.replyTimeoutMs;
    m_handler->startSearch(request.position, request.go);
    if (request.stopAfterMs > 0) m_stopTimer.start(request.stopAfterMs);
    else if (request.replyTimeoutMs > 0) m_replyTimer.start(request.replyTimeoutMs);
}

void UsiEngineSession::stop()
{
    if (isInitializing()) {
        // まだ送っていない探索を取りやめる
        m_pending.clear();
        return;
    }
    m_stopTimer.stop();
    if (!m_handler->isSearching()) return;
    m_handler->sendStop();
    if (m_replyTimeoutMs > 0 && !m_replyTimer.isActive()) m_replyTimer.start(m_replyTimeoutMs);
}

void UsiEngineSession::onStopTimer()
{
    stop();
}

void UsiEngineSession::onSearchFinished()
{
    clearSearchTimers();
}

void UsiEngineSession::clearSearchTimers()
{
    m_stopTimer.stop();
    m_replyTimer.stop();
}

void UsiEngineSession::cancelCurrentOperation()
{
    m_startTimer.stop();
    clearSearchTimers();
    m_pending.clear();
    m_handler->cancelCurrentOperation();
}
