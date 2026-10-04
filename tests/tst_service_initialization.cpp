/// @file tst_service_initialization.cpp
/// @brief サービスの再利用・依存差し替え・配線の冪等性を実行時に検証
#include <QtTest>
#include <QTemporaryDir>
#define private public
#include "mainwindow.h"
#undef private
#include "mainwindowserviceregistry.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowruntimerefs.h"
#include "commentcoordinator.h"
#include "gamerecordmodel.h"
#include "gamestatecontroller.h"
#include "gamerecordpresenter.h"
#include "positioneditcoordinator.h"
#include "prestartcleanuphandler.h"
#include "replaycontroller.h"
#include "timecontrolcontroller.h"
#include "dialogcoordinator.h"
#include "boardsetupcontroller.h"
#include "pvclickcontroller.h"
#include "considerationwiring.h"
#include "appsettings.h"

class TestServiceInitialization : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        AppSettings::setPieceSoundEnabled(false);
    }

    void gameStateKeepsInstanceAndRefreshesMode()
    {
        QObject owner;
        MainWindowCompositionRoot root;
        MainWindowRuntimeRefs refs;
        PlayMode first = PlayMode::HumanVsHuman;
        PlayMode second = PlayMode::EvenEngineVsEngine;
        refs.state.playMode = &first;
        GameStateController* controller = nullptr;
        root.ensureGameStateController(refs, {}, &owner, controller);
        auto* original = controller;
        QVERIFY(controller->isHvH());
        refs.state.playMode = &second;
        root.ensureGameStateController(refs, {}, &owner, controller);
        QCOMPARE(controller, original);
        QVERIFY(!controller->isHvH());
        QVERIFY(!controller->isHumanSide(ShogiGameController::Player1));
        QCOMPARE(owner.findChildren<GameStateController*>().size(), 1);
    }

    void commentCoordinatorUsesRefreshedModel()
    {
        QObject owner;
        MainWindowCompositionRoot root;
        MainWindowRuntimeRefs refs;
        GameRecordModel first, second;
        int ply = 1;
        refs.state.currentMoveIndex = &ply;
        refs.models.gameRecordModel = &first;
        CommentCoordinator* coordinator = nullptr;
        root.ensureCommentCoordinator(refs, &owner, coordinator);
        auto* original = coordinator;
        coordinator->onCommentUpdated(0, QStringLiteral("最初"));
        QCOMPARE(first.comment(1), QStringLiteral("最初"));
        refs.models.gameRecordModel = &second;
        ply = 2;
        root.ensureCommentCoordinator(refs, &owner, coordinator);
        QCOMPARE(coordinator, original);
        coordinator->onCommentUpdated(0, QStringLiteral("更新後"));
        QCOMPARE(second.comment(2), QStringLiteral("更新後"));
        QVERIFY(first.comment(2).isEmpty());
    }

    void registryEnsuresDoNotDuplicateServicesOrConnections()
    {
        MainWindow window;
        auto* registry = window.m_registry.get();
        QVERIFY(registry);
        for (int i = 0; i < 3; ++i) {
            registry->ensureTimeController();
            registry->ensureReplayController();
            registry->ensureGameStateController();
            registry->ensurePreStartCleanupHandler();
            registry->ensureRecordPresenter();
            registry->ensureBoardSetupController();
            registry->ensurePositionEditCoordinator();
            registry->ensurePvClickController();
            registry->ensureDialogCoordinator();
            registry->ensureConsiderationWiring();
            registry->foundation()->ensureCommentCoordinator();
            registry->foundation()->ensurePlayerInfoController();
        }
        QCOMPARE(window.findChildren<TimeControlController*>().size(), 1);
        QCOMPARE(window.findChildren<ReplayController*>().size(), 1);
        QCOMPARE(window.findChildren<GameStateController*>().size(), 1);
        QCOMPARE(window.findChildren<PreStartCleanupHandler*>().size(), 1);
        QCOMPARE(window.findChildren<GameRecordPresenter*>().size(), 1);
        QCOMPARE(window.findChildren<BoardSetupController*>().size(), 1);
        QCOMPARE(window.findChildren<PositionEditCoordinator*>().size(), 1);
        QCOMPARE(window.findChildren<PvClickController*>().size(), 1);
        QCOMPARE(window.findChildren<DialogCoordinator*>().size(), 1);
        QCOMPARE(window.findChildren<ConsiderationWiring*>().size(), 1);
        QCOMPARE(window.findChildren<CommentCoordinator*>().size(), 1);
        auto* editing = window.findChild<PositionEditCoordinator*>();
        QSignalSpy started(editing, &PositionEditCoordinator::positionEditingStarted);
        QSignalSpy finished(editing, &PositionEditCoordinator::positionEditingFinished);
        registry->handleBeginPositionEditing();
        registry->handleFinishPositionEditing();
        QCOMPARE(started.count(), 1);
        QCOMPARE(finished.count(), 1);
    }
};

QTEST_MAIN(TestServiceInitialization)
#include "tst_service_initialization.moc"
