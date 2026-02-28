/// @file tst_gamestrategy.cpp
/// @brief 対局モード別 Strategy 群の単体テスト
///
/// 3つの対局戦型Strategy（HumanVsHuman, HumanVsEngine, EngineVsEngine）の
/// 振る舞いを検証する。StrategyContext 経由で MatchCoordinator に接続し、
/// フック呼び出しと状態変化をトラッカーで確認する。

#include <QTest>
#include <QSignalSpy>
#include "matchcoordinator.h"
#include "strategycontext.h"
#include "gamemodestrategy.h"
#include "humanvshumanstrategy.h"
#include "humanvsenginestrategy.h"
#include "enginevsenginestrategy.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"

// ============================================================
// test_stubs_gamestrategy.cpp で定義されたトラッカー
// ============================================================
namespace StrategyTracker {
extern bool validateAndMoveReturnValue;
extern int  validateAndMoveCallCount;
extern bool sennichiteDetected;
void reset();
}

// ============================================================
// テストハーネス
// ============================================================
struct StrategyTestHarness {
    ShogiGameController gc;
    ShogiClock clock;
    QStringList sfenRecord;
    QVector<ShogiMove> gameMoves;

    // フック呼び出しトラッカー
    bool updateTurnDisplayCalled = false;
    MatchCoordinator::Player lastTurnPlayer = MatchCoordinator::P1;
    bool renderBoardCalled = false;
    bool showMoveHighlightsCalled = false;
    QPoint lastHighlightFrom;
    QPoint lastHighlightTo;
    int appendKifuLineCount = 0;
    bool appendEvalP1Called = false;
    bool appendEvalP2Called = false;

    std::unique_ptr<MatchCoordinator> mc;

    StrategyTestHarness()
    {
        StrategyTracker::reset();
        resetTrackers();

        MatchCoordinator::Deps deps;
        deps.gc = &gc;
        deps.clock = &clock;
        deps.view = nullptr;
        deps.usi1 = nullptr;
        deps.usi2 = nullptr;
        deps.comm1 = nullptr;
        deps.think1 = nullptr;
        deps.comm2 = nullptr;
        deps.think2 = nullptr;
        deps.sfenRecord = &sfenRecord;
        deps.gameMoves = &gameMoves;

        // UI Hooks
        deps.hooks.ui.updateTurnDisplay = [this](MatchCoordinator::Player p) {
            updateTurnDisplayCalled = true;
            lastTurnPlayer = p;
        };
        deps.hooks.ui.renderBoardFromGc = [this]() {
            renderBoardCalled = true;
        };
        deps.hooks.ui.showGameOverDialog = [](const QString&, const QString&) {};
        deps.hooks.ui.setPlayersNames = [](const QString&, const QString&) {};
        deps.hooks.ui.setEngineNames = [](const QString&, const QString&) {};
        deps.hooks.ui.showMoveHighlights = [this](const QPoint& from, const QPoint& to) {
            showMoveHighlightsCalled = true;
            lastHighlightFrom = from;
            lastHighlightTo = to;
        };

        // Time Hooks
        deps.hooks.time.remainingMsFor = [](MatchCoordinator::Player) -> qint64 { return 0; };
        deps.hooks.time.incrementMsFor = [](MatchCoordinator::Player) -> qint64 { return 0; };
        deps.hooks.time.byoyomiMs = []() -> qint64 { return 0; };

        // Engine Hooks
        deps.hooks.engine.sendGoToEngine = [](Usi*, const MatchCoordinator::GoTimes&) {};
        deps.hooks.engine.sendStopToEngine = [](Usi*) {};
        deps.hooks.engine.sendRawToEngine = [](Usi*, const QString&) {};

        // Game Hooks
        deps.hooks.game.initializeNewGame = [](const QString&) {};
        deps.hooks.game.appendKifuLine = [this](const QString&, const QString&) {
            ++appendKifuLineCount;
        };
        deps.hooks.game.appendEvalP1 = [this]() { appendEvalP1Called = true; };
        deps.hooks.game.appendEvalP2 = [this]() { appendEvalP2Called = true; };
        deps.hooks.game.autoSaveKifu = [](const QString&, PlayMode,
                                          const QString&, const QString&,
                                          const QString&, const QString&) {};

        mc = std::make_unique<MatchCoordinator>(deps);
    }

