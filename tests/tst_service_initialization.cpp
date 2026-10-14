/// @file tst_service_initialization.cpp
/// @brief サービスの再利用・依存差し替え・配線の冪等性を実行時に検証
#include <QtTest>
#include <QTemporaryDir>
#include <QPointer>
#include <QSignalSpy>
#define private public
#include "mainwindow.h"
#undef private
#include "mainwindowserviceregistry.h"
#include "mainwindowfoundationregistry.h"
#include "commentcoordinator.h"
#include "gamerecordmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
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
#include "kifusubregistry.h"
#include "matchcoordinatorwiring.h"
#include "matchcoordinator.h"
#include "kifuloadcoordinator.h"
#include "kifufilecontroller.h"
#include "dialoglaunchwiring.h"
#include "kifuanalysislistmodel.h"
#include "kifudisplaycoordinator.h"
#include "kifunavigationcontroller.h"
#include "livegamesession.h"
#include "recordpane.h"
#include "sfencollectiondialog.h"
#include "shogiview.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <QPushButton>

class TestServiceInitialization : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        AppSettings::pieceSoundEnabled.set(false);
    }

    void gameStateKeepsInstanceAndRefreshesMode()
    {
        MainWindow window;
        auto* registry = window.m_registry.get();
        window.m_state.playMode = PlayMode::HumanVsHuman;
        registry->ensureGameStateController();
        GameStateController* controller = window.m_gameStateController;
        QVERIFY(controller);
        QVERIFY(controller->isHvH());
        window.m_state.playMode = PlayMode::EvenEngineVsEngine;
        registry->ensureGameStateController();
        QCOMPARE(window.m_gameStateController, controller);
        QVERIFY(!controller->isHvH());
        QVERIFY(!controller->isHumanSide(Turn::Black));
        QCOMPARE(window.findChildren<GameStateController*>().size(), 1);
    }

    void commentCoordinatorUsesRefreshedModel()
    {
        MainWindow window;
        auto* registry = window.m_registry.get();
        registry->kifu()->ensureGameRecordModel();
        // コメントを書く手は表示中の手数（分岐ツリーのカーソル）
        KifuBranchTree* tree = window.m_branchNav.branchTree;
        QVERIFY(tree);
        tree->setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        KifuBranchNode* ply1 = tree->addMove(tree->root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"));
        KifuBranchNode* ply2 = tree->addMove(ply1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("s2"));
        window.m_branchNav.navState->setCurrentNode(ply1);
        registry->foundation()->ensureCommentCoordinator();
        CommentCoordinator* coordinator = window.m_commentCoordinator;
        QVERIFY(coordinator);
        coordinator->onCommentUpdated(0, QStringLiteral("最初"));
        QCOMPARE(ply1->comment(), QStringLiteral("最初"));
        window.m_branchNav.navState->setCurrentNode(ply2);
        registry->foundation()->ensureCommentCoordinator();
        QCOMPARE(window.m_commentCoordinator.data(), coordinator);
        coordinator->onCommentUpdated(0, QStringLiteral("更新後"));
        QCOMPARE(ply2->comment(), QStringLiteral("更新後"));
        QCOMPARE(ply1->comment(), QStringLiteral("最初"));
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

    /// 司令塔（MatchCoordinator）は起動時に一度だけ作り、作り直さない。起動の最終段や
    /// 「新規」で初期化をやり直しても同じものを使い、それを持つ部品も同じものを指す
    void matchCoordinatorIsCreatedOnce()
    {
        MainWindow window;
        auto* registry = window.m_registry.get();
        const QPointer<MatchCoordinator> first = window.m_match;
        QVERIFY(first);
        QCOMPARE(window.m_matchWiring->match(), first.data());

        registry->initMatchCoordinator();
        registry->resetGameState();
        registry->initMatchCoordinator();

        QVERIFY(first);
        QCOMPARE(window.m_match, first.data());
        QCOMPARE(window.m_matchWiring->match(), first.data());
    }

    /// 棋譜の読み込み担当は初回に1つだけ作り、以後の読み込み（ファイル・文字列・局面・非同期）で使い回す。
    /// 作り直すと、古い担当を持ったままの部品が解放済みのオブジェクトを使ってしまう
    void kifuLoadsReuseOneLoadCoordinator()
    {
        MainWindow window;
        auto* registry = window.m_registry.get();
        registry->kifu()->ensureKifuFileController();
        KifuFileController* files = window.m_kifuFileController;
        QVERIFY(files);
        const QString fixture = QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/test_basic.kif");

        QVERIFY(files->loadKifuFile(fixture));
        const QPointer<KifuLoadCoordinator> first = window.m_kifuLoadCoordinator;
        QVERIFY(first);
        QVERIFY(files->loadKifuText(QStringLiteral("position startpos moves 7g7f 3c3d")));
        QVERIFY(files->applySfenPosition(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1")));
        QVERIFY(files->loadKifuFile(fixture));
        first->loadFileAsync(fixture);
        first->loadTextAsync(QStringLiteral("position startpos moves 2g2f"));
        QSignalSpy finished(first.data(), &KifuLoadCoordinator::loadFinished);
        QTRY_COMPARE(finished.count(), 1);

        QVERIFY(first);
        QCOMPARE(window.m_kifuLoadCoordinator.data(), first.data());
        QCOMPARE(window.findChildren<KifuLoadCoordinator*>().size(), 1);
        // 後から始めた読み込みが残る（先の非同期読み込みは打ち切られる）
        QCOMPARE(window.m_branchNav.branchTree->mainRecordLine().usiMoves(), QStringList{QStringLiteral("2g2f")});
    }

    /// 局面編集を始めると棋譜は開始局面だけになるため、未保存の棋譜があれば先に確認する。
    /// 「キャンセル」なら編集を始めず棋譜も残し、「破棄」なら編集を始める
    void beginPositionEditingAsksAboutUnsavedRecord()
    {
        MainWindow window;
        auto* registry = window.m_registry.get();
        registry->ensurePositionEditCoordinator();
        registry->kifu()->ensureGameRecordModel();
        QVERIFY(window.m_models.gameRecord);
        window.m_models.gameRecord->markDirty();

        auto* editing = window.findChild<PositionEditCoordinator*>();
        QSignalSpy started(editing, &PositionEditCoordinator::positionEditingStarted);

        answerMessageBox(QMessageBox::RejectRole);
        registry->handleBeginPositionEditing();
        QVERIFY(m_answered);
        QCOMPARE(started.count(), 0);
        QVERIFY(window.m_models.gameRecord->isDirty());

        answerMessageBox(QMessageBox::DestructiveRole);
        registry->handleBeginPositionEditing();
        QVERIFY(m_answered);
        QCOMPARE(started.count(), 1);
        registry->handleFinishPositionEditing();
    }

    /// 盤・分岐ナビゲーション・棋譜解析の結果は、起動時にレジストリが1回だけ作って MainWindow が持つ。
    /// 部品どうしは同じものを指し、分岐ナビゲーションの初期化をやり直しても作り直さず、配線も重ならない
    void startupPartsAreCreatedOnceAndShared()
    {
        MainWindow window;
        auto* registry = window.m_registry.get();
        const BranchNavigation& nav = window.m_branchNav;
        QCOMPARE(window.findChildren<ShogiGameController*>(), QList<ShogiGameController*>{window.m_gameController});
        QVERIFY(window.findChildren<ShogiView*>().contains(window.m_shogiView));
        QCOMPARE(window.m_shogiView->board(), window.m_gameController->board());
        QCOMPARE(window.findChildren<KifuBranchTree*>(), QList<KifuBranchTree*>{nav.branchTree});
        QCOMPARE(window.findChildren<KifuNavigationState*>(), QList<KifuNavigationState*>{nav.navState});
        QCOMPARE(window.findChildren<KifuNavigationController*>(),
                 QList<KifuNavigationController*>{nav.kifuNavController});
        QCOMPARE(window.findChildren<KifuDisplayCoordinator*>(), QList<KifuDisplayCoordinator*>{nav.displayCoordinator});
        QCOMPARE(window.findChildren<LiveGameSession*>(), QList<LiveGameSession*>{nav.liveGameSession});
        QCOMPARE(nav.kifuNavController->tree(), nav.branchTree);
        QCOMPARE(nav.kifuNavController->state(), nav.navState);
        QVERIFY(window.m_models.analysis);
        QCOMPARE(window.m_models.analysis->parent(), &window);
        // CSA 通信対局の司令塔は、最初の対局を始めるまで作らない
        QVERIFY(registry->csaGameCoordinator() == nullptr);

        registry->initializeBranchNavigationClasses();
        QCOMPARE(window.findChildren<KifuBranchTree*>(), QList<KifuBranchTree*>{nav.branchTree});

        registry->kifu()->ensureKifuFileController();
        QVERIFY(window.m_kifuFileController->loadKifuText(QStringLiteral("position startpos moves 7g7f 3c3d 2g2f")));
        nav.kifuNavController->goToFirst();
        QCOMPARE(nav.navState->currentPly(), 0);
        QPushButton* next = window.m_recordPane->nextButton();
        QVERIFY(next->isEnabled());
        next->click();
        QCOMPARE(nav.navState->currentPly(), 1);
    }

    /// 局面集ビューアは開いている間は1つだけで、もう一度開こうとすると前面に出す。閉じれば次は作り直す
    void sfenCollectionViewerIsReusedWhileOpen()
    {
        MainWindow window;
        DialogLaunchWiring* dialogs = window.m_dialogLaunchWiring;
        QVERIFY(dialogs);

        dialogs->displaySfenCollectionViewer();
        dialogs->displaySfenCollectionViewer();
        QCOMPARE(window.findChildren<SfenCollectionDialog*>().size(), 1);
        const QPointer<SfenCollectionDialog> first = window.findChild<SfenCollectionDialog*>();
        first->close();
        QTRY_VERIFY(first.isNull());

        dialogs->displaySfenCollectionViewer();
        const QList<SfenCollectionDialog*> reopened = window.findChildren<SfenCollectionDialog*>();
        QCOMPARE(reopened.size(), 1);
        reopened.first()->close();
    }

    /// メニューウィンドウの操作は、起動時に作ったドックの表示と非表示を切り替える
    void menuWindowActionTogglesTheDock()
    {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QDockWidget* dock = window.m_docks.menuWindow;
        QVERIFY(dock);
        const bool wasVisible = dock->isVisible();

        window.m_dialogLaunchWiring->displayMenuWindow();
        QCOMPARE(dock->isVisible(), !wasVisible);
        window.m_dialogLaunchWiring->displayMenuWindow();
        QCOMPARE(dock->isVisible(), wasVisible);
        QCOMPARE(window.m_docks.menuWindow, dock);
    }

private:
    QMessageBox::ButtonRole m_answerRole = QMessageBox::RejectRole;
    bool m_answered = false;

    void answerMessageBox(QMessageBox::ButtonRole role)
    {
        m_answerRole = role;
        m_answered = false;
        QTimer::singleShot(0, this, &TestServiceInitialization::respondToMessageBox);
    }

    void respondToMessageBox()
    {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            QTimer::singleShot(20, this, &TestServiceInitialization::respondToMessageBox);
            return;
        }
        for (QAbstractButton* button : box->buttons()) {
            if (box->buttonRole(button) == m_answerRole) {
                m_answered = true;
                button->click();
                return;
            }
        }
    }
};

QTEST_MAIN(TestServiceInitialization)
#include "tst_service_initialization.moc"
