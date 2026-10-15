/// @file tst_app_branch_navigation.cpp
/// @brief BranchNavigationWiring の配線を実物の部品で確かめるテスト
///
/// 分岐ツリー・表示中の手順・ナビゲーション・表示コーディネータ・ライブセッションは、
/// アプリではレジストリが作って渡す。ここでも同じように組み立てて BranchNavigationWiring に渡し、
/// ボタン操作・分岐ツリーのクリック・新規対局のリセットで起きることを確かめる。

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "branchnavigationwiring.h"
#include "gamerecordmodel.h"
#include "kifubranchlistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudisplaycoordinator.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "recordpane.h"
#include "prestartcleanuphandler.h"
#include "shogimove.h"

namespace {

const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
const QString kNoLanceSfen =
    QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");

/// アプリのレジストリと同じ組み立てをした分岐ナビゲーションの部品一式
struct BranchNavigationParts {
    KifuBranchTree tree;
    KifuNavigationState navState;
    KifuNavigationController navController;
    LiveGameSession liveSession;
    KifuDisplayCoordinator display{&tree, &navState, &navController};
    KifuRecordListModel recordModel;
    KifuBranchListModel branchModel;
    RecordPane recordPane;
    GameRecordModel gameRecord;
    QString startSfen = kHirateSfen;

    BranchNavigationParts()
    {
        // 本譜5手と、2手目から分かれる変化2手
        tree.setRootSfen(kHirateSfen);
        KifuBranchNode* node = tree.root();
        for (int ply = 1; ply <= 5; ++ply) {
            node = tree.addMove(node, ShogiMove(), QStringLiteral("move%1").arg(ply),
                                QStringLiteral("sfen%1").arg(ply));
        }
        KifuBranchNode* branch = tree.addMove(tree.findByPlyOnMainLine(2), ShogiMove(),
                                              QStringLiteral("branch3"), QStringLiteral("bsfen3"));
        tree.addMove(branch, ShogiMove(), QStringLiteral("branch4"), QStringLiteral("bsfen4"));

        navState.setTree(&tree);
        navController.setTreeAndState(&tree, &navState);
        liveSession.setTree(&tree);
        recordPane.setModels(&recordModel, &branchModel);
    }

    BranchNavigationWiring::Deps deps()
    {
        BranchNavigationWiring::Deps d;
        d.branchTree = &tree;
        d.navState = &navState;
        d.kifuNavController = &navController;
        d.displayCoordinator = &display;
        d.liveGameSession = &liveSession;
        d.recordPane = &recordPane;
        d.kifuRecordModel = &recordModel;
        d.kifuBranchModel = &branchModel;
        d.gameRecordModel = &gameRecord;
        d.startSfenStr = &startSfen;
        return d;
    }
};

} // namespace

class TestAppBranchNavigation : public QObject
{
    Q_OBJECT

private slots:
    void navigationButtonsMoveTheCursorAndForwardSignals()
    {
        BranchNavigationParts parts;
        BranchNavigationWiring wiring;
        wiring.updateDeps(parts.deps());
        wiring.initialize();

        QSignalSpy board(&wiring, &BranchNavigationWiring::boardWithHighlightsRequired);
        QSignalSpy handled(&wiring, &BranchNavigationWiring::branchNodeHandled);
        QSignalSpy plyChanged(&wiring, &BranchNavigationWiring::currentPlyChanged);

        parts.recordPane.nextButton()->click();
        QCOMPARE(parts.navState.currentPly(), 1);
        QCOMPARE(board.count(), 1);
        QCOMPARE(board.last().at(0).toString(), QStringLiteral("sfen1"));
        QCOMPARE(board.last().at(1).toString(), kHirateSfen);
        QCOMPARE(handled.count(), 1);
        QCOMPARE(handled.last().at(0).toInt(), 1);
        QCOMPARE(handled.last().at(1).toString(), QStringLiteral("sfen1"));
        QVERIFY(!plyChanged.isEmpty());
        QCOMPARE(plyChanged.last().at(0).toInt(), 1);

        parts.recordPane.lastButton()->click();
        QCOMPARE(parts.navState.currentPly(), 5);
        parts.recordPane.prevButton()->click();
        QCOMPARE(parts.navState.currentPly(), 4);
        parts.recordPane.firstButton()->click();
        QCOMPARE(parts.navState.currentNode(), parts.tree.root());
        QCOMPARE(plyChanged.last().at(0).toInt(), 0);
    }

