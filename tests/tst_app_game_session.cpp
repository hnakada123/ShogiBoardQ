/// @file tst_app_game_session.cpp
/// @brief 実際の対局サービスを使うUI操作フローの回帰テスト
#include <QtTest>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
// 通信対局の司令塔の手番の状態は、サーバーとのやり取りでしか変わらないため、テストでは直接設定する
// （CsaGameCoordinator が TestAppGameSession を friend にしている）
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "gamesessionorchestrator.h"
#include "gamestatecontroller.h"
#include "playmodepolicyservice.h"
#include "replaycontroller.h"
#include "settingscommon.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "startgamedialog.h"

/// 対局開始の調整役が出すシグナルの順を記録する
class StartSignalRecorder : public QObject
{
    Q_OBJECT
public:
    QStringList events;
public slots:
    void onPreStartCleanup() { events.append(QStringLiteral("cleanup")); }
    void onBranchTreeReset() { events.append(QStringLiteral("branch-reset")); }
};

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
    GameSessionOrchestrator session;
    GameSessionOrchestrator::Deps deps;

    SessionHarness()
    {
        gc.newGame(start);
        gc.setCurrentPlayer(Turn::Black);
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
        deps.match = match;
        deps.gameStateController = &state;
        deps.gameController = &gc;
        deps.startSfenStr = &start;
        deps.currentSfenStr = &current;
        deps.playMode = &mode;
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
        // 部品を渡していなくても、操作は何もせずに戻る（null を読まない）
        GameSessionOrchestrator session;
        session.updateDeps(GameSessionOrchestrator::Deps{});
        session.initializeGame();
        session.handleResignation();
        session.handleBreakOffGame();
        session.movePieceImmediately();
        session.stopTsumeSearch();
        session.openWebsiteInExternalBrowser();
    }

    void resignationEndsTheMatch()
    {
        SessionHarness h;
        QSignalSpy ended(h.match, &MatchCoordinator::gameEnded);
        h.session.handleResignation();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(h.match->gameOverState().lastInfo.cause, MatchCoordinator::Cause::Resignation);
        QCOMPARE(h.match->gameOverState().lastInfo.loser, Turn::Black);
        h.session.handleResignation();
        QCOMPARE(ended.count(), 1);
    }

    void interruptionEndsTheMatch()
    {
        SessionHarness h;
        QSignalSpy ended(h.match, &MatchCoordinator::gameEnded);
        h.session.handleBreakOffGame();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(h.match->gameOverState().lastInfo.cause, MatchCoordinator::Cause::BreakOff);
    }

    void networkCommandsDoNotEndLocalMatch()
    {
        SessionHarness h;
        CsaGameCoordinator csa;
        CsaGameWiring csaWiring(CsaGameWiring::Dependencies{});
        csaWiring.setCoordinator(&csa);
        h.mode = PlayMode::CsaNetworkMode;
        h.deps.csaGameWiring = &csaWiring;
        h.session.updateDeps(h.deps);
        h.session.handleResignation();
        h.session.handleBreakOffGame();
        QVERIFY(!h.match->gameOverState().isOver);
    }

    /// 通信対局の盤上の着手は、そのときの司令塔の手番で受け付ける。司令塔は最初の通信対局を始めるときに
    /// 作られるが、判定の依存を更新し直さなくても後から作られたものを使う
    void csaHumanTurnUsesCurrentCoordinator()
    {
        ShogiGameController gc;
        PlayMode mode = PlayMode::CsaNetworkMode;
        std::unique_ptr<CsaGameCoordinator> csa;
        CsaGameWiring csaWiring(CsaGameWiring::Dependencies{});
        PlayModePolicyService policy;
        PlayModePolicyService::Deps deps;
        deps.playMode = &mode;
        deps.gameController = &gc;
        deps.csaGameWiring = &csaWiring;
        policy.updateDeps(deps);
        QVERIFY(!policy.isHumanTurnNow());

        csa = std::make_unique<CsaGameCoordinator>();
        csaWiring.setCoordinator(csa.get());
        csa->m_gameState = CsaGameCoordinator::GameState::InGame;
        csa->m_playerType = CsaGameCoordinator::PlayerType::Human;
        csa->m_isMyTurn = true;
        QVERIFY(policy.isHumanTurnNow());
        csa->m_isMyTurn = false;
        QVERIFY(!policy.isHumanTurnNow());
        csa->m_isMyTurn = true;
        csa->m_playerType = CsaGameCoordinator::PlayerType::Engine;
        QVERIFY(!policy.isHumanTurnNow());
        csa->m_playerType = CsaGameCoordinator::PlayerType::Human;
        csa->m_gameState = CsaGameCoordinator::GameState::GameOver;
        QVERIFY(!policy.isHumanTurnNow());
    }

    void startDialogCancellationKeepsGame()
    {
        SessionHarness h;
        GameStartCoordinator starter({h.match});
        auto deps = h.deps;
        deps.gameStart = &starter;
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

    // 手合割（平手）から始めるとき、開始前の整理は分岐ツリーのリセットのあとに1回だけ行う
    void presetStartCleansUpOnceAfterBranchReset()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.clear();
        settings.setValue("GameSettings/isHuman1", true);
        settings.setValue("GameSettings/isHuman2", true);
        settings.setValue("GameSettings/startingPositionNumber", 1);
        settings.sync();
        SessionHarness h;
        GameStartCoordinator starter({h.match});
        StartSignalRecorder recorder;
        connect(&starter, &GameStartCoordinator::requestPreStartCleanup,
                &recorder, &StartSignalRecorder::onPreStartCleanup);
        connect(&starter, &GameStartCoordinator::requestBranchTreeResetForNewGame,
                &recorder, &StartSignalRecorder::onBranchTreeReset);
        h.deps.gameStart = &starter;
        h.session.updateDeps(h.deps);
        m_acceptDialog = true;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        h.session.initializeGame();
        QVERIFY(m_dialogSeen);
        QCOMPARE(recorder.events, QStringList({QStringLiteral("branch-reset"), QStringLiteral("cleanup")}));
        QCOMPARE(h.match->playMode(), PlayMode::HumanVsHuman);
    }

    // 局面の文字列と盤の手前の側を渡されていなくても、対局を始められる（null を読まない）
    void newGameWithoutPositionAndBoardSidePointers()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.clear();
        settings.setValue("GameSettings/isHuman1", true);
        settings.setValue("GameSettings/isHuman2", true);
        settings.setValue("GameSettings/startingPositionNumber", 1);
        settings.sync();
        SessionHarness h;
        GameStartCoordinator starter({h.match});
        h.deps.gameStart = &starter;
        h.deps.startSfenStr = nullptr;
        h.deps.currentSfenStr = nullptr;
        h.deps.bottomSide = nullptr;
        h.session.updateDeps(h.deps);
        m_acceptDialog = true;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        h.session.initializeGame();
        QVERIFY(m_dialogSeen);
        QCOMPARE(h.match->playMode(), PlayMode::HumanVsHuman);
        QVERIFY(!h.history.isEmpty());
        QCOMPARE(h.history.first(), SfenUtils::hirateSfen());
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
        h.deps.gameStart = &starter;
        h.deps.replayController = &replayController;
        h.session.updateDeps(h.deps);
        m_acceptDialog = true;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        h.session.initializeGame();
        QVERIFY(m_dialogSeen);
        QCOMPARE(h.match->clock(), &h.clock);
        QVERIFY(h.clock.remainingMs(Turn::Black) > 0);
        // 棋譜再生から開始した場合も、新しい対局の時計を動かす。
        QVERIFY(h.clock.isRunning());
        QCOMPARE(h.match->playMode(), PlayMode::HumanVsHuman);
        QVERIFY(!h.history.isEmpty());
        QCOMPARE(h.history.first(), SfenUtils::hirateSfen());
    }
};

QTEST_MAIN(TestAppGameSession)
#include "tst_app_game_session.moc"
