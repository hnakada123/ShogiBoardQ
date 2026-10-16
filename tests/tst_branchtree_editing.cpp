/// @file tst_branchtree_editing.cpp
/// @brief 分岐ツリーの編集（本譜にする・並べ替え・削除）と表示（詰めた配置・折りたたみ・キー操作）のテスト

#include <QtTest>
#include <QAbstractButton>
#include <QApplication>
#include <QGraphicsView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTimer>

#include "branchtreeeditcontroller.h"
#include "branchtreelayout.h"
#include "branchtreemanager.h"
#include "gamerecordmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudisplaypresenter.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifupresentation.h"
#include "livegamesession.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogimove.h"

namespace {
const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// 本譜 m1..m6、m4 からの変化 A（a5 a6）、m1 からの変化 B（b2 b3）、b2 からの変化 C（c3）
struct Fixture {
    KifuBranchTree tree;
    QList<KifuBranchNode*> main;   // main[0] = root
    KifuBranchNode* a5 = nullptr;
    KifuBranchNode* a6 = nullptr;
    KifuBranchNode* b2 = nullptr;
    KifuBranchNode* b3 = nullptr;
    KifuBranchNode* c3 = nullptr;

    Fixture()
    {
        tree.setRootSfen(kHirateSfen);
        main.append(tree.root());
        for (int ply = 1; ply <= 6; ++ply) {
            main.append(add(main.last(), QStringLiteral("m%1").arg(ply)));
        }
        a5 = add(main.at(4), QStringLiteral("a5"));
        a6 = add(a5, QStringLiteral("a6"));
        b2 = add(main.at(1), QStringLiteral("b2"));
        b3 = add(b2, QStringLiteral("b3"));
        c3 = add(b2, QStringLiteral("c3"));
    }

    KifuBranchNode* add(KifuBranchNode* parent, const QString& name)
    {
        const QString mark = (parent->ply() % 2 == 0) ? QStringLiteral("▲") : QStringLiteral("△");
        return tree.addMove(parent, ShogiMove(), mark + name, QStringLiteral("sfen-") + name);
    }
};

/// 分岐の編集に要る部品一式（表示中の手・移動・ライブ対局・未保存の印）
struct EditHarness {
    KifuNavigationState state;
    KifuNavigationController nav;
    LiveGameSession live;
    GameRecordModel record;
    BranchTreeEditController controller;

    explicit EditHarness(KifuBranchTree& tree)
        : state(&tree)
        , nav(&state)
        , live(&tree, &state)
        , controller(deps(tree, state, nav, live, record))
    {
        record.setBranchTree(&tree);
    }

    static BranchTreeEditController::Deps deps(KifuBranchTree& tree, KifuNavigationState& state,
                                               KifuNavigationController& nav, LiveGameSession& live,
                                               GameRecordModel& record)
    {
        BranchTreeEditController::Deps d;
        d.tree = &tree;
        d.navState = &state;
        d.navController = &nav;
        d.liveSession = &live;
        d.gameRecord = &record;
        return d;
    }
};
} // namespace

class TestBranchTreeEditing : public QObject
{
    Q_OBJECT

private:
    QString m_confirmText;                ///< 削除の確認の本文
    QString m_confirmActionText;          ///< 削除の確認で、操作のボタンに表示された文言
    bool m_confirmDefaultIsCancel = false;

    /// 表示中の削除の確認を調べてから、キャンセルで閉じる
    void inspectDeleteConfirmation()
    {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            QTimer::singleShot(10, this, &TestBranchTreeEditing::inspectDeleteConfirmation);
            return;
        }
        m_confirmText = box->text();
        const QAbstractButton* action = box->button(QMessageBox::Yes);
        m_confirmActionText = action ? action->text() : QString();
        m_confirmDefaultIsCancel = box->defaultButton() == box->button(QMessageBox::Cancel);
        box->button(QMessageBox::Cancel)->click();
    }

