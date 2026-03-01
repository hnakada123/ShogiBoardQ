/// @file tst_game_end_handler.cpp
/// @brief GameEndHandler 単体テスト
///
/// 終局処理（投了・中断・千日手・持将棋・入玉宣言）が
/// 正しい GameEndInfo を生成し、正しいフックを呼び出すことを検証する。

#include <QTest>
#include <QSignalSpy>
#include "gameendhandler.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"

// ============================================================
// テスト用トラッカー
// ============================================================
namespace EndTracker {

bool disarmHumanTimerCalled = false;
bool showGameOverDialogCalled = false;
bool autoSaveKifuCalled = false;
int appendKifuLineCount = 0;
QString lastAppendedLine;
QString lastAppendedElapsed;
QString lastDialogTitle;
QString lastDialogMsg;

void reset()
{
    disarmHumanTimerCalled = false;
    showGameOverDialogCalled = false;
    autoSaveKifuCalled = false;
    appendKifuLineCount = 0;
    lastAppendedLine.clear();
    lastAppendedElapsed.clear();
    lastDialogTitle.clear();
    lastDialogMsg.clear();
}

} // namespace EndTracker

// ============================================================
// テストハーネス
// ============================================================
struct EndTestHarness {
    GameEndHandler handler;

    // Refs用の実体
    ShogiGameController gc;
    ShogiClock clock;
    PlayMode playMode = PlayMode::HumanVsHuman;
    MatchCoordinator::GameOverState gameOver;
    QStringList sfenHistory;

    EndTestHarness()
    {
        EndTracker::reset();

        GameEndHandler::Refs refs;
        refs.gc = &gc;
        refs.clock = &clock;
        refs.usi1Provider = []() -> Usi* { return nullptr; };
        refs.usi2Provider = []() -> Usi* { return nullptr; };
        refs.playMode = &playMode;
        refs.gameOver = &gameOver;
        refs.strategyProvider = nullptr;
        refs.sfenHistory = &sfenHistory;
        handler.setRefs(refs);

        GameEndHandler::Hooks hooks;
        hooks.disarmHumanTimerIfNeeded = []() {
            EndTracker::disarmHumanTimerCalled = true;
        };
        hooks.primaryEngine = []() -> Usi* { return nullptr; };
        hooks.turnEpochFor = [](MatchCoordinator::Player) -> qint64 { return -1; };
        hooks.appendKifuLine = [](const QString& line, const QString& elapsed) {
            ++EndTracker::appendKifuLineCount;
            EndTracker::lastAppendedLine = line;
            EndTracker::lastAppendedElapsed = elapsed;
        };
        hooks.showGameOverDialog = [](const QString& title, const QString& msg) {
            EndTracker::showGameOverDialogCalled = true;
            EndTracker::lastDialogTitle = title;
            EndTracker::lastDialogMsg = msg;
        };
        hooks.autoSaveKifuIfEnabled = []() {
            EndTracker::autoSaveKifuCalled = true;
        };
        handler.setHooks(hooks);
    }
};

// ============================================================
// テストクラス
// ============================================================
class Tst_GameEndHandler : public QObject
{
    Q_OBJECT

private slots:
    // === Section A: 投了 ===

    void handleResign_p1Turn_p1Loses();
    void handleResign_p2Turn_p2Loses();
    void handleResign_alreadyOver_ignored();

    // === Section B: エンジン投了 ===

    void handleEngineResign_engine1_p1Loses();
    void handleEngineResign_engine2_p2Loses();

    // === Section C: 中断 ===

    void handleBreakOff_setsGameOverState();
    void handleBreakOff_emitsGameEnded();
    void handleBreakOff_alreadyOver_ignored();
    void handleBreakOff_appendsKifuLine();

    // === Section D: 入玉宣言 ===

    void handleNyugyokuDeclaration_success_opponentLoses();
    void handleNyugyokuDeclaration_draw_jishogi();
    void handleNyugyokuDeclaration_fail_declarerLoses();
    void handleNyugyokuDeclaration_alreadyOver_ignored();

    // === Section E: 持将棋（最大手数） ===

    void handleMaxMovesJishogi_setsState();
    void handleMaxMovesJishogi_appendsKifuLine();
    void handleMaxMovesJishogi_alreadyOver_ignored();

    // === Section F: 千日手 ===

    void handleSennichite_setsState();
    void handleOuteSennichite_p1Loses();
    void handleOuteSennichite_p2Loses();

    // === Section G: 棋譜追記 ===

    void appendGameOverLineAndMark_resignation_p1();
    void appendGameOverLineAndMark_timeout_p2();
    void appendGameOverLineAndMark_doubleCallGuard();
    void appendGameOverLineAndMark_notOver_ignored();
    void appendBreakOffLineAndMark_p1Turn();

