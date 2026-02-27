/// @file tst_game_start_orchestrator.cpp
/// @brief GameStartOrchestrator 単体テスト
///
/// configureAndStart / buildStartOptions / ensureHumanAtBottomIfApplicable の
/// テストケース。非 QObject クラスのため Signal/Slot 検証は不要。

#include <QTest>
#include "gamestartorchestrator.h"
#include "startgamedialog.h"
#include "shogigamecontroller.h"
#include "settingscommon.h"

// ============================================================
// テスト用トラッカー
// ============================================================
namespace OrcTracker {

bool clearGameOverStateCalled = false;
bool initializeNewGameCalled = false;
bool setPlayersNamesCalled = false;
bool setEngineNamesCalled = false;
bool renderBoardFromGcCalled = false;
bool createAndStartModeStrategyCalled = false;
bool flipBoardCalled = false;
bool startInitialEngineMoveIfNeededCalled = false;
bool initializePositionStringsForStartCalled = false;
bool updateTurnDisplayCalled = false;

MatchCoordinator::Player lastTurnDisplayPlayer;
MatchCoordinator::StartOptions lastStrategyOptions;
QString lastInitSfen;
QString lastEngineName1;
QString lastEngineName2;

void reset()
{
    clearGameOverStateCalled = false;
    initializeNewGameCalled = false;
    setPlayersNamesCalled = false;
    setEngineNamesCalled = false;
    renderBoardFromGcCalled = false;
    createAndStartModeStrategyCalled = false;
    flipBoardCalled = false;
    startInitialEngineMoveIfNeededCalled = false;
    initializePositionStringsForStartCalled = false;
    updateTurnDisplayCalled = false;
    lastTurnDisplayPlayer = MatchCoordinator::P1;
    lastStrategyOptions = {};
    lastInitSfen.clear();
    lastEngineName1.clear();
    lastEngineName2.clear();
}

} // namespace OrcTracker

// ============================================================
// テストハーネス
// ============================================================
struct OrcTestHarness {
    GameStartOrchestrator orc;
    ShogiGameController gc;

    // Refs用の実体
    PlayMode playMode = PlayMode::NotStarted;
    int maxMoves = 0;
    MatchCoordinator::Player currentTurn = MatchCoordinator::P1;
    int currentMoveIndex = 0;
    bool autoSaveKifu = false;
    QString kifuSaveDir;
    QString humanName1;
    QString humanName2;
    QString engineNameForSave1;
    QString engineNameForSave2;
    QString positionStr1;
    QString positionPonder1;
    QStringList positionStrHistory;
    QVector<QStringList> allGameHistories;

    OrcTestHarness()
    {
        OrcTracker::reset();

        GameStartOrchestrator::Refs refs;
        refs.gc = &gc;
        refs.playMode = &playMode;
        refs.maxMoves = &maxMoves;
        refs.currentTurn = &currentTurn;
        refs.currentMoveIndex = &currentMoveIndex;
        refs.autoSaveKifu = &autoSaveKifu;
        refs.kifuSaveDir = &kifuSaveDir;
        refs.humanName1 = &humanName1;
        refs.humanName2 = &humanName2;
        refs.engineNameForSave1 = &engineNameForSave1;
        refs.engineNameForSave2 = &engineNameForSave2;
        refs.positionStr1 = &positionStr1;
        refs.positionPonder1 = &positionPonder1;
        refs.positionStrHistory = &positionStrHistory;
        refs.allGameHistories = &allGameHistories;
        orc.setRefs(refs);

        GameStartOrchestrator::Hooks hooks;
        hooks.clearGameOverState = []() {
            OrcTracker::clearGameOverStateCalled = true;
        };
        hooks.initializeNewGame = [](const QString& sfen) {
            OrcTracker::initializeNewGameCalled = true;
            OrcTracker::lastInitSfen = sfen;
        };
        hooks.setPlayersNames = [](const QString&, const QString&) {
            OrcTracker::setPlayersNamesCalled = true;
        };
        hooks.setEngineNames = [](const QString& e1, const QString& e2) {
            OrcTracker::setEngineNamesCalled = true;
            OrcTracker::lastEngineName1 = e1;
            OrcTracker::lastEngineName2 = e2;
        };
        hooks.renderBoardFromGc = []() {
            OrcTracker::renderBoardFromGcCalled = true;
        };
        hooks.updateTurnDisplay = [](MatchCoordinator::Player p) {
            OrcTracker::updateTurnDisplayCalled = true;
            OrcTracker::lastTurnDisplayPlayer = p;
        };
        hooks.initializePositionStringsForStart = [](const QString&) {
            OrcTracker::initializePositionStringsForStartCalled = true;
        };
        hooks.createAndStartModeStrategy = [](const MatchCoordinator::StartOptions& opt) {
            OrcTracker::createAndStartModeStrategyCalled = true;
            OrcTracker::lastStrategyOptions = opt;
        };
        hooks.flipBoard = []() {
            OrcTracker::flipBoardCalled = true;
        };
        hooks.startInitialEngineMoveIfNeeded = []() {
            OrcTracker::startInitialEngineMoveIfNeededCalled = true;
        };
        orc.setHooks(hooks);
    }
};

