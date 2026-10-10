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
#include "shogiboard.h"
#include "nyugyokujudgement.h"

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
QStringList events;
bool handicapNames = false;

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
    events.clear();
    handicapNames = false;
}

} // namespace EndTracker

extern ShogiBoard* g_stubGameBoard;
extern NyugyokuJudgement::Result g_stubNyugyokuResult;
extern bool g_stubNyugyokuHandicapNames;

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
            EndTracker::events.append(QStringLiteral("dialog"));
            EndTracker::showGameOverDialogCalled = true;
            EndTracker::lastDialogTitle = title;
            EndTracker::lastDialogMsg = msg;
        };
        hooks.usesHandicapNames = []() { return EndTracker::handicapNames; };
        hooks.autoSaveKifuIfEnabled = []() {
            EndTracker::events.append(QStringLiteral("save"));
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

public:
    void recordEndProcessed() { EndTracker::events.append(QStringLiteral("processed")); }
    void recordEnd() { EndTracker::events.append(QStringLiteral("ended")); }
    void recordAppendRequest() { EndTracker::events.append(QStringLiteral("append")); }

private slots:
    void timeoutKeepsCauseAndLoser_data()
    {
        QTest::addColumn<int>("loser");
        QTest::newRow("black") << 1;
        QTest::newRow("white") << 2;
    }

    void timeoutKeepsCauseAndLoser()
    {
        QFETCH(int, loser);
        EndTestHarness h;
        const auto side = loser == 1 ? MatchCoordinator::P1 : MatchCoordinator::P2;
        QSignalSpy ended(&h.handler, &GameEndHandler::gameEnded);
        QSignalSpy completed(&h.handler, &GameEndHandler::gameEndProcessed);
        h.handler.handleTimeout(side);
        h.handler.handleTimeout(side);
        h.handler.handleEngineResign(loser);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Timeout);
        QCOMPARE(h.gameOver.lastInfo.loser, side);
        QVERIFY(EndTracker::lastDialogMsg.contains(QStringLiteral("時間切れ")));
        QVERIFY(EndTracker::autoSaveKifuCalled);
    }

    void maxMovesNotifiesAfterAppendingAndBeforeCompletion()
    {
        EndTestHarness h;
        connect(&h.handler, &GameEndHandler::requestAppendGameOverMove,
                this, &Tst_GameEndHandler::recordAppendRequest);
        connect(&h.handler, &GameEndHandler::gameEnded,
                this, &Tst_GameEndHandler::recordEnd);
        connect(&h.handler, &GameEndHandler::gameEndProcessed,
                this, &Tst_GameEndHandler::recordEndProcessed);
        h.handler.handleMaxMovesJishogi();
        h.handler.handleMaxMovesJishogi();
        QCOMPARE(EndTracker::events, QStringList({QStringLiteral("append"), QStringLiteral("ended"),
                 QStringLiteral("dialog"), QStringLiteral("save"), QStringLiteral("processed")}));
    }
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
    void handleEngineWin_showsGameOverMessage();
    void handleEngineWin_judgesDeclaration_data();
    void handleEngineWin_judgesDeclaration();
    void handleEngineWin_passesHandicapNames();
    void appendGameOverLineAndMark_nyugyokuWin_usesDeclarerTime();

    // === Section E: 持将棋（最大手数） ===

    void handleMaxMovesJishogi_setsState();
    void handleMaxMovesJishogi_appendsKifuLine();
    void handleMaxMovesJishogi_alreadyOver_ignored();
    void drawLine_usesSideToMove_data();
    void drawLine_usesSideToMove();
    void outeSennichiteLine_recordsFoulBySideToMove_data();
    void outeSennichiteLine_recordsFoulBySideToMove();

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

    // === Section J: 結果表示の呼び名 ===

    void resultMessage_followsHandicapNames_data();
    void resultMessage_followsHandicapNames();

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

void Tst_GameEndHandler::handleEngineWin_showsGameOverMessage()
{
    // エンジンの入玉宣言（bestmove win）は宣言ダイアログを通らないため、対局終了を知らせる
    EndTestHarness h;
    h.handler.handleEngineWin(2);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::NyugyokuWin);
    QCOMPARE(h.gameOver.lastInfo.loser, MatchCoordinator::P1);
    QVERIFY(EndTracker::showGameOverDialogCalled);
    QVERIFY(EndTracker::lastDialogMsg.contains(QStringLiteral("後手の入玉宣言")));
    QCOMPARE(EndTracker::events.count(QStringLiteral("save")), 1);
}

void Tst_GameEndHandler::handleEngineWin_judgesDeclaration_data()
{
    QTest::addColumn<bool>("success");
    QTest::addColumn<bool>("isDraw");
    QTest::addColumn<int>("cause");
    QTest::addColumn<int>("loser");
    // 後手のエンジン（idx=2）が宣言。失敗なら後手の反則負け、24点法の24〜30点は持将棋
    QTest::newRow("win") << true << false << int(MatchCoordinator::Cause::NyugyokuWin) << int(MatchCoordinator::P1);
    QTest::newRow("draw") << true << true << int(MatchCoordinator::Cause::Jishogi) << int(MatchCoordinator::P2);
    QTest::newRow("fail") << false << false << int(MatchCoordinator::Cause::IllegalMove) << int(MatchCoordinator::P2);
}