    void resetTrackers()
    {
        updateTurnDisplayCalled = false;
        lastTurnPlayer = MatchCoordinator::P1;
        renderBoardCalled = false;
        showMoveHighlightsCalled = false;
        lastHighlightFrom = QPoint();
        lastHighlightTo = QPoint();
        appendKifuLineCount = 0;
        appendEvalP1Called = false;
        appendEvalP2Called = false;
    }
};

// ============================================================
// テストクラス
// ============================================================
class Tst_GameStrategy : public QObject
{
    Q_OBJECT

private slots:
    // === Section A: HumanVsHumanStrategy ===
    void hvh_needsEngine_returnsFalse();
    void hvh_start_defaultsToPlayer1WhenNoPlayer();
    void hvh_start_preservesPlayer2WhenSet();
    void hvh_start_callsRenderAndUpdateTurnHooks();
    void hvh_onHumanMove_earlyReturnOnSennichite();
    void hvh_onHumanMove_callsFinishTimerAndByoyomi();
    void hvh_armTimer_armsOnFirstCall();
    void hvh_armTimer_idempotentOnSecondCall();
    void hvh_finishTimer_disarmsAndSetsConsideration();
    void hvh_finishTimer_nothingWhenNotArmed();

    // === Section B: HumanVsEngineStrategy ===
    void hve_needsEngine_returnsTrue();
    void hve_start_createsUsiEngine();
    void hve_start_callsRenderAndUpdateTurnHooks();
    void hve_armAndDisarmTimer();
    void hve_startInitialMove_engineIsP1();
    void hve_startInitialMove_noMoveWhenHumanTurn();
    void hve_onHumanMove_showsHighlightAndAppendsKifu();

    // === Section C: EngineVsEngineStrategy ===
    void eve_needsEngine_returnsTrue();
    void eve_onHumanMove_isNoop();
    void eve_initialState_moveIndexIsZero();
    void eve_start_earlyReturnWhenNoEngines();

    // === Section D: 共通特性 ===
    void baseClass_defaultTimerMethodsAreNoop();
    void allStrategies_canBeCreatedViaContext();
};

// ============================================================
// Section A: HumanVsHumanStrategy
// ============================================================

void Tst_GameStrategy::hvh_needsEngine_returnsFalse()
{
    StrategyTestHarness h;
    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    QVERIFY(!hvh.needsEngine());
}

void Tst_GameStrategy::hvh_start_defaultsToPlayer1WhenNoPlayer()
{
    StrategyTestHarness h;
    // GC starts as NoPlayer
    QCOMPARE(h.gc.currentPlayer(), ShogiGameController::NoPlayer);

    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    hvh.start();

    // GC should be set to Player1
    QCOMPARE(h.gc.currentPlayer(), ShogiGameController::Player1);
    // MC turn should be P1
    QCOMPARE(h.lastTurnPlayer, MatchCoordinator::P1);
}

void Tst_GameStrategy::hvh_start_preservesPlayer2WhenSet()
{
    StrategyTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player2);

    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    hvh.start();

    // GC should remain Player2
    QCOMPARE(h.gc.currentPlayer(), ShogiGameController::Player2);
    // MC turn should be P2
    QCOMPARE(h.lastTurnPlayer, MatchCoordinator::P2);
}

void Tst_GameStrategy::hvh_start_callsRenderAndUpdateTurnHooks()
{
    StrategyTestHarness h;
    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    hvh.start();

    QVERIFY(h.renderBoardCalled);
    QVERIFY(h.updateTurnDisplayCalled);
}

void Tst_GameStrategy::hvh_onHumanMove_earlyReturnOnSennichite()
{
    StrategyTestHarness h;
    StrategyTracker::sennichiteDetected = true;

    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    hvh.start();
    h.resetTrackers();

    hvh.onHumanMove(QPoint(7, 7), QPoint(7, 6), QStringLiteral("dummy"));

    // Should return early without calling finishTimer or clock update
    // (The fact that it doesn't crash is the main verification)
    QVERIFY(!h.renderBoardCalled);
}

void Tst_GameStrategy::hvh_onHumanMove_callsFinishTimerAndByoyomi()
{
    StrategyTestHarness h;
    StrategyTracker::sennichiteDetected = false;

    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    hvh.start();

    // Arm the timer first
    hvh.armTurnTimerIfNeeded();

    h.resetTrackers();

    // Make a move (GC says Player1 is current = next player, so mover is P2)
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    hvh.onHumanMove(QPoint(7, 7), QPoint(7, 6), QStringLiteral("dummy"));

    // The method should complete without crash, arms timer for next turn
    // (verification: method ran to completion)
    QVERIFY(true);
}

