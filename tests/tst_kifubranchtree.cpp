#include <QtTest>
#include <QSignalSpy>

#include "kifubranchtree.h"
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
        auto* n1 = tree.addMove(root, dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"));
        auto* n2 = tree.addMove(n1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("sfen2"));
        auto* n3 = tree.addMove(n2, dummyMove, QStringLiteral("▲２六歩"), QStringLiteral("sfen3"));
        auto* n4 = tree.addMove(n3, dummyMove, QStringLiteral("△８四歩"), QStringLiteral("sfen4"));
        auto* n5 = tree.addMove(n4, dummyMove, QStringLiteral("▲２五歩"), QStringLiteral("sfen5"));
        auto* n6 = tree.addMove(n5, dummyMove, QStringLiteral("△８五歩"), QStringLiteral("sfen6"));
        tree.addMove(n6, dummyMove, QStringLiteral("▲７八金"), QStringLiteral("sfen7"));

        // Branch at ply 3 (from n2, alternative to n3)
        auto* b1 = tree.addMove(n2, dummyMove, QStringLiteral("▲６六歩"), QStringLiteral("branch_sfen1"));
        tree.addMove(b1, dummyMove, QStringLiteral("△８四歩"), QStringLiteral("branch_sfen2"));
    }

private slots:
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
        auto* node = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"));
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
        auto* branchEnd = branchLine.nodes.last();
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
        QVERIFY(tree.hasBranch(mainline[2]));
        // n1 (ply 1) should not have a branch
        QVERIFY(!tree.hasBranch(mainline[1]));
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

    void signal_treeChanged()
    {
        KifuBranchTree tree;
        QSignalSpy spy(&tree, &KifuBranchTree::treeChanged);
        QVERIFY(spy.isValid());

        tree.setRootSfen(kHirateSfen);
        QVERIFY(spy.count() >= 1);
    }

    void batchUpdate_emitsTreeChangedOnce()
    {
        KifuBranchTree tree;
        QSignalSpy spy(&tree, &KifuBranchTree::treeChanged);
        ShogiMove move;

        tree.beginBatchUpdate();
        tree.setRootSfen(kHirateSfen);
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("m1"), QStringLiteral("s1"));
        auto* n2 = tree.addMove(n1, move, QStringLiteral("m2"), QStringLiteral("s2"));
        tree.addTerminalMove(n2, TerminalType::Resign, QStringLiteral("▲投了"));
        QCOMPARE(spy.count(), 0);           // 一括更新中は発火しない
        tree.endBatchUpdate();
        QCOMPARE(spy.count(), 1);           // 終了時に1回だけ

        // 変更が無ければ発火しない
        tree.beginBatchUpdate();
        tree.endBatchUpdate();
        QCOMPARE(spy.count(), 1);

        // 入れ子: 最も外側の end で1回だけ
        tree.beginBatchUpdate();
        tree.beginBatchUpdate();
        tree.addMove(n1, move, QStringLiteral("m2b"), QStringLiteral("s2b"));
        tree.endBatchUpdate();
        QCOMPARE(spy.count(), 1);
        tree.addMove(n2, move, QStringLiteral("m3"), QStringLiteral("s3"));
        tree.endBatchUpdate();
        QCOMPARE(spy.count(), 2);

        // 一括更新の外では従来どおり毎回発火
        tree.addMove(n2, move, QStringLiteral("m3b"), QStringLiteral("s3b"));
        QCOMPARE(spy.count(), 3);

        // 対応しない end は無視され、その後も通常動作
        tree.endBatchUpdate();
        tree.addMove(n1, move, QStringLiteral("m2c"), QStringLiteral("s2c"));
        QCOMPARE(spy.count(), 4);
    }

    void stressTest_200Moves()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* current = tree.root();
        for (int i = 0; i < 200; ++i) {
            current = tree.addMove(current, move,
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

        auto* node = tree.findByPlyOnMainLine(3);
        QVERIFY(node != nullptr);
        QCOMPARE(node->ply(), 3);
    }

    void findBySfen_skipsTerminalNode()
    {
        // 終局手ノードは親と同じ局面を持つが、findBySfen は親（局面を表すノード）を返す
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("board1 w - 2"));
        auto* n2 = tree.addMove(n1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("board2 b - 3"));
        auto* resign = tree.addMove(n2, dummyMove, QStringLiteral("▲投了"), QStringLiteral("board2 b - 4"));
        QVERIFY(resign->isTerminal());

        for (int i = 0; i < 20; ++i) {
            QCOMPARE(tree.findBySfen(QStringLiteral("board2 b - 3")), n2);
            QCOMPARE(tree.findBySfen(QStringLiteral("board2 b - 4")), n2);
        }
        // 終局手しか持たない局面は見つからない（親が持つ局面として扱う）
        QVERIFY(tree.findBySfen(QStringLiteral("board9 b - 1")) == nullptr);
    }

    void findBySfen_prefersMainLineOnTransposition()
    {
        // 同じ局面が本譜と分岐の両方にある（手順前後）場合は本譜側を返す
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("boardA w - 2"));
        auto* n2 = tree.addMove(n1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("boardX b - 3"));
        auto* b1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("▲２六歩"), QStringLiteral("boardB w - 2"));
        auto* b2 = tree.addMove(b1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("boardX b - 3"));
        QVERIFY(b2 != nullptr);

        QCOMPARE(tree.findBySfen(QStringLiteral("boardX b - 3")), n2);
        QCOMPARE(tree.findBySfen(QStringLiteral("boardB w - 2")), b1);
    }

    void findBySfen_withPlyFilter()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("m1"), QStringLiteral("same b - 2"));
        auto* n2 = tree.addMove(n1, dummyMove, QStringLiteral("m2"), QStringLiteral("other w - 3"));
        auto* n3 = tree.addMove(n2, dummyMove, QStringLiteral("m3"), QStringLiteral("same b - 4"));

        QCOMPARE(tree.findBySfen(QStringLiteral("same b - 4")), n1);      // 手数指定なしは最初の一致
        QCOMPARE(tree.findBySfen(QStringLiteral("same b - 4"), 3), n3);
        QCOMPARE(tree.findBySfen(QStringLiteral("same b - 4"), 1), n1);
        QVERIFY(tree.findBySfen(QStringLiteral("same b - 4"), 2) == nullptr);
        QCOMPARE(tree.findBySfen(kHirateSfen, 0), tree.root());
    }

    void findBySfen_ignoresHandOrderAndMoveNumber()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("m1"), QStringLiteral("board w RPb 2"));

        QCOMPARE(tree.findBySfen(QStringLiteral("board w RbP 99")), n1);
        QCOMPARE(tree.findBySfen(QStringLiteral("board w RPb")), n1);
        QVERIFY(tree.findBySfen(QStringLiteral("board w")) == nullptr);   // 不完全な SFEN
        QVERIFY(tree.findBySfen(QString()) == nullptr);
    }

    void findMatchingChild_bySfenIgnoringMoveNumber()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("▲７六歩(77)"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));

        // 手数部分が異なっても同じ局面なら一致する
        auto* found = tree.findMatchingChild(tree.root(), dummyMove, QStringLiteral("▲７六歩(77)"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 5"));
        QCOMPARE(found, n1);

        // 局面が異なれば一致しない
        QVERIFY(tree.findMatchingChild(tree.root(), dummyMove, QStringLiteral("▲２六歩(27)"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/7P1/PPPPPPP1P/1B5R1/LNSGKGSNL w - 2")) == nullptr);

        // 空の SFEN 同士は一致扱いにしない
        auto* n2 = tree.addMove(tree.root(), dummyMove, QStringLiteral("▲２六歩(27)"), QString());
        QVERIFY(n2 != nullptr);
        QVERIFY(tree.findMatchingChild(tree.root(), dummyMove, QStringLiteral("x"), QString()) == nullptr);
    }

    void findMatchingChild_ignoresHandOrder()
    {
        // ShogiBoard は持ち駒を駒種ごとに先後交互、SfenPositionTracer は先手分→後手分の順で出力する
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("m1"),
                                QStringLiteral("board w RPb 2"));
        QCOMPARE(tree.findMatchingChild(tree.root(), dummyMove, QStringLiteral("m1"),
                                        QStringLiteral("board w RbP 2")), n1);
        QVERIFY(tree.findMatchingChild(tree.root(), dummyMove, QStringLiteral("m1"),
                                       QStringLiteral("board w R2Pb 2")) == nullptr);
        QVERIFY(tree.findMatchingChild(tree.root(), dummyMove, QStringLiteral("m1"),
                                       QStringLiteral("board b RPb 2")) == nullptr);
    }

    void findMatchingChild_byMoveWhenSfenMissing()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        const ShogiMove pawnMove(QPoint(2, 6), QPoint(2, 5), Piece::BlackPawn, Piece::None, false);
        const ShogiMove otherMove(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        auto* n1 = tree.addMove(tree.root(), pawnMove, QStringLiteral("▲２六歩(27)"), QString());

        QCOMPARE(tree.findMatchingChild(tree.root(), pawnMove, QStringLiteral("▲２六歩(27)"), QString()), n1);
        QVERIFY(tree.findMatchingChild(tree.root(), otherMove, QStringLiteral("▲７六歩(77)"), QString()) == nullptr);
    }

    void findMatchingChild_terminalByType()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;
        auto* n1 = tree.addMove(tree.root(), dummyMove, QStringLiteral("▲７六歩"), QStringLiteral("board w - 2"));
        auto* resign = tree.addMove(n1, dummyMove, QStringLiteral("△投了"), QStringLiteral("board w - 3"));
        QVERIFY(resign->isTerminal());

        // 同じ種類の終局手は一致する
        QCOMPARE(tree.findMatchingChild(n1, dummyMove, QStringLiteral("△投了"), QStringLiteral("board w - 3")), resign);
        // 種類が違う終局手は一致しない
        QVERIFY(tree.findMatchingChild(n1, dummyMove, QStringLiteral("△中断"), QStringLiteral("board w - 3")) == nullptr);
        // 通常の指し手は、局面が同じでも終局手ノードとは一致しない
        QVERIFY(tree.findMatchingChild(n1, dummyMove, QStringLiteral("△３四歩"), QStringLiteral("board w - 3")) == nullptr);
        // 親が nullptr なら nullptr
        QVERIFY(tree.findMatchingChild(nullptr, dummyMove, QStringLiteral("△投了"), QStringLiteral("board w - 3")) == nullptr);
    }
};

QTEST_MAIN(TestKifuBranchTree)
#include "tst_kifubranchtree.moc"
