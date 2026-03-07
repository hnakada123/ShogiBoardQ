/// @file engineprocessmanager_wait.cpp
/// @brief 将棋エンジンプロセス管理クラスの待機・遷移実装

#include "engineprocessmanager.h"

#include "logcategories.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <functional>
#include <utility>

namespace {
constexpr int kStartTimeoutMs = 5000;

class TransitionScopeGuard
{
public:
    explicit TransitionScopeGuard(bool& flag)
        : m_flag(flag)
    {
        m_flag = true;
    }

    ~TransitionScopeGuard()
    {
        m_flag = false;
    }

private:
    bool& m_flag;
};

class ProcessConditionWaiter : public QObject
{
public:
    using Condition = std::function<bool()>;

    ProcessConditionWaiter(QEventLoop* loop, Condition condition)
        : m_loop(loop)
        , m_condition(std::move(condition))
    {
    }

    void onStateChanged(QProcess::ProcessState)
    {
        quitIfDone();
    }

    void onStarted()
    {
        quitIfDone();
    }

    void onProcessError(QProcess::ProcessError)
    {
        quitLoop();
    }

    void onFinished(int, QProcess::ExitStatus)
    {
        quitLoop();
    }

private:
    void quitIfDone()
    {
        if (m_loop && m_condition && m_condition()) {
            m_loop->quit();
        }
    }

    void quitLoop()
    {
        if (m_loop) {
            m_loop->quit();
        }
    }

    QEventLoop* m_loop = nullptr;
    Condition m_condition;
};

class ReadyReadWaiter : public QObject
{
public:
    ReadyReadWaiter(EngineProcessManager* owner, QEventLoop* loop, bool* ready)
        : m_owner(owner)
        , m_loop(loop)
        , m_ready(ready)
    {
    }

    void onReadyRead()
    {
        if (m_ready) {
            *m_ready = true;
        }
        if (m_owner) {
            m_owner->pumpPendingOutput();
        }
        quitLoop();
    }

    void onProcessError(QProcess::ProcessError)
    {
        quitLoop();
    }

    void onFinished(int, QProcess::ExitStatus)
    {
        quitLoop();
    }

private:
    void quitLoop()
    {
        if (m_loop) {
            m_loop->quit();
        }
    }

    EngineProcessManager* m_owner = nullptr;
    QEventLoop* m_loop = nullptr;
    bool* m_ready = nullptr;
};

bool waitForProcessCondition(QProcess& process,
                             const ProcessConditionWaiter::Condition& isDone,
                             int timeoutMs)
{
    if (isDone()) {
        return true;
    }
    if (timeoutMs <= 0) {
        return false;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    ProcessConditionWaiter waiter(&loop, isDone);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&process, &QProcess::stateChanged,
                     &waiter, &ProcessConditionWaiter::onStateChanged);
    QObject::connect(&process, &QProcess::started,
                     &waiter, &ProcessConditionWaiter::onStarted);
    QObject::connect(&process,
                     QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
                     &waiter,
                     &ProcessConditionWaiter::onProcessError);
    QObject::connect(&process,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     &waiter,
                     &ProcessConditionWaiter::onFinished);

    timer.start(timeoutMs);
    while (timer.isActive() && !isDone()) {
        loop.exec(QEventLoop::AllEvents);
    }
    return isDone();
}

bool waitForStartedResponsive(QProcess& process, int timeoutMs)
{
    return waitForProcessCondition(
        process,
        [&process]() { return process.state() == QProcess::Running; },
        timeoutMs);
}

bool waitForFinishedResponsive(QProcess& process, int timeoutMs)
{
    return waitForProcessCondition(
        process,
        [&process]() { return process.state() == QProcess::NotRunning; },
        timeoutMs);
}
} // namespace

