/// @file tst_game_end_handler.cpp
/// @brief GameEndHandler 単体テスト
///
/// 終局処理（投了・中断・千日手・持将棋・入玉宣言）が
/// 正しい GameEndInfo を生成し、正しいフックを呼び出すことを検証する。

#include <QTest>
#include <QSettings>
#include "settingscommon.h"
#include "sfenutils.h"
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

namespace {
// 先手玉は敵陣にあり、敵陣15枚と持ち駒で28点。
const QString blackWin = QStringLiteral(
    "5+B+RGS/6GKS/+P+P+P+P+P+P+P+P+P/9/p1p1p1p1p/1s1g1g1s1/2n3n1n/1r2k4/2b6 b NL3P3lp 1");
void setRule(int rule)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("GameSettings/jishogiRule"), rule);
    settings.sync();
}


// 終局行の消費時間がどちらの側のものかを見分けるため、先手・後手で違う考慮時間を時計に入れる
const QString kBlackElapsed = QStringLiteral("01:01/00:01:01");  // 先手の考慮61秒
const QString kWhiteElapsed = QStringLiteral("02:02/00:02:02");  // 後手の考慮122秒

void setDistinctConsideration(ShogiClock& clock)
{
    clock.setConsiderationTimeMs(Turn::Black, 61000);
    clock.setConsiderationTimeMs(Turn::White, 122000);
}

} // namespace


// ============================================================
// テストハーネス
// ============================================================
struct EndTestHarness {
    GameEndHandler handler;

    // Refs用の実体
    ShogiGameController gc;
    ShogiClock clock;
    MatchSessionState session;
    const MatchGameOverState& gameOver = session.gameOver();
    QStringList sfenHistory;

    explicit EndTestHarness(bool withBoard = true)
    {
        EndTracker::reset();
        QString initial = SfenUtils::hirateSfen();
        if (withBoard) gc.newGame(initial);

        GameEndHandler::Refs refs;
        refs.gc = &gc;
        refs.clock = &clock;
        refs.usi1Provider = []() -> Usi* { return nullptr; };
        refs.usi2Provider = []() -> Usi* { return nullptr; };
        session.setPlayMode(PlayMode::HumanVsHuman);
        refs.session = &session;
        refs.strategyProvider = nullptr;
        refs.sfenHistory = &sfenHistory;
        handler.setRefs(refs);

        GameEndHandler::Hooks hooks;
        hooks.disarmHumanTimerIfNeeded = []() {
            EndTracker::disarmHumanTimerCalled = true;
        };
        hooks.primaryEngine = []() -> Usi* { return nullptr; };
        hooks.appendRecord = [](const RecordEntry& entry) {
        const QString line = entry.displayText;
        const QString elapsed = entry.time.toText();
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
        const auto side = loser == 1 ? Turn::Black : Turn::White;
        QSignalSpy ended(&h.handler, &GameEndHandler::gameEnded);
        QSignalSpy completed(&h.handler, &GameEndHandler::gameEndProcessed);
        h.handler.handleTimeout(side);
        h.handler.handleTimeout(side);
        h.handler.handleEngineResign(side);
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
    void handleOuteSennichite_blackLoses();
    void handleOuteSennichite_whiteLoses();

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
    h.gc.setCurrentPlayer(Turn::Black);
    h.handler.handleResign();

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::Black);
    QVERIFY(EndTracker::showGameOverDialogCalled);
    QVERIFY(EndTracker::autoSaveKifuCalled);
}

void Tst_GameEndHandler::handleResign_p2Turn_p2Loses()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(Turn::White);
    h.handler.handleResign();

    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::White);
}

void Tst_GameEndHandler::handleResign_alreadyOver_ignored()
{
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});
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
    h.session.setPlayMode(PlayMode::EvenHumanVsEngine);
    h.handler.handleEngineResign(Turn::Black);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::Black);
}

void Tst_GameEndHandler::handleEngineResign_engine2_p2Loses()
{
    EndTestHarness h;
    h.session.setPlayMode(PlayMode::EvenHumanVsEngine);
    h.handler.handleEngineResign(Turn::White);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::White);
}

// ============================================================
// Section C: 中断
// ============================================================

void Tst_GameEndHandler::handleBreakOff_setsGameOverState()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(Turn::Black);
    h.handler.handleBreakOff();

    QVERIFY(h.gameOver.isOver);
    QVERIFY(h.gameOver.hasLast);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::BreakOff);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::Black);
}

