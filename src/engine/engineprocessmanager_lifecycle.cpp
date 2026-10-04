/// @file engineprocessmanager_lifecycle.cpp
/// @brief エンジンプロセスの非同期起動・終了

#include "engineprocessmanager.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>

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
    m_shutdownState = ShutdownState::Running;
    m_startupErrorReported = false;
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
    m_shutdownState = ShutdownState::IgnoreAll;
    m_postQuitInfoStringLinesLeft = 0;
    m_currentEnginePath.clear();
}