private slots:
    void initTestCase()
    {
        // 表示倍率・配置の設定を利用者の設定ファイルに書かない
        QStandardPaths::setTestModeEnabled(true);
    }

    // ===== KifuBranchTree の編集 =====

    void tree_promoteToMainLine()
    {
        Fixture f;
        QSignalSpy changed(&f.tree, &KifuBranchTree::treeChanged);
        QVERIFY(f.tree.promoteToMainLine(f.c3));
        QCOMPARE(changed.count(), 1);
        const QList<KifuBranchNode*> mainLine = f.tree.mainLine();
        QCOMPARE(mainLine.size(), 4);   // root m1 b2 c3
        QCOMPARE(mainLine.last(), f.c3);
        // 既に本譜なら何もしない
        QVERIFY(!f.tree.promoteToMainLine(f.c3));
        QCOMPARE(changed.count(), 1);
    }

    void tree_moveChildAndVariationHead()
    {
        Fixture f;
        QCOMPARE(KifuBranchTree::variationHead(f.a6), f.a5);
        QCOMPARE(KifuBranchTree::variationHead(f.c3), f.c3);
        QCOMPARE(KifuBranchTree::variationHead(f.b3), f.b3);
        QCOMPARE(KifuBranchTree::variationHead(f.main.at(3)), f.main.at(2));   // m2 は b2 と兄弟
        QVERIFY(f.tree.moveChild(f.c3, 0));
        QCOMPARE(f.b2->children().first(), f.c3);
        QVERIFY(!f.tree.moveChild(f.c3, 0));   // 位置が同じなら変更なし
    }

    void tree_removeSubtree()
    {
        Fixture f;
        const int before = f.tree.nodeCount();
        QCOMPARE(KifuBranchTree::subtreeSize(f.b2), 3);
        const int b3Id = f.b3->nodeId();
        QVERIFY(f.tree.removeSubtree(f.b2));
        QCOMPARE(f.tree.nodeCount(), before - 3);
        QVERIFY(f.tree.nodeAt(b3Id) == nullptr);
        QCOMPARE(f.main.at(1)->childCount(), 1);
        QVERIFY(!f.tree.removeSubtree(f.tree.root()));
    }

    // ===== BranchTreeEditController =====

    void controller_deleteMovesCurrentOutOfSubtree()
    {
        Fixture f;
        EditHarness h(f.tree);
        h.nav.goToNode(f.b3);
        QSignalSpy changed(&h.state, &KifuNavigationState::changed);

        QVERIFY(h.controller.apply(BranchTreeEditController::Operation::DeleteFromHere, f.b2, false));
        QCOMPARE(h.state.currentNode(), f.main.at(1));
        QVERIFY(h.record.isDirty());
        QCOMPARE(f.main.at(1)->childCount(), 1);
        // 表示中の手の移動と削除をまとめて1回だけ知らせる
        QCOMPARE(changed.count(), 1);
    }

    // 削除の確認は、KDE で訳されない「Yes」ではなく操作名のボタンで尋ね、キャンセルなら削除しない
    void controller_deleteConfirmationNamesTheAction()
    {
        Fixture f;
        EditHarness h(f.tree);

        const int before = f.tree.nodeCount();
        m_confirmActionText.clear();
        m_confirmDefaultIsCancel = false;
        QTimer::singleShot(0, this, &TestBranchTreeEditing::inspectDeleteConfirmation);
        QVERIFY(!h.controller.apply(BranchTreeEditController::Operation::DeleteFromHere, f.b2));
        QCOMPARE(m_confirmActionText, BranchTreeEditController::tr("削除する"));
        QVERIFY(m_confirmDefaultIsCancel);
        QCOMPARE(f.tree.nodeCount(), before);
        QVERIFY(!h.record.isDirty());
    }

    // 削除の確認の手は、分岐ツリーと同じ表記（英語式の表記なら P-7f）で示す
    void controller_deleteConfirmationUsesDisplayedNotation()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        EditHarness h(tree);
        ShogiGameController gc;

        QPoint from(7, 7), to(7, 6);
        QVERIFY(h.controller.recordBoardMove(from, to, &gc));
        KifuBranchNode* move = tree.root()->childAt(0);

        KifuPresentation::configure(QStringLiteral("en"), QStringLiteral("western"), false);
        m_confirmText.clear();
        QTimer::singleShot(0, this, &TestBranchTreeEditing::inspectDeleteConfirmation);
        const bool deleted = h.controller.apply(BranchTreeEditController::Operation::DeleteFromHere, move);
        KifuPresentation::configure(QStringLiteral("ja"), QStringLiteral("japanese"), false);
        QVERIFY(!deleted);
        QVERIFY2(m_confirmText.contains(QStringLiteral("P-7f")), qPrintable(m_confirmText));
        QVERIFY2(!m_confirmText.contains(QStringLiteral("七")), qPrintable(m_confirmText));
    }

    void controller_promoteKeepsCurrentNode()
    {
        Fixture f;
        EditHarness h(f.tree);
        h.nav.goToNode(f.a6);

        using Op = BranchTreeEditController::Operation;
        QVERIFY(h.controller.isApplicable(Op::PromoteToMainLine, f.a6));
        QVERIFY(h.controller.isApplicable(Op::MoveUp, f.a6));
        QVERIFY(!h.controller.isApplicable(Op::MoveDown, f.a6));
        QSignalSpy changed(&h.state, &KifuNavigationState::changed);
        QVERIFY(h.controller.apply(Op::PromoteToMainLine, f.a6));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(h.state.currentNode(), f.a6);
        QCOMPARE(h.state.currentLineIndex(), 0);
        QVERIFY(!h.controller.isApplicable(Op::PromoteToMainLine, f.a6));
        QVERIFY(h.controller.isApplicable(Op::MoveDown, f.a6));
    }

    void controller_notApplicableDuringGame()
    {
        Fixture f;
        EditHarness h(f.tree);
        h.live.startFromRoot();

        QVERIFY(!h.controller.canEdit());
        QVERIFY(!h.controller.canRecordBoardMove());
        QVERIFY(!h.controller.isApplicable(BranchTreeEditController::Operation::DeleteFromHere, f.b2));
        h.live.discard();
        QVERIFY(h.controller.canEdit());
    }

    // 対局していないときの盤上の手：棋譜が空なら開始局面から始め、同じ手は既存の手へ移動する
    // 対局していないときの盤上の手：表示中の手の次の手として記録し、同じ手は既存の手へ移動する
    void controller_recordsBoardMoveAsVariation()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);   // 起動直後と同じく開始局面だけ
        EditHarness h(tree);
        ShogiGameController gc;
        QVERIFY(h.controller.canRecordBoardMove());

        QSignalSpy changed(&h.state, &KifuNavigationState::changed);
        QPoint from(7, 7), to(7, 6);
        QVERIFY(h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(tree.root()->childCount(), 1);
        QCOMPARE(h.state.currentNode(), tree.root()->childAt(0));
        QVERIFY(h.state.currentNode()->displayText().contains(QStringLiteral("７六歩")));
        QCOMPARE(gc.board()->toSfen(), h.state.currentSfen());
        QCOMPARE(changed.count(), 1);
        QVERIFY(h.record.isDirty());

        // 同じ手を指し直しても重複した変化は作らない
        h.record.clearDirty();
        h.nav.goToNode(tree.root());
        from = QPoint(7, 7);
        to = QPoint(7, 6);
        QVERIFY(h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(tree.root()->childCount(), 1);
        QVERIFY(!h.record.isDirty());

        // 違う手は新しい変化になる
        h.nav.goToNode(tree.root());
        from = QPoint(2, 7);
        to = QPoint(2, 6);
        QVERIFY(h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(tree.root()->childCount(), 2);
        QVERIFY(h.record.isDirty());
        QCOMPARE(h.state.currentLineIndex(), 1);

        // 後手番の局面（▲２六歩の後）で先手の駒は動かせず、記録もしない
        QCOMPARE(h.state.currentNode()->ply(), 1);
        const int nodesBefore = tree.nodeCount();
        from = QPoint(2, 6);
        to = QPoint(2, 5);
        QVERIFY(!h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(tree.nodeCount(), nodesBefore);
    }

    // ===== BranchTreeManager の表示 =====

    void manager_compactLayoutPacksLanes()
    {
        Fixture f;
        QGraphicsView view;
        BranchTreeManager manager;
        manager.setView(&view);
        manager.setCompactLayout(true);
        manager.setBranchTree(&f.tree);

        // ライン: 0=本譜, 1=A(5手目から), 2=B(2手目から), 3=C(Bの3手目から)
        QCOMPARE(manager.rowCount(), 4);
        QCOMPARE(manager.laneForRow(0), 0);
        QCOMPARE(manager.laneForRow(1), 1);
        QCOMPARE(manager.laneForRow(2), 1);   // A と手数が重ならないので同じ段に詰める
        QCOMPARE(manager.laneForRow(3), 2);   // 親（B）より下

        manager.setCompactLayout(false);
        QCOMPARE(manager.laneForRow(1), 1);
        QCOMPARE(manager.laneForRow(2), 2);
        QCOMPARE(manager.laneForRow(3), 3);
    }

    /// 行の折りたたみと段の割り当ては描画なしで決まる
    void layout_collapseCountsAndReveals()
    {
        // 0=本譜(8手), 1=A(5手目から), 2=B(2手目から6手目まで), 3=C(Bの4手目から)
        BranchTreeLayout layout;
        layout.rows = {{1, -1, -1, 9}, {5, 0, 10, 7}, {2, 0, 20, 7}, {4, 2, 30, 6}};
        layout.assignLanes(true);
        QCOMPARE(layout.laneForRow(1), 1);
        QCOMPARE(layout.laneForRow(2), 2);   // A と手数が重なるので下の段
        QCOMPARE(layout.laneForRow(3), 3);
        QCOMPARE(layout.laneCount(), 4);

        QVERIFY(layout.isCollapsible(2));
        QVERIFY(layout.setCollapsed(2, true));
        QVERIFY(!layout.setCollapsed(2, true));  // 変わらなければ false
        QVERIFY(layout.isHidden(3));
        QCOMPARE(layout.lastVisiblePly(2), 2);
        // B の続き（3〜6手目の4手）と C の手（4〜5手目の2手）
        QCOMPARE(layout.hiddenNodeCount(2), 6);
        layout.assignLanes(true);
        QCOMPARE(layout.laneForRow(3), -1);

        // 隠れた C の手を表示すると祖先の折りたたみを解く
        QVERIFY(layout.reveal(3, 5));
        QVERIFY(!layout.isCollapsed(2));
        QVERIFY(!layout.reveal(3, 5));
        QVERIFY(!layout.expandAll());
    }

    void manager_collapseHidesDescendants()
    {
        Fixture f;
        QGraphicsView view;
        BranchTreeManager manager;
        manager.setView(&view);
        manager.setCompactLayout(true);
        manager.setBranchTree(&f.tree);

        QVERIFY(manager.isRowCollapsible(2));
        QVERIFY(!manager.isRowCollapsible(3));
        manager.setRowCollapsed(2, true);
        QVERIFY(manager.isRowCollapsed(2));
        QVERIFY(manager.nodeIdFor(2, 2) > 0);    // 先頭の手は残る
        QCOMPARE(manager.nodeIdFor(2, 3), -1);   // 続きは隠れる
        QCOMPARE(manager.laneForRow(3), -1);     // 先の変化も隠れる

        // 折りたたみはツリーを作り直しても保たれる
        manager.setBranchTree(&f.tree);
        QVERIFY(manager.isRowCollapsed(2));

        // 隠れた手を表示すると展開される
        manager.highlightBranchTreeAt(3, 3);
        QVERIFY(!manager.isRowCollapsed(2));
        QCOMPARE(manager.lastHighlightedRow(), 3);
        QCOMPARE(manager.lastHighlightedPly(), 3);
    }

    void manager_keyboardNavigation()
    {
        Fixture f;
        QGraphicsView view;
        BranchTreeManager manager;
        manager.setView(&view);
        manager.setCompactLayout(true);
        manager.setBranchTree(&f.tree);
        QSignalSpy activated(&manager, &BranchTreeManager::branchNodeActivated);

        manager.highlightBranchTreeAt(0, 5);
        QTest::keyClick(&view, Qt::Key_Down);
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.last().at(0).toInt(), 1);
        QCOMPARE(activated.last().at(1).toInt(), 5);

        QTest::keyClick(&view, Qt::Key_Right);
        QCOMPARE(activated.last().at(0).toInt(), 1);
        QCOMPARE(activated.last().at(1).toInt(), 6);

        QTest::keyClick(&view, Qt::Key_Left);
        QCOMPARE(activated.last().at(1).toInt(), 5);

        QTest::keyClick(&view, Qt::Key_Up);
        QCOMPARE(activated.last().at(0).toInt(), 0);

        QTest::keyClick(&view, Qt::Key_End);
        QCOMPARE(activated.last().at(0).toInt(), 0);
        QCOMPARE(activated.last().at(1).toInt(), 6);

        QTest::keyClick(&view, Qt::Key_Home);
        QCOMPARE(activated.last().at(1).toInt(), 0);

        // 対局中など、クリックできない間はキーでも移動しない
        const auto count = activated.count();
        manager.setBranchTreeClickEnabled(false);
        QTest::keyClick(&view, Qt::Key_Right);
        QCOMPARE(activated.count(), count);
    }

    // 図形はツリーのノード ID を持ち、分岐前の手は親の行の図形を強調する。
    // 手順の末尾に1手増えただけなら作り直さずに描き足す
    void manager_readsTreeDirectly()
    {
        Fixture f;
        QGraphicsView view;
        BranchTreeManager manager;
        manager.setView(&view);
        manager.setBranchTree(&f.tree);

        QCOMPARE(manager.nodeIdFor(0, 0), f.tree.root()->nodeId());
        QCOMPARE(manager.nodeIdFor(0, 3), f.main.at(3)->nodeId());
        QCOMPARE(manager.nodeIdFor(1, 5), f.a5->nodeId());
        QCOMPARE(manager.nodeIdFor(1, 3), -1);   // 分岐前の手は本譜の行に描く
        QCOMPARE(manager.rowDispCount(1), 7);

        manager.highlightBranchTreeAt(1, 3);
        QCOMPARE(manager.lastHighlightedRow(), 0);
        QCOMPARE(manager.lastHighlightedPly(), 3);
        manager.highlightBranchTreeAt(1, 6);
        QCOMPARE(manager.lastHighlightedRow(), 1);

        const int rebuilds = manager.rebuildCount();
        KifuBranchNode* a7 = f.add(f.a6, QStringLiteral("a7"));
        manager.setBranchTree(&f.tree);
        QCOMPARE(manager.rebuildCount(), rebuilds);
        QCOMPARE(manager.nodeIdFor(1, 7), a7->nodeId());
        QCOMPARE(manager.rowDispCount(1), 8);

        // 新しい変化ができたときは作り直す
        f.add(f.a5, QStringLiteral("x6"));
        manager.setBranchTree(&f.tree);
        QCOMPARE(manager.rebuildCount(), rebuilds + 1);
        QCOMPARE(manager.rowCount(), f.tree.lineCount());

        // ツリーを外すと開始局面だけになる
        manager.setBranchTree(nullptr);
        QCOMPARE(manager.rowCount(), 0);
        QCOMPARE(manager.nodeIdFor(0, 1), -1);
    }

    void manager_zoomIsClamped()
    {
        QGraphicsView view;
        BranchTreeManager manager;
        manager.setView(&view);
        manager.setZoomPercent(1000);
        QCOMPARE(manager.zoomPercent(), BranchTreeManager::kMaxZoomPercent);
        manager.setZoomPercent(1);
        QCOMPARE(manager.zoomPercent(), BranchTreeManager::kMinZoomPercent);
        manager.setZoomPercent(100);
    }
};

QTEST_MAIN(TestBranchTreeEditing)
#include "tst_branchtree_editing.moc"
