/// 対局を始める前の棋譜の整理（SessionResetCoordinator::prepareGameStart）を、本体の部品で確かめる。
/// 新しい対局は開始局面だけの棋譜に、現在局面からの対局は選んだ手までの手順を残して始める。
#include <QtTest>
#include <QTemporaryDir>

#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "sessionresetcoordinator.h"
#include "sfenutils.h"
#include "test_replay.h"

class TestPresetGameStartCleanup : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;

    /// 本譜 ▲７六歩 △３四歩 ▲２六歩 を作り、最終手を表示する
    static void buildRecord(MainWindowComponents& p)
    {
        const QStringList usi{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")};
        const auto moves = TestReplay::buildGameMoves(SfenUtils::hirateSfen(), usi);
        const auto sfens = TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), usi, false);
        p.branchTree->setRootSfen(SfenUtils::hirateSfen());
        p.liveGameSession->startFromRoot();
        for (int i = 0; i < 3; ++i)
            p.liveGameSession->appendRecord(RecordEntry::played(moves.at(i), usi.at(i), sfens.at(i + 1)));
        p.liveGameSession->commit();
        p.navState->setCurrentNode(p.branchTree->mainLine().last());
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void freshGameClearsPreviousTree_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("hirate") << SfenUtils::hirateSfen();
        QTest::newRow("handicap") << QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    }

    void freshGameClearsPreviousTree()
    {
        QFETCH(QString, sfen);
        MainWindow window;
        auto& p = window.composer().parts();
        buildRecord(p);
        KifuBranchNode* root = p.sessionReset->prepareGameStart({GameStartPreparation::Mode::NewGame, sfen, -1});
        QCOMPARE(root, p.branchTree->root());
        QCOMPARE(p.branchTree->nodeCount(), 1);
        QCOMPARE(root->sfen(), sfen);
        QCOMPARE(p.kifuRecord->rowCount(), 1);
        QCOMPARE(p.navState->currentNode(), root);
        QVERIFY(p.liveGameSession->isActive());
        p.liveGameSession->discard();
    }

    void currentPositionPreservesSelectedPath()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        buildRecord(p);
        KifuBranchNode* selected = p.branchTree->mainLine().at(2);
        KifuBranchNode* start = p.sessionReset->prepareGameStart({GameStartPreparation::Mode::CurrentPosition,
                                                                  SfenUtils::hirateSfen(), selected->nodeId()});
        QCOMPARE(start, selected);
        QCOMPARE(p.branchTree->nodeCount(), 4);
        // 対局中の棋譜欄は開始した手まで（その先の以前の手順は出さない）
        QCOMPARE(p.kifuRecord->rowCount(), 3);
        QCOMPARE(p.navState->currentNode(), selected);
        QCOMPARE(p.kifuRecord->currentHighlightRow(), 2);
        QVERIFY(p.liveGameSession->isActive());
        p.liveGameSession->discard();
    }

    void invalidNodeDoesNotFallBackToMatchingPosition()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        buildRecord(p);
        const int original = p.branchTree->nodeCount();
        KifuBranchNode* selected = p.navState->currentNode();
        QVERIFY(!p.sessionReset->prepareGameStart({GameStartPreparation::Mode::CurrentPosition,
                                                   SfenUtils::hirateSfen(), 123456}));
        QCOMPARE(p.branchTree->nodeCount(), original);
        QCOMPARE(p.navState->currentNode(), selected);
        QVERIFY(!p.liveGameSession->isActive());
    }

    void interruptContinuesFromParent()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        buildRecord(p);
        KifuBranchNode* parent = p.navState->currentNode();
        p.liveGameSession->startFromNode(parent);
        p.liveGameSession->appendRecord(RecordEntry::ending(TerminalType::Interrupt, Turn::White));
        KifuBranchNode* terminal = p.liveGameSession->liveNode();
        const int terminalId = terminal->nodeId();
        p.liveGameSession->commit();
        QCOMPARE(p.navState->currentNode(), terminal);
        QCOMPARE(p.sessionReset->prepareGameStart({GameStartPreparation::Mode::CurrentPosition,
                                                   SfenUtils::hirateSfen(), terminalId}), parent);
        QVERIFY(!p.branchTree->nodeAt(terminalId));
        QCOMPARE(p.navState->currentNode(), parent);
        QCOMPARE(p.kifuRecord->rowCount(), 4);
        p.liveGameSession->discard();
    }
};
QTEST_MAIN(TestPresetGameStartCleanup)
#include "tst_preset_gamestart_cleanup.moc"