// ============================================================
// テストクラス
// ============================================================
class Tst_GameStartOrchestrator : public QObject
{
    Q_OBJECT

private slots:
    // === Section A: configureAndStart 基本フロー ===

    void configureAndStart_setsPlayMode();
    void configureAndStart_setsMaxMoves();
    void configureAndStart_setsAutoSaveKifu();
    void configureAndStart_setsPlayerNames();
    void configureAndStart_callsAllHooks();
    void configureAndStart_setsCurrentMoveIndexToZero();

    // === Section B: 手番決定 ===

    void configureAndStart_senteSfen_turnsP1();
    void configureAndStart_goteSfen_turnsP2();

    // === Section C: buildStartOptions（static） ===

    void buildStartOptions_setsMode();
    void buildStartOptions_setsSfenStart();
    void buildStartOptions_sfenFromRecord();
    void buildStartOptions_engineIsP1_forEvH();

    // === Section D: 対局履歴の蓄積 ===

    void configureAndStart_archivesHistory();
    void configureAndStart_historyCapAt10();

    // === Section E: prepareAndStartGame ===

    void prepareAndStartGame_callsConfigureAndStart();
    void prepareAndStartGame_callsStartInitialEngineMove();

    // === Section F: ensureHumanAtBottomIfApplicable ===

    void ensureHumanAtBottom_noDialog_noFlip();

    // === Section G: position文字列の初期化（フォールバック） ===

    void configureAndStart_noHistory_fallback();

    // === Section H: 再開始（終局後の再対局） ===

    void restart_afterGameEnd_clearsGameOverState();
    void restart_afterGameEnd_secondGameHistoryArchived();
};

// ============================================================
// Section A: configureAndStart 基本フロー
// ============================================================

void Tst_GameStartOrchestrator::configureAndStart_setsPlayMode()
{
    OrcTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    h.orc.configureAndStart(opt);

    QCOMPARE(h.playMode, PlayMode::HumanVsHuman);
}

void Tst_GameStartOrchestrator::configureAndStart_setsMaxMoves()
{
    OrcTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("startpos");
    opt.maxMoves = 256;

    h.orc.configureAndStart(opt);

    QCOMPARE(h.maxMoves, 256);
}

void Tst_GameStartOrchestrator::configureAndStart_setsAutoSaveKifu()
{
    OrcTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("startpos");
    opt.autoSaveKifu = true;
    opt.kifuSaveDir = QStringLiteral("/tmp/kifu");

    h.orc.configureAndStart(opt);

    QVERIFY(h.autoSaveKifu);
    QCOMPARE(h.kifuSaveDir, QStringLiteral("/tmp/kifu"));
}

void Tst_GameStartOrchestrator::configureAndStart_setsPlayerNames()
{
    OrcTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::EvenHumanVsEngine;
    opt.sfenStart = QStringLiteral("startpos");
    opt.humanName1 = QStringLiteral("Player1");
    opt.humanName2 = QStringLiteral("Player2");
    opt.engineName1 = QStringLiteral("Engine1");
    opt.engineName2 = QStringLiteral("Engine2");

    h.orc.configureAndStart(opt);

    QCOMPARE(h.humanName1, QStringLiteral("Player1"));
    QCOMPARE(h.humanName2, QStringLiteral("Player2"));
    QCOMPARE(h.engineNameForSave1, QStringLiteral("Engine1"));
    QCOMPARE(h.engineNameForSave2, QStringLiteral("Engine2"));
}

