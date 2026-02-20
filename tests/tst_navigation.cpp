#include <QtTest>
#include <QSignalSpy>
#include <QRandomGenerator>

#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestNavigation : public QObject
{
    Q_OBJECT

private:
    void buildTestTree(KifuBranchTree& tree)
    {
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* current = tree.root();
        for (int i = 1; i <= 7; ++i) {
            current = tree.addMove(current, move,
                                   QStringLiteral("move%1").arg(i),
                                   QStringLiteral("sfen%1").arg(i));
        }
        // Branch at ply 3
        auto* n2 = tree.findByPlyOnMainLine(2);
        auto* b1 = tree.addMove(n2, move, QStringLiteral("branch1"), QStringLiteral("bsfen1"));
        tree.addMove(b1, move, QStringLiteral("branch2"), QStringLiteral("bsfen2"));
    }

private slots:
    void goToFirst()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        // Navigate to ply 5 first
        controller.goToPly(5);
        QCOMPARE(state.currentPly(), 5);

        controller.goToFirst();
        QCOMPARE(state.currentPly(), 0);
        QCOMPARE(state.currentNode(), tree.root());
    }

    void goToLast()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        controller.goToLast();
        QCOMPARE(state.currentPly(), 7);
    }

    void goForward()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        controller.goForward();
        QCOMPARE(state.currentPly(), 1);

        controller.goForward();
        QCOMPARE(state.currentPly(), 2);
    }

    void goBack()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        // Navigate to ply 5 first, then go back
        controller.goToPly(5);
        QCOMPARE(state.currentPly(), 5);

        controller.goBack();
        QCOMPARE(state.currentPly(), 4);

        controller.goBack();
        QCOMPARE(state.currentPly(), 3);
    }

    void canGoForward_atEnd()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.setCurrentNode(tree.findByPlyOnMainLine(7));

        QVERIFY(!state.canGoForward());
    }

    void canGoBack_atRoot()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        QVERIFY(!state.canGoBack());
    }

    void isOnMainLine()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.setCurrentNode(tree.findByPlyOnMainLine(3));

        QVERIFY(state.isOnMainLine());
    }

    void goToPly()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        controller.goToPly(5);
        QCOMPARE(state.currentPly(), 5);
    }

    // 対局終了後のナビゲーション：resetBranchContext() → goToRoot() の後、
    // ナビ状態を最終手に再設定すれば goBack が正しく動作することを確認する。
    void goBack_afterGameOverNavSync()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        // 対局終了時の流れをシミュレート:
        // 1) resetBranchContext() → goToRoot()
        state.goToRoot();
        QCOMPARE(state.currentPly(), 0);

        // 2) updatePlyState で最終手に同期
        KifuBranchNode* lastNode = tree.findByPlyOnMainLine(7);
        QVERIFY(lastNode != nullptr);
        state.setCurrentNode(lastNode);
        QCOMPARE(state.currentPly(), 7);

        // 3) goBack(1) で正しく1手戻れることを確認
        QSignalSpy spy(&controller, &KifuNavigationController::recordHighlightRequired);
        controller.goBack(1);
        QCOMPARE(state.currentPly(), 6);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 6);

        // 4) さらにもう1手戻る
        controller.goBack(1);
        QCOMPARE(state.currentPly(), 5);
    }

    // goToRoot() 直後（ナビ同期なし）に goBack しても、ply 0 のまま留まることを確認。
    void goBack_atRootStaysAtRoot()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        QSignalSpy spy(&controller, &KifuNavigationController::recordHighlightRequired);
        controller.goBack(1);
        // ルートからは戻れないが、シグナルは発火する（ply 0 のまま）
        QCOMPARE(state.currentPly(), 0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
    }

    void stressTest_randomNavigation()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        auto* rng = QRandomGenerator::global();

        for (int i = 0; i < 1000; ++i) {
            int action = rng->bounded(4);
            switch (action) {
            case 0: controller.goToFirst(); break;
            case 1: controller.goToLast(); break;
            case 2: controller.goForward(); break;
            case 3: controller.goBack(); break;
            }

            // Invariants
            QVERIFY(state.currentNode() != nullptr);
            QVERIFY(state.currentPly() >= 0);
        }
    }
};

QTEST_MAIN(TestNavigation)
#include "tst_navigation.moc"
