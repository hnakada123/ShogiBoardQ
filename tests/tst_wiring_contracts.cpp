/// @file tst_wiring_contracts.cpp
/// @brief MatchCoordinatorWiring のシグナル転送契約テスト
///
/// wireForwardingSignals() と ensureMenuGameStartCoordinator() が
/// 正しくシグナルを転送するかを検証する。

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "matchcoordinatorwiring.h"
#include "matchcoordinator.h"
#include "gamestartcoordinator.h"
#include "gamesessionorchestrator.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfowiring.h"
#include "kifunavigationcoordinator.h"
#include "branchnavigationwiring.h"
#include "shogiview.h"
#include "shogigamecontroller.h"

// TestTracker は test_stubs_wiring_contracts.cpp で定義
namespace TestTracker {
    extern bool onRequestAppendGameOverMoveCalled;
    extern bool onBoardFlippedCalled;
    extern bool onGameOverStateChangedCalled;
    extern bool onMatchGameEndedCalled;
    extern bool onResignationTriggeredCalled;
    extern bool onPreStartCleanupRequestedCalled;
    extern bool onApplyTimeControlRequestedCalled;
    extern bool onMenuPlayerNamesResolvedCalled;
    extern bool onConsecutiveGamesConfiguredCalled;
    extern bool onGameStartedCalled;
    extern bool selectKifuRowCalled;
    extern bool onBranchTreeResetForNewGameCalled;
    extern int  selectKifuRowValue;
    extern void reset();
}

class TestWiringContracts : public QObject
{
    Q_OBJECT

private:
    /// wireForwardingSignals テスト用の共通セットアップ
    struct ForwardingFixture {
        MatchCoordinatorWiring mcw;
        GameSessionOrchestrator gso;
        MainWindowAppearanceController appearance;
        KifuNavigationCoordinator kifuNav;
        BranchNavigationWiring branchNav;

        ForwardingFixture()
        {
            TestTracker::reset();

            MatchCoordinatorWiring::ForwardingTargets targets;
            targets.appearance = &appearance;
            targets.kifuNav    = &kifuNav;
            targets.branchNav  = &branchNav;
            // targets.playerInfo は別途テスト

            mcw.wireForwardingSignals(targets, &gso);
        }
    };

    /// PlayerInfoWiring 用 ForwardingFixture
    struct PlayerInfoForwardingFixture {
        MatchCoordinatorWiring mcw;
        GameSessionOrchestrator gso;
        PlayerInfoWiring::Dependencies piDeps;
        PlayerInfoWiring piw{piDeps};

        PlayerInfoForwardingFixture()
        {
            TestTracker::reset();

            MatchCoordinatorWiring::ForwardingTargets targets;
            targets.playerInfo = &piw;

            mcw.wireForwardingSignals(targets, &gso);
        }
    };

private slots:
    // ================================================================
    // wireForwardingSignals テスト群
    // ================================================================

    void wireForwarding_requestAppendGameOverMove_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.requestAppendGameOverMove(MatchCoordinator::GameEndInfo{});

