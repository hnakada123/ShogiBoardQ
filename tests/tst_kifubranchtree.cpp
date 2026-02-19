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
};

QTEST_MAIN(TestKifuBranchTree)
#include "tst_kifubranchtree.moc"