void Tst_GameEndHandler::handleEngineWin_judgesDeclaration()
{
    QFETCH(bool, success);
    QFETCH(bool, isDraw);
    QFETCH(int, cause);
    QFETCH(int, loser);
    EndTestHarness h;
    ShogiBoard board;
    g_stubGameBoard = &board;
    g_stubNyugyokuResult = {};
    g_stubNyugyokuResult.success = success;
    g_stubNyugyokuResult.isDraw = isDraw;
    g_stubNyugyokuResult.message = QStringLiteral("判定の説明");
    h.handler.handleEngineWin(2);
    g_stubGameBoard = nullptr;

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(int(h.gameOver.lastInfo.cause), cause);
    QCOMPARE(int(h.gameOver.lastInfo.loser), loser);
    QCOMPARE(EndTracker::lastDialogTitle, QStringLiteral("入玉宣言結果"));
    QCOMPARE(EndTracker::lastDialogMsg, QStringLiteral("判定の説明"));
}

void Tst_GameEndHandler::handleEngineWin_passesHandicapNames()
{
    // 駒落ちでは入玉宣言結果の説明も宣言側を下手・上手と呼ぶ
    EndTestHarness h;
    EndTracker::handicapNames = true;
    ShogiBoard board;
    g_stubGameBoard = &board;
    g_stubNyugyokuResult = {};
    g_stubNyugyokuResult.success = true;
    g_stubNyugyokuHandicapNames = false;
    h.handler.handleEngineWin(2);
    g_stubGameBoard = nullptr;

    QVERIFY(h.gameOver.isOver);
    QVERIFY(g_stubNyugyokuHandicapNames);
}

void Tst_GameEndHandler::appendGameOverLineAndMark_nyugyokuWin_usesDeclarerTime()
{
    // 入玉勝ちの行は宣言した勝者の手番の行なので、勝者の消費時間を記録する
    EndTestHarness h;
    h.gameOver.isOver = true;
    GameEndHandler::Hooks hooks;
    hooks.turnEpochFor = [](MatchCoordinator::Player p) -> qint64 {
        EndTracker::events.append(p == MatchCoordinator::P1 ? QStringLiteral("epoch-P1")
                                                            : QStringLiteral("epoch-P2"));
        return -1;
    };
    hooks.appendKifuLine = [](const QString& line, const QString&) {
        EndTracker::lastAppendedLine = line;
    };
    h.handler.setHooks(hooks);
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::NyugyokuWin, MatchCoordinator::P2);

    QCOMPARE(EndTracker::lastAppendedLine, QStringLiteral("▲入玉勝ち"));
    QCOMPARE(EndTracker::events, QStringList{QStringLiteral("epoch-P1")});
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

void Tst_GameEndHandler::drawLine_usesSideToMove_data()
{
    QTest::addColumn<bool>("sennichite");
    QTest::addColumn<int>("sideToMove");
    QTest::addColumn<QString>("expectedLine");
    QTest::addColumn<QString>("expectedEpoch");

    QTest::newRow("jishogi-white-to-move") << false << 2 << QStringLiteral("△持将棋") << QStringLiteral("epoch-P2");
    QTest::newRow("jishogi-black-to-move") << false << 1 << QStringLiteral("▲持将棋") << QStringLiteral("epoch-P1");
    QTest::newRow("sennichite-white-to-move") << true << 2 << QStringLiteral("△千日手") << QStringLiteral("epoch-P2");
    QTest::newRow("sennichite-black-to-move") << true << 1 << QStringLiteral("▲千日手") << QStringLiteral("epoch-P1");
}

void Tst_GameEndHandler::drawLine_usesSideToMove()
{
    // 千日手・持将棋の終局行は、終局した局面の手番側の印と消費時間で記録する
    // （棋譜ファイルを読み込んだときに付く印と同じ）
    QFETCH(bool, sennichite);
    QFETCH(int, sideToMove);
    QFETCH(QString, expectedLine);
    QFETCH(QString, expectedEpoch);

    EndTestHarness h;
    h.gc.setCurrentPlayer(sideToMove == 1 ? ShogiGameController::Player1 : ShogiGameController::Player2);
    GameEndHandler::Hooks hooks;
    hooks.turnEpochFor = [](MatchCoordinator::Player p) -> qint64 {
        EndTracker::events.append(p == MatchCoordinator::P1 ? QStringLiteral("epoch-P1")
                                                            : QStringLiteral("epoch-P2"));
        return -1;
    };
    hooks.appendKifuLine = [](const QString& line, const QString&) {
        EndTracker::lastAppendedLine = line;
    };
    h.handler.setHooks(hooks);

    if (sennichite) h.handler.handleSennichite();
    else            h.handler.handleMaxMovesJishogi();

    QCOMPARE(EndTracker::lastAppendedLine, expectedLine);
    QCOMPARE(EndTracker::events, QStringList{expectedEpoch});
}