bool EngineProcessManager::startProcess(const QString& engineFile)
{
    if (m_transitionInProgress) {
        const QString msg = tr("Engine transition is already in progress.");
        qCWarning(lcEngine) << logPrefix() << msg;
        emit processError(QProcess::UnknownError, msg);
        return false;
    }

    m_currentEnginePath = engineFile;

    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        emit processError(QProcess::FailedToStart,
                          tr("Engine file does not exist: %1").arg(engineFile));
        return false;
    }

    if (m_process) {
        stopProcess();
    }

    m_stopRequestedDuringTransition = false;
    {
        TransitionScopeGuard transitionGuard(m_transitionInProgress);

        m_startupErrorReported = false;
        m_process = std::make_unique<QProcess>();

        QFileInfo engineFileInfo(engineFile);
        m_process->setWorkingDirectory(engineFileInfo.absolutePath());

        connect(m_process.get(), &QProcess::readyReadStandardOutput,
                this, &EngineProcessManager::onReadyReadStdout);
        connect(m_process.get(), &QProcess::readyReadStandardError,
                this, &EngineProcessManager::onReadyReadStderr);
        connect(m_process.get(), &QProcess::errorOccurred,
                this, &EngineProcessManager::onProcessError);
        connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &EngineProcessManager::onProcessFinished);

        m_process->start(engineFile, QStringList(), QIODevice::ReadWrite);

        if (!waitForStartedResponsive(*m_process, kStartTimeoutMs)) {
            if (!m_startupErrorReported) {
                const QString errorMsg = tr("Failed to start engine: %1").arg(engineFile);
                emit processError(QProcess::FailedToStart, errorMsg);
            }

            m_process.reset();
            return false;
        }
    }

    if (m_stopRequestedDuringTransition) {
        qCWarning(lcEngine) << logPrefix()
                            << "起動待機中に停止要求を受信したため、起動直後に停止します";
        m_stopRequestedDuringTransition = false;
        stopProcess();
        return false;
    }

    m_shutdownState = ShutdownState::Running;
    return true;
}

void EngineProcessManager::stopProcess()
{
    if (m_transitionInProgress) {
        m_stopRequestedDuringTransition = true;
        qCDebug(lcEngine) << logPrefix() << "遷移中のため stopProcess を遅延します";
        return;
    }
    if (!m_process) return;

    TransitionScopeGuard transitionGuard(m_transitionInProgress);

    disconnect(m_process.get(), nullptr, this, nullptr);

    if (m_process->state() == QProcess::Running) {
        if (!waitForFinishedResponsive(*m_process, 3000)) {
            m_process->terminate();
            if (!waitForFinishedResponsive(*m_process, 1000)) {
                m_process->kill();
                if (!waitForFinishedResponsive(*m_process, 2000)) {
                    qCWarning(lcEngine) << logPrefix()
                                        << "プロセス終了待機がタイムアウトしたため、強制的に解放します";
                }
            }
        }
    }

    m_process.reset();

    m_shutdownState = ShutdownState::Running;
    m_postQuitInfoStringLinesLeft = 0;
    m_currentEnginePath.clear();
    m_startupErrorReported = false;
    m_stopRequestedDuringTransition = false;
}

bool EngineProcessManager::waitForReadyReadAndPump(int timeoutMs)
{
    if (!m_process) return false;
    if (m_process->state() != QProcess::Running && m_process->state() != QProcess::Starting) {
        pumpPendingOutput();
        return false;
    }

    pumpPendingOutput();
    if (m_process->bytesAvailable() > 0) {
        return true;
    }

    QProcess* process = m_process.get();
    bool ready = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    ReadyReadWaiter waiter(this, &loop, &ready);

    connect(process, &QProcess::readyReadStandardOutput,
            &waiter, &ReadyReadWaiter::onReadyRead);
    connect(process, &QProcess::readyReadStandardError,
            &waiter, &ReadyReadWaiter::onReadyRead);
    connect(process,
            QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
            &waiter,
            &ReadyReadWaiter::onProcessError);
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            &waiter,
            &ReadyReadWaiter::onFinished);
    connect(process, QOverload<QObject*>::of(&QObject::destroyed),
            &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    while (timer.isActive()
           && !ready
           && m_process.get() == process
           && (process->state() == QProcess::Running
               || process->state() == QProcess::Starting)) {
        loop.exec(QEventLoop::AllEvents);
    }

    pumpPendingOutput();
    return ready;
}
