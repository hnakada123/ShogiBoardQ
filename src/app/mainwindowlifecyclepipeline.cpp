/// @file mainwindowlifecyclepipeline.cpp
/// @brief MainWindow の起動/終了フローと終了確認の実装

#include "mainwindowlifecyclepipeline.h"

#include <utility>

MainWindowLifecyclePipeline::MainWindowLifecyclePipeline(Deps deps)
    : m_deps(std::move(deps))
{
}

void MainWindowLifecyclePipeline::runStartup()
{
    MainWindowStartupSequence(m_deps.startup).run();
}

void MainWindowLifecyclePipeline::runShutdown()
{
    (void)MainWindowShutdownSequence(m_deps.shutdown).runOnce(m_shutdownDone);
}

bool MainWindowLifecyclePipeline::confirmClose() const
{
    if (m_shutdownDone || (m_deps.isShuttingDown && m_deps.isShuttingDown())) return true;

    return (!m_deps.confirmDiscardUnsavedKifu || m_deps.confirmDiscardUnsavedKifu())
        && (!m_deps.confirmCloseJoseki || m_deps.confirmCloseJoseki());
}

void MainWindowLifecyclePipeline::requestClose()
{
    if (!m_deps.closeWindow || !m_deps.closeWindow()) return;

    runShutdown();
    if (m_deps.quitApplication) {
        m_deps.quitApplication();
    }
}
