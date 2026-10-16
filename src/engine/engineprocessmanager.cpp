/// @file engineprocessmanager.cpp
/// @brief 将棋エンジンプロセス管理クラスの実装

#include "engineprocessmanager.h"
#include "logcategories.h"

#include <QTimer>
#include <QPointer>
#include <QCoreApplication>
#include <QFileInfo>

EngineProcessManager::EngineProcessManager(QObject* parent)
    : QObject(parent)
{
}

EngineProcessManager::~EngineProcessManager()
{
    stopProcessAsync();
}

bool EngineProcessManager::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
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
    QString head = QStringLiteral("[") + (m_logEngineTag.isEmpty() ? QStringLiteral("E?") : m_logEngineTag);
    if (!m_logSideTag.isEmpty()) head += QLatin1Char('/') + m_logSideTag;
    head += QLatin1Char(']');
    if (!m_logEngineName.isEmpty()) head += QLatin1Char(' ') + m_logEngineName;
    return head;
}

void EngineProcessManager::onReadyReadStdout()
{
    if (!m_process) return;
    QPointer<EngineProcessManager> guard(this);
    int processedLines = 0;
    while (m_process && m_process->canReadLine() && processedLines < kMaxLinesPerChunk) {
        const QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        ++processedLines;
        if (line.isEmpty()) continue;
        qCDebug(lcEngine).nospace() << logPrefix() << " 受信: " << line;
        emit dataReceived(line);
        if (!guard) return;
    }
    scheduleMoreReading();
}

void EngineProcessManager::onReadyReadStderr()
{
    if (!m_process) return;
    const QStringList lines = QString::fromUtf8(m_process->readAllStandardError()).split('\n', Qt::SkipEmptyParts);
    for (const QString& l : lines) {
        const QString line = l.trimmed();
        if (line.isEmpty()) continue;
        qCDebug(lcEngine).nospace() << logPrefix() << " stderr: " << line;
        emit stderrReceived(line);
    }
}

void EngineProcessManager::onProcessError(QProcess::ProcessError error)
{
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
    // 手放したプロセスは切り離してあるので、ここに来るのは手放す前に終わったとき
    QPointer<EngineProcessManager> guard(this);
    if (m_process) {
        const QStringList lines = QString::fromUtf8(m_process->readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            emit dataReceived(trimmed);
            if (!guard) return;
        }
    }
    emit processExited();
}

void EngineProcessManager::scheduleMoreReading()
{
    if (m_process && m_process->canReadLine()) {
        QTimer::singleShot(0, this, &EngineProcessManager::onReadyReadStdout);
    }
}

// ============================================================
// エンジンプロセスの非同期起動・終了
// ============================================================

namespace {
// 呼出元の破棄後もquit→terminate→killをイベント駆動で完了させる。
class RetiringEngineProcess : public QObject
{
public:
    explicit RetiringEngineProcess(QProcess* process)
        : QObject(QCoreApplication::instance()), m_process(process)
    {
        m_process->setParent(this);
        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, &RetiringEngineProcess::advance);
        connect(m_process, &QProcess::finished, this, &QObject::deleteLater);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &RetiringEngineProcess::drain);
        connect(m_process, &QProcess::readyReadStandardError, this, &RetiringEngineProcess::drain);
        if (m_process->state() == QProcess::NotRunning) deleteLater();
        else m_timer.start(3000);
    }
private:
    void drain() { m_process->readAllStandardOutput(); m_process->readAllStandardError(); }
    void advance()
    {
        if (m_process->state() == QProcess::NotRunning) { deleteLater(); return; }
        if (m_terminated) m_process->kill();
        else { m_terminated = true; m_process->terminate(); }
        m_timer.start(1000);
    }
    QProcess* m_process;
    QTimer m_timer;
    bool m_terminated = false;
};
}

bool EngineProcessManager::startProcessAsync(const QString& engineFile)
{
    if (engineFile.isEmpty() || !QFileInfo(engineFile).isFile() || !QFileInfo(engineFile).isExecutable()) {
        emit processError(QProcess::FailedToStart, tr("Failed to start engine: %1").arg(engineFile));
        return false;
    }
    stopProcessAsync();
    m_currentEnginePath = engineFile;
    m_process = std::make_unique<QProcess>();
    m_process->setWorkingDirectory(QFileInfo(engineFile).absolutePath());
    connect(m_process.get(), &QProcess::started, this, &EngineProcessManager::processStarted);
    connect(m_process.get(), &QProcess::readyReadStandardOutput, this, &EngineProcessManager::onReadyReadStdout);
    connect(m_process.get(), &QProcess::readyReadStandardError, this, &EngineProcessManager::onReadyReadStderr);
    connect(m_process.get(), &QProcess::errorOccurred, this, &EngineProcessManager::onProcessError);
    connect(m_process.get(), &QProcess::finished, this, &EngineProcessManager::onProcessFinished);
    m_process->start(engineFile, {}, QIODevice::ReadWrite);
    return true;
}

void EngineProcessManager::stopProcessAsync()
{
    if (!m_process) return;
    auto* process = m_process.release();
    disconnect(process, nullptr, this, nullptr);
    if (process->state() == QProcess::Running) process->write("quit\n");
    new RetiringEngineProcess(process);
    m_currentEnginePath.clear();
}