void Tst_GameEndHandler::outeSennichiteLine_recordsFoulBySideToMove_data()
{
    QTest::addColumn<bool>("p1Loses");
    QTest::addColumn<int>("sideToMove");
    QTest::addColumn<QString>("expectedLine");
    QTest::addColumn<QString>("expectedEpoch");

    // 王手をかけた手で千日手になると、手番は勝った側（反則勝ち）
    QTest::newRow("black-checks-white-to-move") << true << 2 << QStringLiteral("△反則勝ち") << QStringLiteral("epoch-P2");
    QTest::newRow("white-checks-black-to-move") << false << 1 << QStringLiteral("▲反則勝ち") << QStringLiteral("epoch-P1");
    // 王手を逃げた手で千日手になると、手番は王手を続けた側（反則負け）
    QTest::newRow("black-checks-black-to-move") << true << 1 << QStringLiteral("▲反則負け") << QStringLiteral("epoch-P1");
    QTest::newRow("white-checks-white-to-move") << false << 2 << QStringLiteral("△反則負け") << QStringLiteral("epoch-P2");
}

void Tst_GameEndHandler::outeSennichiteLine_recordsFoulBySideToMove()
{
    // 連続王手の千日手は棋譜形式に終局語が無いので、手番側の「反則勝ち／反則負け」で記録し、
    // 保存しても王手を続けた側の負けが残るようにする
    QFETCH(bool, p1Loses);
    QFETCH(int, sideToMove);
    QFETCH(QString, expectedLine);
    QFETCH(QString, expectedEpoch);

    EndTestHarness h;
    h.gc.setCurrentPlayer(sideToMove == 1 ? ShogiGameController::Player1 : ShogiGameController::Player2);
    GameEndHandler::Hooks hooks;
    hooks.turnEpochFor = [](MatchCoordinator::Player p) -> qint64 {
        EndTracker::events.append(p == MatchCoordinator::P1 ? QStringLiteral("epoch-P1")
                                                            : QStringLiteral("epoch-P2"));
        return -1;
    };
    hooks.appendKifuLine = [](const QString& line, const QString&) {
        EndTracker::lastAppendedLine = line;
    };
    h.handler.setHooks(hooks);

    h.handler.handleOuteSennichite(p1Loses);

    QCOMPARE(EndTracker::lastAppendedLine, expectedLine);
    QCOMPARE(EndTracker::events, QStringList{expectedEpoch});
    QCOMPARE(h.gameOver.lastInfo.loser, p1Loses ? MatchCoordinator::P1 : MatchCoordinator::P2);
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
// ============================================================
// Section J: 結果表示の呼び名
// ============================================================

void Tst_GameEndHandler::resultMessage_followsHandicapNames_data()
{
    QTest::addColumn<bool>("handicapNames");
    QTest::addColumn<int>("cause");
    QTest::addColumn<QString>("message");
    // 駒落ちで対局情報の見出しが下手・上手なら、対局終了のダイアログも下手（先手側）・上手（後手側）と呼ぶ
    QTest::newRow("timeout") << false << int(MatchCoordinator::Cause::Timeout)
                             << QStringLiteral("後手の時間切れ。先手の勝ちです。");
    QTest::newRow("timeout-handicap") << true << int(MatchCoordinator::Cause::Timeout)
                                      << QStringLiteral("上手の時間切れ。下手の勝ちです。");
    QTest::newRow("resign-handicap") << true << int(MatchCoordinator::Cause::Resignation)
                                     << QStringLiteral("上手の投了。下手の勝ちです。");
    QTest::newRow("nyugyoku-handicap") << true << int(MatchCoordinator::Cause::NyugyokuWin)
                                       << QStringLiteral("下手の入玉宣言。下手の勝ちです。");
}

void Tst_GameEndHandler::resultMessage_followsHandicapNames()
{
    QFETCH(bool, handicapNames);
    QFETCH(int, cause);
    QFETCH(QString, message);
    EndTestHarness h;
    EndTracker::handicapNames = handicapNames;
    switch (static_cast<MatchCoordinator::Cause>(cause)) {
    case MatchCoordinator::Cause::Timeout:
        h.handler.handleTimeout(MatchCoordinator::P2);
        break;
    case MatchCoordinator::Cause::Resignation:
        h.gc.setCurrentPlayer(ShogiGameController::Player2);
        h.handler.handleResign();
        break;
    default:
        // 盤面のない入玉宣言は判定を省いて宣言勝ちとし、対局終了のダイアログで知らせる
        h.handler.handleEngineWin(1);
        break;
    }

    QCOMPARE(EndTracker::lastDialogTitle, QStringLiteral("対局終了"));
    QCOMPARE(EndTracker::lastDialogMsg, message);
}

QTEST_MAIN(Tst_GameEndHandler)
#include "tst_game_end_handler.moc"
