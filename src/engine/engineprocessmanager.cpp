/// @file engineprocessmanager.cpp
/// @brief 将棋エンジンプロセス管理クラスの実装

#include "engineprocessmanager.h"
#include "usi.h"
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>

namespace {
constexpr int kStartTimeoutMs = 5000;
constexpr int kWaitSliceMs = 50;
const QEventLoop::ProcessEventsFlags kWaitPumpFlags =
    QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers;

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

void pumpResponsiveEvents(int maxMs)
{
    QCoreApplication::processEvents(kWaitPumpFlags, maxMs);
}

template<typename DoneFn, typename WaitFn>
bool waitForProcessResponsive(const DoneFn& isDone, const WaitFn& waitSlice, int timeoutMs)
{
    if (isDone()) return true;
    if (timeoutMs <= 0) return isDone();

    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired() && !isDone()) {
        pumpResponsiveEvents(1);
        if (isDone()) {
            return true;
        }

        const qint64 remainingMs = deadline.remainingTime();
        const int sliceMs = (remainingMs > 0)
                                ? qMax(1, qMin(kWaitSliceMs, static_cast<int>(remainingMs)))
                                : 1;
        waitSlice(sliceMs);
        pumpResponsiveEvents(1);
    }

    return isDone();
}

bool waitForStartedResponsive(QProcess& process, int timeoutMs)
{
    return waitForProcessResponsive(
        [&process]() { return process.state() == QProcess::Running; },
        [&process](int sliceMs) { (void)process.waitForStarted(sliceMs); },
        timeoutMs);
}

bool waitForFinishedResponsive(QProcess& process, int timeoutMs)
{
    return waitForProcessResponsive(
        [&process]() { return process.state() == QProcess::NotRunning; },
        [&process](int sliceMs) { (void)process.waitForFinished(sliceMs); },
        timeoutMs);
}
} // namespace

EngineProcessManager::EngineProcessManager(QObject* parent)
    : QObject(parent)
{
}

EngineProcessManager::~EngineProcessManager()
{
    stopProcess();
}
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

bool EngineProcessManager::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

QProcess::ProcessState EngineProcessManager::state() const
{
    return m_process ? m_process->state() : QProcess::NotRunning;
}

void EngineProcessManager::sendCommand(const QString& command)
{
    if (!m_process || 
        (m_process->state() != QProcess::Running && 
         m_process->state() != QProcess::Starting)) {
        qCWarning(lcEngine) << logPrefix() << "プロセス未準備、コマンド無視:" << command;
        return;
    }

    if (m_process->write((command + "\n").toUtf8()) == -1) {
        qCWarning(lcEngine) << logPrefix() << "コマンド送信失敗:" << command;
        return;
    }

    emit commandSent(command);
    qCDebug(lcEngine).nospace() << logPrefix() << " 送信: " << command;
}

void EngineProcessManager::closeWriteChannel()
{
    if (m_process) {
        m_process->closeWriteChannel();
    }
}

void EngineProcessManager::pumpPendingOutput()
{
    if (!m_process) return;

    int loops = 0;
    while (m_process && m_process->canReadLine() && loops < 16) {
        onReadyReadStdout();
        ++loops;
    }
    onReadyReadStderr();
}

bool EngineProcessManager::waitForReadyReadAndPump(int timeoutMs)
{
    if (!m_process) return false;
    if (m_process->state() != QProcess::Running && m_process->state() != QProcess::Starting) {
        pumpPendingOutput();
        return false;
    }

    const bool ready = m_process->waitForReadyRead(timeoutMs);
    pumpPendingOutput();
    return ready;
}

void EngineProcessManager::setShutdownState(ShutdownState state)
{
    m_shutdownState = state;
}

void EngineProcessManager::setPostQuitInfoStringLinesLeft(int count)
{
    m_postQuitInfoStringLinesLeft = count;
}

void EngineProcessManager::decrementPostQuitLines()
{
    if (m_postQuitInfoStringLinesLeft > 0) {
        --m_postQuitInfoStringLinesLeft;
        if (m_postQuitInfoStringLinesLeft <= 0) {
            m_shutdownState = ShutdownState::IgnoreAll;
        }
    }
}

void EngineProcessManager::discardStdout()
{
    if (m_process) {
        (void)m_process->readAllStandardOutput();
    }
}

void EngineProcessManager::discardStderr()
{
    if (m_process) {
        (void)m_process->readAllStandardError();
    }
}

bool EngineProcessManager::canReadLine() const
{
    return m_process && m_process->canReadLine();
}

QByteArray EngineProcessManager::readLine()
{
    return m_process ? m_process->readLine() : QByteArray();
}

QByteArray EngineProcessManager::readAllStderr()
{
    return m_process ? m_process->readAllStandardError() : QByteArray();
}

