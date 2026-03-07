/// @file mainwindowlifecyclesequence.cpp
/// @brief MainWindow ライフサイクル順序制御の実装

#include "mainwindowlifecyclesequence.h"

namespace {
void invokeIfSet(const std::function<void()>& fn)
{
    if (fn) {
        fn();
    }
}
} // namespace

MainWindowStartupSequence::MainWindowStartupSequence(Steps steps)
    : m_steps(std::move(steps))
{
}

void MainWindowStartupSequence::run() const
{
    invokeIfSet(m_steps.createFoundationObjects);
    invokeIfSet(m_steps.setupUiSkeleton);
    invokeIfSet(m_steps.initializeCoreComponents);
    invokeIfSet(m_steps.initializeEarlyServices);
    invokeIfSet(m_steps.buildGamePanels);
    invokeIfSet(m_steps.restoreWindowAndSync);
    invokeIfSet(m_steps.connectSignals);
    invokeIfSet(m_steps.finalizeAndConfigureUi);
}

MainWindowShutdownSequence::MainWindowShutdownSequence(Steps steps)
    : m_steps(std::move(steps))
{
}

bool MainWindowShutdownSequence::runOnce(bool& shutdownDone) const
{
    if (shutdownDone) {
        return false;
    }

    shutdownDone = true;
    invokeIfSet(m_steps.beginShutdown);
    invokeIfSet(m_steps.saveSettings);
    invokeIfSet(m_steps.destroyEngines);
    invokeIfSet(m_steps.invalidateRuntimeDeps);
    invokeIfSet(m_steps.releaseOwnedResources);
    return true;
}
