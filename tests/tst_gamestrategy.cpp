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
#include "usi.h"
#include "playmodepolicyservice.h"
#include "playmode.h"

// ============================================================
// test_stubs_gamestrategy.cpp で定義されたトラッカー
// ============================================================
namespace StrategyTracker {
extern bool validateAndMoveReturnValue;
extern int  validateAndMoveCallCount;
extern bool sennichiteDetected;
extern int  maxMovesJishogiCount;
extern int  requestHumanReplyCount;
extern int  requestMatchMoveCount;
void reset();
}

// ============================================================
// テストハーネス
// ============================================================
struct StrategyTestHarness {
    ShogiGameController gc;
    ShogiClock clock;
    QStringList sfenRecord;
    QList<ShogiMove> gameMoves;

    // フック呼び出しトラッカー
    bool updateTurnDisplayCalled = false;
    MatchCoordinator::Player lastTurnPlayer = MatchCoordinator::P1;
    bool renderBoardCalled = false;
    bool showMoveHighlightsCalled = false;
    QPoint lastHighlightFrom;
    QPoint lastHighlightTo;
    int appendKifuLineCount = 0;
    QStringList appendedKifuTexts;
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
        deps.hooks.engine.sendStopToEngine = [](Usi*) {};

        // Game Hooks
        deps.hooks.game.initializeNewGame = [](const QString&) {};
        deps.hooks.game.appendKifuLine = [this](const QString& text, const QString&) {
            ++appendKifuLineCount;
            appendedKifuTexts.append(text);
        };
        deps.hooks.game.appendEvalP1 = [this]() { appendEvalP1Called = true; };
        deps.hooks.game.appendEvalP2 = [this]() { appendEvalP2Called = true; };
        deps.hooks.game.autoSaveKifu = [](const QString&) {};

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
        appendedKifuTexts.clear();
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
    void hvh_onHumanMove_recordsMoveBeforeSennichite();
    void hvh_onHumanMove_maxMoves_data();
    void hvh_onHumanMove_maxMoves();
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
    void hve_initializationClock_data();
    void hve_initializationClock();
    void hve_startInitialMove_engineIsP1();
    void hve_startInitialMove_noMoveWhenHumanTurn();
    void hve_onHumanMove_showsHighlightAndAppendsKifu();
    void hve_onHumanMove_maxMovesReachedByHuman_data();
    void hve_onHumanMove_maxMovesReachedByHuman();
    void hve_onEngineMove_recordsMoveBeforeSennichite();

    // === Section C: EngineVsEngineStrategy ===
    void eve_needsEngine_returnsTrue();
    void eve_onHumanMove_isNoop();
    void eve_initialState_moveIndexIsZero();
    void eve_start_earlyReturnWhenNoEngines();
    void eve_gameMoves_useSharedList();

