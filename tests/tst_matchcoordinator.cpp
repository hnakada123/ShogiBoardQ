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
#include "sfenutils.h"
#include "usi.h"
#include <QTemporaryDir>

// ============================================================
// テストハーネス
// ============================================================
struct MCTestHarness {
    ShogiGameController gc;
    ShogiClock clock;
    QStringList sfenRecord;
    QList<ShogiMove> gameMoves;

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
        QString initial = SfenUtils::hirateSfen();
        gc.newGame(initial);
        gc.setCurrentPlayer(ShogiGameController::Player1);

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

        deps.hooks.engine.sendStopToEngine = [](Usi*) {};

        deps.hooks.game.initializeNewGame = [this](const QString&) {
            initializeNewGameCalled = true;
        };
        deps.hooks.game.appendKifuLine = [this](const QString&, const QString&) {
            ++appendKifuLineCount;
        };
        deps.hooks.game.appendEvalP1 = []() {};
        deps.hooks.game.appendEvalP2 = []() {};
        deps.hooks.game.autoSaveKifu = [](const QString&) {};

        mc = std::make_unique<MatchCoordinator>(deps);
    }
};

// ============================================================
// テストクラス
// ============================================================
class Tst_MatchCoordinator : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase() {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
    }
    // === Section A: 初期状態 ===
    void initialState_playModeIsNotStarted();
    void initialState_gameOverStateIsCleared();
    void initialState_gameMovesIsEmpty();
    void gameMovesUsesSharedRecord();

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
    void handleResign_endsGame();
    void handleEngineResign_recordsLoser();
    void handleEngineWin_recordsWinner();
    void handleBreakOff_appendsTerminalOnce();
    void configureAndStart_preparesHumanGame();
    void setTimeControlConfig_updatesClockPolicy();
    void startAnalysis_initializesAndStopsRealSession();
    void stopAnalysisEngine_isIdempotent();

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

void Tst_MatchCoordinator::gameMovesUsesSharedRecord()
{
    MCTestHarness h;
    h.gameMoves.append(ShogiMove(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false));
    QCOMPARE(h.mc->gameMoves(), h.gameMoves);
    h.gameMoves.clear();
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

void Tst_MatchCoordinator::handleResign_endsGame()
{
    MCTestHarness h;
    h.mc->handleResign();
    QVERIFY(h.mc->gameOverState().isOver);
    QCOMPARE(h.mc->gameOverState().lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.mc->gameOverState().lastInfo.loser, MatchCoordinator::P1);
    QVERIFY(h.showGameOverDialogCalled);
}

void Tst_MatchCoordinator::handleEngineResign_recordsLoser()
{
    MCTestHarness h;

    h.mc->handleEngineResign(1);
    QCOMPARE(h.mc->gameOverState().lastInfo.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.mc->gameOverState().lastInfo.loser, MatchCoordinator::P1);

    h.mc->clearGameOverState();
    h.mc->handleEngineResign(2);
    QCOMPARE(h.mc->gameOverState().lastInfo.loser, MatchCoordinator::P2);
}

void Tst_MatchCoordinator::handleEngineWin_recordsWinner()
{
    MCTestHarness h;

    // エンジンの入玉宣言（bestmove win）も盤面で判定する。初期局面では条件を満たさず宣言側の負け
    h.mc->handleEngineWin(1);
    QCOMPARE(h.mc->gameOverState().lastInfo.cause, MatchCoordinator::Cause::IllegalMove);
    QCOMPARE(h.mc->gameOverState().lastInfo.loser, MatchCoordinator::P1);

    // 先手玉２二、敵陣に15枚・持ち駒と合わせて28点（27点法で宣言勝ち）
    h.mc->clearGameOverState();
    QString entered = QStringLiteral(
        "5+B+RGS/6GKS/+P+P+P+P+P+P+P+P+P/9/p1p1p1p1p/1s1g1g1s1/2n3n1n/1r2k4/2b6 b NL3P3lp 1");
    h.gc.newGame(entered);
    h.mc->handleEngineWin(1);
    QCOMPARE(h.mc->gameOverState().lastInfo.cause, MatchCoordinator::Cause::NyugyokuWin);
    QCOMPARE(h.mc->gameOverState().lastInfo.loser, MatchCoordinator::P2);
}

void Tst_MatchCoordinator::handleBreakOff_appendsTerminalOnce()
{
    MCTestHarness h;
    h.mc->handleBreakOff();
    QVERIFY(h.mc->gameOverState().isOver);
    QCOMPARE(h.mc->gameOverState().lastInfo.cause, MatchCoordinator::Cause::BreakOff);
    QVERIFY(h.mc->gameOverState().moveAppended);
    QCOMPARE(h.appendKifuLineCount, 1);
}

void Tst_MatchCoordinator::configureAndStart_preparesHumanGame()
{
    MCTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = SfenUtils::hirateSfen();
    h.mc->configureAndStart(opt);
    QCOMPARE(h.mc->playMode(), PlayMode::HumanVsHuman);
    QCOMPARE(h.gc.currentPlayer(), ShogiGameController::Player1);
    QVERIFY(h.initializeNewGameCalled);
    QVERIFY(!h.mc->gameOverState().isOver);
}

void Tst_MatchCoordinator::setTimeControlConfig_updatesClockPolicy()
{
    MCTestHarness h;
    h.mc->setTimeControlConfig(true, 30000, 30000, 0, 0, true);
    QVERIFY(h.mc->timeControl().useByoyomi);
    QCOMPARE(h.mc->timeControl().byoyomiMs1, 30000);
    QCOMPARE(h.mc->timeControl().byoyomiMs2, 30000);
}

void Tst_MatchCoordinator::startAnalysis_initializesAndStopsRealSession()
{
    MCTestHarness h;
    MatchCoordinator::AnalysisOptions opt;
    opt.mode = PlayMode::ConsiderationMode;
    opt.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
#ifdef Q_OS_WIN
    opt.enginePath += QStringLiteral(".exe");
#endif
    opt.engineName = QStringLiteral("CoordinatorTest");
    opt.positionStr = QStringLiteral("position startpos");
    h.mc->startAnalysis(opt);
    QVERIFY(h.mc->primaryEngine());
    QSignalSpy ready(h.mc->primaryEngine(), &Usi::engineInitialized);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
    QCOMPARE(h.mc->playMode(), PlayMode::ConsiderationMode);
    QSignalSpy ended(h.mc.get(), &MatchCoordinator::considerationModeEnded);
    h.mc->stopAnalysisEngine();
    QCOMPARE(ended.count(), 1);
    QCOMPARE(h.mc->primaryEngine(), nullptr);
}

void Tst_MatchCoordinator::stopAnalysisEngine_isIdempotent()
{
    MCTestHarness h;
    QSignalSpy ended(h.mc.get(), &MatchCoordinator::considerationModeEnded);
    h.mc->stopAnalysisEngine();
    h.mc->stopAnalysisEngine();
    QCOMPARE(h.mc->primaryEngine(), nullptr);
    QCOMPARE(ended.count(), 0);
}

// ============================================================
// Section F: handlePlayerTimeOut
// ============================================================

void Tst_MatchCoordinator::handlePlayerTimeOut_callsGcMethods()
{
    MCTestHarness h;
    h.mc->handlePlayerTimeOut(1);
    QCOMPARE(h.gc.result(), ShogiGameController::Player2Wins);
    h.gc.resetResult();
    h.mc->clearGameOverState();
    h.mc->handlePlayerTimeOut(2);
    QCOMPARE(h.gc.result(), ShogiGameController::Player1Wins);
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