void EngineProcessManager::setLogIdentity(const QString& engineTag,
                                          const QString& sideTag,
                                          const QString& engineName)
{
    m_logEngineTag = engineTag;
    m_logSideTag = sideTag;
    m_logEngineName = engineName;
}

QString EngineProcessManager::logPrefix() const
{
    QString within = m_logEngineTag.isEmpty() ? "[E?]" : m_logEngineTag;
    if (!m_logSideTag.isEmpty()) {
        within.insert(within.size() - 1, "/" + m_logSideTag);
    }

    QString head = within;
    if (!m_logEngineName.isEmpty()) {
        head += " " + m_logEngineName;
    }

    return head;
}

void EngineProcessManager::onReadyReadStdout()
{
    if (!m_process) return;
    QPointer<EngineProcessManager> guard(this);

    m_processedLines = 0;

    while (m_process && m_process->canReadLine() && m_processedLines < kMaxLinesPerChunk) {
        const QByteArray data = m_process->readLine();
        QString line = QString::fromUtf8(data).trimmed();

        if (line.isEmpty()) {
            ++m_processedLines;
            continue;
        }

        if (line.startsWith(QStringLiteral("id name "))) {
            const QString name = line.mid(8).trimmed();
            if (!name.isEmpty() && m_logEngineName.isEmpty()) {
                m_logEngineName = name;
                if (!guard) return;
            }
        }

        if (m_shutdownState == ShutdownState::IgnoreAll) {
            qCDebug(lcEngine).nospace() << logPrefix() << " [drop-ignore-all] " << line;
            ++m_processedLines;
            continue;
        }

        if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
            if (line.startsWith(QStringLiteral("info string")) &&
                m_postQuitInfoStringLinesLeft > 0) {
                emit dataReceived(line);
                if (!guard) return;
                decrementPostQuitLines();
            } else {
                qCDebug(lcEngine).nospace() << logPrefix() << " [drop-after-quit] " << line;
            }
            ++m_processedLines;
            continue;
        }

        emit dataReceived(line);
        if (!guard) return;
        qCDebug(lcEngine).nospace() << logPrefix() << " 受信: " << line;

        ++m_processedLines;
    }

    scheduleMoreReading();
}

void EngineProcessManager::onReadyReadStderr()
{
    if (!m_process) return;

    if (m_shutdownState == ShutdownState::IgnoreAll) {
        discardStderr();
        return;
    }

    const QByteArray data = m_process->readAllStandardError();
    if (data.isEmpty()) return;

    const QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const QString& l : lines) {
        const QString line = l.trimmed();
        if (line.isEmpty()) continue;

        if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
            if (m_postQuitInfoStringLinesLeft > 0 &&
                line.startsWith(QStringLiteral("info string"))) {
                emit stderrReceived(line);
                decrementPostQuitLines();
            }
            continue;
        }

        emit stderrReceived(line);
        qCDebug(lcEngine).nospace() << logPrefix() << " stderr: " << line;
    }
}

void EngineProcessManager::onProcessError(QProcess::ProcessError error)
{
    m_startupErrorReported = true;

    QString errorMessage;

    switch (error) {
    case QProcess::FailedToStart:
        errorMessage = tr("The process failed to start.");
        qCCritical(lcEngine) << logPrefix() << errorMessage << "path=" << m_currentEnginePath;
        break;
    case QProcess::Crashed:
        errorMessage = tr("The process crashed.");
        qCCritical(lcEngine) << logPrefix() << errorMessage;
        break;
    case QProcess::Timedout:
        errorMessage = tr("The process timed out.");
        qCWarning(lcEngine) << logPrefix() << errorMessage;
        break;
    case QProcess::WriteError:
        errorMessage = tr("An error occurred while writing data.");
        qCWarning(lcEngine) << logPrefix() << errorMessage;
        break;
    case QProcess::ReadError:
        errorMessage = tr("An error occurred while reading data.");
        qCWarning(lcEngine) << logPrefix() << errorMessage;
        break;
    case QProcess::UnknownError:
    default:
        errorMessage = tr("An unknown error occurred.");
        qCWarning(lcEngine) << logPrefix() << errorMessage;
        break;
    }

    emit processError(error, errorMessage);
}

void EngineProcessManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    if (m_process) {
        QPointer<EngineProcessManager> guard(this);
        const QByteArray outTail = m_process->readAllStandardOutput();
        m_process->readAllStandardError();

        if (!outTail.isEmpty()) {
            const QStringList lines = QString::fromUtf8(outTail).split('\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                const QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() &&
                    m_shutdownState != ShutdownState::IgnoreAll) {
                    emit dataReceived(trimmed);
                    if (!guard) return;
                }
            }
        }
    }

    m_shutdownState = ShutdownState::IgnoreAll;
}

void EngineProcessManager::scheduleMoreReading()
{
    if (m_process && m_process->canReadLine()) {
        QTimer::singleShot(0, this, &EngineProcessManager::onReadyReadStdout);
    }
}