    // === Section H: 二重終局ガード ===

    void doubleEnd_resignThenBreakOff_secondIgnored();

    // === Section I: gameEndedシグナル ===

    void handleResign_emitsGameEndedSignal();
    void handleBreakOff_emitsGameEndedSignal();
};

// ============================================================
// Section A: 投了
// ============================================================

void Tst_GameEndHandler::handleResign_p1Turn_p1Loses()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    h.handler.handleResign();

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P1);
    QVERIFY(h.gameOver.lastLoserIsP1);
    QVERIFY(EndTracker::showGameOverDialogCalled);
    QVERIFY(EndTracker::autoSaveKifuCalled);
}

void Tst_GameEndHandler::handleResign_p2Turn_p2Loses()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player2);
    h.handler.handleResign();

    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P2);
    QVERIFY(!h.gameOver.lastLoserIsP1);
}

void Tst_GameEndHandler::handleResign_alreadyOver_ignored()
{
    EndTestHarness h;
    h.gameOver.isOver = true;
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);
    h.handler.handleResign();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!EndTracker::showGameOverDialogCalled);
}

// ============================================================
// Section B: エンジン投了
// ============================================================

void Tst_GameEndHandler::handleEngineResign_engine1_p1Loses()
{
    EndTestHarness h;
    h.playMode = PlayMode::EvenHumanVsEngine;
    h.handler.handleEngineResign(1);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P1);
}

void Tst_GameEndHandler::handleEngineResign_engine2_p2Loses()
{
    EndTestHarness h;
    h.playMode = PlayMode::EvenHumanVsEngine;
    h.handler.handleEngineResign(2);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P2);
}

// ============================================================
// Section C: 中断
// ============================================================

void Tst_GameEndHandler::handleBreakOff_setsGameOverState()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    h.handler.handleBreakOff();

    QVERIFY(h.gameOver.isOver);
    QVERIFY(h.gameOver.hasLast);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::BreakOff);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P1);
}

void Tst_GameEndHandler::handleBreakOff_emitsGameEnded()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player2);
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    h.handler.handleBreakOff();

    QCOMPARE(spy.count(), 1);
    auto info = spy.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(info.cause, MatchCoordinator::Cause::BreakOff);
    QCOMPARE(info.loser, MatchCoordinator::P2);
}

void Tst_GameEndHandler::handleBreakOff_alreadyOver_ignored()
{
    EndTestHarness h;
    h.gameOver.isOver = true;
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    h.handler.handleBreakOff();

    QCOMPARE(spy.count(), 0);
}

void Tst_GameEndHandler::handleBreakOff_appendsKifuLine()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    h.handler.handleBreakOff();

    QVERIFY(EndTracker::appendKifuLineCount > 0);
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("中断")));
    QVERIFY(h.gameOver.moveAppended);
}

// ============================================================
// Section D: 入玉宣言
// ============================================================

void Tst_GameEndHandler::handleNyugyokuDeclaration_success_opponentLoses()
{
    EndTestHarness h;
    h.handler.handleNyugyokuDeclaration(MatchCoordinator::P1, true, false);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::NyugyokuWin);
    // P1 declares and wins → P2 loses
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P2);
    QVERIFY(!h.gameOver.lastLoserIsP1);
}

void Tst_GameEndHandler::handleNyugyokuDeclaration_draw_jishogi()
{
    EndTestHarness h;
    h.handler.handleNyugyokuDeclaration(MatchCoordinator::P1, true, true);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Jishogi);
    // isDraw: declarer is the "loser" in the info struct
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P1);
}

void Tst_GameEndHandler::handleNyugyokuDeclaration_fail_declarerLoses()
{
    EndTestHarness h;
    h.handler.handleNyugyokuDeclaration(MatchCoordinator::P2, false, false);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::IllegalMove);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P2);
}

void Tst_GameEndHandler::handleNyugyokuDeclaration_alreadyOver_ignored()
{
    EndTestHarness h;
    h.gameOver.isOver = true;
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);
    h.handler.handleNyugyokuDeclaration(MatchCoordinator::P1, true, false);

    QCOMPARE(spy.count(), 0);
}

// ============================================================
// Section E: 持将棋（最大手数）
// ============================================================

void Tst_GameEndHandler::handleMaxMovesJishogi_setsState()
{
    EndTestHarness h;
    h.handler.handleMaxMovesJishogi();

    QVERIFY(h.gameOver.isOver);
    QVERIFY(h.gameOver.hasLast);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Jishogi);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P1);
    QVERIFY(EndTracker::showGameOverDialogCalled);
    QVERIFY(EndTracker::autoSaveKifuCalled);
}

