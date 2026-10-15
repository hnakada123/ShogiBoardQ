/// 明示的な開始要求で棋譜を整理し、クリーンアップだけでは対局を開始しない。
#include <QtTest>
#include "prestartcleanuphandler.h"
#include "kifurecordlistmodel.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "sfenpositiontracer.h"

struct CleanupHarness {
    KifuBranchTree tree;
    KifuNavigationState nav;
    KifuRecordListModel rows;
    LiveGameSession live;
    PreStartCleanupHandler cleanup;
    CleanupHarness() : cleanup(deps())
    {
        tree.setRootSfen(SfenUtils::hirateSfen());
        nav.setTree(&tree);
        live.setTree(&tree);
        const QStringList usi{"7g7f", "3c3d", "2g2f"};
        const auto moves = SfenPositionTracer::buildGameMoves(SfenUtils::hirateSfen(), usi);
        const auto sfens = SfenPositionTracer::buildSfenRecord(SfenUtils::hirateSfen(), usi, false);
        live.startFromRoot();
        for (int i = 0; i < 3; ++i) live.addMove(moves.at(i), usi.at(i), sfens.at(i + 1), {});
        live.commit();
        nav.setCurrentNode(tree.mainLine().last());
    }
    PreStartCleanupHandler::Dependencies deps()
    {
        PreStartCleanupHandler::Dependencies d;
        d.branchTree = &tree;
        d.navState = &nav;
        d.kifuRecordModel = &rows;
        d.liveGameSession = &live;
        return d;
    }
};
class TestPresetGameStartCleanup : public QObject {
    Q_OBJECT
private slots:
    void freshGameClearsPreviousTree_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("hirate") << SfenUtils::hirateSfen();
        QTest::newRow("handicap") << QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    }
    void freshGameClearsPreviousTree()
    {
        QFETCH(QString, sfen);
        CleanupHarness h;
        auto* root = h.cleanup.performCleanup({GameStartPreparation::Mode::NewGame, sfen, -1});
        QCOMPARE(root, h.tree.root());
        QCOMPARE(h.tree.nodeCount(), 1);
        QCOMPARE(root->sfen(), sfen);
        QCOMPARE(h.rows.rowCount(), 1);
        QCOMPARE(h.nav.currentPly(), 0);
        QVERIFY(!h.live.isActive());
    }
    void currentPositionPreservesSelectedPath()
    {
        CleanupHarness h;
        auto* selected = h.tree.mainLine().at(2);
        auto* start = h.cleanup.performCleanup({GameStartPreparation::Mode::CurrentPosition,
                                                SfenUtils::hirateSfen(), selected->nodeId()});
        QCOMPARE(start, selected);
        QCOMPARE(h.tree.nodeCount(), 4);
        QCOMPARE(h.rows.rowCount(), 3);
        QCOMPARE(h.nav.currentNode(), selected);
        QCOMPARE(h.rows.currentHighlightRow(), 2);
        QVERIFY(!h.live.isActive());
    }
    void invalidNodeDoesNotFallBackToMatchingPosition()
    {
        CleanupHarness h;
        const int original = h.tree.nodeCount();
        auto* selected = h.nav.currentNode();
        QVERIFY(!h.cleanup.performCleanup({GameStartPreparation::Mode::CurrentPosition,
                                           SfenUtils::hirateSfen(), 123456}));
        QCOMPARE(h.tree.nodeCount(), original);
        QCOMPARE(h.nav.currentNode(), selected);
    }
    void interruptContinuesFromParent()
    {
        CleanupHarness h;
        auto* parent = h.nav.currentNode();
        h.live.startFromNode(parent);
        h.live.appendRecord(RecordEntry::ending(TerminalType::Interrupt, Turn::White));
        auto* terminal = h.live.liveNode();
        const int terminalId = terminal->nodeId();
        h.live.commit();
        h.nav.setCurrentNode(terminal);
        QCOMPARE(h.cleanup.performCleanup({GameStartPreparation::Mode::CurrentPosition,
                                           SfenUtils::hirateSfen(), terminalId}), parent);
        QVERIFY(!h.tree.nodeAt(terminalId));
        QCOMPARE(h.nav.currentNode(), parent);
        QCOMPARE(h.rows.rowCount(), 4);
    }
};
QTEST_MAIN(TestPresetGameStartCleanup)
#include "tst_preset_gamestart_cleanup.moc"
