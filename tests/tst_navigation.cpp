#include <QtTest>
#include <QSignalSpy>
#include <QRandomGenerator>

#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifubranchnode.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestNavigation : public QObject
{
    Q_OBJECT

private:
    /// index 番の手順（KifuBranchTree::allLines()）の末尾の手の ID
    static int lineEnd(const KifuBranchTree& tree, int index) { return tree.allLines().at(index).nodes.constLast()->nodeId(); }

    void buildTestTree(KifuBranchTree& tree)
    {
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* current = tree.root();
        for (int i = 1; i <= 7; ++i) {
            current = TestTree::addMove(tree, current, move,
                                   QStringLiteral("move%1").arg(i),
                                   QStringLiteral("sfen%1").arg(i));
        }
        // Branch at ply 3
        auto* n2 = TestTree::findByPlyOnMainLine(tree, 2);
        auto* b1 = TestTree::addMove(tree, n2, move, QStringLiteral("branch1"), QStringLiteral("bsfen1"));
        TestTree::addMove(tree, b1, move, QStringLiteral("branch2"), QStringLiteral("bsfen2"));
    }

private slots:
    void selectedPathSurvivesReorderingAndSiblingDeletion()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);
        controller.goToPlyOnLine(lineEnd(tree, 1), 2);
        auto* parent = state.currentNode();
        auto* main = parent->childAt(0);
        auto* selected = parent->childAt(1);
        const int selectedId = selected->nodeId();
        QVERIFY(tree.moveChild(selected, 0));
        QCOMPARE(state.currentLineIndex(), 0);
        controller.goForward();
        QCOMPARE(state.currentNode()->nodeId(), selectedId);
        QVERIFY(tree.removeSubtree(main));
        controller.goBack();
        controller.goForward();
        QCOMPARE(state.currentNode()->nodeId(), selectedId);
        controller.goToLast();
        QCOMPARE(state.currentNode()->sfen(), QStringLiteral("bsfen2"));
    }

    void removedCursorNeverAliasesNewNodes()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        KifuNavigationState state(&tree);
        state.setCurrentNode(TestTree::findByPlyOnMainLine(tree, 3));
        const int oldId = state.currentNode()->nodeId();
        QVERIFY(tree.removeSubtree(state.currentNode()));
        // 表示中の手が消えたら開始局面へ移る（消えたノードの ID は使い回さない）
        QCOMPARE(state.currentNode(), tree.root());
        const int oldRootId = tree.root()->nodeId();
        tree.setRootSfen(kHirateSfen);
        QVERIFY(tree.root()->nodeId() != oldId);
        QVERIFY(tree.root()->nodeId() != oldRootId);
        QCOMPARE(state.currentNode(), tree.root());
        QCOMPARE(state.currentLineIndex(), 0);
    }

    void explicitLineNavigationPreservesBranchAtSharedPositions()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);

        controller.goToPlyOnLine(lineEnd(tree, 1), 0);
        QCOMPARE(state.currentNode(), tree.root());
        QCOMPARE(state.currentLineIndex(), 1);
        controller.goForward(3);
        QCOMPARE(state.currentNode()->sfen(), QStringLiteral("bsfen1"));
        controller.goToPlyOnLine(lineEnd(tree, 0), 2);
        QCOMPARE(state.currentLineIndex(), 0);
        controller.goForward();
        QCOMPARE(state.currentNode()->sfen(), QStringLiteral("sfen3"));
        controller.goToPlyOnLine(lineEnd(tree, 1), 20);
        QCOMPARE(state.currentPly(), 4);
        QCOMPARE(state.currentLineIndex(), 1);

        // 存在しない手へは移らず、通知も出さない
        QSignalSpy changed(&state, &KifuNavigationState::changed);
        controller.goToPlyOnLine(-1, 0);
        controller.goToPlyOnLine(9999, 0);
        QCOMPARE(state.currentPly(), 4);
        QCOMPARE(changed.count(), 0);
    }

    /// 手順を末尾の手で覚えると、前に分岐を足しても本譜にしても同じ手順を指す（番号は変わる）
    void lineEndSurvivesBranchEditing()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);
        const int branchEnd = lineEnd(tree, 1);
        // 本譜の4手目の後に変化を足すと、深さ優先の番号では先に数えるので分岐は2番になる
        ShogiMove move;
        TestTree::addMove(tree, TestTree::findByPlyOnMainLine(tree, 4), move, QStringLiteral("other"),
                          QStringLiteral("osfen"));
        QCOMPARE(tree.findLineIndexForNode(tree.nodeAt(branchEnd)).value_or(-1), 2);
        controller.goToPlyOnLine(branchEnd, 3);
        QCOMPARE(state.currentNode()->sfen(), QStringLiteral("bsfen1"));
        // 分岐を本譜にしても同じ手を指す
        QVERIFY(tree.moveChild(state.currentNode(), 0));
        QCOMPARE(state.currentLineIndex(), 0);
        controller.goToPlyOnLine(branchEnd, 4);
        QCOMPARE(state.currentNode()->sfen(), QStringLiteral("bsfen2"));
        QCOMPARE(tree.nodeOnLine(branchEnd, 2), TestTree::findByPlyOnMainLine(tree, 2));
    }

    /// 表示中の手が変わるたびに changed() を1回だけ出し、変わらなければ出さない
    /// （盤・棋譜欄・検討はこの通知を受けて表示中の局面を読む）
    void navigationNotifiesChangedOncePerMove()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        const ShogiMove move(UsiMove::fromString(u"7g7f"), Piece::BlackPawn);
        const QString after = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        TestTree::addMove(tree, tree.root(), move, QStringLiteral("７六歩"), after);
        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);
        QSignalSpy changed(&state, &KifuNavigationState::changed);

        controller.goToPly(1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(state.currentSfen(), after);
        QCOMPARE(state.currentNode()->move().move.toString(), QStringLiteral("7g7f"));
        controller.goToFirst();
        QCOMPARE(changed.count(), 2);
        QCOMPARE(state.currentSfen(), kHirateSfen);
        controller.goToFirst();
        QCOMPARE(changed.count(), 2);
        controller.goToLast();
        QCOMPARE(changed.count(), 3);
        QCOMPARE(state.currentSfen(), after);
        controller.selectBranchTreeNode(state.tree()->root()->nodeId());
        QCOMPARE(changed.count(), 4);
        QCOMPARE(state.currentSfen(), kHirateSfen);
    }

    void goToFirst()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);

        KifuNavigationController controller(&state);

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

        KifuNavigationState state(&tree);
        state.goToRoot();

        KifuNavigationController controller(&state);

        controller.goToLast();
        QCOMPARE(state.currentPly(), 7);
    }

    void goForward()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);
        state.goToRoot();

        KifuNavigationController controller(&state);

        controller.goForward();
        QCOMPARE(state.currentPly(), 1);

        controller.goForward();
        QCOMPARE(state.currentPly(), 2);
    }

    void goBack()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);

        KifuNavigationController controller(&state);

        // Navigate to ply 5 first, then go back
        controller.goToPly(5);
        QCOMPARE(state.currentPly(), 5);

        controller.goBack();
        QCOMPARE(state.currentPly(), 4);

        controller.goBack();
        QCOMPARE(state.currentPly(), 3);
    }

    /// 最終手から進む・開始局面から戻る操作では動かず、通知も出さない
    void goForwardAtEnd_andGoBackAtRoot_stay()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);
        state.setCurrentNode(TestTree::findByPlyOnMainLine(tree, 7));
        QCOMPARE(state.nextNode(state.currentNode()), nullptr);

        QSignalSpy changed(&state, &KifuNavigationState::changed);
        controller.goForward();
        QCOMPARE(state.currentPly(), 7);
        controller.goToFirst();
        QCOMPARE(changed.count(), 1);
        controller.goBack();
        QCOMPARE(state.currentNode(), tree.root());
        QCOMPARE(changed.count(), 1);
    }

    void isOnMainLine()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);
        state.setCurrentNode(TestTree::findByPlyOnMainLine(tree, 3));

        QVERIFY(state.isOnMainLine());
    }

    void goToPly()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);
        state.goToRoot();

        KifuNavigationController controller(&state);

        controller.goToPly(5);
        QCOMPARE(state.currentPly(), 5);
    }

    // 対局終了後のナビゲーション：goToRoot() の後、
    // ナビ状態を最終手に再設定すれば goBack が正しく動作することを確認する。
    void goBack_afterGameOverNavSync()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);

        KifuNavigationController controller(&state);

        // 対局終了時の流れをシミュレート:
        // 1) goToRoot()
        state.goToRoot();
        QCOMPARE(state.currentPly(), 0);

        // 2) syncCursorToPly で最終手に同期
        KifuBranchNode* lastNode = TestTree::findByPlyOnMainLine(tree, 7);
        QVERIFY(lastNode != nullptr);
        state.setCurrentNode(lastNode);
        QCOMPARE(state.currentPly(), 7);

        // 3) goBack(1) で正しく1手戻れることを確認
        QSignalSpy spy(&state, &KifuNavigationState::changed);
        controller.goBack(1);
        QCOMPARE(state.currentPly(), 6);
        QCOMPARE(spy.count(), 1);

        // 4) さらにもう1手戻る
        controller.goBack(1);
        QCOMPARE(state.currentPly(), 5);
    }

    // goToRoot() 直後（ナビ同期なし）に goBack しても、ply 0 のまま留まることを確認。
    void goBack_atRootStaysAtRoot()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);
        state.goToRoot();

        KifuNavigationController controller(&state);

        QSignalSpy spy(&state, &KifuNavigationState::changed);
        controller.goBack(1);
        // ルートからは戻れず、表示中の手も変わらないので通知しない
        QCOMPARE(state.currentPly(), 0);
        QCOMPARE(spy.count(), 0);
    }

    void selectBranchTreeNode_switchesToMainLineFromNestedBranch()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* root = tree.root();
        QVERIFY(root != nullptr);

        // 本譜: 1 -> 2 -> 3(main) -> 4(main) -> 5(main)
        auto* main1 = TestTree::addMove(tree, root, move, QStringLiteral("main1"), QStringLiteral("sfen1"));
        auto* main2 = TestTree::addMove(tree, main1, move, QStringLiteral("main2"), QStringLiteral("sfen2"));
        auto* main3 = TestTree::addMove(tree, main2, move, QStringLiteral("main3"), QStringLiteral("sfen3"));
        auto* main4 = TestTree::addMove(tree, main3, move, QStringLiteral("main4"), QStringLiteral("sfen4"));
        QVERIFY(TestTree::addMove(tree, main4, move, QStringLiteral("main5"), QStringLiteral("sfen5")) != nullptr);

        // 分岐: 2手目から分岐し、さらに5手目で入れ子分岐
        auto* branch3 = TestTree::addMove(tree, main2, move, QStringLiteral("branch3"), QStringLiteral("bsfen3"));
        auto* branch4 = TestTree::addMove(tree, branch3, move, QStringLiteral("branch4"), QStringLiteral("bsfen4"));
        QVERIFY(TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_main"), QStringLiteral("bsfen5m")) != nullptr);
        auto* branch5Nested = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_nested"), QStringLiteral("bsfen5n"));
        QVERIFY(branch5Nested != nullptr);

        KifuNavigationState state(&tree);

        KifuNavigationController controller(&state);

        const auto nestedLineIndex = tree.findLineIndexForNode(branch5Nested);
        QVERIFY(nestedLineIndex.has_value() && *nestedLineIndex > 0);

        // まず入れ子分岐（5手目）を選択
        controller.selectBranchTreeNode(branch5Nested->nodeId());
        QCOMPARE(state.currentNode(), branch5Nested);
        QCOMPARE(state.currentLineIndex(), *nestedLineIndex);

        // 次に本譜3手目をクリックしたとき、分岐3手目に吸われず本譜3手目へ遷移すること
        controller.selectBranchTreeNode(main3->nodeId());
        QCOMPARE(state.currentNode(), main3);
        QCOMPARE(state.currentLineIndex(), 0);
        QVERIFY(state.isOnMainLine());
    }

    void isOnMainLine_nestedBranchFirstChildIsNotMainLine()
    {
        // 入れ子分岐の先頭子ノードは KifuBranchNode::isMainLine()（親の最初の子か）では true になるが、
        // 表示中ラインは本譜ではないので KifuNavigationState::isOnMainLine() は false であること
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* main1 = TestTree::addMove(tree, tree.root(), move, QStringLiteral("main1"), QStringLiteral("sfen1"));
        auto* main2 = TestTree::addMove(tree, main1, move, QStringLiteral("main2"), QStringLiteral("sfen2"));
        auto* main3 = TestTree::addMove(tree, main2, move, QStringLiteral("main3"), QStringLiteral("sfen3"));
        auto* main4 = TestTree::addMove(tree, main3, move, QStringLiteral("main4"), QStringLiteral("sfen4"));
        auto* main5 = TestTree::addMove(tree, main4, move, QStringLiteral("main5"), QStringLiteral("sfen5"));

        auto* branch3 = TestTree::addMove(tree, main2, move, QStringLiteral("branch3"), QStringLiteral("bsfen3"));
        auto* branch4 = TestTree::addMove(tree, branch3, move, QStringLiteral("branch4"), QStringLiteral("bsfen4"));
        auto* branch5First = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_first"), QStringLiteral("bsfen5f"));
        auto* branch5Nested = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_nested"), QStringLiteral("bsfen5n"));
        QVERIFY(branch5Nested != nullptr);

        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);

        const auto firstLineIndex = tree.findLineIndexForNode(branch5First);
        QVERIFY(firstLineIndex.has_value() && *firstLineIndex > 0);

        controller.selectBranchTreeNode(branch5First->nodeId());
        QCOMPARE(state.currentNode(), branch5First);
        QVERIFY(branch5First->isMainLine());     // ノード単位では親の最初の子
        QVERIFY(!state.isOnMainLine());          // 表示中ラインは分岐

        // 分岐点の子（2番目の子）でも当然 false
        controller.selectBranchTreeNode(branch3->nodeId());
        QCOMPARE(state.currentNode(), branch3);
        QVERIFY(!state.isOnMainLine());

        // 本譜へ戻れば true
        controller.goToMainLineAtCurrentPly();
        QCOMPARE(state.currentNode(), main3);
        QVERIFY(state.isOnMainLine());
        controller.goToNode(main5);
        QVERIFY(state.isOnMainLine());
    }

    void selectBranchTreeNode_sharedNodeOnParentBranchRowKeepsCurrentLine()
    {
        // 入れ子分岐（ライン2）を表示中に、親分岐（ライン1）の行に描かれた共有ノードを
        // クリックしても現在ラインに留まること（本譜行の共有ノードと同じ扱い）。
        // 共有でないノードをクリックしたときはそのラインへ切り替わること。
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* main1 = TestTree::addMove(tree, tree.root(), move, QStringLiteral("main1"), QStringLiteral("sfen1"));
        auto* main2 = TestTree::addMove(tree, main1, move, QStringLiteral("main2"), QStringLiteral("sfen2"));
        auto* main3 = TestTree::addMove(tree, main2, move, QStringLiteral("main3"), QStringLiteral("sfen3"));
        auto* main4 = TestTree::addMove(tree, main3, move, QStringLiteral("main4"), QStringLiteral("sfen4"));
        QVERIFY(TestTree::addMove(tree, main4, move, QStringLiteral("main5"), QStringLiteral("sfen5")) != nullptr);

        auto* branch3 = TestTree::addMove(tree, main2, move, QStringLiteral("branch3"), QStringLiteral("bsfen3"));
        auto* branch4 = TestTree::addMove(tree, branch3, move, QStringLiteral("branch4"), QStringLiteral("bsfen4"));
        auto* branch5First = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_first"), QStringLiteral("bsfen5f"));
        auto* branch5Nested = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_nested"), QStringLiteral("bsfen5n"));

        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);

        const int line1 = tree.findLineIndexForNode(branch5First).value_or(-1);
        const int line2 = tree.findLineIndexForNode(branch5Nested).value_or(-1);
        QCOMPARE(line1, 1);
        QCOMPARE(line2, 2);

        // 入れ子分岐（ライン2）の5手目を表示中
        controller.selectBranchTreeNode(branch5Nested->nodeId());
        QCOMPARE(state.currentNode(), branch5Nested);
        QCOMPARE(state.currentLineIndex(), line2);

        // ライン1の行に描かれる共有ノード（3手目 branch3）をクリック → ライン2に留まる
        controller.selectBranchTreeNode(branch3->nodeId());
        QCOMPARE(state.currentNode(), branch3);
        QCOMPARE(state.currentLineIndex(), line2);

        // 本譜行の共有ノード（2手目 main2）をクリック → ライン2に留まる
        controller.selectBranchTreeNode(main2->nodeId());
        QCOMPARE(state.currentNode(), main2);
        QCOMPARE(state.currentLineIndex(), line2);

        // ライン1だけにあるノード（5手目 branch5_first）をクリック → ライン1へ切り替わる
        controller.selectBranchTreeNode(branch5First->nodeId());
        QCOMPARE(state.currentNode(), branch5First);
        QCOMPARE(state.currentLineIndex(), line1);

        // 本譜だけにあるノード（3手目 main3）をクリック → 本譜へ切り替わる
        controller.selectBranchTreeNode(main3->nodeId());
        QCOMPARE(state.currentNode(), main3);
        QCOMPARE(state.currentLineIndex(), 0);
    }

    /// 移った手までの経路の分岐点で選んだ子を覚え、経路の外の分岐点で選んだ子は忘れない
    void selectedChildrenAreRememberedAtEveryBranchPoint()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* main1 = TestTree::addMove(tree, tree.root(), move, QStringLiteral("main1"), QStringLiteral("sfen1"));
        auto* main2 = TestTree::addMove(tree, main1, move, QStringLiteral("main2"), QStringLiteral("sfen2"));
        auto* main3 = TestTree::addMove(tree, main2, move, QStringLiteral("main3"), QStringLiteral("sfen3"));
        QVERIFY(main3 != nullptr);
        auto* branch3 = TestTree::addMove(tree, main2, move, QStringLiteral("branch3"), QStringLiteral("bsfen3"));
        auto* branch4 = TestTree::addMove(tree, branch3, move, QStringLiteral("branch4"), QStringLiteral("bsfen4"));
        auto* branch5First = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_first"), QStringLiteral("bsfen5f"));
        auto* branch5Nested = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_nested"), QStringLiteral("bsfen5n"));

        KifuNavigationState state(&tree);
        QCOMPARE(state.nextNode(main2), main3);
        QCOMPARE(state.nextNode(branch4), branch5First);

        state.setCurrentNode(branch5Nested);
        QCOMPARE(state.nextNode(main2), branch3);
        QCOMPARE(state.nextNode(branch4), branch5Nested);
        QCOMPARE(state.selectedPath().constLast(), branch5Nested);

        state.setCurrentNode(main3);
        QCOMPARE(state.nextNode(main2), main3);
        QCOMPARE(state.nextNode(branch4), branch5Nested);   // 経路外の分岐点は変わらない

        state.resetToNode(main3);                          // 選択を忘れる
        QCOMPARE(state.nextNode(branch4), branch5First);
    }

    void goToMainLineAtCurrentPly_whenMainLineIsShorter()
    {
        // 本譜3手、分岐が5手まで続く。分岐の5手目で「本譜へ戻る」と本譜の最終手（3手目）へ移動すること
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* main1 = TestTree::addMove(tree, tree.root(), move, QStringLiteral("main1"), QStringLiteral("sfen1"));
        auto* main2 = TestTree::addMove(tree, main1, move, QStringLiteral("main2"), QStringLiteral("sfen2"));
        auto* main3 = TestTree::addMove(tree, main2, move, QStringLiteral("main3"), QStringLiteral("sfen3"));

        auto* branch3 = TestTree::addMove(tree, main2, move, QStringLiteral("branch3"), QStringLiteral("bsfen3"));
        auto* branch4 = TestTree::addMove(tree, branch3, move, QStringLiteral("branch4"), QStringLiteral("bsfen4"));
        QVERIFY(TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_first"), QStringLiteral("bsfen5f")) != nullptr);
        auto* branch5Nested = TestTree::addMove(tree, branch4, move, QStringLiteral("branch5_nested"), QStringLiteral("bsfen5n"));
        QVERIFY(branch5Nested != nullptr);

        KifuNavigationState state(&tree);
        KifuNavigationController controller(&state);

        const auto nestedLineIndex = tree.findLineIndexForNode(branch5Nested);
        QVERIFY(nestedLineIndex.has_value() && *nestedLineIndex > 0);
        controller.selectBranchTreeNode(branch5Nested->nodeId());
        QCOMPARE(state.currentNode(), branch5Nested);
        QVERIFY(state.offersBackToMainLine());   // 「本譜へ戻る」が表示される状況
        QVERIFY(!state.isOnMainLine());

        QSignalSpy changed(&state, &KifuNavigationState::changed);
        controller.goToMainLineAtCurrentPly();

        QCOMPARE(state.currentNode(), main3);
        QVERIFY(state.isOnMainLine());
        QCOMPARE(state.currentLineIndex(), 0);
        QVERIFY(!state.offersBackToMainLine());
        QCOMPARE(changed.count(), 1);

        // 以降のナビゲーションも本譜を辿ること
        controller.goBack(1);
        QCOMPARE(state.currentNode(), main2);
        controller.goForward(1);
        QCOMPARE(state.currentNode(), main3);
    }

    void stressTest_randomNavigation()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        KifuNavigationState state(&tree);
        state.goToRoot();

        KifuNavigationController controller(&state);

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