void Tst_GameEndHandler::handleMaxMovesJishogi_appendsKifuLine()
{
    EndTestHarness h;
    h.handler.handleMaxMovesJishogi();

    // appendGameOverLineAndMark is called from handleMaxMovesJishogi
    QVERIFY(EndTracker::appendKifuLineCount > 0);
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("持将棋")));
    QVERIFY(h.gameOver.moveAppended);
}

void Tst_GameEndHandler::handleMaxMovesJishogi_alreadyOver_ignored()
{
    EndTestHarness h;
    h.gameOver.isOver = true;
    h.handler.handleMaxMovesJishogi();

    QVERIFY(!EndTracker::showGameOverDialogCalled);
    QCOMPARE(EndTracker::appendKifuLineCount, 0);
}

// ============================================================
// Section F: 千日手
// ============================================================

void Tst_GameEndHandler::handleSennichite_setsState()
{
    EndTestHarness h;
    h.handler.handleSennichite();

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Sennichite);
    QVERIFY(EndTracker::disarmHumanTimerCalled);
    QVERIFY(EndTracker::showGameOverDialogCalled);
}

void Tst_GameEndHandler::handleOuteSennichite_p1Loses()
{
    EndTestHarness h;
    h.handler.handleOuteSennichite(true);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::OuteSennichite);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P1);
    QVERIFY(h.gameOver.lastLoserIsP1);
}

void Tst_GameEndHandler::handleOuteSennichite_p2Loses()
{
    EndTestHarness h;
    h.handler.handleOuteSennichite(false);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::OuteSennichite);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P2);
    QVERIFY(!h.gameOver.lastLoserIsP1);
}

// ============================================================
// Section G: 棋譜追記
// ============================================================

void Tst_GameEndHandler::appendGameOverLineAndMark_resignation_p1()
{
    EndTestHarness h;
    h.gameOver.isOver = true;  // must be over for append to work
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         MatchCoordinator::P1);

    QVERIFY(EndTracker::appendKifuLineCount > 0);
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("投了")));
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("▲")));
    QVERIFY(h.gameOver.moveAppended);
}

void Tst_GameEndHandler::appendGameOverLineAndMark_timeout_p2()
{
    EndTestHarness h;
    h.gameOver.isOver = true;
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Timeout,
                                         MatchCoordinator::P2);

    QVERIFY(EndTracker::appendKifuLineCount > 0);
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("時間切れ")));
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("△")));
}

void Tst_GameEndHandler::appendGameOverLineAndMark_doubleCallGuard()
{
    EndTestHarness h;
    h.gameOver.isOver = true;

    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         MatchCoordinator::P1);
    int firstCount = EndTracker::appendKifuLineCount;

    // Simulate that moveAppended was marked
    h.gameOver.moveAppended = true;

    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         MatchCoordinator::P1);
    // Second call should be no-op
    QCOMPARE(EndTracker::appendKifuLineCount, firstCount);
}

void Tst_GameEndHandler::appendGameOverLineAndMark_notOver_ignored()
{
    EndTestHarness h;
    h.gameOver.isOver = false;
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         MatchCoordinator::P1);
    QCOMPARE(EndTracker::appendKifuLineCount, 0);
}

void Tst_GameEndHandler::appendBreakOffLineAndMark_p1Turn()
{
    EndTestHarness h;
    h.gameOver.isOver = true;
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    h.handler.appendBreakOffLineAndMark();

    QVERIFY(EndTracker::appendKifuLineCount > 0);
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("▲中断")));
    QVERIFY(h.gameOver.moveAppended);
}

// ============================================================
// Section H: 二重終局ガード
// ============================================================

void Tst_GameEndHandler::doubleEnd_resignThenBreakOff_secondIgnored()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player1);

    // First: resign → sets gameOver.isOver = true
    h.handler.handleResign();
    QVERIFY(h.gameOver.isOver);

    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    // Second: breakoff (should be ignored because isOver is already true)
    h.handler.handleBreakOff();
    QCOMPARE(spy.count(), 0);
}

// ============================================================
// Section I: gameEndedシグナル
// ============================================================

void Tst_GameEndHandler::handleResign_emitsGameEndedSignal()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player1);
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    h.handler.handleResign();

    QCOMPARE(spy.count(), 1);
    auto info = spy.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(info.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(info.loser, MatchCoordinator::P1);
}

void Tst_GameEndHandler::handleBreakOff_emitsGameEndedSignal()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(ShogiGameController::Player2);
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    h.handler.handleBreakOff();

    QCOMPARE(spy.count(), 1);
    auto info = spy.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(info.cause, MatchCoordinator::Cause::BreakOff);
}

// ============================================================
QTEST_MAIN(Tst_GameEndHandler)
#include "tst_game_end_handler.moc"