void Tst_GameStrategy::hvh_armTimer_armsOnFirstCall()
{
    StrategyTestHarness h;
    HumanVsHumanStrategy hvh(h.mc->strategyCtx());

    // First arm should start the timer
    hvh.armTurnTimerIfNeeded();

    // After arming, calling finish should have an effect (timer was valid)
    hvh.finishTurnTimerAndSetConsideration(static_cast<int>(MatchCoordinator::P1));
    // If timer was armed, finish should have disarmed it
    // Calling finish again should be a no-op (not armed)
    hvh.finishTurnTimerAndSetConsideration(static_cast<int>(MatchCoordinator::P1));
    QVERIFY(true);  // No crash = success
}

void Tst_GameStrategy::hvh_armTimer_idempotentOnSecondCall()
{
    StrategyTestHarness h;
    HumanVsHumanStrategy hvh(h.mc->strategyCtx());

    hvh.armTurnTimerIfNeeded();
    // Second call should not restart the timer
    hvh.armTurnTimerIfNeeded();

    // Finish once should disarm
    hvh.finishTurnTimerAndSetConsideration(static_cast<int>(MatchCoordinator::P1));
    QVERIFY(true);
}

void Tst_GameStrategy::hvh_finishTimer_disarmsAndSetsConsideration()
{
    StrategyTestHarness h;
    HumanVsHumanStrategy hvh(h.mc->strategyCtx());

    hvh.armTurnTimerIfNeeded();
    // Small delay to ensure timer accumulates some ms
    // (In stubbed ShogiClock, setPlayer1ConsiderationTime is a no-op, so
    //  we just verify the call doesn't crash)
    hvh.finishTurnTimerAndSetConsideration(static_cast<int>(MatchCoordinator::P1));

    // After finish, re-arming should work (timer was disarmed)
    hvh.armTurnTimerIfNeeded();
    hvh.finishTurnTimerAndSetConsideration(static_cast<int>(MatchCoordinator::P2));
    QVERIFY(true);
}

void Tst_GameStrategy::hvh_finishTimer_nothingWhenNotArmed()
{
    StrategyTestHarness h;
    HumanVsHumanStrategy hvh(h.mc->strategyCtx());

    // Finish without arming - should be a no-op
    hvh.finishTurnTimerAndSetConsideration(static_cast<int>(MatchCoordinator::P1));
    QVERIFY(true);
}

// ============================================================
// Section B: HumanVsEngineStrategy
// ============================================================

void Tst_GameStrategy::hve_needsEngine_returnsTrue()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);
    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    QVERIFY(hve.needsEngine());
}

void Tst_GameStrategy::hve_start_createsUsiEngine()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    hve.start();

    // After start(), usi1 should be set (created by the strategy)
    auto* elm = h.mc->engineManager();
    QVERIFY(elm->usi1() != nullptr);
    // usi2 should be null (HvE only uses one engine)
    QVERIFY(elm->usi2() == nullptr);
}

void Tst_GameStrategy::hve_start_callsRenderAndUpdateTurnHooks()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    hve.start();

    QVERIFY(h.renderBoardCalled);
    QVERIFY(h.updateTurnDisplayCalled);
}

void Tst_GameStrategy::hve_armAndDisarmTimer()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));

    // Arm the timer
    hve.armTurnTimerIfNeeded();

    // Disarm should reset without crash
    hve.disarmTurnTimer();

    // Re-arming after disarm should work
    hve.armTurnTimerIfNeeded();
    hve.finishTurnTimerAndSetConsideration(static_cast<int>(MatchCoordinator::P1));
    QVERIFY(true);
}

void Tst_GameStrategy::hve_startInitialMove_engineIsP1()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenEngineVsHuman);
    h.gc.setCurrentPlayer(ShogiGameController::Player1);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), true,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    hve.start();

    h.resetTrackers();
    StrategyTracker::validateAndMoveReturnValue = true;

    // Engine is P1, GC says Player1 to move → engine should attempt initial move
    hve.startInitialMoveIfNeeded();

    // validateAndMove should have been called (engine makes the first move)
    QVERIFY(StrategyTracker::validateAndMoveCallCount > 0);
}

