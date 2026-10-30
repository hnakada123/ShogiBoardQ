#include <QtTest>
#include <QSignalSpy>

#include <memory>

#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifubranchnode.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestKifuBranchTree : public QObject
{
    Q_OBJECT

private:
    // Helper: build a 7-move mainline + 2-move branch at ply 3
    void buildTestTree(KifuBranchTree& tree)
    {
        tree.setRootSfen(kHirateSfen);
        auto* root = tree.root();
        QVERIFY(root != nullptr);

        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, root, dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"));
        auto* n2 = TestTree::addMove(tree, n1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("sfen2"));
        auto* n3 = TestTree::addMove(tree, n2, dummyMove, QStringLiteral("▲２六歩"), QStringLiteral("sfen3"));
        auto* n4 = TestTree::addMove(tree, n3, dummyMove, QStringLiteral("△８四歩"), QStringLiteral("sfen4"));
        auto* n5 = TestTree::addMove(tree, n4, dummyMove, QStringLiteral("▲２五歩"), QStringLiteral("sfen5"));
        auto* n6 = TestTree::addMove(tree, n5, dummyMove, QStringLiteral("△８五歩"), QStringLiteral("sfen6"));
        TestTree::addMove(tree, n6, dummyMove, QStringLiteral("▲７八金"), QStringLiteral("sfen7"));

        // Branch at ply 3 (from n2, alternative to n3)
        auto* b1 = TestTree::addMove(tree, n2, dummyMove, QStringLiteral("▲６六歩"), QStringLiteral("branch_sfen1"));
        TestTree::addMove(tree, b1, dummyMove, QStringLiteral("△８四歩"), QStringLiteral("branch_sfen2"));
    }

private slots:
    void removeLeaf_invalidatesCacheAndKeepsOtherBranches()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("m1"), QStringLiteral("s1"));
        auto* main = TestTree::addMove(tree, first, ShogiMove(), QStringLiteral("m2"), QStringLiteral("s2"));
        auto* branch = TestTree::addMove(tree, first, ShogiMove(), QStringLiteral("b2"), QStringLiteral("b2s"));
        const int removedId = branch->nodeId();
        QCOMPARE(tree.allLines().size(), 2); // キャッシュを作ってから削除する
        QSignalSpy removed(&tree, &KifuBranchTree::subtreeRemoved);
        QVERIFY(!tree.removeLeaf(tree.root()));
        QVERIFY(!tree.removeLeaf(first));
        QVERIFY(!tree.removeLeaf(nullptr));
        QCOMPARE(removed.count(), 0);
        QVERIFY(tree.removeLeaf(branch));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0).at(0).toInt(), first->nodeId());
        QCOMPARE(removed.at(0).at(1).toInt(), removedId);
        QCOMPARE(tree.allLines().size(), 1);
        QCOMPARE(tree.nodeCount(), 3);
        QCOMPARE(first->childCount(), 1);
        QCOMPARE(first->childAt(0), main);
        QVERIFY(tree.nodeAt(removedId) == nullptr);
        auto* next = TestTree::addMove(tree, main, ShogiMove(), QStringLiteral("m3"), QStringLiteral("s3"));
        QVERIFY(next->nodeId() > removedId); // 削除されたノードIDを再利用しない
        QCOMPARE(tree.mainLine().last(), next);
    }

    void removeSubtree_forgetsDescendants()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("m1"), QStringLiteral("s1"));
        auto* second = TestTree::addMove(tree, first, ShogiMove(), QStringLiteral("m2"), QStringLiteral("s2"));
        auto* third = TestTree::addMove(tree, second, ShogiMove(), QStringLiteral("m3"), QStringLiteral("s3"));
        const int secondId = second->nodeId();
        const int thirdId = third->nodeId();
        QSignalSpy removed(&tree, &KifuBranchTree::subtreeRemoved);
        QVERIFY(tree.removeSubtree(second));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0).at(0).toInt(), first->nodeId());
        QCOMPARE(removed.at(0).at(1).toInt(), secondId);
        QVERIFY(tree.nodeAt(secondId) == nullptr);
        QVERIFY(tree.nodeAt(thirdId) == nullptr);
        QCOMPARE(tree.nodeCount(), 2);
        QCOMPARE(first->childCount(), 0);
    }

    void moveChild_reordersLinesAndReportsParent()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("m1"), QStringLiteral("s1"));
        auto* main = TestTree::addMove(tree, first, ShogiMove(), QStringLiteral("m2"), QStringLiteral("s2"));
        auto* branch = TestTree::addMove(tree, first, ShogiMove(), QStringLiteral("b2"), QStringLiteral("b2s"));
        QCOMPARE(tree.mainLine().last(), main); // キャッシュを作ってから並べ替える
        const quint64 before = tree.revision();
        QSignalSpy reordered(&tree, &KifuBranchTree::childrenReordered);

        QVERIFY(tree.moveChild(branch, 0));
        QCOMPARE(reordered.count(), 1);
        QCOMPARE(reordered.at(0).at(0).toInt(), first->nodeId());
        QVERIFY(tree.revision() > before);
        QCOMPARE(first->childAt(0), branch);
        QCOMPARE(first->childAt(1), main);
        QCOMPARE(first->indexOfChild(main), 1);
        QCOMPARE(tree.mainLine().last(), branch);
        QCOMPARE(tree.findLineIndexForNode(main).value_or(-1), 1);

        // 同じ位置・範囲外・ルートは変更しない。
        const quint64 unchanged = tree.revision();
        QVERIFY(!tree.moveChild(branch, 0));
        QVERIFY(!tree.moveChild(branch, 2));
        QVERIFY(!tree.moveChild(tree.root(), 0));
        QVERIFY(!tree.moveChild(nullptr, 0));
        QCOMPARE(tree.revision(), unchanged);
        QCOMPARE(reordered.count(), 1);
    }

    void moveChild_rotatesAcrossSeveralSiblings()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        QList<KifuBranchNode*> kids;
        for (int i = 0; i < 4; ++i)
            kids << TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("m%1").arg(i), QStringLiteral("s%1").arg(i));
        QVERIFY(tree.moveChild(kids.at(3), 1));
        QCOMPARE(tree.root()->children(), (QList<KifuBranchNode*>{kids.at(0), kids.at(3), kids.at(1), kids.at(2)}));
        QVERIFY(tree.moveChild(kids.at(0), 3));
        QCOMPARE(tree.root()->children(), (QList<KifuBranchNode*>{kids.at(3), kids.at(1), kids.at(2), kids.at(0)}));
    }

    void revision_increasesOnEveryStructuralChange()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        quint64 last = tree.revision();
        auto* node = TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("m1"), QStringLiteral("s1"));
        QVERIFY(tree.revision() > last);
        last = tree.revision();
        TestTree::record(node).comment = QStringLiteral("comment");
        QCOMPARE(tree.revision(), last); // コメントは構造変更ではない
        QVERIFY(tree.removeLeaf(node));
        QVERIFY(tree.revision() > last);
        last = tree.revision();
        tree.clear();
        QVERIFY(tree.revision() > last);
    }

    void longLineIsReleasedWithoutDeepRecursion()
    {
        // 長い手順の解放が再帰の深さに比例しないこと（スタックを使い切らない）
        auto tree = std::make_unique<KifuBranchTree>();
        tree->setRootSfen(kHirateSfen);
        KifuBranchNode* current = tree->root();
        for (int i = 0; i < 200000; ++i)
            current = TestTree::addMove(*tree, current, ShogiMove(), QStringLiteral("m"), QStringLiteral("s"));
        QCOMPARE(tree->nodeCount(), 200001);
        tree.reset();
    }

    void longLineIsListedWithoutDeepRecursion()
    {
        // 手順の一覧を作るのも手数の深さの再帰にしない（7万手の棋譜を開くと落ちていた。セキュリティ監査 S5）
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuBranchNode* current = tree.root();
        KifuBranchNode* middle = nullptr;
        for (int i = 0; i < 200000; ++i) {
            current = TestTree::addMove(tree, current, ShogiMove(), QStringLiteral("m"), QStringLiteral("s"));
            if (i == 100000) middle = current;
        }
        const QList<BranchLine> lines = tree.allLines();
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines.at(0).leaf, current);
        QCOMPARE(lines.at(0).length(), 200001);
        QCOMPARE(lines.at(0).nodesFrom(199999).size(), 2);
        QCOMPARE(tree.findLineIndexForNode(middle), std::optional<int>(0));
        QCOMPARE(tree.sfenListForLine(0).size(), 200001);
    }

    void linesAreBuiltFromTheirLeaves()
    {
        // 手順はノードの列を持たず、終端から作る。分岐点は経路で最後に「最初の子でない」手の親
        KifuBranchTree tree;
        buildTestTree(tree);
        KifuBranchNode* n4 = TestTree::findByPlyOnMainLine(tree, 4);
        QVERIFY(n4);
        // 変化の中の変化（3手目の変化の2手目＝4手目から、さらに分かれる）
        KifuBranchNode* b1 = tree.allLines().at(1).nodesFrom(3).first();
        KifuBranchNode* nested = TestTree::addMove(tree, b1, ShogiMove(), QStringLiteral("△１四歩"), QStringLiteral("nested"));

        const QList<BranchLine> lines = tree.allLines();
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines.at(0).branchPly, 0);
        QCOMPARE(lines.at(0).length(), 8);
        QCOMPARE(lines.at(1).branchPly, 3);
        QCOMPARE(lines.at(1).nodes().size(), 5);
        QCOMPARE(lines.at(1).nodes().at(2), n4->parent()->parent());   // 分岐の前は本譜の手
        QCOMPARE(lines.at(2).leaf, nested);
        QCOMPARE(lines.at(2).branchPly, 4);
        QCOMPARE(lines.at(2).branchPoint, b1);
        QCOMPARE(lines.at(2).nodesFrom(4), QList<KifuBranchNode*>({nested}));
        QVERIFY(lines.at(2).nodesFrom(5).isEmpty());
        QCOMPARE(tree.findLineIndexForNode(b1), std::optional<int>(1));
        QCOMPARE(tree.findLineIndexForNode(nested), std::optional<int>(2));
        QCOMPARE(tree.findLineIndexForNode(n4), std::optional<int>(0));
        QCOMPARE(tree.exportLine(2).moveCount(), 4);
    }

    void setRootSfen()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        QVERIFY(tree.root() != nullptr);
        QCOMPARE(tree.root()->ply(), 0);
        QCOMPARE(tree.root()->sfen(), kHirateSfen);
    }

    void addMove_basic()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* node = TestTree::addMove(tree, tree.root(), move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"));
        QVERIFY(node != nullptr);
        QCOMPARE(node->ply(), 1);
        QCOMPARE(node->parent(), tree.root());
    }

    void nodeCount()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        // root + 7 mainline + 2 branch = 10
        QCOMPARE(tree.nodeCount(), 10);
    }

    void mainLine()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        auto line = tree.mainLine();
        // root + 7 moves = 8 nodes
        QCOMPARE(line.size(), 8);
        QCOMPARE(line[0]->ply(), 0); // root
        QCOMPARE(line[7]->ply(), 7); // last move
    }

    void allLines()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        auto lines = tree.allLines();
        QCOMPARE(lines.size(), 2); // mainline + 1 branch
    }

    void pathToNode()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        auto lines = tree.allLines();
        QVERIFY(lines.size() >= 2);

        // Path to branch end node
        auto& branchLine = lines[1];
        auto* branchEnd = branchLine.leaf;
        auto path = tree.pathToNode(branchEnd);
        QVERIFY(!path.isEmpty());
        QCOMPARE(path.first()->ply(), 0); // starts at root
    }

    void hasBranch()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        auto mainline = tree.mainLine();
        // n2 (ply 2) should have a branch (two children: n3 and b1)
        QVERIFY(mainline[2]->hasBranch());
        // n1 (ply 1) should not have a branch
        QVERIFY(!mainline[1]->hasBranch());
    }

    void clear()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        QVERIFY(tree.root() != nullptr);

        tree.clear();
        QVERIFY(tree.root() == nullptr);
        QVERIFY(tree.isEmpty());
    }

    void signals_nameTheChangedNodes()
    {
        KifuBranchTree tree;
        QSignalSpy reset(&tree, &KifuBranchTree::reset);
        QSignalSpy appended(&tree, &KifuBranchTree::nodeAppended);

        tree.setRootSfen(kHirateSfen);
        QCOMPARE(reset.count(), 1);   // 空にしてから作り直すが、知らせは1回
        auto* node = TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("m1"), QStringLiteral("s1"));
        QCOMPARE(appended.count(), 1);
        QCOMPARE(appended.at(0).at(0).toInt(), tree.root()->nodeId());
        QCOMPARE(appended.at(0).at(1).toInt(), node->nodeId());
        // 終局の行の後には足せない
        auto* resign = TestTree::addTerminalMove(tree, node, TerminalType::Resign, Turn::White);
        QVERIFY(TestTree::addMove(tree, resign, ShogiMove(), QStringLiteral("x"), QStringLiteral("s")) == nullptr);
        QCOMPARE(appended.count(), 2);

        tree.clear();
        QCOMPARE(reset.count(), 2);
    }

    void addMove_terminalInheritsParentSfenAndHasNoMove()
    {
        // 終局手ノードは追加経路（KIF: 空 or 親の複製、ライブ: 盤面から生成）によらず
        // 親と同じ局面SFENを持ち、無関係な ShogiMove を保持しない
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        const ShogiMove realMove(UsiMove::fromString(u"3g3f"), Piece::BlackPawn);
        auto* n1 = TestTree::addMove(tree, tree.root(), realMove, QStringLiteral("▲２六歩(27)"), QStringLiteral("board1 w - 2"));

        // KIF 本譜由来: SFEN が空
        auto* t1 = TestTree::addTerminalMove(tree, n1, TerminalType::Resign, Turn::White);
        QVERIFY(t1->isTerminal());
        QCOMPARE(t1->sfen(), n1->sfen());
        QCOMPARE(t1->move().movingPiece, Piece::None);
        QVERIFY(!t1->isActualMove());
        QCOMPARE(t1->displayText(), QStringLiteral("△投了"));

        // 局面・指し手を持った終局の行を渡されても、親の局面になり指し手は持たない
        RecordEntry interrupted = RecordEntry::ending(TerminalType::Interrupt, Turn::White);
        interrupted.move = realMove;
        interrupted.sfen = QStringLiteral("board1 w - 3");
        auto* t2 = tree.addRecord(n1, interrupted);
        QVERIFY(t2->isTerminal());
        QCOMPARE(t2->sfen(), n1->sfen());
        QCOMPARE(t2->move().movingPiece, Piece::None);

        // 通常の手は従来どおり渡された値を保持する
        auto* n2 = TestTree::addMove(tree, n1, realMove, QStringLiteral("△３四歩(33)"), QStringLiteral("board2 b - 3"));
        QCOMPARE(n2->sfen(), QStringLiteral("board2 b - 3"));
        QVERIFY(n2->move() == realMove);
    }

    void batchUpdate_replaysChangesInOrder()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* n1 = TestTree::addMove(tree, tree.root(), move, QStringLiteral("m1"), QStringLiteral("s1"));
        QSignalSpy appended(&tree, &KifuBranchTree::nodeAppended);
        QSignalSpy reordered(&tree, &KifuBranchTree::childrenReordered);
        QSignalSpy reset(&tree, &KifuBranchTree::reset);

        // 入れ子でも、最も外側の end で古い順に知らせる
        tree.beginBatchUpdate();
        tree.beginBatchUpdate();
        auto* a = TestTree::addMove(tree, n1, move, QStringLiteral("a"), QStringLiteral("sa"));
        auto* b = TestTree::addMove(tree, n1, move, QStringLiteral("b"), QStringLiteral("sb"));
        tree.endBatchUpdate();
        QVERIFY(tree.moveChild(b, 0));
        QCOMPARE(appended.count(), 0);      // 一括更新中は知らせない
        QCOMPARE(reordered.count(), 0);
        tree.endBatchUpdate();
        QCOMPARE(appended.count(), 2);
        QCOMPARE(appended.at(0).at(1).toInt(), a->nodeId());
        QCOMPARE(appended.at(1).at(1).toInt(), b->nodeId());
        QCOMPARE(reordered.count(), 1);
        QCOMPARE(reset.count(), 0);

        // 変更が無ければ知らせない
        tree.beginBatchUpdate();
        tree.endBatchUpdate();
        QCOMPARE(appended.count(), 2);

        // 一括更新の外では毎回知らせる。対応しない end は無視する
        tree.endBatchUpdate();
        TestTree::addMove(tree, a, move, QStringLiteral("a2"), QStringLiteral("sa2"));
        QCOMPARE(appended.count(), 3);
    }

    void batchUpdate_collapsesIntoResetWhenRebuilt()
    {
        // 棋譜の読み込み: 作り直しを含む一括更新は reset を1回だけ知らせる
        KifuBranchTree tree;
        QSignalSpy appended(&tree, &KifuBranchTree::nodeAppended);
        QSignalSpy reset(&tree, &KifuBranchTree::reset);
        ShogiMove move;

        tree.beginBatchUpdate();
        tree.setRootSfen(kHirateSfen);
        auto* n1 = TestTree::addMove(tree, tree.root(), move, QStringLiteral("m1"), QStringLiteral("s1"));
        auto* n2 = TestTree::addMove(tree, n1, move, QStringLiteral("m2"), QStringLiteral("s2"));
        TestTree::addTerminalMove(tree, n2, TerminalType::Resign, Turn::Black);
        QCOMPARE(reset.count(), 0);
        tree.endBatchUpdate();
        QCOMPARE(reset.count(), 1);
        QCOMPARE(appended.count(), 0);
    }

    void stressTest_200Moves()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* current = tree.root();
        for (int i = 0; i < 200; ++i) {
            current = TestTree::addMove(tree, current, move,
                                   QStringLiteral("move%1").arg(i),
                                   QStringLiteral("sfen%1").arg(i));
            QVERIFY(current != nullptr);
        }
        QCOMPARE(tree.nodeCount(), 201); // root + 200
        QCOMPARE(tree.mainLine().size(), 201);
    }

    void lineCount()
    {
        KifuBranchTree tree;
        buildTestTree(tree);
        QCOMPARE(tree.lineCount(), 2);
    }

    void findByPlyOnMainLine()
    {
        KifuBranchTree tree;
        buildTestTree(tree);

        auto* node = TestTree::findByPlyOnMainLine(tree, 3);
        QVERIFY(node != nullptr);
        QCOMPARE(node->ply(), 3);
    }

    void findBySfen_skipsTerminalNode()
    {
        // 終局手ノードは親と同じ局面を持つが、findBySfen は親（局面を表すノード）を返す
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("board1 w - 2"));
        auto* n2 = TestTree::addMove(tree, n1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("board2 b - 3"));
        auto* resign = TestTree::addTerminalMove(tree, n2, TerminalType::Resign, Turn::Black);
        QVERIFY(resign->isTerminal());

        for (int i = 0; i < 20; ++i) {
            QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("board2 b - 3")), n2);
            QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("board2 b - 4")), n2);
        }
        // 終局手しか持たない局面は見つからない（親が持つ局面として扱う）
        QVERIFY(TestTree::findBySfen(tree, QStringLiteral("board9 b - 1")) == nullptr);
    }

    void findBySfen_prefersMainLineOnTransposition()
    {
        // 同じ局面が本譜と分岐の両方にある（手順前後）場合は本譜側を返す
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("boardA w - 2"));
        auto* n2 = TestTree::addMove(tree, n1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("boardX b - 3"));
        auto* b1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("▲２六歩"), QStringLiteral("boardB w - 2"));
        auto* b2 = TestTree::addMove(tree, b1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("boardX b - 3"));
        QVERIFY(b2 != nullptr);

        QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("boardX b - 3")), n2);
        QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("boardB w - 2")), b1);
    }

    void findBySfen_withPlyFilter()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("m1"), QStringLiteral("same b - 2"));
        auto* n2 = TestTree::addMove(tree, n1, dummyMove, QStringLiteral("m2"), QStringLiteral("other w - 3"));
        auto* n3 = TestTree::addMove(tree, n2, dummyMove, QStringLiteral("m3"), QStringLiteral("same b - 4"));

        QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("same b - 4")), n1);      // 手数指定なしは最初の一致
        QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("same b - 4"), 3), n3);
        QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("same b - 4"), 1), n1);
        QVERIFY(TestTree::findBySfen(tree, QStringLiteral("same b - 4"), 2) == nullptr);
        QCOMPARE(TestTree::findBySfen(tree, kHirateSfen, 0), tree.root());
    }

    void findBySfen_ignoresHandOrderAndMoveNumber()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("m1"), QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w RPb 2"));

        QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w RbP 99")), n1);
        QCOMPARE(TestTree::findBySfen(tree, QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w RPb")), n1);
        QVERIFY(TestTree::findBySfen(tree, QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w")) == nullptr);   // 不完全な SFEN
        QVERIFY(TestTree::findBySfen(tree, QString()) == nullptr);
    }

    void findMatchingChild_bySfenIgnoringMoveNumber()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("▲７六歩(77)"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));

        // 手数部分が異なっても同じ局面なら一致する
        auto* found = tree.findMatchingChild(tree.root(), RecordEntry::played(dummyMove, QStringLiteral("▲７六歩(77)"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 5")));
        QCOMPARE(found, n1);

        // 局面が異なれば一致しない
        QVERIFY(tree.findMatchingChild(tree.root(), RecordEntry::played(dummyMove, QStringLiteral("▲２六歩(27)"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/7P1/PPPPPPP1P/1B5R1/LNSGKGSNL w - 2"))) == nullptr);

        // 空の SFEN 同士は一致扱いにしない
        auto* n2 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("▲２六歩(27)"), QString());
        QVERIFY(n2 != nullptr);
        QVERIFY(tree.findMatchingChild(tree.root(), RecordEntry::played(dummyMove, QStringLiteral("x"), QString())) == nullptr);
    }

    void findMatchingChild_ignoresHandOrder()
    {
        // 以前のこのアプリは持ち駒を駒種ごとに先後交互に書いた。標準は先手分→後手分の順
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("m1"),
                                QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w RPb 2"));
        QCOMPARE(tree.findMatchingChild(tree.root(), RecordEntry::played(dummyMove, QStringLiteral("m1"),
                                        QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w RbP 2"))), n1);
        QVERIFY(tree.findMatchingChild(tree.root(), RecordEntry::played(dummyMove, QStringLiteral("m1"),
                                       QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w R2Pb 2"))) == nullptr);
        QVERIFY(tree.findMatchingChild(tree.root(), RecordEntry::played(dummyMove, QStringLiteral("m1"),
                                       QStringLiteral("board b RPb 2"))) == nullptr);
    }

    void findMatchingChild_byMoveWhenSfenMissing()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        const ShogiMove pawnMove(UsiMove::fromString(u"3g3f"), Piece::BlackPawn);
        const ShogiMove otherMove(UsiMove::fromString(u"7g7f"), Piece::BlackPawn);
        auto* n1 = TestTree::addMove(tree, tree.root(), pawnMove, QStringLiteral("▲２六歩(27)"), QString());

        QCOMPARE(tree.findMatchingChild(tree.root(), RecordEntry::played(pawnMove, QStringLiteral("▲２六歩(27)"), QString())), n1);
        QVERIFY(tree.findMatchingChild(tree.root(), RecordEntry::played(otherMove, QStringLiteral("▲７六歩(77)"), QString())) == nullptr);
    }

    void findMatchingChild_terminalByType()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = TestTree::addMove(tree, tree.root(), dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("board w - 2"));
        auto* resign = TestTree::addTerminalMove(tree, n1, TerminalType::Resign, Turn::White);
        QVERIFY(resign->isTerminal());

        // 同じ種類・同じ側の終局手は一致する
        QCOMPARE(tree.findMatchingChild(n1, RecordEntry::ending(TerminalType::Resign, Turn::White)), resign);
        // 種類か側が違う終局手は一致しない
        QVERIFY(tree.findMatchingChild(n1, RecordEntry::ending(TerminalType::Interrupt, Turn::White)) == nullptr);
        QVERIFY(tree.findMatchingChild(n1, RecordEntry::ending(TerminalType::Resign, Turn::Black)) == nullptr);
        // 通常の指し手は、局面が同じでも終局手ノードとは一致しない
        QVERIFY(tree.findMatchingChild(n1, RecordEntry::played(dummyMove, QStringLiteral("△３四歩"), QStringLiteral("board w - 3"))) == nullptr);
        // 親が nullptr なら nullptr
        QVERIFY(tree.findMatchingChild(nullptr, RecordEntry::played(dummyMove, QStringLiteral("△投了"), QStringLiteral("board w - 3"))) == nullptr);
    }
};

QTEST_MAIN(TestKifuBranchTree)
#include "tst_kifubranchtree.moc"
