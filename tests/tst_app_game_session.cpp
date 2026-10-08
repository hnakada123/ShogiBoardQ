/// @file tst_app_game_session.cpp
/// @brief 実際の対局サービスを使うUI操作フローの回帰テスト
#include <QtTest>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include "gamesessionorchestrator.h"
#include "gamestatecontroller.h"
#include "csagamecoordinator.h"
#include "replaycontroller.h"
#include "settingscommon.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "startgamedialog.h"

namespace {
struct SessionHarness {
    ShogiGameController gc;
    ShogiClock clock;
    QStringList history;
    QString start = SfenUtils::hirateSfen();
    QString current = start;
    PlayMode mode = PlayMode::HumanVsHuman;
    std::unique_ptr<MatchCoordinator> matchOwner;
    MatchCoordinator* match = nullptr;
    GameStateController state;
    GameStateController* statePtr = nullptr;
    GameSessionOrchestrator session;
    GameSessionOrchestrator::Deps deps;
    int ensured = 0;

    SessionHarness()
    {
        gc.newGame(start);
        gc.setCurrentPlayer(ShogiGameController::Player1);
        MatchCoordinator::Deps matchDeps;
        matchDeps.gc = &gc;
        matchDeps.clock = &clock;
        matchDeps.sfenRecord = &history;
        matchDeps.hooks.game.initializeNewGame = [this](const QString& sfen) {
            QString position = sfen;
            gc.newGame(position);
        };
        matchOwner = std::make_unique<MatchCoordinator>(matchDeps);
        match = matchOwner.get();
        match->setPlayMode(mode);
        state.setMatchCoordinator(match);
        deps.match = &match;
        deps.gameStateController = &statePtr;
        deps.gameController = &gc;
        deps.startSfenStr = &start;
        deps.currentSfenStr = &current;
        deps.playMode = &mode;
        deps.ensureGameStateController = [this]() { ++ensured; statePtr = &state; };
        session.updateDeps(deps);
    }
};
}

class TestAppGameSession : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    bool m_acceptDialog = false;
    bool m_dialogSeen = false;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void respondToStartDialog()
    {
        auto* dialog = qobject_cast<StartGameDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        m_dialogSeen = true;
        if (m_acceptDialog) {
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            QVERIFY(buttons);
            buttons->button(QDialogButtonBox::Ok)->click();
        }
        else dialog->reject();
    }

    void missingDependenciesAreSafe()
    {
        GameSessionOrchestrator session;
        int starts = 0, states = 0, dialogs = 0;
        GameSessionOrchestrator::Deps deps;
        deps.ensureGameStartCoordinator = [&]() { ++starts; };
        deps.ensureGameStateController = [&]() { ++states; };
        deps.ensureDialogCoordinator = [&]() { ++dialogs; };
        session.updateDeps(deps);
        session.initializeGame();
        session.handleResignation();
        session.handleBreakOffGame();
        session.onResignationTriggered();
        session.movePieceImmediately();
        session.stopTsumeSearch();
        session.openWebsiteInExternalBrowser();
        QCOMPARE(starts, 1);
        QCOMPARE(states, 3);
        QCOMPARE(dialogs, 1);
    }

    void resignationUsesLazilyCreatedController()
    {
        SessionHarness h;
        QSignalSpy ended(h.match, &MatchCoordinator::gameEnded);
        h.session.onResignationTriggered();
        QCOMPARE(h.ensured, 1);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(h.match->gameOverState().lastInfo.cause, MatchCoordinator::Cause::Resignation);
        QCOMPARE(h.match->gameOverState().lastInfo.loser, MatchCoordinator::P1);
        h.session.handleResignation();
        QCOMPARE(ended.count(), 1);
    }

    void interruptionUsesLazilyCreatedController()
    {
        SessionHarness h;
        QSignalSpy ended(h.match, &MatchCoordinator::gameEnded);
        h.session.handleBreakOffGame();
        QCOMPARE(h.ensured, 1);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(h.match->gameOverState().lastInfo.cause, MatchCoordinator::Cause::BreakOff);
    }

    void networkCommandsDoNotEndLocalMatch()
    {
        SessionHarness h;
        CsaGameCoordinator csa;
        CsaGameCoordinator* csaPtr = &csa;
        h.mode = PlayMode::CsaNetworkMode;
        h.deps.csaGameCoordinator = &csaPtr;
        h.session.updateDeps(h.deps);
        h.session.handleResignation();
        h.session.handleBreakOffGame();
        QCOMPARE(h.ensured, 0);
        QVERIFY(!h.match->gameOverState().isOver);
    }

    void controllerReplacementIsObserved()
    {
        SessionHarness first, second;
        GameStateController* active = &first.state;
        auto deps = first.deps;
        deps.gameStateController = &active;
        deps.ensureGameStateController = {};
        first.session.updateDeps(deps);
        active = &second.state;
        first.session.handleResignation();
        QVERIFY(!first.match->gameOverState().isOver);
        QVERIFY(second.match->gameOverState().isOver);
    }

    void startDialogCancellationKeepsGame()
    {
        SessionHarness h;
        GameStartCoordinator starter({h.match});
        GameStartCoordinator* ptr = nullptr;
        auto deps = h.deps;
        deps.gameStart = &ptr;
        deps.ensureGameStartCoordinator = [&]() { ptr = &starter; };
        h.session.updateDeps(deps);
        QSignalSpy cleanup(&starter, &GameStartCoordinator::requestPreStartCleanup);
        m_acceptDialog = false;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        h.session.initializeGame();
        QVERIFY(m_dialogSeen);
        QCOMPARE(cleanup.count(), 0);
        QVERIFY(!h.match->gameOverState().isOver);
    }

    void newGameStartsMatchClockFromLiveOrReplay_data()
    {
        QTest::addColumn<bool>("replay");
        QTest::newRow("live") << false;
        QTest::newRow("replay") << true;
    }

    void newGameStartsMatchClockFromLiveOrReplay()
    {
        QFETCH(bool, replay);
        auto& settings = SettingsCommon::openSettings();
        settings.clear();
        settings.setValue("GameSettings/isHuman1", true);
        settings.setValue("GameSettings/isHuman2", true);
        settings.setValue("GameSettings/startingPositionNumber", 1);
        settings.setValue("GameSettings/basicTimeMinutes1", 5);
        settings.sync();
        SessionHarness h;
        ReplayController replayController;
        replayController.setReplayMode(replay);
        GameStartCoordinator starter({h.match});
        GameStartCoordinator* ptr = &starter;
        h.deps.gameStart = &ptr;
        h.deps.replayController = &replayController;
        h.session.updateDeps(h.deps);
        m_acceptDialog = true;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        h.session.initializeGame();
        QVERIFY(m_dialogSeen);
        QCOMPARE(h.match->clock(), &h.clock);
        QVERIFY(h.clock.getPlayer1TimeIntMs() > 0);
        // 棋譜再生から開始した場合も、新しい対局の時計を動かす。
        QVERIFY(h.clock.isRunning());
        QCOMPARE(h.match->playMode(), PlayMode::HumanVsHuman);
        QVERIFY(!h.history.isEmpty());
        QCOMPARE(h.history.first(), SfenUtils::hirateSfen());
    }
};

QTEST_MAIN(TestAppGameSession)
#include "tst_app_game_session.moc"