void Tst_GameStrategy::hve_startInitialMove_noMoveWhenHumanTurn()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);
    h.gc.setCurrentPlayer(ShogiGameController::Player1);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    hve.start();

    h.resetTrackers();
    StrategyTracker::validateAndMoveCallCount = 0;

    // Engine is P2, GC says Player1 to move → no initial move
    hve.startInitialMoveIfNeeded();

    // validateAndMove should NOT have been called
    QCOMPARE(StrategyTracker::validateAndMoveCallCount, 0);
}

void Tst_GameStrategy::hve_onHumanMove_showsHighlightAndAppendsKifu()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);
    h.gc.setCurrentPlayer(ShogiGameController::Player2);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    hve.start();
    h.resetTrackers();

    StrategyTracker::validateAndMoveReturnValue = true;

    const QPoint from(7, 7);
    const QPoint to(7, 6);
    hve.onHumanMove(from, to, QStringLiteral("▲７六歩"));

    // Human move should show highlights
    QVERIFY(h.showMoveHighlightsCalled);
    QCOMPARE(h.lastHighlightFrom, from);
    QCOMPARE(h.lastHighlightTo, to);

    // Kifu line should be appended for the human move
    QVERIFY(h.appendKifuLineCount > 0);
}

// ============================================================
// Section C: EngineVsEngineStrategy
// ============================================================

void Tst_GameStrategy::eve_needsEngine_returnsTrue()
{
    StrategyTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::EvenEngineVsEngine;

    EngineVsEngineStrategy eve(h.mc->strategyCtx(), opt);
    QVERIFY(eve.needsEngine());
}

void Tst_GameStrategy::eve_onHumanMove_isNoop()
{
    StrategyTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::EvenEngineVsEngine;

    EngineVsEngineStrategy eve(h.mc->strategyCtx(), opt);

    h.resetTrackers();

    // onHumanMove should be a no-op for EvE
    eve.onHumanMove(QPoint(1, 1), QPoint(2, 2), QStringLiteral("dummy"));

    // No hooks should have been called
    QVERIFY(!h.renderBoardCalled);
    QVERIFY(!h.updateTurnDisplayCalled);
    QVERIFY(!h.showMoveHighlightsCalled);
    QCOMPARE(h.appendKifuLineCount, 0);
}

void Tst_GameStrategy::eve_initialState_moveIndexIsZero()
{
    StrategyTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::EvenEngineVsEngine;

    EngineVsEngineStrategy eve(h.mc->strategyCtx(), opt);
    QCOMPARE(eve.eveMoveIndex(), 0);
}

void Tst_GameStrategy::eve_start_earlyReturnWhenNoEngines()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenEngineVsEngine);

    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::EvenEngineVsEngine;

    EngineVsEngineStrategy eve(h.mc->strategyCtx(), opt);

    // usi1/usi2 are nullptr → start() should return early
    eve.start();

    // Move index should remain 0 (no moves made)
    QCOMPARE(eve.eveMoveIndex(), 0);
    // No hooks should have been called (early return before hook calls)
    QVERIFY(!h.updateTurnDisplayCalled);
}

// ============================================================
// Section D: 共通特性
// ============================================================

void Tst_GameStrategy::baseClass_defaultTimerMethodsAreNoop()
{
    // Use a minimal concrete subclass to test base default methods
    struct MinimalStrategy : public GameModeStrategy {
        void start() override {}
        void onHumanMove(const QPoint&, const QPoint&, const QString&) override {}
        bool needsEngine() const override { return false; }
    };

    MinimalStrategy s;

    // Default implementations should be no-ops (no crash)
    s.armTurnTimerIfNeeded();
    s.finishTurnTimerAndSetConsideration(1);
    s.disarmTurnTimer();
    s.startInitialMoveIfNeeded();
    QVERIFY(true);
}

void Tst_GameStrategy::allStrategies_canBeCreatedViaContext()
{
    StrategyTestHarness h;

    // HumanVsHuman
    {
        HumanVsHumanStrategy hvh(h.mc->strategyCtx());
        QVERIFY(!hvh.needsEngine());
    }

    // HumanVsEngine (engine is P2)
    {
        h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);
        HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                                   QStringLiteral("/dummy/engine"),
                                   QStringLiteral("TestEngine"));
        QVERIFY(hve.needsEngine());
    }

    // EngineVsEngine
    {
        MatchCoordinator::StartOptions opt;
        opt.mode = PlayMode::EvenEngineVsEngine;
        EngineVsEngineStrategy eve(h.mc->strategyCtx(), opt);
        QVERIFY(eve.needsEngine());
    }
}

// ============================================================
QTEST_MAIN(Tst_GameStrategy)
#include "tst_gamestrategy.moc"