    void initializeTwiceDoesNotDuplicateConnections()
    {
        BranchNavigationParts parts;
        BranchNavigationWiring wiring;
        wiring.updateDeps(parts.deps());
        wiring.initialize();
        wiring.initialize();

        QSignalSpy board(&wiring, &BranchNavigationWiring::boardWithHighlightsRequired);
        QSignalSpy handled(&wiring, &BranchNavigationWiring::branchNodeHandled);
        parts.recordPane.nextButton()->click();

        QCOMPARE(parts.navState.currentPly(), 1);
        QCOMPARE(board.count(), 1);
        QCOMPARE(handled.count(), 1);
    }

    void initializeWaitsUntilThePartsArePassed()
    {
        BranchNavigationParts parts;
        BranchNavigationWiring wiring;
        wiring.updateDeps({});
        wiring.initialize();
        QVERIFY(wiring.editController() == nullptr);
        parts.recordPane.nextButton()->click();
        QCOMPARE(parts.navState.currentPly(), 0);

        // 部品を受け取ってから初期化すれば配線される
        wiring.updateDeps(parts.deps());
        QVERIFY(wiring.editController() != nullptr);
        wiring.initialize();
        parts.recordPane.nextButton()->click();
        QCOMPARE(parts.navState.currentPly(), 1);
    }

    void branchNodeActivationMovesToTheChosenLine()
    {
        BranchNavigationParts parts;
        BranchNavigationWiring wiring;
        wiring.updateDeps(parts.deps());
        wiring.initialize();
        QSignalSpy handled(&wiring, &BranchNavigationWiring::branchNodeHandled);

        wiring.onBranchNodeActivated(1, 3);

        QCOMPARE(parts.navState.currentNode()->sfen(), QStringLiteral("bsfen3"));
        QCOMPARE(parts.navState.currentLineIndex(), 1);
        QCOMPARE(handled.count(), 1);
        QCOMPARE(handled.last().at(0).toInt(), 3);
        QCOMPARE(handled.last().at(1).toString(), QStringLiteral("bsfen3"));
    }

    void branchNodeActivationWithoutControllerIsIgnored()
    {
        BranchNavigationWiring wiring;
        QSignalSpy handled(&wiring, &BranchNavigationWiring::branchNodeHandled);
        wiring.onBranchNodeActivated(0, 1);
        wiring.onBranchTreeBuilt();
        QCOMPARE(handled.count(), 0);
    }

    void treeBuiltRebindsGameRecordModelAndCursor()
    {
        BranchNavigationParts parts;
        BranchNavigationWiring wiring;
        wiring.updateDeps(parts.deps());
        wiring.initialize();
        parts.navController.goToPly(3);
        QVERIFY(parts.gameRecord.branchTree() == nullptr);

        wiring.onBranchTreeBuilt();

        QCOMPARE(parts.gameRecord.branchTree(), &parts.tree);
        QCOMPARE(parts.navState.currentNode(), parts.tree.root());
    }

    void resetForNewGameDiscardsLiveSessionAndReturnsToRoot()
    {
        BranchNavigationParts parts;
        BranchNavigationWiring wiring;
        wiring.updateDeps(parts.deps());
        wiring.initialize();
        parts.navController.goToLine(1, 4);
        QCOMPARE(parts.navState.preferredLineIndex(), 1);
        parts.liveSession.startFromRoot();
        QVERIFY(parts.liveSession.isActive());
        parts.startSfen = kNoLanceSfen;

        PreStartCleanupHandler::Dependencies cleanupDeps;
        cleanupDeps.liveGameSession = &parts.liveSession;
        cleanupDeps.branchTree = &parts.tree;
        cleanupDeps.navState = &parts.navState;
        PreStartCleanupHandler cleanup(cleanupDeps);
        cleanup.performCleanup({GameStartPreparation::Mode::NewGame, parts.startSfen, -1});

        QVERIFY(!parts.liveSession.isActive());
        QCOMPARE(parts.tree.root()->sfen(), kNoLanceSfen);
        QCOMPARE(parts.navState.currentNode(), parts.tree.root());
        QCOMPARE(parts.navState.preferredLineIndex(), -1);
    }
};

QTEST_MAIN(TestAppBranchNavigation)

#include "tst_app_branch_navigation.moc"