void Tst_GameEndHandler::handleBreakOff_emitsGameEnded()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(Turn::White);
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    h.handler.handleBreakOff();

    QCOMPARE(spy.count(), 1);
    auto info = spy.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(info.cause, MatchCoordinator::Cause::BreakOff);
    QCOMPARE(info.loser, Turn::White);
}

void Tst_GameEndHandler::handleBreakOff_alreadyOver_ignored()
{
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    h.handler.handleBreakOff();

    QCOMPARE(spy.count(), 0);
}

void Tst_GameEndHandler::handleBreakOff_appendsKifuLine()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(Turn::Black);
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
    h.handler.handleNyugyokuDeclaration(Turn::Black, true, false);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::NyugyokuWin);
    // P1 declares and wins → P2 loses
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::White);
}

void Tst_GameEndHandler::handleNyugyokuDeclaration_draw_jishogi()
{
    EndTestHarness h;
    h.handler.handleNyugyokuDeclaration(Turn::Black, true, true);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::Jishogi);
    // isDraw: declarer is the "loser" in the info struct
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::Black);
}

void Tst_GameEndHandler::handleNyugyokuDeclaration_fail_declarerLoses()
{
    EndTestHarness h;
    h.handler.handleNyugyokuDeclaration(Turn::White, false, false);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::IllegalMove);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::White);
}

void Tst_GameEndHandler::handleNyugyokuDeclaration_alreadyOver_ignored()
{
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);
    h.handler.handleNyugyokuDeclaration(Turn::Black, true, false);

    QCOMPARE(spy.count(), 0);
}

void Tst_GameEndHandler::handleEngineWin_showsGameOverMessage()
{
    // エンジンの入玉宣言（bestmove win）は宣言ダイアログを通らないため、対局終了を知らせる
    EndTestHarness h;
    h.gc.board()->setSfen(blackWin);
    setRule(NyugyokuJudgement::Rule27);
    h.handler.handleEngineWin(Turn::Black);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::NyugyokuWin);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::White);
    QVERIFY(EndTracker::showGameOverDialogCalled);
    QVERIFY(EndTracker::lastDialogMsg.contains(QStringLiteral("先手の入玉宣言")));
    QCOMPARE(EndTracker::events.count(QStringLiteral("save")), 1);
}

void Tst_GameEndHandler::handleEngineWin_judgesDeclaration_data()
{
    QTest::addColumn<bool>("success");
    QTest::addColumn<bool>("isDraw");
    QTest::addColumn<int>("cause");
    QTest::addColumn<int>("loser");
    // 後手のエンジンが宣言。失敗なら後手の反則負け、24点法の24〜30点は持将棋
    QTest::newRow("win") << true << false << int(MatchCoordinator::Cause::NyugyokuWin) << int(Turn::Black);
    QTest::newRow("draw") << true << true << int(MatchCoordinator::Cause::Jishogi) << int(Turn::White);
    QTest::newRow("fail") << false << false << int(MatchCoordinator::Cause::IllegalMove) << int(Turn::White);
}

void Tst_GameEndHandler::handleEngineWin_judgesDeclaration()
{
    QFETCH(bool, success);
    QFETCH(bool, isDraw);
    QFETCH(int, cause);
    QFETCH(int, loser);
    EndTestHarness h;
    h.gc.board()->setSfen(success ? blackWin : SfenUtils::hirateSfen());
    setRule(isDraw ? NyugyokuJudgement::Rule24 : NyugyokuJudgement::Rule27);
    h.handler.handleEngineWin(Turn::Black);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(int(h.gameOver.lastInfo.cause), cause);
    QCOMPARE(int(h.gameOver.lastInfo.loser), loser == int(Turn::Black) ? int(Turn::White) : int(Turn::Black));
    QCOMPARE(EndTracker::lastDialogTitle, QStringLiteral("入玉宣言結果"));
    QVERIFY(EndTracker::lastDialogMsg.contains(QStringLiteral("先手の入玉宣言")));
}

void Tst_GameEndHandler::handleEngineWin_passesHandicapNames()
{
    // 駒落ちでは入玉宣言結果の説明も宣言側を下手・上手と呼ぶ
    EndTestHarness h;
    EndTracker::handicapNames = true;
    h.gc.board()->setSfen(blackWin);
    setRule(NyugyokuJudgement::Rule27);
    h.handler.handleEngineWin(Turn::Black);
    QVERIFY(h.gameOver.isOver);
    QVERIFY(EndTracker::lastDialogMsg.contains(QStringLiteral("下手の入玉宣言")));
}

