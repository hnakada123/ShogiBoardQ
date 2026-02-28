/// @file tst_matchcoordinator.cpp
/// @brief MatchCoordinator 単体テスト
///
/// 対局進行コーディネータ（司令塔）の主要フロー
/// （初期状態・終局状態管理・盤面反転・デリゲーション）を検証する。

#include <QTest>
#include <QSignalSpy>
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"

// ============================================================
// test_stubs_matchcoordinator.cpp で定義されたトラッカー
// ============================================================
namespace MCTracker {
extern bool gameEndHandlerResignCalled;
extern bool gameEndHandlerBreakOffCalled;
extern int  gameEndHandlerEngineResignIdx;
extern int  gameEndHandlerEngineWinIdx;
extern bool gameStartOrchestratorConfigureCalled;
extern bool matchTimekeeperSetTimeControlCalled;
extern bool analysisSessionStartCalled;
extern bool analysisSessionStopCalled;
void reset();
}

// ============================================================
// テストハーネス
// ============================================================
struct MCTestHarness {
    ShogiGameController gc;
    ShogiClock clock;
    QStringList sfenRecord;
    QVector<ShogiMove> gameMoves;

    // フック呼び出しトラッカー
    bool updateTurnDisplayCalled = false;
    MatchCoordinator::Player lastTurnPlayer = MatchCoordinator::P1;
    bool renderBoardCalled = false;
    bool showGameOverDialogCalled = false;
    QString lastDialogTitle;
    QString lastDialogMessage;
    bool initializeNewGameCalled = false;
    int appendKifuLineCount = 0;

    std::unique_ptr<MatchCoordinator> mc;

    MCTestHarness()
    {
        MCTracker::reset();

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

        // Hooks設定
        deps.hooks.ui.updateTurnDisplay = [this](MatchCoordinator::Player p) {
            updateTurnDisplayCalled = true;
            lastTurnPlayer = p;
        };
        deps.hooks.ui.renderBoardFromGc = [this]() {
            renderBoardCalled = true;
        };
        deps.hooks.ui.showGameOverDialog = [this](const QString& title, const QString& msg) {
            showGameOverDialogCalled = true;
            lastDialogTitle = title;
            lastDialogMessage = msg;
        };
        deps.hooks.ui.setPlayersNames = [](const QString&, const QString&) {};
        deps.hooks.ui.setEngineNames = [](const QString&, const QString&) {};
        deps.hooks.ui.showMoveHighlights = [](const QPoint&, const QPoint&) {};

        deps.hooks.time.remainingMsFor = [](MatchCoordinator::Player) -> qint64 { return 0; };
        deps.hooks.time.incrementMsFor = [](MatchCoordinator::Player) -> qint64 { return 0; };
        deps.hooks.time.byoyomiMs = []() -> qint64 { return 0; };

        deps.hooks.engine.sendGoToEngine = [](Usi*, const MatchCoordinator::GoTimes&) {};
        deps.hooks.engine.sendStopToEngine = [](Usi*) {};
        deps.hooks.engine.sendRawToEngine = [](Usi*, const QString&) {};

        deps.hooks.game.initializeNewGame = [this](const QString&) {
            initializeNewGameCalled = true;
        };
        deps.hooks.game.appendKifuLine = [this](const QString&, const QString&) {
            ++appendKifuLineCount;
        };
        deps.hooks.game.appendEvalP1 = []() {};
        deps.hooks.game.appendEvalP2 = []() {};
        deps.hooks.game.autoSaveKifu = [](const QString&, PlayMode,
                                          const QString&, const QString&,
                                          const QString&, const QString&) {};

        mc = std::make_unique<MatchCoordinator>(deps);
    }
};

// ============================================================
// テストクラス
// ============================================================
class Tst_MatchCoordinator : public QObject
{
    Q_OBJECT

private slots:
    // === Section A: 初期状態 ===
    void initialState_playModeIsNotStarted();
    void initialState_gameOverStateIsCleared();
    void initialState_gameMovesIsEmpty();

