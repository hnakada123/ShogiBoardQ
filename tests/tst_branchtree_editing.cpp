/// @file tst_branchtree_editing.cpp
/// @brief 分岐ツリーの編集（本譜にする・並べ替え・削除）と表示（詰めた配置・折りたたみ・キー操作）のテスト

#include <QtTest>
#include <QAbstractButton>
#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTimer>

#include "branchtreeeditcontroller.h"
#include "branchtreelayout.h"
#include "branchtreeitemroles.h"
#include "branchtreeview.h"
#include "kifudocument.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
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
        return TestTree::addMove(tree, parent, ShogiMove(), mark + name, QStringLiteral("sfen-") + name);
    }
};

/// 分岐の編集に要る部品一式（表示中の手・移動・ライブ対局・未保存の印）
struct EditHarness {
    KifuNavigationState state;
    KifuNavigationController nav;
    LiveGameSession live;
    KifuDocument document;   ///< 未保存かどうか（ツリーの変更で決まる）
    BranchTreeEditController controller;

    explicit EditHarness(KifuBranchTree& tree)
        : state(&tree)
        , nav(&state)
        , live(&tree, &state)
        , document(&tree)
        , controller(deps(tree, state, nav, live))
    {
    }

    static BranchTreeEditController::Deps deps(KifuBranchTree& tree, KifuNavigationState& state,
                                               KifuNavigationController& nav, LiveGameSession& live)
    {
        BranchTreeEditController::Deps d;
        d.tree = &tree;
        d.navState = &state;
        d.navController = &nav;
        d.liveSession = &live;
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
        QSignalSpy changed(&f.tree, &KifuBranchTree::childrenReordered);
        QVERIFY(f.tree.promoteToMainLine(f.c3));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toInt(), f.main.at(1)->nodeId());   // 並びを変えた最もルートに近い手
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
        QVERIFY(h.document.isModified());
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
        QVERIFY(!h.document.isModified());
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
        // 記録した手は、指す前の局面と一緒に知らせる（定跡ウィンドウの「盤の手を登録」）
        QSignalSpy recorded(&h.controller, &BranchTreeEditController::boardMoveRecorded);
        QPoint from(7, 7), to(7, 6);
        QVERIFY(h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(recorded.count(), 1);
        QCOMPARE(recorded.last().at(0).toString(), tree.root()->sfen());
        QCOMPARE(recorded.last().at(1).toString(), QStringLiteral("7g7f"));
        QCOMPARE(tree.root()->childCount(), 1);
        QCOMPARE(h.state.currentNode(), tree.root()->childAt(0));
        QVERIFY(h.state.currentNode()->displayText().contains(QStringLiteral("７六歩")));
        QCOMPARE(gc.board()->toSfen(), h.state.currentSfen());
        QCOMPARE(changed.count(), 1);
        QVERIFY(h.document.isModified());

        // 同じ手を指し直しても重複した変化は作らない
        h.document.markSaved(QString());
        h.nav.goToNode(tree.root());
        from = QPoint(7, 7);
        to = QPoint(7, 6);
        QVERIFY(h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(tree.root()->childCount(), 1);
        QVERIFY(!h.document.isModified());
        QCOMPARE(recorded.count(), 2);

        // 違う手は新しい変化になる
        h.nav.goToNode(tree.root());
        from = QPoint(2, 7);
        to = QPoint(2, 6);
        QVERIFY(h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(tree.root()->childCount(), 2);
        QVERIFY(h.document.isModified());
        QCOMPARE(h.state.currentLineIndex(), 1);

        // 後手番の局面（▲２六歩の後）で先手の駒は動かせず、記録もしない
        QCOMPARE(h.state.currentNode()->ply(), 1);
        const int nodesBefore = tree.nodeCount();
        from = QPoint(2, 6);
        to = QPoint(2, 5);
        QVERIFY(!h.controller.recordBoardMove(from, to, &gc));
        QCOMPARE(tree.nodeCount(), nodesBefore);
        QCOMPARE(recorded.count(), 3);
        QCOMPARE(recorded.last().at(1).toString(), QStringLiteral("2g2f"));
    }

    // ===== BranchTreeView の表示 =====

    void view_compactLayoutPacksLanes()
    {
        Fixture f;
        BranchTreeView view;
        view.setCompactLayout(true);
        view.setTree(&f.tree);

        // 変化: A(5手目から), B(2手目から), C(Bの3手目から)
        QCOMPARE(view.laneOf(f.main.at(1)->nodeId()), 0);
        QCOMPARE(view.laneOf(f.a5->nodeId()), 1);
        QCOMPARE(view.laneOf(f.b2->nodeId()), 1);   // A と手数が重ならないので同じ段に詰める
        QCOMPARE(view.laneOf(f.c3->nodeId()), 2);   // 親（B）より下

        view.setCompactLayout(false);
        QCOMPARE(view.laneOf(f.a5->nodeId()), 1);
        QCOMPARE(view.laneOf(f.b2->nodeId()), 2);
        QCOMPARE(view.laneOf(f.c3->nodeId()), 3);
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

    void view_collapseHidesDescendants()
    {
        Fixture f;
        BranchTreeView view;
        view.setCompactLayout(true);
        view.setTree(&f.tree);

        QVERIFY(view.isCollapsible(f.b3->nodeId()));   // 変化の中のどの手でもその変化を指す
        QVERIFY(!view.isCollapsible(f.c3->nodeId()));
        QVERIFY(!view.isCollapsible(f.main.at(2)->nodeId()));   // 本譜は折りたためない
        view.setCollapsed(f.b2->nodeId(), true);
        QVERIFY(view.isCollapsed(f.b3->nodeId()));
        QVERIFY(view.isDrawn(f.b2->nodeId()));    // 先頭の手は残る
        QVERIFY(!view.isDrawn(f.b3->nodeId()));   // 続きは隠れる
        QCOMPARE(view.laneOf(f.c3->nodeId()), -1);   // 先の変化も隠れる

        // 折りたたみはツリーを作り直しても保たれる
        view.setTree(&f.tree);
        QVERIFY(view.isCollapsed(f.b2->nodeId()));

        // 隠れた手を表示すると展開される
        view.setCurrentNode(f.c3->nodeId());
        QVERIFY(!view.isCollapsed(f.b2->nodeId()));
        QCOMPARE(view.currentNodeId(), f.c3->nodeId());
    }

    void view_keyboardNavigation()
    {
        Fixture f;
        BranchTreeView view;
        view.setCompactLayout(true);
        view.setTree(&f.tree);
        QSignalSpy activated(&view, &BranchTreeView::nodeActivated);

        view.setCurrentNode(f.main.at(5)->nodeId());
        QTest::keyClick(&view, Qt::Key_Down);
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.last().at(0).toInt(), f.a5->nodeId());

        QTest::keyClick(&view, Qt::Key_Right);
        QCOMPARE(activated.last().at(0).toInt(), f.a6->nodeId());

        QTest::keyClick(&view, Qt::Key_Left);
        QCOMPARE(activated.last().at(0).toInt(), f.a5->nodeId());

        QTest::keyClick(&view, Qt::Key_Up);
        QCOMPARE(activated.last().at(0).toInt(), f.main.at(5)->nodeId());

        QTest::keyClick(&view, Qt::Key_End);
        QCOMPARE(activated.last().at(0).toInt(), f.main.at(6)->nodeId());

        QTest::keyClick(&view, Qt::Key_Home);
        QCOMPARE(activated.last().at(0).toInt(), f.tree.root()->nodeId());
        QCOMPARE(view.currentNodeId(), f.tree.root()->nodeId());

        // 対局中など、クリックできない間はキーでも移動しない
        const auto count = activated.count();
        view.setClickEnabled(false);
        QTest::keyClick(&view, Qt::Key_Right);
        QCOMPARE(activated.count(), count);
    }

    // 手は、その手を初めて含む変化の行に描く（分岐前の手は親の手順の行）。
    // ツリーの変更を受けて描き直し、手順の末尾に1手増えただけなら作り直さずに描き足す
    void view_followsTreeChanges()
    {
        Fixture f;
        BranchTreeView view;
        view.setTree(&f.tree);

        QVERIFY(view.isDrawn(f.tree.root()->nodeId()));
        QCOMPARE(view.laneOf(f.main.at(3)->nodeId()), 0);   // 分岐前の手は本譜の行に描く
        QCOMPARE(view.laneOf(f.a5->nodeId()), 1);

        view.setCurrentNode(f.main.at(3)->nodeId());
        QCOMPARE(view.currentNodeId(), f.main.at(3)->nodeId());
        view.setCurrentNode(f.a6->nodeId());
        QCOMPARE(view.currentNodeId(), f.a6->nodeId());

        // 表示中の手を、描く前に知らされても、描いた時点で目立たせる
        const int rebuilds = view.rebuildCount();
        f.tree.beginBatchUpdate();
        KifuBranchNode* a7 = f.add(f.a6, QStringLiteral("a7"));
        view.setCurrentNode(a7->nodeId());
        QCOMPARE(view.currentNodeId(), -1);
        f.tree.endBatchUpdate();
        QCOMPARE(view.rebuildCount(), rebuilds);
        QCOMPARE(view.laneOf(a7->nodeId()), 1);
        QCOMPARE(view.currentNodeId(), a7->nodeId());

        // 新しい変化ができたとき・並べ替えたときは作り直す
        KifuBranchNode* x6 = f.add(f.a5, QStringLiteral("x6"));
        const int x6Id = x6->nodeId();
        QCOMPARE(view.rebuildCount(), rebuilds + 1);
        QVERIFY(view.isDrawn(x6Id));
        QVERIFY(f.tree.moveChild(x6, 0));
        QCOMPARE(view.rebuildCount(), rebuilds + 2);
        QVERIFY(f.tree.removeSubtree(x6));
        QCOMPARE(view.rebuildCount(), rebuilds + 3);
        QVERIFY(!view.isDrawn(x6Id));
        QVERIFY(view.isDrawn(a7->nodeId()));

        // ツリーを外すと開始局面だけになる
        view.setTree(nullptr);
        QVERIFY(!view.isDrawn(f.main.at(1)->nodeId()));
        // 外したツリーの変更では描き直さない
        const int detached = view.rebuildCount();
        f.add(a7, QStringLiteral("a8"));
        QCOMPARE(view.rebuildCount(), detached);
    }

    void view_commentMarkerFollowsDocument()
    {
        Fixture f;
        BranchTreeView view;
        view.setTree(&f.tree);
        const auto tipOf = [&view](int nodeId) {
            const QList<QGraphicsItem*> items = view.scene()->items();
            for (const QGraphicsItem* item : items) {
                if (item->data(BranchTreeItemRoles::NodeId).toInt() == nodeId) return item->toolTip();
            }
            return QString();
        };
        QVERIFY(!tipOf(f.a5->nodeId()).contains(QStringLiteral("注目の一手")));
        f.a5->record().comment = QStringLiteral("注目の一手");
        view.refreshNode(f.a5->nodeId());
        QVERIFY(tipOf(f.a5->nodeId()).contains(QStringLiteral("注目の一手")));
    }

    void view_zoomIsClamped()
    {
        BranchTreeView view;
        view.setZoomPercent(1000);
        QCOMPARE(view.zoomPercent(), BranchTreeView::kMaxZoomPercent);
        view.setZoomPercent(1);
        QCOMPARE(view.zoomPercent(), BranchTreeView::kMinZoomPercent);
        view.setZoomPercent(100);
    }
};

QTEST_MAIN(TestBranchTreeEditing)
#include "tst_branchtree_editing.moc"