void Tst_GameStartOrchestrator::configureAndStart_callsAllHooks()
{
    OrcTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("startpos");
    opt.engineName1 = QStringLiteral("E1");
    opt.engineName2 = QStringLiteral("E2");

    h.orc.configureAndStart(opt);

    QVERIFY(OrcTracker::clearGameOverStateCalled);
    QVERIFY(OrcTracker::initializeNewGameCalled);
    QVERIFY(OrcTracker::setPlayersNamesCalled);
    QVERIFY(OrcTracker::setEngineNamesCalled);
    QVERIFY(OrcTracker::renderBoardFromGcCalled);
    QVERIFY(OrcTracker::updateTurnDisplayCalled);
    QVERIFY(OrcTracker::createAndStartModeStrategyCalled);
    QCOMPARE(OrcTracker::lastEngineName1, QStringLiteral("E1"));
    QCOMPARE(OrcTracker::lastEngineName2, QStringLiteral("E2"));
}

void Tst_GameStartOrchestrator::configureAndStart_setsCurrentMoveIndexToZero()
{
    OrcTestHarness h;
    h.currentMoveIndex = 42;

    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("startpos");

    h.orc.configureAndStart(opt);

    QCOMPARE(h.currentMoveIndex, 0);
}

// ============================================================
// Section B: 手番決定
// ============================================================

void Tst_GameStartOrchestrator::configureAndStart_senteSfen_turnsP1()
{
    OrcTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    h.orc.configureAndStart(opt);

    QCOMPARE(h.currentTurn, MatchCoordinator::P1);
    QCOMPARE(OrcTracker::lastTurnDisplayPlayer, MatchCoordinator::P1);
}

void Tst_GameStartOrchestrator::configureAndStart_goteSfen_turnsP2()
{
    OrcTestHarness h;
    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    // SFEN with "w" (gote's turn)
    opt.sfenStart = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");

    h.orc.configureAndStart(opt);

    QCOMPARE(h.currentTurn, MatchCoordinator::P2);
    QCOMPARE(OrcTracker::lastTurnDisplayPlayer, MatchCoordinator::P2);
}

// ============================================================
// Section C: buildStartOptions（static）
// ============================================================

void Tst_GameStartOrchestrator::buildStartOptions_setsMode()
{
    auto opt = GameStartOrchestrator::buildStartOptions(
        PlayMode::EvenEngineVsEngine,
        QStringLiteral("startpos"),
        nullptr,
        nullptr);

    QCOMPARE(opt.mode, PlayMode::EvenEngineVsEngine);
}

void Tst_GameStartOrchestrator::buildStartOptions_setsSfenStart()
{
    const QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    auto opt = GameStartOrchestrator::buildStartOptions(
        PlayMode::HumanVsHuman, sfen, nullptr, nullptr);

    QCOMPARE(opt.sfenStart, sfen);
}

void Tst_GameStartOrchestrator::buildStartOptions_sfenFromRecord()
{
    QStringList record;
    record << QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/9/LNSGKGSNL b - 1");

    auto opt = GameStartOrchestrator::buildStartOptions(
        PlayMode::HumanVsHuman, QString(), &record, nullptr);

    QCOMPARE(opt.sfenStart, record.first());
}

void Tst_GameStartOrchestrator::buildStartOptions_engineIsP1_forEvH()
{
    auto opt = GameStartOrchestrator::buildStartOptions(
        PlayMode::EvenEngineVsHuman,
        QStringLiteral("startpos"),
        nullptr,
        nullptr);

    QVERIFY(opt.engineIsP1);
    QVERIFY(!opt.engineIsP2);
}

// ============================================================
// Section D: 対局履歴の蓄積
// ============================================================

void Tst_GameStartOrchestrator::configureAndStart_archivesHistory()
{
    OrcTestHarness h;
    // Simulate previous game's position history
    h.positionStr1 = QStringLiteral("position startpos moves 7g7f");
    h.positionStrHistory.append(QStringLiteral("position startpos"));

    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("startpos");

    h.orc.configureAndStart(opt);

    // Previous history should be archived
    QCOMPARE(h.allGameHistories.size(), 1);
}

