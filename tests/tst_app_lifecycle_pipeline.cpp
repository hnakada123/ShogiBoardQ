/// @file tst_app_lifecycle_pipeline.cpp
/// @brief MainWindow ライフサイクル順序制御の実行時テスト

#include <QtTest>

#include <utility>

#include "mainwindowlifecyclesequence.h"
#include "mainwindowlifecyclepipeline.h"

class TestAppLifecyclePipeline : public QObject
{
    Q_OBJECT

private slots:
    void startupRunsAllEightStepsInOrder();
    void startupAllowsMissingCallbacks();
    void shutdownRunsStepsInOrderAndSetsGuard();
    void shutdownSkipsSecondRun();
    void closeConfirmation_data();
    void closeConfirmation();
    void rejectedCloseDoesNotShutdownOrQuit();
    void acceptedCloseShutsDownBeforeQuit();
    void closeEventShutdownIsNotRepeated();
    void shutdownSkipsConfirmationAndReentrantShutdown();
    void externalShutdownSkipsConfirmationButRunsCleanup();
    void missingCloseCallbackDoesNotQuit();
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

    MainWindowLifecyclePipeline::Deps deps;
    deps.startup = std::move(steps);
    MainWindowLifecyclePipeline(std::move(deps)).runStartup();

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

    MainWindowLifecyclePipeline::Deps deps;
    deps.startup = std::move(steps);
    MainWindowLifecyclePipeline(std::move(deps)).runStartup();

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

void TestAppLifecyclePipeline::closeConfirmation_data()
{
    QTest::addColumn<bool>("discardKifu");
    QTest::addColumn<bool>("closeJoseki");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<QStringList>("expectedCalls");

    QTest::newRow("cancel-kifu") << false << true << false
        << QStringList{QStringLiteral("kifu")};
    QTest::newRow("cancel-joseki") << true << false << false
        << QStringList{QStringLiteral("kifu"), QStringLiteral("joseki")};
    QTest::newRow("accept-both") << true << true << true
        << QStringList{QStringLiteral("kifu"), QStringLiteral("joseki")};
}

void TestAppLifecyclePipeline::closeConfirmation()
{
    QFETCH(bool, discardKifu);
    QFETCH(bool, closeJoseki);
    QFETCH(bool, expected);
    QFETCH(QStringList, expectedCalls);
    QStringList calls;
    MainWindowLifecyclePipeline::Deps deps;
    deps.confirmDiscardUnsavedKifu = [&]() {
        calls << QStringLiteral("kifu");
        return discardKifu;
    };
    deps.confirmCloseJoseki = [&]() {
        calls << QStringLiteral("joseki");
        return closeJoseki;
    };
    deps.shutdown.saveSettings = [&]() { calls << QStringLiteral("save"); };
    deps.quitApplication = [&]() { calls << QStringLiteral("quit"); };

    MainWindowLifecyclePipeline pipeline(std::move(deps));
    QCOMPARE(pipeline.confirmClose(), expected);
    QCOMPARE(calls, expectedCalls);
}

void TestAppLifecyclePipeline::rejectedCloseDoesNotShutdownOrQuit()
{
    QStringList calls;
    MainWindowLifecyclePipeline::Deps deps;
    deps.closeWindow = [&]() {
        calls << QStringLiteral("close");
        return false;
    };
    deps.shutdown.saveSettings = [&]() { calls << QStringLiteral("save"); };
    deps.quitApplication = [&]() { calls << QStringLiteral("quit"); };

    MainWindowLifecyclePipeline(std::move(deps)).requestClose();
    QCOMPARE(calls, QStringList{QStringLiteral("close")});
}

void TestAppLifecyclePipeline::acceptedCloseShutsDownBeforeQuit()
{
    QStringList calls;
    MainWindowLifecyclePipeline::Deps deps;
    deps.closeWindow = [&]() {
        calls << QStringLiteral("close");
        return true;
    };
    deps.shutdown.beginShutdown = [&]() { calls << QStringLiteral("begin"); };
    deps.shutdown.saveSettings = [&]() { calls << QStringLiteral("save"); };
    deps.shutdown.destroyEngines = [&]() { calls << QStringLiteral("engines"); };
    deps.shutdown.invalidateRuntimeDeps = [&]() { calls << QStringLiteral("invalidate"); };
    deps.shutdown.releaseOwnedResources = [&]() { calls << QStringLiteral("release"); };
    deps.quitApplication = [&]() { calls << QStringLiteral("quit"); };

    MainWindowLifecyclePipeline pipeline(std::move(deps));
    pipeline.requestClose();
    pipeline.runShutdown(); // MainWindow のデストラクタからの再呼び出し
    const QStringList expected{QStringLiteral("close"), QStringLiteral("begin"),
        QStringLiteral("save"), QStringLiteral("engines"), QStringLiteral("invalidate"),
        QStringLiteral("release"), QStringLiteral("quit")};
    QCOMPARE(calls, expected);
}

void TestAppLifecyclePipeline::closeEventShutdownIsNotRepeated()
{
    MainWindowLifecyclePipeline* activePipeline = nullptr;
    QStringList calls;
    MainWindowLifecyclePipeline::Deps deps;
    deps.closeWindow = [&]() {
        // QWidget::close() 内で closeEvent が同期実行される場合
        activePipeline->runShutdown();
        return true;
    };
    deps.shutdown.saveSettings = [&]() { calls << QStringLiteral("save"); };
    deps.quitApplication = [&]() { calls << QStringLiteral("quit"); };

    MainWindowLifecyclePipeline pipeline(std::move(deps));
    activePipeline = &pipeline;
    pipeline.requestClose();
    pipeline.runShutdown();
    const QStringList expected{QStringLiteral("save"), QStringLiteral("quit")};
    QCOMPARE(calls, expected);
}

void TestAppLifecyclePipeline::shutdownSkipsConfirmationAndReentrantShutdown()
{
    MainWindowLifecyclePipeline* activePipeline = nullptr;
    QStringList calls;
    MainWindowLifecyclePipeline::Deps deps;
    deps.confirmDiscardUnsavedKifu = [&]() { calls << QStringLiteral("kifu"); return false; };
    deps.confirmCloseJoseki = [&]() { calls << QStringLiteral("joseki"); return false; };
    deps.shutdown.beginShutdown = [&]() {
        QVERIFY(activePipeline->confirmClose());
        activePipeline->runShutdown();
        calls << QStringLiteral("begin");
    };
    deps.shutdown.saveSettings = [&]() { calls << QStringLiteral("save"); };

    MainWindowLifecyclePipeline pipeline(std::move(deps));
    activePipeline = &pipeline;
    pipeline.runShutdown();
    QVERIFY(pipeline.confirmClose());
    const QStringList expected{QStringLiteral("begin"), QStringLiteral("save")};
    QCOMPARE(calls, expected);
}

void TestAppLifecyclePipeline::externalShutdownSkipsConfirmationButRunsCleanup()
{
    bool externalShutdown = false;
    QStringList calls;
    MainWindowLifecyclePipeline::Deps deps;
    deps.isShuttingDown = [&]() { return externalShutdown; };
    deps.confirmDiscardUnsavedKifu = [&]() { calls << QStringLiteral("kifu"); return false; };
    deps.confirmCloseJoseki = [&]() { calls << QStringLiteral("joseki"); return false; };
    deps.shutdown.saveSettings = [&]() { calls << QStringLiteral("save"); };
    deps.shutdown.destroyEngines = [&]() { calls << QStringLiteral("engines"); };

    MainWindowLifecyclePipeline pipeline(std::move(deps));
    QVERIFY(!pipeline.confirmClose());
    QCOMPARE(calls, QStringList{QStringLiteral("kifu")});

    // 自動化 API は Pipeline 作成後に終了フラグを設定してから close() を呼ぶ。
    externalShutdown = true;
    QVERIFY(pipeline.confirmClose());
    pipeline.runShutdown();
    pipeline.runShutdown();
    const QStringList expected{QStringLiteral("kifu"), QStringLiteral("save"), QStringLiteral("engines")};
    QCOMPARE(calls, expected);
}

void TestAppLifecyclePipeline::missingCloseCallbackDoesNotQuit()
{
    bool quitCalled = false;
    MainWindowLifecyclePipeline::Deps deps;
    deps.quitApplication = [&]() { quitCalled = true; };
    MainWindowLifecyclePipeline pipeline(std::move(deps));
    QVERIFY(pipeline.confirmClose());
    pipeline.requestClose();
    QVERIFY(!quitCalled);
}

QTEST_MAIN(TestAppLifecyclePipeline)
#include "tst_app_lifecycle_pipeline.moc"