    // === Section B: 終局状態管理 ===
    void setGameOver_setsStateAndEmitsSignals();
    void setGameOver_doubleCallIgnored();
    void setGameOver_emitsRequestAppendGameOverMove();
    void clearGameOverState_clearsAndEmitsSignal();
    void clearGameOverState_notOverNoSignal();
    void markGameOverMoveAppended_marksAndEmitsSignal();
    void markGameOverMoveAppended_notOverIgnored();
    void markGameOverMoveAppended_doubleCallIgnored();

    // === Section C: 盤面反転 ===
    void flipBoard_callsHookAndEmitsSignal();

    // === Section D: PlayMode ===
    void setPlayMode_roundTrip();

    // === Section E: デリゲーション ===
    void handleResign_delegatesToGameEndHandler();
    void handleEngineResign_delegatesToGameEndHandler();
    void handleEngineWin_delegatesToGameEndHandler();
    void handleBreakOff_delegatesToGameEndHandler();
    void configureAndStart_delegatesToGameStartOrchestrator();
    void setTimeControlConfig_delegatesToTimekeeper();
    void startAnalysis_delegatesToAnalysisSession();
    void stopAnalysisEngine_delegatesToAnalysisSession();

    // === Section F: handlePlayerTimeOut ===
    void handlePlayerTimeOut_callsGcMethods();

    // === Section G: シグナル検証 ===
    void gameEnded_signalCarriesCorrectInfo();
    void gameOverStateChanged_signalCarriesState();
};

// ============================================================
// Section A: 初期状態
// ============================================================

void Tst_MatchCoordinator::initialState_playModeIsNotStarted()
{
    MCTestHarness h;
    QCOMPARE(h.mc->playMode(), PlayMode::NotStarted);
}

void Tst_MatchCoordinator::initialState_gameOverStateIsCleared()
{
    MCTestHarness h;
    const auto& st = h.mc->gameOverState();
    QVERIFY(!st.isOver);
    QVERIFY(!st.hasLast);
    QVERIFY(!st.moveAppended);
}

void Tst_MatchCoordinator::initialState_gameMovesIsEmpty()
{
    MCTestHarness h;
    QVERIFY(h.mc->gameMoves().isEmpty());
}

// ============================================================
// Section B: 終局状態管理
// ============================================================