        QVERIFY(TestTracker::onRequestAppendGameOverMoveCalled);
    }

    void wireForwarding_boardFlipped_reachesAppearance()
    {
        ForwardingFixture f;

        emit f.mcw.boardFlipped(true);

        QVERIFY(TestTracker::onBoardFlippedCalled);
    }

    void wireForwarding_gameOverStateChanged_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.gameOverStateChanged(MatchCoordinator::GameOverState{});

        QVERIFY(TestTracker::onGameOverStateChangedCalled);
    }

    void wireForwarding_matchGameEnded_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.matchGameEnded(MatchCoordinator::GameEndInfo{});

        QVERIFY(TestTracker::onMatchGameEndedCalled);
    }

    void wireForwarding_resignationTriggered_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.resignationTriggered();

        QVERIFY(TestTracker::onResignationTriggeredCalled);
    }

    void wireForwarding_requestPreStartCleanup_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.requestPreStartCleanup();

        QVERIFY(TestTracker::onPreStartCleanupRequestedCalled);
    }

    void wireForwarding_requestApplyTimeControl_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.requestApplyTimeControl(GameStartCoordinator::TimeControl{});

        QVERIFY(TestTracker::onApplyTimeControlRequestedCalled);
    }

    void wireForwarding_menuPlayerNamesResolved_reachesPlayerInfo()
    {
        PlayerInfoForwardingFixture f;

        emit f.mcw.menuPlayerNamesResolved(
            QStringLiteral("human1"), QStringLiteral("human2"),
            QStringLiteral("engine1"), QStringLiteral("engine2"), 1);

        QVERIFY(TestTracker::onMenuPlayerNamesResolvedCalled);
    }

    void wireForwarding_consecutiveGamesConfigured_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.consecutiveGamesConfigured(5, true);

        QVERIFY(TestTracker::onConsecutiveGamesConfiguredCalled);
    }

    void wireForwarding_gameStarted_reachesGso()
    {
        ForwardingFixture f;

        emit f.mcw.gameStarted(MatchCoordinator::StartOptions{});

        QVERIFY(TestTracker::onGameStartedCalled);
    }

    void wireForwarding_requestSelectKifuRow_reachesKifuNav()
    {
        ForwardingFixture f;

        emit f.mcw.requestSelectKifuRow(42);

        QVERIFY(TestTracker::selectKifuRowCalled);
        QCOMPARE(TestTracker::selectKifuRowValue, 42);
    }

    void wireForwarding_requestBranchTreeReset_reachesBranchNav()
    {
        ForwardingFixture f;

        emit f.mcw.requestBranchTreeResetForNewGame();

        QVERIFY(TestTracker::onBranchTreeResetForNewGameCalled);
    }

    // ================================================================
    // wireForwardingSignals 防御テスト
    // ================================================================

    void wireForwarding_nullGso_doesNotCrash()
    {
        MatchCoordinatorWiring mcw;
        MatchCoordinatorWiring::ForwardingTargets targets;
        // gso=nullptr → 何もしない（クラッシュしない）
        mcw.wireForwardingSignals(targets, nullptr);
    }

    // ================================================================
    // ensureMenuGameStartCoordinator テスト群
    // ================================================================

    void ensureMenuGSC_createsCoordinator()
    {
        MatchCoordinatorWiring mcw;

        // 最低限の Deps をセットアップ（m_gc, m_view が必要）
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        QVERIFY(mcw.menuGameStartCoordinator() == nullptr);

        mcw.ensureMenuGameStartCoordinator();

        QVERIFY(mcw.menuGameStartCoordinator() != nullptr);
    }

    void ensureMenuGSC_idempotent()
    {
        MatchCoordinatorWiring mcw;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        mcw.ensureMenuGameStartCoordinator();
        auto* first = mcw.menuGameStartCoordinator();

        mcw.ensureMenuGameStartCoordinator();
        auto* second = mcw.menuGameStartCoordinator();

        QCOMPARE(first, second);
    }

    void ensureMenuGSC_requestPreStartCleanup_forwarded()
    {
        MatchCoordinatorWiring mcw;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        mcw.ensureMenuGameStartCoordinator();

        QSignalSpy spy(&mcw, &MatchCoordinatorWiring::requestPreStartCleanup);

        emit mcw.menuGameStartCoordinator()->requestPreStartCleanup();

        QCOMPARE(spy.count(), 1);
    }

    void ensureMenuGSC_started_forwarded()
    {
        MatchCoordinatorWiring mcw;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        mcw.ensureMenuGameStartCoordinator();

        QSignalSpy spy(&mcw, &MatchCoordinatorWiring::gameStarted);

        emit mcw.menuGameStartCoordinator()->started(MatchCoordinator::StartOptions{});

        QCOMPARE(spy.count(), 1);
    }

    void ensureMenuGSC_playerNamesResolved_forwarded()
    {
        MatchCoordinatorWiring mcw;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        mcw.ensureMenuGameStartCoordinator();

        QSignalSpy spy(&mcw, &MatchCoordinatorWiring::menuPlayerNamesResolved);

        emit mcw.menuGameStartCoordinator()->playerNamesResolved(
            QStringLiteral("h1"), QStringLiteral("h2"),
            QStringLiteral("e1"), QStringLiteral("e2"), 0);

        QCOMPARE(spy.count(), 1);
    }

    void ensureMenuGSC_boardFlipped_forwarded()
    {
        MatchCoordinatorWiring mcw;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        mcw.ensureMenuGameStartCoordinator();

        QSignalSpy spy(&mcw, &MatchCoordinatorWiring::boardFlipped);

        emit mcw.menuGameStartCoordinator()->boardFlipped(true);

        QCOMPARE(spy.count(), 1);
    }

    void ensureMenuGSC_requestSelectKifuRow_forwarded()
    {
        MatchCoordinatorWiring mcw;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        mcw.ensureMenuGameStartCoordinator();

        QSignalSpy spy(&mcw, &MatchCoordinatorWiring::requestSelectKifuRow);

        emit mcw.menuGameStartCoordinator()->requestSelectKifuRow(7);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 7);
    }

    void ensureMenuGSC_requestBranchTreeReset_forwarded()
    {
        MatchCoordinatorWiring mcw;
        ShogiGameController gc;
        ShogiView view;
        MatchCoordinatorWiring::Deps deps;
        deps.gc   = &gc;
        deps.view = &view;
        mcw.updateDeps(deps);

        mcw.ensureMenuGameStartCoordinator();

        QSignalSpy spy(&mcw, &MatchCoordinatorWiring::requestBranchTreeResetForNewGame);

        emit mcw.menuGameStartCoordinator()->requestBranchTreeResetForNewGame();

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestWiringContracts)

#include "tst_wiring_contracts.moc"
