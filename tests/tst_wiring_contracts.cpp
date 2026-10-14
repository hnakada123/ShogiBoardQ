/// @file tst_wiring_contracts.cpp
/// @brief MatchCoordinatorWiring の配線契約テスト
///
/// 司令塔（MatchCoordinator）と GameStartCoordinator のシグナルが、登録した受け手へ
/// 決まった順で届くかを検証する。受け手はスタブで、呼ばれた順を TestTracker::calls に記録する。

#include <QtTest/QtTest>

#include "matchcoordinatorwiring.h"
#include "matchcoordinator.h"
#include "gamestartcoordinator.h"
#include "gamesessionorchestrator.h"
#include "sessionlifecyclecoordinator.h"
#include "gamestatecontroller.h"
#include "consecutivegamescontroller.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfowiring.h"
#include "kifunavigationcoordinator.h"
#include "branchnavigationwiring.h"
#include "uistatepolicymanager.h"
#include "shogiview.h"
#include "shogigamecontroller.h"

// TestTracker は test_stubs_wiring_contracts.cpp で定義
namespace TestTracker {
    extern QStringList calls;
    extern int selectKifuRowValue;
    extern void reset();
}

class TestWiringContracts : public QObject
{
    Q_OBJECT

private:
    /// 受け手をすべて登録し、司令塔と GameStartCoordinator を作った状態
    struct WiredFixture {
        GameSessionOrchestrator gso;
        SessionLifecycleCoordinator slc;
        GameStateController gsc;
        ConsecutiveGamesController cgc;
        MainWindowAppearanceController appearance;
        PlayerInfoWiring::Dependencies piDeps;
        PlayerInfoWiring playerInfo{piDeps};
        KifuNavigationCoordinator kifuNav;
        BranchNavigationWiring branchNav;
        UiStatePolicyManager uiPolicy;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring mcw;

        WiredFixture()
        {
            TestTracker::reset();

            MatchCoordinatorWiring::SignalTargets targets;
            targets.gameSession = &gso;
            targets.sessionLifecycle = &slc;
            targets.gameStateController = &gsc;
            targets.consecutiveGamesController = &cgc;
            targets.appearance = &appearance;
            targets.playerInfo = &playerInfo;
            targets.kifuNav = &kifuNav;
            targets.branchNav = &branchNav;
            targets.uiStatePolicy = &uiPolicy;
            mcw.setSignalTargets(targets);

            MatchCoordinatorWiring::Deps deps;
            deps.gc = &gc;
            deps.view = &view;
            mcw.updateDeps(deps);
            mcw.initializeSession();
            mcw.createMenuGameStartCoordinator();
        }

        MatchCoordinator* match() const { return mcw.match(); }
        GameStartCoordinator* gameStart() const { return mcw.menuGameStartCoordinator(); }
    };

private slots:
    // ================================================================
    // 司令塔のシグナル
    // ================================================================

    void match_requestAppendGameOverMove_appendsBeforeCommit()
    {
        WiredFixture f;
        QVERIFY(f.match());

        emit f.match()->requestAppendGameOverMove(MatchCoordinator::GameEndInfo{});

        // 終局手を棋譜に足してから、その手を含めてライブセッションを確定する
        const QStringList expected = {
            QStringLiteral("GSC::onRequestAppendGameOverMove"),
            QStringLiteral("SLC::commitLiveGameSessionIfActive"),
        };
        QCOMPARE(TestTracker::calls, expected);
    }

    void match_gameEnded_recordsBeforeReturningToIdle()
    {
        WiredFixture f;

        emit f.match()->gameEnded(MatchCoordinator::GameEndInfo{});

        // 終局の記録（時計の停止・終局時刻）→ UI状態を「アイドル」へ（docs/dev/ui-state-policy.md）
        const QStringList expected = {
            QStringLiteral("SLC::handleGameEnded"),
            QStringLiteral("UiPolicy::transitionToIdle"),
        };
        QCOMPARE(TestTracker::calls, expected);
    }

    void match_gameEnded_orderKeptAfterSessionRebind()
    {
        WiredFixture f;
        // 起動の最終段などで2回目以降の initializeSession が呼ばれても、接続は増えず順も変わらない
        QVERIFY(f.mcw.initializeSession());
        QVERIFY(f.mcw.initializeSession());

        emit f.match()->gameEnded(MatchCoordinator::GameEndInfo{});

        const QStringList expected = {
            QStringLiteral("SLC::handleGameEnded"),
            QStringLiteral("UiPolicy::transitionToIdle"),
        };
        QCOMPARE(TestTracker::calls, expected);
    }