void Tst_MatchCoordinator::setGameOver_setsStateAndEmitsSignals()
{
    MCTestHarness h;
    QSignalSpy endedSpy(h.mc.get(), &MatchCoordinator::gameEnded);
    QSignalSpy stateSpy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Resignation;
    info.loser = MatchCoordinator::P1;

    h.mc->setGameOver(info, true);

    // 状態確認
    const auto& st = h.mc->gameOverState();
    QVERIFY(st.isOver);
    QVERIFY(st.hasLast);
    QVERIFY(st.lastLoserIsP1);
    QCOMPARE(st.lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(st.lastInfo.loser, MatchCoordinator::P1);

    // シグナル確認
    QCOMPARE(endedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
}

void Tst_MatchCoordinator::setGameOver_doubleCallIgnored()
{
    MCTestHarness h;

    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Resignation;
    info.loser = MatchCoordinator::P1;

    h.mc->setGameOver(info, true);

    QSignalSpy endedSpy(h.mc.get(), &MatchCoordinator::gameEnded);
    QSignalSpy stateSpy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    // 2回目は無視される
    info.loser = MatchCoordinator::P2;
    h.mc->setGameOver(info, false);

    QCOMPARE(endedSpy.count(), 0);
    QCOMPARE(stateSpy.count(), 0);
    // 最初の値が残っている
    QVERIFY(h.mc->gameOverState().lastLoserIsP1);
}

void Tst_MatchCoordinator::setGameOver_emitsRequestAppendGameOverMove()
{
    MCTestHarness h;
    QSignalSpy appendSpy(h.mc.get(), &MatchCoordinator::requestAppendGameOverMove);

    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Timeout;
    info.loser = MatchCoordinator::P2;

    h.mc->setGameOver(info, false, true);  // appendMoveOnce=true

    QCOMPARE(appendSpy.count(), 1);
    auto receivedInfo = appendSpy.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(receivedInfo.cause, MatchCoordinator::Cause::Timeout);
    QCOMPARE(receivedInfo.loser, MatchCoordinator::P2);
}

void Tst_MatchCoordinator::clearGameOverState_clearsAndEmitsSignal()
{
    MCTestHarness h;

    // まず終局状態にする
    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Resignation;
    info.loser = MatchCoordinator::P1;
    h.mc->setGameOver(info, true);
    QVERIFY(h.mc->gameOverState().isOver);

    QSignalSpy stateSpy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    h.mc->clearGameOverState();

    QVERIFY(!h.mc->gameOverState().isOver);
    QVERIFY(!h.mc->gameOverState().hasLast);
    QCOMPARE(stateSpy.count(), 1);
}

void Tst_MatchCoordinator::clearGameOverState_notOverNoSignal()
{
    MCTestHarness h;
    QSignalSpy stateSpy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    // 既に isOver=false なので、シグナルは発火しない
    h.mc->clearGameOverState();

    QCOMPARE(stateSpy.count(), 0);
}

void Tst_MatchCoordinator::markGameOverMoveAppended_marksAndEmitsSignal()
{
    MCTestHarness h;

    // まず終局状態にする
    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Resignation;
    info.loser = MatchCoordinator::P1;
    h.mc->setGameOver(info, true, false);  // appendMoveOnce=false

    QVERIFY(!h.mc->gameOverState().moveAppended);

    QSignalSpy stateSpy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    h.mc->markGameOverMoveAppended();

    QVERIFY(h.mc->gameOverState().moveAppended);
    QCOMPARE(stateSpy.count(), 1);
}

void Tst_MatchCoordinator::markGameOverMoveAppended_notOverIgnored()
{
    MCTestHarness h;
    QSignalSpy stateSpy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    h.mc->markGameOverMoveAppended();

    QCOMPARE(stateSpy.count(), 0);
}

void Tst_MatchCoordinator::markGameOverMoveAppended_doubleCallIgnored()
{
    MCTestHarness h;

    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Resignation;
    info.loser = MatchCoordinator::P1;
    h.mc->setGameOver(info, true, false);
    h.mc->markGameOverMoveAppended();
    QVERIFY(h.mc->gameOverState().moveAppended);

    QSignalSpy stateSpy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    // 2回目は無視される
    h.mc->markGameOverMoveAppended();

    QCOMPARE(stateSpy.count(), 0);
}

// ============================================================
// Section C: 盤面反転
// ============================================================

void Tst_MatchCoordinator::flipBoard_callsHookAndEmitsSignal()
{
    MCTestHarness h;
    QSignalSpy flipSpy(h.mc.get(), &MatchCoordinator::boardFlipped);

    h.mc->flipBoard();

    QVERIFY(h.renderBoardCalled);
    QCOMPARE(flipSpy.count(), 1);
    QCOMPARE(flipSpy.first().first().toBool(), true);
}

// ============================================================
// Section D: PlayMode
// ============================================================

void Tst_MatchCoordinator::setPlayMode_roundTrip()
{
    MCTestHarness h;
    QCOMPARE(h.mc->playMode(), PlayMode::NotStarted);

    h.mc->setPlayMode(PlayMode::HumanVsHuman);
    QCOMPARE(h.mc->playMode(), PlayMode::HumanVsHuman);

    h.mc->setPlayMode(PlayMode::EvenHumanVsEngine);
    QCOMPARE(h.mc->playMode(), PlayMode::EvenHumanVsEngine);

    h.mc->setPlayMode(PlayMode::NotStarted);
    QCOMPARE(h.mc->playMode(), PlayMode::NotStarted);
}

// ============================================================
// Section E: デリゲーション
// ============================================================

void Tst_MatchCoordinator::handleResign_delegatesToGameEndHandler()
{
    MCTestHarness h;
    h.mc->handleResign();
    QVERIFY(MCTracker::gameEndHandlerResignCalled);
}

void Tst_MatchCoordinator::handleEngineResign_delegatesToGameEndHandler()
{
    MCTestHarness h;

    h.mc->handleEngineResign(1);
    QCOMPARE(MCTracker::gameEndHandlerEngineResignIdx, 1);

    MCTracker::reset();
    h.mc->handleEngineResign(2);
    QCOMPARE(MCTracker::gameEndHandlerEngineResignIdx, 2);
}

void Tst_MatchCoordinator::handleEngineWin_delegatesToGameEndHandler()
{
    MCTestHarness h;

    h.mc->handleEngineWin(1);
    QCOMPARE(MCTracker::gameEndHandlerEngineWinIdx, 1);

    MCTracker::reset();
    h.mc->handleEngineWin(2);
    QCOMPARE(MCTracker::gameEndHandlerEngineWinIdx, 2);
}

void Tst_MatchCoordinator::handleBreakOff_delegatesToGameEndHandler()
{
    MCTestHarness h;
    h.mc->handleBreakOff();
    QVERIFY(MCTracker::gameEndHandlerBreakOffCalled);
}

void Tst_MatchCoordinator::configureAndStart_delegatesToGameStartOrchestrator()
{
    MCTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    h.mc->configureAndStart(opt);
    QVERIFY(MCTracker::gameStartOrchestratorConfigureCalled);
}

void Tst_MatchCoordinator::setTimeControlConfig_delegatesToTimekeeper()
{
    MCTestHarness h;
    h.mc->setTimeControlConfig(true, 30000, 30000, 0, 0, true);
    QVERIFY(MCTracker::matchTimekeeperSetTimeControlCalled);
}

void Tst_MatchCoordinator::startAnalysis_delegatesToAnalysisSession()
{
    MCTestHarness h;
    MatchCoordinator::AnalysisOptions opt;
    opt.mode = PlayMode::ConsiderationMode;
    h.mc->startAnalysis(opt);
    QVERIFY(MCTracker::analysisSessionStartCalled);
}

void Tst_MatchCoordinator::stopAnalysisEngine_delegatesToAnalysisSession()
{
    MCTestHarness h;
    h.mc->stopAnalysisEngine();
    QVERIFY(MCTracker::analysisSessionStopCalled);
}

// ============================================================
// Section F: handlePlayerTimeOut
// ============================================================

void Tst_MatchCoordinator::handlePlayerTimeOut_callsGcMethods()
{
    MCTestHarness h;
    // GC メソッドはスタブで何もしないが、クラッシュしないことを検証
    h.mc->handlePlayerTimeOut(1);
    h.mc->handlePlayerTimeOut(2);
    // GC がnullでないことが前提、正常実行されることを確認
    QVERIFY(true);
}

// ============================================================
// Section G: シグナル検証
// ============================================================

void Tst_MatchCoordinator::gameEnded_signalCarriesCorrectInfo()
{
    MCTestHarness h;
    QSignalSpy spy(h.mc.get(), &MatchCoordinator::gameEnded);

    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Timeout;
    info.loser = MatchCoordinator::P2;

    h.mc->setGameOver(info, false);

    QCOMPARE(spy.count(), 1);
    auto receivedInfo = spy.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(receivedInfo.cause, MatchCoordinator::Cause::Timeout);
    QCOMPARE(receivedInfo.loser, MatchCoordinator::P2);
}

void Tst_MatchCoordinator::gameOverStateChanged_signalCarriesState()
{
    MCTestHarness h;
    QSignalSpy spy(h.mc.get(), &MatchCoordinator::gameOverStateChanged);

    MatchCoordinator::GameEndInfo info;
    info.cause = MatchCoordinator::Cause::Sennichite;
    info.loser = MatchCoordinator::P1;

    h.mc->setGameOver(info, true);

    QCOMPARE(spy.count(), 1);
    auto state = spy.first().first().value<MatchCoordinator::GameOverState>();
    QVERIFY(state.isOver);
    QVERIFY(state.hasLast);
    QVERIFY(state.lastLoserIsP1);
}

// ============================================================
QTEST_MAIN(Tst_MatchCoordinator)
#include "tst_matchcoordinator.moc"