void Tst_GameStartOrchestrator::configureAndStart_historyCapAt10()
{
    OrcTestHarness h;

    // Pre-fill with 10 games
    for (int i = 0; i < 10; ++i) {
        QStringList hist;
        hist << QStringLiteral("position startpos moves 7g7f");
        h.allGameHistories.append(hist);
    }

    // Simulate a current game in progress
    h.positionStr1 = QStringLiteral("position startpos moves 2g2f");
    h.positionStrHistory.append(QStringLiteral("position startpos"));

    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("startpos");

    h.orc.configureAndStart(opt);

    // Should be capped at 10 (oldest removed)
    QVERIFY(h.allGameHistories.size() <= 10);
}

// ============================================================
// Section E: prepareAndStartGame
// ============================================================

void Tst_GameStartOrchestrator::prepareAndStartGame_callsConfigureAndStart()
{
    OrcTestHarness h;
    h.orc.prepareAndStartGame(
        PlayMode::HumanVsHuman,
        QStringLiteral("startpos"),
        nullptr,
        nullptr,
        true);

    // configureAndStart should have been called (hooks invoked)
    QVERIFY(OrcTracker::clearGameOverStateCalled);
    QVERIFY(OrcTracker::createAndStartModeStrategyCalled);
    QCOMPARE(h.playMode, PlayMode::HumanVsHuman);
}

void Tst_GameStartOrchestrator::prepareAndStartGame_callsStartInitialEngineMove()
{
    OrcTestHarness h;
    h.orc.prepareAndStartGame(
        PlayMode::EvenHumanVsEngine,
        QStringLiteral("startpos"),
        nullptr,
        nullptr,
        true);

    QVERIFY(OrcTracker::startInitialEngineMoveIfNeededCalled);
}

// ============================================================
// Section F: ensureHumanAtBottomIfApplicable
// ============================================================

void Tst_GameStartOrchestrator::ensureHumanAtBottom_noDialog_noFlip()
{
    OrcTestHarness h;
    h.orc.ensureHumanAtBottomIfApplicable(nullptr, true);

    QVERIFY(!OrcTracker::flipBoardCalled);
}

// ============================================================
// Section G: position文字列の初期化（フォールバック）
// ============================================================

void Tst_GameStartOrchestrator::configureAndStart_noHistory_fallback()
{
    OrcTestHarness h;

    MatchCoordinator::StartOptions opt;
    opt.mode = PlayMode::HumanVsHuman;
    opt.sfenStart = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    h.orc.configureAndStart(opt);

    // No matching history → fallback to initializePositionStringsForStart
    QVERIFY(OrcTracker::initializePositionStringsForStartCalled);
}

// ============================================================
// Section H: 再開始（終局後の再対局）
// ============================================================

void Tst_GameStartOrchestrator::restart_afterGameEnd_clearsGameOverState()
{
    OrcTestHarness h;

    // First game
    MatchCoordinator::StartOptions opt1;
    opt1.mode = PlayMode::HumanVsHuman;
    opt1.sfenStart = QStringLiteral("startpos");
    h.orc.configureAndStart(opt1);

    OrcTracker::clearGameOverStateCalled = false;

    // Second game (restart after end)
    MatchCoordinator::StartOptions opt2;
    opt2.mode = PlayMode::EvenHumanVsEngine;
    opt2.sfenStart = QStringLiteral("startpos");
    h.orc.configureAndStart(opt2);

    QVERIFY(OrcTracker::clearGameOverStateCalled);
    QCOMPARE(h.playMode, PlayMode::EvenHumanVsEngine);
}

void Tst_GameStartOrchestrator::restart_afterGameEnd_secondGameHistoryArchived()
{
    OrcTestHarness h;

    // First game: set up position strings as if a game was played
    MatchCoordinator::StartOptions opt1;
    opt1.mode = PlayMode::HumanVsHuman;
    opt1.sfenStart = QStringLiteral("startpos");
    h.orc.configureAndStart(opt1);

    // Simulate playing moves
    h.positionStr1 = QStringLiteral("position startpos moves 7g7f 3c3d");
    h.positionStrHistory.clear();
    h.positionStrHistory.append(QStringLiteral("position startpos moves 7g7f 3c3d"));

    // Second game start (should archive first game)
    MatchCoordinator::StartOptions opt2;
    opt2.mode = PlayMode::HumanVsHuman;
    opt2.sfenStart = QStringLiteral("startpos");
    h.orc.configureAndStart(opt2);

    // The first game's history should be archived
    QVERIFY(!h.allGameHistories.isEmpty());
}

// ============================================================
QTEST_MAIN(Tst_GameStartOrchestrator)
#include "tst_game_start_orchestrator.moc"