void Tst_GameEndHandler::appendGameOverLineAndMark_nyugyokuWin_usesDeclarerTime()
{
    // 入玉勝ちの行は宣言した勝者の手番の行なので、勝者の消費時間を記録する
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});
    setDistinctConsideration(h.clock);
    GameEndHandler::Hooks hooks;
    hooks.appendRecord = [](const RecordEntry& entry) {
        const QString line = entry.displayText;
        const QString elapsed = entry.time.toText();
        EndTracker::lastAppendedLine = line;
        EndTracker::lastAppendedElapsed = elapsed;
    };
    h.handler.setHooks(hooks);
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::NyugyokuWin, Turn::White);

    QCOMPARE(EndTracker::lastAppendedLine, QStringLiteral("▲入玉勝ち"));
    QCOMPARE(EndTracker::lastAppendedElapsed, kBlackElapsed);
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
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::Black);
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
    h.session.finish({MatchCause::Resignation, Turn::Black});
    h.handler.handleMaxMovesJishogi();

    QVERIFY(!EndTracker::showGameOverDialogCalled);
    QCOMPARE(EndTracker::appendKifuLineCount, 0);
}

void Tst_GameEndHandler::drawLine_usesSideToMove_data()
{
    QTest::addColumn<bool>("sennichite");
    QTest::addColumn<int>("sideToMove");
    QTest::addColumn<QString>("expectedLine");
    QTest::addColumn<QString>("expectedElapsed");

    QTest::newRow("jishogi-white-to-move") << false << 2 << QStringLiteral("△持将棋") << kWhiteElapsed;
    QTest::newRow("jishogi-black-to-move") << false << 1 << QStringLiteral("▲持将棋") << kBlackElapsed;
    QTest::newRow("sennichite-white-to-move") << true << 2 << QStringLiteral("△千日手") << kWhiteElapsed;
    QTest::newRow("sennichite-black-to-move") << true << 1 << QStringLiteral("▲千日手") << kBlackElapsed;
}

void Tst_GameEndHandler::drawLine_usesSideToMove()
{
    // 千日手・持将棋の終局行は、終局した局面の手番側の印と消費時間で記録する
    // （棋譜ファイルを読み込んだときに付く印と同じ）
    QFETCH(bool, sennichite);
    QFETCH(int, sideToMove);
    QFETCH(QString, expectedLine);
    QFETCH(QString, expectedElapsed);

    EndTestHarness h;
    h.gc.setCurrentPlayer(sideToMove == 1 ? Turn::Black : Turn::White);
    setDistinctConsideration(h.clock);
    GameEndHandler::Hooks hooks;
    hooks.appendRecord = [](const RecordEntry& entry) {
        const QString line = entry.displayText;
        const QString elapsed = entry.time.toText();
        EndTracker::lastAppendedLine = line;
        EndTracker::lastAppendedElapsed = elapsed;
    };
    h.handler.setHooks(hooks);

    if (sennichite) h.handler.handleSennichite();
    else            h.handler.handleMaxMovesJishogi();

    QCOMPARE(EndTracker::lastAppendedLine, expectedLine);
    QCOMPARE(EndTracker::lastAppendedElapsed, expectedElapsed);
}

void Tst_GameEndHandler::outeSennichiteLine_recordsFoulBySideToMove_data()
{
    QTest::addColumn<bool>("blackLoses");
    QTest::addColumn<int>("sideToMove");
    QTest::addColumn<QString>("expectedLine");
    QTest::addColumn<QString>("expectedElapsed");

    // 王手をかけた手で千日手になると、手番は勝った側（反則勝ち）
    QTest::newRow("black-checks-white-to-move") << true << 2 << QStringLiteral("△反則勝ち") << kWhiteElapsed;
    QTest::newRow("white-checks-black-to-move") << false << 1 << QStringLiteral("▲反則勝ち") << kBlackElapsed;
    // 王手を逃げた手で千日手になると、手番は王手を続けた側（反則負け）
    QTest::newRow("black-checks-black-to-move") << true << 1 << QStringLiteral("▲反則負け") << kBlackElapsed;
    QTest::newRow("white-checks-white-to-move") << false << 2 << QStringLiteral("△反則負け") << kWhiteElapsed;
}

