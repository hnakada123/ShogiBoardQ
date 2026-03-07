/// @file tst_app_lifecycle_pipeline.cpp
/// @brief MainWindow ライフサイクル順序制御の実行時テスト

#include <QtTest>

#include <utility>

#include "mainwindowlifecyclesequence.h"

class TestAppLifecyclePipeline : public QObject
{
    Q_OBJECT

private slots:
    void startupRunsAllEightStepsInOrder();
    void startupAllowsMissingCallbacks();
    void shutdownRunsStepsInOrderAndSetsGuard();
    void shutdownSkipsSecondRun();
};

void TestAppLifecyclePipeline::startupRunsAllEightStepsInOrder()
{
    QStringList calls;

    MainWindowStartupSequence::Steps steps;
    steps.createFoundationObjects = [&calls]() { calls << QStringLiteral("createFoundationObjects"); };
    steps.setupUiSkeleton = [&calls]() { calls << QStringLiteral("setupUiSkeleton"); };
    steps.initializeCoreComponents = [&calls]() { calls << QStringLiteral("initializeCoreComponents"); };
    steps.initializeEarlyServices = [&calls]() { calls << QStringLiteral("initializeEarlyServices"); };
    steps.buildGamePanels = [&calls]() { calls << QStringLiteral("buildGamePanels"); };
    steps.restoreWindowAndSync = [&calls]() { calls << QStringLiteral("restoreWindowAndSync"); };
    steps.connectSignals = [&calls]() { calls << QStringLiteral("connectSignals"); };
    steps.finalizeAndConfigureUi = [&calls]() { calls << QStringLiteral("finalizeAndConfigureUi"); };

    MainWindowStartupSequence(std::move(steps)).run();

    const QStringList expected{
        QStringLiteral("createFoundationObjects"),
        QStringLiteral("setupUiSkeleton"),
        QStringLiteral("initializeCoreComponents"),
        QStringLiteral("initializeEarlyServices"),
        QStringLiteral("buildGamePanels"),
        QStringLiteral("restoreWindowAndSync"),
        QStringLiteral("connectSignals"),
        QStringLiteral("finalizeAndConfigureUi"),
    };
    QCOMPARE(calls, expected);
}

void TestAppLifecyclePipeline::startupAllowsMissingCallbacks()
{
    QStringList calls;

    MainWindowStartupSequence::Steps steps;
    steps.createFoundationObjects = [&calls]() { calls << QStringLiteral("createFoundationObjects"); };
    steps.finalizeAndConfigureUi = [&calls]() { calls << QStringLiteral("finalizeAndConfigureUi"); };

    MainWindowStartupSequence(std::move(steps)).run();

    const QStringList expected{
        QStringLiteral("createFoundationObjects"),
        QStringLiteral("finalizeAndConfigureUi"),
    };
    QCOMPARE(calls, expected);
}

void TestAppLifecyclePipeline::shutdownRunsStepsInOrderAndSetsGuard()
{
    bool shutdownDone = false;
    QStringList calls;

    MainWindowShutdownSequence::Steps steps;
    steps.beginShutdown = [&calls, &shutdownDone]() {
        QVERIFY(shutdownDone);
        calls << QStringLiteral("beginShutdown");
    };
    steps.saveSettings = [&calls]() { calls << QStringLiteral("saveSettings"); };
    steps.destroyEngines = [&calls]() { calls << QStringLiteral("destroyEngines"); };
    steps.invalidateRuntimeDeps = [&calls]() { calls << QStringLiteral("invalidateRuntimeDeps"); };
    steps.releaseOwnedResources = [&calls]() { calls << QStringLiteral("releaseOwnedResources"); };

    QVERIFY(MainWindowShutdownSequence(std::move(steps)).runOnce(shutdownDone));
    QVERIFY(shutdownDone);
    const QStringList expected{
        QStringLiteral("beginShutdown"),
        QStringLiteral("saveSettings"),
        QStringLiteral("destroyEngines"),
        QStringLiteral("invalidateRuntimeDeps"),
        QStringLiteral("releaseOwnedResources"),
    };
    QCOMPARE(calls, expected);
}

void TestAppLifecyclePipeline::shutdownSkipsSecondRun()
{
    bool shutdownDone = false;
    int runCount = 0;

    MainWindowShutdownSequence::Steps steps;
    steps.saveSettings = [&runCount]() { ++runCount; };

    MainWindowShutdownSequence sequence(std::move(steps));
    QVERIFY(sequence.runOnce(shutdownDone));
    QVERIFY(shutdownDone);
    QVERIFY(!sequence.runOnce(shutdownDone));
    QCOMPARE(runCount, 1);
}

QTEST_MAIN(TestAppLifecyclePipeline)
#include "tst_app_lifecycle_pipeline.moc"
