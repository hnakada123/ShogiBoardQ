#include <QtTest>
#include <QSignalSpy>

#include "livegamesession.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestLiveGameSession : public QObject
{
    Q_OBJECT

private slots:
    void startFromRoot()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        QVERIFY(session.isActive());
        QCOMPARE(session.anchorPly(), 0);
    }

    void startFromNode()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("move1"), QStringLiteral("sfen1"));
        auto* n2 = tree.addMove(n1, move, QStringLiteral("move2"), QStringLiteral("sfen2"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n2);

        QVERIFY(session.isActive());
        QCOMPARE(session.anchorPly(), 2);
    }

    void addMove_incrementsMoveCount()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        QCOMPARE(session.moveCount(), 0);

        ShogiMove move;
        session.addMove(move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"), QStringLiteral("0:10"));
        QCOMPARE(session.moveCount(), 1);

        session.addMove(move, QStringLiteral("△３四歩"), QStringLiteral("sfen2"), QStringLiteral("0:05"));
        QCOMPARE(session.moveCount(), 2);
    }

    void commit_addsToTree()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        int nodesBefore = tree.nodeCount();

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        ShogiMove move;
        session.addMove(move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"), QString());
        session.addMove(move, QStringLiteral("△３四歩"), QStringLiteral("sfen2"), QString());

        auto* result = session.commit();
        QVERIFY(result != nullptr);
        QVERIFY(!session.isActive());

        // Tree should have more nodes now
        QVERIFY(tree.nodeCount() > nodesBefore);
    }

    void discard_resetsSession()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();
        QVERIFY(session.isActive());

        ShogiMove move;
        session.addMove(move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"), QString());
        QCOMPARE(session.moveCount(), 1);

        // discard resets session state (nodes already added stay in tree)
        session.discard();
        QVERIFY(!session.isActive());
        QCOMPARE(session.moveCount(), 0);
    }

    void stressTest_repeatedSessions()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);

        for (int cycle = 0; cycle < 50; ++cycle) {
            session.startFromRoot();
            QVERIFY(session.isActive());

            ShogiMove move;
            for (int i = 0; i < 10; ++i) {
                session.addMove(move,
                               QStringLiteral("move%1_%2").arg(cycle).arg(i),
                               QStringLiteral("sfen%1_%2").arg(cycle).arg(i),
                               QString());
            }
            QCOMPARE(session.moveCount(), 10);

            session.commit();
            QVERIFY(!session.isActive());
        }

        // Tree should have all committed moves
        QVERIFY(tree.lineCount() >= 1);
    }

    void totalPly()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("m1"), QStringLiteral("s1"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n1);

        session.addMove(move, QStringLiteral("m2"), QStringLiteral("s2"), QString());
        session.addMove(move, QStringLiteral("m3"), QStringLiteral("s3"), QString());

        QCOMPARE(session.totalPly(), 3); // anchorPly(1) + moveCount(2)
    }
};

QTEST_MAIN(TestLiveGameSession)
#include "tst_livegamesession.moc"
