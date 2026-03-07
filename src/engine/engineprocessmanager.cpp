/// @file engineprocessmanager.cpp
/// @brief 将棋エンジンプロセス管理クラスの実装

#include "engineprocessmanager.h"
#include "logcategories.h"

#include <QTimer>

EngineProcessManager::EngineProcessManager(QObject* parent)
    : QObject(parent)
{
}

EngineProcessManager::~EngineProcessManager()
{
    stopProcess();
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

    for (const QString& l : std::as_const(lines)) {
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
            for (const QString& line : std::as_const(lines)) {
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
