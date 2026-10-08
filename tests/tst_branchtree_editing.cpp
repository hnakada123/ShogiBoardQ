/// @file tst_branchtree_editing.cpp
/// @brief 分岐ツリーの編集（本譜にする・並べ替え・削除）と表示（詰めた配置・折りたたみ・キー操作）のテスト

#include <QtTest>
#include <QGraphicsView>
#include <QSignalSpy>
#include <QStandardPaths>

#include "branchtreeeditcontroller.h"
#include "branchtreemanager.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudisplaypresenter.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
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
} // namespace

class TestBranchTreeEditing : public QObject
{
    Q_OBJECT

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
        KifuNavigationState state;
        state.setTree(&f.tree);
        KifuNavigationController nav;
        nav.setTreeAndState(&f.tree, &state);
        nav.goToNode(f.b3);

        int dirty = 0;
        BranchTreeEditController controller;
        BranchTreeEditController::Deps deps;
        deps.tree = &f.tree;
        deps.navState = &state;
        deps.navController = &nav;
        deps.markGameRecordDirty = [&dirty]() { ++dirty; };
        controller.updateDeps(deps);

        QVERIFY(controller.apply(BranchTreeEditController::Operation::DeleteFromHere, f.b2, false));
        QCOMPARE(state.currentNode(), f.main.at(1));
        QCOMPARE(dirty, 1);
        QCOMPARE(f.main.at(1)->childCount(), 1);
    }

    void controller_promoteKeepsCurrentNode()
    {
        Fixture f;
        KifuNavigationState state;
        state.setTree(&f.tree);
        KifuNavigationController nav;
        nav.setTreeAndState(&f.tree, &state);
        nav.goToNode(f.a6);

        BranchTreeEditController controller;
        BranchTreeEditController::Deps deps;
        deps.tree = &f.tree;
        deps.navState = &state;
        deps.navController = &nav;
        controller.updateDeps(deps);

        using Op = BranchTreeEditController::Operation;
        QVERIFY(controller.isApplicable(Op::PromoteToMainLine, f.a6));
        QVERIFY(controller.isApplicable(Op::MoveUp, f.a6));
        QVERIFY(!controller.isApplicable(Op::MoveDown, f.a6));
        QVERIFY(controller.apply(Op::PromoteToMainLine, f.a6));
        QCOMPARE(state.currentNode(), f.a6);
        QCOMPARE(state.currentLineIndex(), 0);
        QVERIFY(!controller.isApplicable(Op::PromoteToMainLine, f.a6));
        QVERIFY(controller.isApplicable(Op::MoveDown, f.a6));
    }

    void controller_notApplicableDuringGame()
    {
        Fixture f;
        LiveGameSession session;
        session.setTree(&f.tree);
        session.startFromRoot();

        BranchTreeEditController controller;
        BranchTreeEditController::Deps deps;
        deps.tree = &f.tree;
        deps.liveSession = &session;
        controller.updateDeps(deps);
        QVERIFY(!controller.canEdit());
        QVERIFY(!controller.isApplicable(BranchTreeEditController::Operation::DeleteFromHere, f.b2));
        session.discard();
        QVERIFY(controller.canEdit());
    }

    // ===== BranchTreeManager の表示 =====

    void manager_compactLayoutPacksLanes()
    {
        Fixture f;
        QGraphicsView view;
        BranchTreeManager manager;
        manager.setView(&view);
        manager.setCompactLayout(true);
        manager.setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(&f.tree));

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

    void manager_collapseHidesDescendants()
    {
        Fixture f;
        QGraphicsView view;
        BranchTreeManager manager;
        manager.setView(&view);
        manager.setCompactLayout(true);
        manager.setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(&f.tree));

        QVERIFY(manager.isRowCollapsible(2));
        QVERIFY(!manager.isRowCollapsible(3));
        manager.setRowCollapsed(2, true);
        QVERIFY(manager.isRowCollapsed(2));
        QVERIFY(manager.nodeIdFor(2, 2) > 0);    // 先頭の手は残る
        QCOMPARE(manager.nodeIdFor(2, 3), -1);   // 続きは隠れる
        QCOMPARE(manager.laneForRow(3), -1);     // 先の変化も隠れる

        // 折りたたみはツリーを作り直しても保たれる
        manager.setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(&f.tree));
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
        manager.setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(&f.tree));
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