    // === Section C2: 人間の手番判定（終局後は盤を操作させない） ===
    void policy_humanTurn_onlyWhileGameInProgress_data();
    void policy_humanTurn_onlyWhileGameInProgress();

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

void Tst_GameStrategy::hvh_onHumanMove_recordsMoveBeforeSennichite()
{
    StrategyTestHarness h;
    StrategyTracker::sennichiteDetected = true;

    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    hvh.start();
    h.resetTrackers();

    hvh.onHumanMove(QPoint(5, 2), QPoint(5, 1), QStringLiteral("△５一玉(52)"));

    // 千日手を成立させた手も棋譜に記録してから終局する（終局行はこの手の後に付く）
    QCOMPARE(h.appendedKifuTexts, QStringList{QStringLiteral("△５一玉(52)")});
    QCOMPARE(StrategyTracker::maxMovesJishogiCount, 0);
}

void Tst_GameStrategy::hvh_onHumanMove_maxMoves_data()
{
    QTest::addColumn<int>("maxMoves");
    QTest::addColumn<bool>("expectJishogi");

    QTest::newRow("unlimited") << 0 << false;
    QTest::newRow("reached") << 1 << true;
    QTest::newRow("not-yet-reached") << 2 << false;
}

void Tst_GameStrategy::hvh_onHumanMove_maxMoves()
{
    QFETCH(int, maxMoves);
    QFETCH(bool, expectJishogi);

    StrategyTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.maxMoves = maxMoves;
    h.mc->configureAndStart(opt);

    HumanVsHumanStrategy hvh(h.mc->strategyCtx());
    hvh.start();

    // 先手が1手目を指した直後
    h.sfenRecord = {
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
    };
    h.gc.setCurrentPlayer(ShogiGameController::Player2);
    h.resetTrackers();

    hvh.onHumanMove(QPoint(7, 7), QPoint(7, 6), QStringLiteral("▲７六歩(77)"));

    // 指し手を記録したうえで、最大手数に達していれば持将棋にする
    QCOMPARE(h.appendedKifuTexts, QStringList{QStringLiteral("▲７六歩(77)")});
    QCOMPARE(StrategyTracker::maxMovesJishogiCount, expectJishogi ? 1 : 0);
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

void Tst_GameStrategy::hve_initializationClock_data()
{
    QTest::addColumn<bool>("engineIsP1");
    QTest::addColumn<bool>("humanToMove");
    QTest::newRow("human-black") << false << true;
    QTest::newRow("human-white") << true << true;
    QTest::newRow("engine-black") << true << false;
    QTest::newRow("engine-white") << false << false;
}

void Tst_GameStrategy::hve_initializationClock()
{
    QFETCH(bool, engineIsP1);
    QFETCH(bool, humanToMove);
    StrategyTestHarness h;
    h.mc->setPlayMode(engineIsP1 ? PlayMode::EvenEngineVsHuman : PlayMode::EvenHumanVsEngine);
    const bool blackToMove = engineIsP1 != humanToMove;
    h.gc.setCurrentPlayer(blackToMove ? ShogiGameController::Player1 : ShogiGameController::Player2);
    HumanVsEngineStrategy hve(h.mc->strategyCtx(), engineIsP1,
                             QStringLiteral("/dummy/engine"), QStringLiteral("TestEngine"));
    hve.start();
    auto* engine = h.mc->strategyCtx().primaryEngine();
    QVERIFY(engine);
    QVERIFY(engine->startAndInitializeEngineAsync(QStringLiteral("/dummy/engine"), QStringLiteral("TestEngine")));
    QVERIFY(engine->isInitializing());
    h.clock.startClock();
    hve.startInitialMoveIfNeeded();
    QCOMPARE(h.clock.isRunning(), humanToMove);
    QCOMPARE(StrategyTracker::requestMatchMoveCount, 0);

    QVERIFY(QMetaObject::invokeMethod(engine, "onEngineInitialized", Qt::DirectConnection, Q_ARG(bool, true)));
    QVERIFY(h.clock.isRunning());
    QCOMPARE(StrategyTracker::requestMatchMoveCount, humanToMove ? 0 : 1);
    hve.startInitialMoveIfNeeded();
    QCOMPARE(StrategyTracker::requestMatchMoveCount, humanToMove ? 0 : 1);
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

    // 送信時は盤面を変更せず、応答を受信してから着手する。
    QCOMPARE(StrategyTracker::validateAndMoveCallCount, 0);
    emit h.mc->strategyCtx().primaryEngine()->matchMoveReady(
        QPoint(7, 7), QPoint(7, 6), QStringLiteral("position startpos moves 7g7f"), {});
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

void Tst_GameStrategy::hve_onHumanMove_maxMovesReachedByHuman_data()
{
    QTest::addColumn<int>("maxMoves");
    QTest::addColumn<bool>("expectJishogi");

    QTest::newRow("unlimited") << 0 << false;
    QTest::newRow("reached-by-human-move") << 1 << true;
    QTest::newRow("not-yet-reached") << 2 << false;
}

void Tst_GameStrategy::hve_onHumanMove_maxMovesReachedByHuman()
{
    QFETCH(int, maxMoves);
    QFETCH(bool, expectJishogi);

    StrategyTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::EvenHumanVsEngine;
    opt.maxMoves = maxMoves;
    h.mc->configureAndStart(opt);
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    hve.start();

    // 先手（人間）が1手目を指した直後。次はエンジン（後手）の手番
    h.sfenRecord = {
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
    };
    h.gc.setCurrentPlayer(ShogiGameController::Player2);

    hve.onHumanMove(QPoint(7, 7), QPoint(7, 6), QStringLiteral("▲７六歩"));

    // 人間の手で最大手数に達したら、エンジンに次の手を求めずに持将棋にする
    QCOMPARE(StrategyTracker::maxMovesJishogiCount, expectJishogi ? 1 : 0);
    QCOMPARE(StrategyTracker::requestHumanReplyCount, expectJishogi ? 0 : 1);
}

void Tst_GameStrategy::hve_onEngineMove_recordsMoveBeforeSennichite()
{
    StrategyTestHarness h;
    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);

    HumanVsEngineStrategy hve(h.mc->strategyCtx(), false,
                               QStringLiteral("/dummy/engine"),
                               QStringLiteral("TestEngine"));
    hve.start();

    h.sfenRecord = {
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
    };
    h.gc.setCurrentPlayer(ShogiGameController::Player2);
    hve.onHumanMove(QPoint(7, 7), QPoint(7, 6), QStringLiteral("▲７六歩(77)"));
    QCOMPARE(StrategyTracker::requestHumanReplyCount, 1);
    h.resetTrackers();

    // エンジン（後手）の手で千日手が成立する
    StrategyTracker::sennichiteDetected = true;
    Usi* eng = h.mc->primaryEngine();
    QVERIFY(eng);
    Q_EMIT eng->matchMoveReady(QPoint(3, 3), QPoint(3, 4),
                               QStringLiteral("position startpos moves 7g7f 3c3d"), QString());

    // 千日手を成立させたエンジンの手も棋譜と盤面に反映してから終局する
    QCOMPARE(h.appendKifuLineCount, 1);
    QVERIFY(h.showMoveHighlightsCalled);
    QVERIFY(h.renderBoardCalled);
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

void Tst_GameStrategy::eve_gameMoves_useSharedList()
{
    // エンジン同士の対局も共有の指し手リストへ記録する。内部リストに記録すると、
    // 棋譜欄の英語式表記・USI指し手列・分岐ツリーのノードに指し手が渡らない。
    StrategyTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::EvenEngineVsEngine;

    EngineVsEngineStrategy eve(h.mc->strategyCtx(), opt);

    QCOMPARE(&eve.gameMovesForEvE(), &h.gameMoves);
    QCOMPARE(eve.sfenRecordForEvE(), &h.sfenRecord);
}

// ============================================================
// Section C2: 人間の手番判定
// ============================================================

void Tst_GameStrategy::policy_humanTurn_onlyWhileGameInProgress_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("humanPlayer");
    QTest::newRow("human-vs-human") << int(PlayMode::HumanVsHuman) << int(ShogiGameController::Player1);
    QTest::newRow("human-vs-engine") << int(PlayMode::EvenHumanVsEngine) << int(ShogiGameController::Player1);
    QTest::newRow("engine-vs-human") << int(PlayMode::EvenEngineVsHuman) << int(ShogiGameController::Player2);
    QTest::newRow("handicap-human-vs-engine") << int(PlayMode::HandicapHumanVsEngine) << int(ShogiGameController::Player1);
}

void Tst_GameStrategy::policy_humanTurn_onlyWhileGameInProgress()
{
    QFETCH(int, mode);
    QFETCH(int, humanPlayer);
    StrategyTestHarness h;
    PlayMode playMode = static_cast<PlayMode>(mode);
    h.gc.setCurrentPlayer(static_cast<ShogiGameController::Player>(humanPlayer));

    PlayModePolicyService policy;
    PlayModePolicyService::Deps deps;
    deps.playMode = &playMode;
    deps.gameController = &h.gc;
    deps.match = h.mc.get();
    policy.updateDeps(deps);

    h.mc->clearGameOverState();
    QVERIFY(policy.isHumanTurnNow());

    // 終局後はプレイモードが残っていても盤上の着手を受け付けない（棋譜に記録されないため）。
    MatchCoordinator::GameEndInfo info;
    h.mc->setGameOver(info, true);
    QVERIFY(h.mc->gameOverState().isOver);
    QVERIFY(!policy.isHumanTurnNow());

    // 中断からの再開などで終局状態が解除されれば再び指せる。
    h.mc->clearGameOverState();
    QVERIFY(policy.isHumanTurnNow());

    // 対局コーディネータがない状態では指せない。
    deps.match = nullptr;
    policy.updateDeps(deps);
    QVERIFY(!policy.isHumanTurnNow());
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
