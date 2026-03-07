#ifndef MAINWINDOWLIFECYCLESEQUENCE_H
#define MAINWINDOWLIFECYCLESEQUENCE_H

/// @file mainwindowlifecyclesequence.h
/// @brief MainWindow ライフサイクル順序制御の小さな実行ヘルパ

#include <functional>

class MainWindowStartupSequence
{
public:
    struct Steps {
        std::function<void()> createFoundationObjects;
        std::function<void()> setupUiSkeleton;
        std::function<void()> initializeCoreComponents;
        std::function<void()> initializeEarlyServices;
        std::function<void()> buildGamePanels;
        std::function<void()> restoreWindowAndSync;
        std::function<void()> connectSignals;
        std::function<void()> finalizeAndConfigureUi;
    };

    explicit MainWindowStartupSequence(Steps steps);

    void run() const;

private:
    Steps m_steps;
};

class MainWindowShutdownSequence
{
public:
    struct Steps {
        std::function<void()> beginShutdown;
        std::function<void()> saveSettings;
        std::function<void()> destroyEngines;
        std::function<void()> invalidateRuntimeDeps;
        std::function<void()> releaseOwnedResources;
    };

    explicit MainWindowShutdownSequence(Steps steps);

    [[nodiscard]] bool runOnce(bool& shutdownDone) const;

private:
    Steps m_steps;
};

#endif // MAINWINDOWLIFECYCLESEQUENCE_H