void Tst_GameEndHandler::outeSennichiteLine_recordsFoulBySideToMove()
{
    // 連続王手の千日手は棋譜形式に終局語が無いので、手番側の「反則勝ち／反則負け」で記録し、
    // 保存しても王手を続けた側の負けが残るようにする
    QFETCH(bool, blackLoses);
    QFETCH(int, sideToMove);
    QFETCH(QString, expectedLine);
    QFETCH(QString, expectedElapsed);

    EndTestHarness h;
    h.gc.setCurrentPlayer(sideToMove == 1 ? Turn::Black : Turn::White);
    setDistinctConsideration(h.clock);
    GameEndHandler::Hooks hooks;
    hooks.appendRecord = [](const RecordEntry& entry) {
        const QString line = entry.displayText;
        const QString elapsed = entry.time.toText();
        EndTracker::lastAppendedLine = line;
        EndTracker::lastAppendedElapsed = elapsed;
    };
    h.handler.setHooks(hooks);

    h.handler.handleOuteSennichite(blackLoses ? Turn::Black : Turn::White);

    QCOMPARE(EndTracker::lastAppendedLine, expectedLine);
    QCOMPARE(EndTracker::lastAppendedElapsed, expectedElapsed);
    QCOMPARE(h.gameOver.lastInfo.loser, blackLoses ? Turn::Black : Turn::White);
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

void Tst_GameEndHandler::handleOuteSennichite_blackLoses()
{
    EndTestHarness h;
    h.handler.handleOuteSennichite(Turn::Black);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::OuteSennichite);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::Black);
}

void Tst_GameEndHandler::handleOuteSennichite_whiteLoses()
{
    EndTestHarness h;
    h.handler.handleOuteSennichite(Turn::White);

    QVERIFY(h.gameOver.isOver);
    QCOMPARE(h.gameOver.lastInfo.cause, MatchCoordinator::Cause::OuteSennichite);
    QCOMPARE(h.gameOver.lastInfo.loser, Turn::White);
}

// ============================================================
// Section G: 棋譜追記
// ============================================================

void Tst_GameEndHandler::appendGameOverLineAndMark_resignation_p1()
{
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});  // must be over for append to work
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         Turn::Black);

    QVERIFY(EndTracker::appendKifuLineCount > 0);
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("投了")));
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("▲")));
    QVERIFY(h.gameOver.moveAppended);
}

void Tst_GameEndHandler::appendGameOverLineAndMark_timeout_p2()
{
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Timeout,
                                         Turn::White);

    QVERIFY(EndTracker::appendKifuLineCount > 0);
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("時間切れ")));
    QVERIFY(EndTracker::lastAppendedLine.contains(QStringLiteral("△")));
}

void Tst_GameEndHandler::appendGameOverLineAndMark_doubleCallGuard()
{
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});

    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         Turn::Black);
    int firstCount = EndTracker::appendKifuLineCount;

    // Simulate that moveAppended was marked
    h.session.markTerminalRecorded();

    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         Turn::Black);
    // Second call should be no-op
    QCOMPARE(EndTracker::appendKifuLineCount, firstCount);
}

void Tst_GameEndHandler::appendGameOverLineAndMark_notOver_ignored()
{
    EndTestHarness h;
    h.session.clearGameOver();
    h.handler.appendGameOverLineAndMark(MatchCoordinator::Cause::Resignation,
                                         Turn::Black);
    QCOMPARE(EndTracker::appendKifuLineCount, 0);
}

void Tst_GameEndHandler::appendBreakOffLineAndMark_p1Turn()
{
    EndTestHarness h;
    h.session.finish({MatchCause::Resignation, Turn::Black});
    h.gc.setCurrentPlayer(Turn::Black);
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
    h.gc.setCurrentPlayer(Turn::Black);

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
    h.gc.setCurrentPlayer(Turn::Black);
    QSignalSpy spy(&h.handler, &GameEndHandler::gameEnded);

    h.handler.handleResign();

    QCOMPARE(spy.count(), 1);
    auto info = spy.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(info.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(info.loser, Turn::Black);
}

void Tst_GameEndHandler::handleBreakOff_emitsGameEndedSignal()
{
    EndTestHarness h;
    h.gc.setCurrentPlayer(Turn::White);
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
    EndTestHarness h(cause != int(MatchCoordinator::Cause::NyugyokuWin));
    EndTracker::handicapNames = handicapNames;
    switch (static_cast<MatchCoordinator::Cause>(cause)) {
    case MatchCoordinator::Cause::Timeout:
        h.handler.handleTimeout(Turn::White);
        break;
    case MatchCoordinator::Cause::Resignation:
        h.gc.setCurrentPlayer(Turn::White);
        h.handler.handleResign();
        break;
    default:
        // 盤面のない入玉宣言は判定を省いて宣言勝ちとし、対局終了のダイアログで知らせる
        h.handler.handleEngineWin(Turn::Black);
        break;
    }

    QCOMPARE(EndTracker::lastDialogTitle, QStringLiteral("対局終了"));
    QCOMPARE(EndTracker::lastDialogMsg, message);
}

QTEST_MAIN(Tst_GameEndHandler)
#include "tst_game_end_handler.moc"
