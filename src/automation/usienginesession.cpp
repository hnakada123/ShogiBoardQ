/// @file usienginesession.cpp
/// @brief GUI モデルを持たない USI エンジン接続の実装

#include "usienginesession.h"

#include "engineprocessmanager.h"
#include "logcategories.h"
#include "usiprotocolhandler.h"

UsiEngineSession::UsiEngineSession(QObject* parent)
    : QObject(parent)
    , m_process(new EngineProcessManager(this))
    , m_handler(new UsiProtocolHandler(this))
{
    m_handler->setProcessManager(m_process);
    connect(m_process, &EngineProcessManager::dataReceived,
            m_handler, &UsiProtocolHandler::onDataReceived);
    connect(m_process, &EngineProcessManager::processError,
            this, &UsiEngineSession::onProcessError);
    connect(m_handler, &UsiProtocolHandler::errorOccurred,
            this, &UsiEngineSession::onHandlerError);
}

UsiEngineSession::~UsiEngineSession()
{
    quit();
}

bool UsiEngineSession::start(const QString& enginePath, const QString& engineName, QString* error)
{
    m_lastError.clear();
    m_starting = true;
    bool ok = m_process->startProcess(enginePath);
    if (ok) {
        m_handler->loadEngineOptions(engineName);
        ok = m_handler->initializeEngine(engineName);
    }
    m_starting = false;
    if (!ok) {
        quit();
        if (error) {
            *error = m_lastError.isEmpty()
                ? QStringLiteral("Failed to start or initialize the engine: %1").arg(enginePath)
                : m_lastError;
        }
        return false;
    }
    return true;
}

void UsiEngineSession::quit()
{
    if (m_process->isRunning()) {
        m_handler->sendQuit();
    }
    m_process->stopProcess();
}

bool UsiEngineSession::isRunning() const
{
    return m_process->isRunning();
}

void UsiEngineSession::onProcessError(QProcess::ProcessError error, const QString& message)
{
    Q_UNUSED(error)
    qCWarning(lcEngine) << "UsiEngineSession process error:" << message;
    m_lastError = message;
    if (!m_starting) emit errorOccurred(message);
}

void UsiEngineSession::onHandlerError(const QString& message)
{
    qCWarning(lcEngine) << "UsiEngineSession protocol error:" << message;
    m_lastError = message;
    if (!m_starting) emit errorOccurred(message);
}
