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
#include "menuwindowcontroller.h"
#include "kifuanalysislistmodel.h"
#include "boardsyncpresenter.h"
#include "dockregistry.h"
#include "boardviewerwidget.h"
#include "pvboarddialog.h"
#include "pvtext.h"
#include "fontsizecontrol.h"
#include "panelbutton.h"
#include "docklayoutmanager.h"
#include "docksettings.h"
#include "boardinteractioncontroller.h"
#include "shogiboard.h"
#include "boardconstants.h"
#include "kifudisplaypresenter.h"
#include "kifunavigationcontroller.h"
#include "livegamesession.h"
#include "recordpane.h"
#include "stepbuttons.h"
#include "sfencollectiondialog.h"
#include "shogiview.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>

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

    /// 別の手へ移る前の未更新のコメントは、棋譜・定跡と同じ確認（保存・破棄・キャンセル）で尋ねる。
    /// 保存は書いていた手に確定して移り、破棄は捨てて移り、キャンセルは移らない
    void leavingWithPendingCommentAsksToSaveOrDiscard()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        KifuBranchTree* tree = parts.branchTree;
        tree->setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        KifuBranchNode* ply1 = TestTree::addMove(*tree, tree->root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"));
        KifuBranchNode* ply2 = TestTree::addMove(*tree, ply1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("s2"));
        parts.navState->setCurrentNode(ply1);
        auto* edit = parts.commentEditor->findChild<QTextEdit*>();
        QVERIFY(edit);
        QAction* next = parts.kifuNavController->action(KifuNavigationController::Next);
        QAction* prev = parts.kifuNavController->action(KifuNavigationController::Prev);

        edit->setPlainText(QStringLiteral("残す"));
        answerMessageBox(QMessageBox::RejectRole);   // キャンセル
        next->trigger();
        QVERIFY(m_answered);
        QCOMPARE(parts.navState->currentNode(), ply1);
        QVERIFY(parts.commentEditor->hasUnsavedComment());

        answerMessageBox(QMessageBox::AcceptRole);   // 保存
        next->trigger();
        QVERIFY(m_answered);
        QCOMPARE(parts.navState->currentNode(), ply2);
        QCOMPARE(ply1->comment(), QStringLiteral("残す"));
        QVERIFY(!parts.commentEditor->hasUnsavedComment());

        edit->setPlainText(QStringLiteral("捨てる"));
        answerMessageBox(QMessageBox::DestructiveRole);   // 破棄
        prev->trigger();
        QVERIFY(m_answered);
        QCOMPARE(parts.navState->currentNode(), ply1);
        QVERIFY(ply2->comment().isEmpty());
        QCOMPARE(edit->toPlainText(), QStringLiteral("残す"));
        QVERIFY(!parts.commentEditor->hasUnsavedComment());
        parts.document->markSaved(QString());
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

        parts.actions->startNewRecord();
        parts.actions->startNewRecord();

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
        // CSA 通信対局の司令塔も起動時に1つだけ作る
        QCOMPARE(window.findChildren<CsaGameCoordinator*>(), QList<CsaGameCoordinator*>{nav.csaGame});

        QVERIFY(nav.kifuFileController->loadKifuText(QStringLiteral("position startpos moves 7g7f 3c3d 2g2f")));
        nav.kifuNavController->goToFirst();
        QCOMPARE(nav.navState->currentPly(), 0);
        QPushButton* next = nav.recordPane->stepButtons()->nextButton();
        QVERIFY(next->isEnabled());
        next->click();
        QCOMPARE(nav.navState->currentPly(), 1);
    }

    /// 局面集ビューアは開いている間は1つだけで、もう一度開こうとすると前面に出す。閉じれば次は作り直す
    void sfenCollectionViewerIsReusedWhileOpen()
    {
        MainWindow window;
        KifuFileController* files = window.composer().parts().kifuFileController;
        QVERIFY(files);

        files->openSfenCollection();
        files->openSfenCollection();
        QCOMPARE(window.findChildren<SfenCollectionDialog*>().size(), 1);
        const QPointer<SfenCollectionDialog> first = window.findChild<SfenCollectionDialog*>();
        first->close();
        QTRY_VERIFY(first.isNull());

        files->openSfenCollection();
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

        parts.menuWindowController->toggleWindow();
        QCOMPARE(dock->isVisible(), !wasVisible);
        // 表示の切り替えはドックの toggleViewAction（「表示」メニューの項目）と同じ
        QCOMPARE(dock->toggleViewAction()->isChecked(), dock->isVisible());
        parts.menuWindowController->toggleWindow();
        QCOMPARE(dock->isVisible(), wasVisible);
        QCOMPARE(parts.dockRegistry->dock(DockRegistry::DockId::Menu), dock);
    }

    /// チェックできるメニューの項目は、設定を持つ部品の今の値から始め、変えるとその部品が保存する
    void checkableMenuItemsStartFromTheirOwners()
    {
        AppSettings::legalMovesVisible.set(false);
        AppSettings::toolbarVisible.set(false);
        AppSettings::notationOrigin.set(true);
        DockSettings::docksLocked.set(true);
        {
            MainWindow window;
            auto& parts = window.composer().parts();
            QAction* legalMoves = window.findChild<QAction*>(QStringLiteral("actionLegalMoves"));
            QAction* toolBar = window.findChild<QAction*>(QStringLiteral("actionToolBar"));
            QAction* origin = window.findChild<QAction*>(QStringLiteral("actionNotationOrigin"));
            QAction* lockDocks = window.findChild<QAction*>(QStringLiteral("actionLockDocks"));
            QVERIFY(legalMoves && toolBar && origin && lockDocks);
            QVERIFY(!legalMoves->isChecked());
            QVERIFY(!parts.boardController->legalMovesVisible());
            QVERIFY(!toolBar->isChecked());
            QVERIFY(origin->isChecked());
            QVERIFY(lockDocks->isChecked());
            QVERIFY(parts.dockLayoutManager->docksLocked());

            legalMoves->trigger();
            QVERIFY(parts.boardController->legalMovesVisible());
            QVERIFY(AppSettings::legalMovesVisible());
            lockDocks->trigger();
            QVERIFY(!DockSettings::docksLocked());
            toolBar->trigger();
            QVERIFY(AppSettings::toolbarVisible());
        }
        AppSettings::notationOrigin.set(false);
    }

    /// ドックのパネルの A-/A+ は、パネルによらず同じ規則（PanelButton）の大きさになる。
    /// 定跡ウィンドウだけは操作の列が大きめなので、ダイアログと同じ大きさ
    void dockPanelFontButtonsShareOneSize()
    {
        MainWindow window;
        int checked = 0;
        const QList<QDockWidget*> docks = window.findChildren<QDockWidget*>();
        for (QDockWidget* dock : docks) {
            const QList<QAbstractButton*> buttons = dock->findChildren<QAbstractButton*>();
            for (QAbstractButton* button : buttons) {
                if (button->property(FontSizeControl::kRoleProperty).toString().isEmpty()) continue;
                if (button->objectName().startsWith(QStringLiteral("joseki"))) continue;
                QVERIFY2(button->minimumSize() == button->maximumSize(), qPrintable(button->objectName()));
                QCOMPARE(button->minimumSize(), PanelButton::sizeFor(button));
                QVERIFY(button->minimumWidth() >= PanelButton::kSize.width());
                ++checked;
            }
        }
        QVERIFY2(checked >= 16, qPrintable(QString::number(checked)));   // 8つ以上のパネルの組
        // アイコンだけのボタンは、QAction から付いた文字（「元に戻す」など）では広げない
        auto* undo = window.findChild<QToolButton*>(QStringLiteral("gameInfoUndo"));
        QVERIFY(undo && !undo->text().isEmpty());
        QCOMPARE(undo->size(), PanelButton::kSize);
    }

    /// 盤は設定を読まずに既定の大きさで始まり、本体の盤の大きさは保存した値から始めて変わるたびに保存する。
    /// 読み筋盤は本体の盤の今の大きさで開く（前回の終了時の大きさではない）
    void squareSizeFollowsTheMainBoard()
    {
        AppSettings::squareSize.set(60);
        {
            ShogiView standalone;
            QCOMPARE(standalone.squareSize(), BoardSquareSize::kDefault);
        }
        MainWindow window;
        auto& parts = window.composer().parts();
        QCOMPARE(parts.shogiView->squareSize(), 60);
        parts.shogiView->enlargeBoard();
        parts.shogiView->enlargeBoard();
        QCOMPARE(parts.shogiView->squareSize(), 62);
        QCOMPARE(AppSettings::squareSize(), 62);

        PvBoardDialog* dialog = parts.pvClickController->showPvBoard(
            PvText::play(QStringLiteral("startpos"), QStringLiteral("7g7f 3c3d")), QString());
        QVERIFY(dialog);
        QCOMPARE(dialog->findChild<BoardViewerWidget*>()->view()->squareSize(), 62);
        dialog->close();
        AppSettings::squareSize.remove();
    }

    /// 駒箱の先後は局面編集（PositionEditSession）が持ち、盤は表示するだけ。置く駒は編集の持ち主が決める
    void pieceBoxSideBelongsToPositionEdit()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        parts.actions->beginPositionEditing();
        QVERIFY(parts.positionEdit->isEditing());
        auto* white = parts.shogiView->findChild<QToolButton*>(QStringLiteral("pieceBoxWhiteButton"));
        QVERIFY(white);
        white->click();
        QCOMPARE(parts.positionEdit->pieceBoxSide(), Turn::White);
        QCOMPARE(parts.shogiView->pieceBoxSide(), Turn::White);

        parts.shogiView->setPieceBoxSide(Turn::Black);   // 表示だけを変えても、置く駒は変わらない
        parts.positionEdit->returnAllPiecesToBox();
        ShogiBoard* board = parts.gameBoard->board();
        int rank = 1;
        while (rank <= 8 && board->pieceCharacter(BoardConstants::kPieceBoxFile, rank) != Piece::BlackGold) ++rank;
        QVERIFY(rank <= 8);
        QVERIFY(parts.positionEdit->applyEditMove(QPoint(BoardConstants::kPieceBoxFile, rank), QPoint(5, 5)));
        QCOMPARE(board->pieceCharacter(5, 5), Piece::WhiteGold);

        parts.positionEdit->finish();
        parts.positionEdit->setPieceBoxSide(Turn::Black);
        parts.document->markSaved(QString());
    }

    /// 局面編集の盤の項目は、編集中の盤を変える
    void positionEditMenuItemsReachTheSession()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        QAction* changeTurn = window.findChild<QAction*>(QStringLiteral("actionChangeTurn"));
        QVERIFY(changeTurn);
        parts.actions->beginPositionEditing();
        QVERIFY(parts.positionEdit->isEditing());
        const Turn before = parts.gameBoard->board()->currentPlayer();
        changeTurn->trigger();
        QCOMPARE(parts.gameBoard->board()->currentPlayer(), opponent(before));
        parts.positionEdit->finish();
        parts.document->markSaved(QString());
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