    void match_gameEndProcessed_reachesSessionLifecycle()
    {
        WiredFixture f;

        emit f.match()->gameEndProcessed(MatchCoordinator::GameEndInfo{});

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("SLC::handleGameEndProcessed")});
    }

    void match_gameOverStateChanged_reachesGameStateController()
    {
        WiredFixture f;

        emit f.match()->gameOverStateChanged(MatchCoordinator::GameOverState{});

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("GSC::onGameOverStateChanged")});
    }

    void match_analysisModesEnded_returnToIdle()
    {
        WiredFixture f;

        emit f.match()->considerationModeEnded();
        emit f.match()->tsumeSearchModeEnded();

        const QStringList expected = {
            QStringLiteral("UiPolicy::transitionToIdle"),
            QStringLiteral("UiPolicy::transitionToIdle"),
        };
        QCOMPARE(TestTracker::calls, expected);
    }

    void match_boardFlipped_reachesAppearance()
    {
        WiredFixture f;

        emit f.match()->boardFlipped(true);

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("Appearance::onBoardFlipped")});
    }

    void match_gameInterrupted_reachesGameSession()
    {
        WiredFixture f;

        emit f.match()->gameInterrupted();

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("GSO::onGameInterrupted")});
    }

    // ================================================================
    // GameStartCoordinator のシグナル
    // ================================================================

    void gameStart_started_entersGameBeforeSessionUpdates()
    {
        WiredFixture f;
        QVERIFY(f.gameStart());

        emit f.gameStart()->started(MatchCoordinator::StartOptions{});

        // UI状態を「対局中」に → 開始局面とモードの反映 → 中断した対局の破棄
        const QStringList expected = {
            QStringLiteral("UiPolicy::transitionToDuringGame"),
            QStringLiteral("SLC::handleGameStarted"),
            QStringLiteral("GSO::discardInterruptedGame"),
        };
        QCOMPARE(TestTracker::calls, expected);
    }

    void gameStart_requestPreStartCleanup_reachesSessionLifecycle()
    {
        WiredFixture f;

        emit f.gameStart()->requestPreStartCleanup();

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("SLC::performPreStartCleanup")});
    }

    void gameStart_requestApplyTimeControl_reachesSessionLifecycle()
    {
        WiredFixture f;

        emit f.gameStart()->requestApplyTimeControl(GameStartCoordinator::TimeSettings{});

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("SLC::applyTimeControl")});
    }

    void gameStart_playerNamesResolved_reachesPlayerInfo()
    {
        WiredFixture f;

        emit f.gameStart()->playerNamesResolved(
            QStringLiteral("h1"), QStringLiteral("h2"),
            QStringLiteral("e1"), QStringLiteral("e2"), 0);

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("PlayerInfo::onMenuPlayerNamesResolved")});
    }

    void gameStart_consecutiveGamesConfigured_reachesConsecutiveGames()
    {
        WiredFixture f;

        emit f.gameStart()->consecutiveGamesConfigured(5, true);

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("CGC::configure")});
    }

    void gameStart_requestSelectKifuRow_reachesKifuNav()
    {
        WiredFixture f;

        emit f.gameStart()->requestSelectKifuRow(42);

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("KifuNav::selectKifuRow")});
        QCOMPARE(TestTracker::selectKifuRowValue, 42);
    }

    void gameStart_requestBranchTreeReset_reachesBranchNav()
    {
        WiredFixture f;

        emit f.gameStart()->requestBranchTreeResetForNewGame();

        QCOMPARE(TestTracker::calls, QStringList{QStringLiteral("BranchNav::onBranchTreeResetForNewGame")});
    }

    // ================================================================
    // 生成と防御
    // ================================================================

    void noTargets_signalsDoNotCrash()
    {
        TestTracker::reset();
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring mcw;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);
        QVERIFY(mcw.initializeSession());
        mcw.createMenuGameStartCoordinator();

        emit mcw.match()->gameEnded(MatchCoordinator::GameEndInfo{});
        emit mcw.menuGameStartCoordinator()->started(MatchCoordinator::StartOptions{});

        QVERIFY(TestTracker::calls.isEmpty());
    }

    void initializeSession_withoutBoard_createsNothing()
    {
        MatchCoordinatorWiring mcw;

        QVERIFY(!mcw.initializeSession());
        QVERIFY(mcw.match() == nullptr);
    }

    void initializeSession_keepsSameCoordinator()
    {
        WiredFixture f;
        MatchCoordinator* first = f.match();

        QVERIFY(f.mcw.initializeSession());

        QCOMPARE(f.match(), first);
    }

    void ensureMenuGSC_idempotent()
    {
        WiredFixture f;
        GameStartCoordinator* first = f.gameStart();
        QVERIFY(first);

        f.mcw.createMenuGameStartCoordinator();

        QCOMPARE(f.gameStart(), first);
    }
};

QTEST_MAIN(TestWiringContracts)

#include "tst_wiring_contracts.moc"
