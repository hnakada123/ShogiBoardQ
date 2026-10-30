/// @file tst_service_initialization.cpp
/// @brief サービスの再利用・依存差し替え・配線の冪等性を実行時に検証
#include <QtTest>
#include <QTemporaryDir>
#include <QPointer>
#include <QSignalSpy>
#include <QTextEdit>
#include "appactivitystate.h"
#include "mainwindow.h"
#include "mainwindowactions.h"
#include "mainwindowcomposer.h"
#include "csagamecoordinator.h"
#include "csagamesession.h"
#include "analysissession.h"
#include "engineslots.h"
#include "gamesession.h"
#include "commentcoordinator.h"
#include "commenteditorpanel.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifunavigationstate.h"
#include "positioneditsession.h"
#include "kifuanalysiscontroller.h"
#include "tsumesearchcontroller.h"
#include "boardmovecontroller.h"
#include "pvclickcontroller.h"
#include "considerationuicontroller.h"
#include "appsettings.h"
#include "matchcoordinator.h"
#include "gameboard.h"
#include "kifuloader.h"
#include "kifufilecontroller.h"
#include "dialoglauncher.h"
#include "kifuanalysislistmodel.h"
#include "boardsyncpresenter.h"
#include "dockregistry.h"
#include "kifudisplaypresenter.h"
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

    void commentCoordinatorUsesRefreshedModel()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        // コメントを書く手は表示中の手数（分岐ツリーのカーソル）
        KifuBranchTree* tree = parts.branchTree;
        QVERIFY(tree);
        tree->setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        KifuBranchNode* ply1 = TestTree::addMove(*tree, tree->root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"));
        KifuBranchNode* ply2 = TestTree::addMove(*tree, ply1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("s2"));
        parts.navState->setCurrentNode(ply1);
        CommentCoordinator* coordinator = parts.commentCoordinator;
        QVERIFY(coordinator);
        coordinator->onCommentUpdated(QStringLiteral("最初"));
        QCOMPARE(ply1->comment(), QStringLiteral("最初"));
        parts.navState->setCurrentNode(ply2);
        coordinator->onCommentUpdated(QStringLiteral("更新後"));
        QCOMPARE(ply2->comment(), QStringLiteral("更新後"));
        QCOMPARE(ply1->comment(), QStringLiteral("最初"));
    }

    /// 「コメント更新」を押していないコメントは、棋譜の未保存に数え、書き出すときにその手に確定する。
    /// 対局の進行などで表示中の手が変わったときは、書いていた手に確定してから切り替える
    void pendingCommentIsCountedAndCommittedToItsMove()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        KifuBranchTree* tree = parts.branchTree;
        tree->setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        KifuBranchNode* ply1 = TestTree::addMove(*tree, tree->root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"));
        KifuBranchNode* ply2 = TestTree::addMove(*tree, ply1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("s2"));
        parts.navState->setCurrentNode(ply1);
        parts.document->markSaved(QString());
        QVERIFY(!parts.kifuFileController->hasUnsavedChanges());

        auto* edit = parts.commentEditor->findChild<QTextEdit*>();
        QVERIFY(edit);
        edit->setPlainText(QStringLiteral("書きかけ"));
        QVERIFY(parts.commentEditor->hasUnsavedComment());
        QVERIFY(parts.kifuFileController->hasUnsavedChanges());
        QVERIFY(ply1->comment().isEmpty());

        // 書き出すときに、書いていた手に確定する
        const QStringList kif = KifuExport::lines(*parts.document, KifuFormat::Kif);
        QVERIFY2(kif.contains(QStringLiteral("*書きかけ")), qPrintable(kif.join(QLatin1Char('\n'))));
        QCOMPARE(ply1->comment(), QStringLiteral("書きかけ"));
        QVERIFY(!parts.commentEditor->hasUnsavedComment());

        // 確認を通さずに表示中の手が変わる（対局の進行など）と、書きかけの本文は書いていた手に入る
        edit->setPlainText(QStringLiteral("書き直し"));
        parts.navState->setCurrentNode(ply2);
        QCOMPARE(ply1->comment(), QStringLiteral("書き直し"));
        QVERIFY(ply2->comment().isEmpty());
        QVERIFY(edit->toPlainText().isEmpty());
        QVERIFY(!parts.commentEditor->hasUnsavedComment());
    }

    /// 部品は起動時に1回だけ作られ、どれも1つずつある。局面編集の開始・終了の通知も1回ずつ届く
    void componentsAreCreatedOnceWithoutDuplicateConnections()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        QCOMPARE(window.findChildren<GameSession*>().size(), 1);
        QCOMPARE(window.findChildren<MatchCoordinator*>().size(), 1);
        QCOMPARE(window.findChildren<AnalysisSession*>().size(), 1);
        QCOMPARE(window.findChildren<EngineSlots*>().size(), 1);
        QCOMPARE(window.findChildren<KifuDisplayPresenter*>().size(), 1);
        QCOMPARE(window.findChildren<BoardSyncPresenter*>().size(), 1);
        QCOMPARE(window.findChildren<BoardMoveController*>().size(), 1);
        QCOMPARE(window.findChildren<CsaGameCoordinator*>().size(), 1);
        QCOMPARE(window.findChildren<CsaGameSession*>().size(), 1);
        QCOMPARE(window.findChildren<PositionEditSession*>().size(), 1);
        QCOMPARE(window.findChildren<PvClickController*>().size(), 1);
        QCOMPARE(window.findChildren<KifuAnalysisController*>().size(), 1);
        QCOMPARE(window.findChildren<TsumeSearchController*>().size(), 1);
        QCOMPARE(window.findChildren<ConsiderationUiController*>().size(), 1);
        QCOMPARE(window.findChildren<CommentCoordinator*>().size(), 1);
        auto* editing = window.findChild<PositionEditSession*>();
        QSignalSpy started(editing, &PositionEditSession::editingStarted);
        QSignalSpy finished(editing, &PositionEditSession::editingFinished);
        parts.actions->beginPositionEditing();
        parts.positionEdit->finish();
        QCOMPARE(started.count(), 1);
        QCOMPARE(finished.count(), 1);
    }

    /// 局面編集中にほかの部品が別の動作を始めて動作状態を取り上げたら、盤を編集の表示のまま残さず、
    /// 編集した局面で編集を終える（編集は待機中だけ始め、終えられるのは持ち主だけ）
    void positionEditEndsWhenActivityIsTaken()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        AppActivityState& activity = window.composer().activity();
        QSignalSpy finished(parts.positionEdit, &PositionEditSession::editingFinished);
        parts.actions->beginPositionEditing();
        QVERIFY(parts.positionEdit->isEditing());
        QVERIFY(parts.shogiView->positionEditMode());

        QObject other;
        ActivityLease consideration = activity.begin(AppActivityState::Activity::DuringConsideration, &other);
        QVERIFY(!parts.positionEdit->isEditing());
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!parts.shogiView->positionEditMode());
        // 取り上げた部品の動作状態は変わらない。編集は待機中でなければ始めない
        QCOMPARE(activity.activity(), AppActivityState::Activity::DuringConsideration);
        parts.document->markSaved(QString());
        parts.actions->beginPositionEditing();
        QVERIFY(!parts.positionEdit->isEditing());
        consideration.reset();
        QCOMPARE(activity.activity(), AppActivityState::Activity::Idle);
        parts.document->markSaved(QString());
    }

    /// 司令塔（MatchCoordinator）・エンジンの枠・検討のセッションは起動時に一度だけ作り、作り直さない。
    /// 「新規」で初期化をやり直しても同じものを使う
    void matchCoordinatorIsCreatedOnce()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        const QPointer<MatchCoordinator> first = parts.match;
        const QPointer<EngineSlots> engines = parts.engines;
        const QPointer<AnalysisSession> analysis = parts.analysisSession;
        QVERIFY(first && engines && analysis);

        parts.gameSession->resetToInitialState();
        parts.gameSession->resetToInitialState();

        QVERIFY(first && engines && analysis);
        QCOMPARE(parts.match, first.data());
        QCOMPARE(parts.engines, engines.data());
        QCOMPARE(parts.analysisSession, analysis.data());
    }

    /// 棋譜の読み込み担当は初回に1つだけ作り、以後の読み込み（ファイル・文字列・局面・非同期）で使い回す。
    /// 作り直すと、古い担当を持ったままの部品が解放済みのオブジェクトを使ってしまう
    void kifuLoadsReuseOneLoadCoordinator()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        KifuFileController* files = parts.kifuFileController;
        QVERIFY(files);
        const QString fixture = QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/test_basic.kif");

        QVERIFY(files->loadKifuFile(fixture));
        const QPointer<KifuLoader> first = parts.kifuLoader;
        QVERIFY(first);
        QVERIFY(files->loadKifuText(QStringLiteral("position startpos moves 7g7f 3c3d")));
        QVERIFY(files->applySfenPosition(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1")));
        QVERIFY(files->loadKifuFile(fixture));
        first->loadFileAsync(fixture);
        first->loadTextAsync(QStringLiteral("position startpos moves 2g2f"));
        QSignalSpy finished(first.data(), &KifuLoader::loadFinished);
        QTRY_COMPARE(finished.count(), 1);

        QVERIFY(first);
        QCOMPARE(parts.kifuLoader, first.data());
        QCOMPARE(window.findChildren<KifuLoader*>().size(), 1);
        // 後から始めた読み込みが残る（先の非同期読み込みは打ち切られる）
        QCOMPARE(parts.branchTree->mainRecordLine().usiMoves(), QStringList{QStringLiteral("2g2f")});
    }

    /// 局面編集を始めると棋譜は開始局面だけになるため、未保存の棋譜があれば先に確認する。
    /// 「キャンセル」なら編集を始めず棋譜も残し、「破棄」なら編集を始める
    void beginPositionEditingAsksAboutUnsavedRecord()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        QVERIFY(parts.document);
        parts.document->markUnsavedContent();

        auto* editing = window.findChild<PositionEditSession*>();
        QSignalSpy started(editing, &PositionEditSession::editingStarted);

        answerMessageBox(QMessageBox::RejectRole);
        parts.actions->beginPositionEditing();
        QVERIFY(m_answered);
        QCOMPARE(started.count(), 0);
        QVERIFY(parts.document->isModified());

        answerMessageBox(QMessageBox::DestructiveRole);
        parts.actions->beginPositionEditing();
        QVERIFY(m_answered);
        QCOMPARE(started.count(), 1);
        parts.positionEdit->finish();
    }

    /// 盤・分岐ナビゲーション・棋譜解析の結果は、起動時に合成ルートが1回だけ作って MainWindow が持つ。
    /// 部品どうしは同じものを指し、配線も重ならない
    void startupPartsAreCreatedOnceAndShared()
    {
        MainWindow window;
        auto& nav = window.composer().parts();
        QCOMPARE(window.findChildren<GameBoard*>(), QList<GameBoard*>{nav.gameBoard});
        QVERIFY(window.findChildren<ShogiView*>().contains(nav.shogiView));
        QCOMPARE(nav.shogiView->board(), nav.gameBoard->board());
        QCOMPARE(window.findChildren<KifuBranchTree*>(), QList<KifuBranchTree*>{nav.branchTree});
        QCOMPARE(window.findChildren<KifuNavigationState*>(), QList<KifuNavigationState*>{nav.navState});
        QCOMPARE(window.findChildren<KifuNavigationController*>(),
                 QList<KifuNavigationController*>{nav.kifuNavController});
        QCOMPARE(window.findChildren<KifuDisplayPresenter*>(), QList<KifuDisplayPresenter*>{nav.kifuDisplay});
        QCOMPARE(window.findChildren<LiveGameSession*>(), QList<LiveGameSession*>{nav.liveGameSession});
        QCOMPARE(nav.navState->tree(), nav.branchTree);
        QCOMPARE(nav.kifuNavController->state(), nav.navState);
        QVERIFY(nav.analysis);
        QCOMPARE(nav.analysis->parent(), &window);
        // CSA 通信対局の司令塔も起動時に1つだけ作り、通信対局のセッションと対局の操作が同じものを使う
        QCOMPARE(window.findChildren<CsaGameCoordinator*>(), QList<CsaGameCoordinator*>{nav.csaGame});
        QCOMPARE(nav.csaSession->coordinator(), nav.csaGame);

        QVERIFY(nav.kifuFileController->loadKifuText(QStringLiteral("position startpos moves 7g7f 3c3d 2g2f")));
        nav.kifuNavController->goToFirst();
        QCOMPARE(nav.navState->currentPly(), 0);
        QPushButton* next = nav.recordPane->nextButton();
        QVERIFY(next->isEnabled());
        next->click();
        QCOMPARE(nav.navState->currentPly(), 1);
    }

    /// 局面集ビューアは開いている間は1つだけで、もう一度開こうとすると前面に出す。閉じれば次は作り直す
    void sfenCollectionViewerIsReusedWhileOpen()
    {
        MainWindow window;
        DialogLauncher* dialogs = window.composer().parts().dialogLauncher;
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
        auto& parts = window.composer().parts();
        QDockWidget* dock = parts.dockRegistry->dock(DockRegistry::DockId::Menu);
        QVERIFY(dock);
        const bool wasVisible = dock->isVisible();

        parts.dialogLauncher->displayMenuWindow();
        QCOMPARE(dock->isVisible(), !wasVisible);
        parts.dialogLauncher->displayMenuWindow();
        QCOMPARE(dock->isVisible(), wasVisible);
        QCOMPARE(parts.dockRegistry->dock(DockRegistry::DockId::Menu), dock);
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
