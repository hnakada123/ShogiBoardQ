/// @file tst_app_game_session.cpp
/// @brief 実際の対局サービス（本体の合成ルートで組み立てた GameSession）を使うUI操作フローの回帰テスト
#include <QtTest>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
// 通信対局の司令塔の手番の状態は、サーバーとのやり取りでしか変わらないため、テストでは直接設定する
// （CsaGameCoordinator が TestAppGameSession を friend にしている）
#include "appstate.h"
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "gamesession.h"
#include "gamestartcoordinator.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "matchcoordinator.h"
#include "humanturnpolicy.h"
#include "replaycontroller.h"
#include "settingscommon.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "startgamedialog.h"
#include "timecontrolcontroller.h"

/// 対局開始の調整役が出すシグナルの順を記録する
class StartSignalRecorder : public QObject
{
    Q_OBJECT
public:
    QStringList events;
public slots:
    void onPreStartCleanup() { events.append(QStringLiteral("cleanup")); }
};

class TestAppGameSession : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    bool m_acceptDialog = false;
    bool m_dialogSeen = false;
    QTimer m_messageCloser;

    static void humanVsHumanSettings(int basicMinutes = 0)
    {
        auto& settings = SettingsCommon::openSettings();
        settings.clear();
        settings.setValue("GameSettings/isHuman1", true);
        settings.setValue("GameSettings/isHuman2", true);
        settings.setValue("GameSettings/startingPositionNumber", 1);
        if (basicMinutes > 0) settings.setValue("GameSettings/basicTimeMinutes1", basicMinutes);
        settings.sync();
    }

    /// 対局開始ダイアログから人間同士の対局を始める
    void startFromDialog(MainWindow& window)
    {
        m_acceptDialog = true;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        window.composer().parts().gameSession->startGameFromDialog();
        QVERIFY(m_dialogSeen);
    }

public:
    void closeMessageBoxes()
    {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->accept();
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

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        // 終局の知らせ（メッセージボックス）は出たらすぐ閉じる
        m_messageCloser.setInterval(10);
        connect(&m_messageCloser, &QTimer::timeout, this, &TestAppGameSession::closeMessageBoxes);
        m_messageCloser.start();
    }

    void resignationEndsTheMatch()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        startFromDialog(window);
        QSignalSpy ended(p.match, &MatchCoordinator::gameEnded);
        p.gameSession->resign();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(p.match->gameOverState().lastInfo.cause, MatchCoordinator::Cause::Resignation);
        QCOMPARE(p.match->gameOverState().lastInfo.loser, Turn::Black);
        p.gameSession->resign();
        QCOMPARE(ended.count(), 1);
        // 終局の行を書き、待機中に戻る
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::Idle);
    }

    void interruptionEndsTheMatch()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        startFromDialog(window);
        QSignalSpy ended(p.match, &MatchCoordinator::gameEnded);
        p.gameSession->breakOff();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(p.match->gameOverState().lastInfo.cause, MatchCoordinator::Cause::BreakOff);
    }

    void networkCommandsDoNotEndLocalMatch()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        startFromDialog(window);
        CsaGameCoordinator csa;
        p.csaGameWiring->setCoordinator(&csa);
        window.composer().state().activity.setActivity(AppActivityState::Activity::DuringCsaGame);
        p.gameSession->resign();
        p.gameSession->breakOff();
        QVERIFY(!p.match->gameOverState().isOver);
        p.csaGameWiring->setCoordinator(nullptr);
    }

    /// 通信対局の盤上の着手は、そのときの司令塔の手番で受け付ける。司令塔は最初の通信対局を始めるときに
    /// 作られるが、判定の依存を更新し直さなくても後から作られたものを使う
    void csaHumanTurnUsesCurrentCoordinator()
    {
        ShogiGameController gc;
        AppActivityState activity;
        activity.setActivity(AppActivityState::Activity::DuringCsaGame);
        std::unique_ptr<CsaGameCoordinator> csa;
        CsaGameWiring csaWiring(CsaGameWiring::Dependencies{});
        HumanTurnPolicy policy;
        HumanTurnPolicy::Deps deps;
        deps.activity = &activity;
        deps.gameController = &gc;
        deps.csaGameWiring = &csaWiring;
        policy.updateDeps(deps);
        QVERIFY(!policy.isHumanTurnNow());

        csa = std::make_unique<CsaGameCoordinator>();
        csaWiring.setCoordinator(csa.get());
        csa->m_gameState = CsaGameCoordinator::GameState::InGame;
        csa->m_playerType = CsaGameCoordinator::PlayerType::Human;
        csa->m_client->m_isMyTurn = true;
        QVERIFY(policy.isHumanTurnNow());
        csa->m_client->m_isMyTurn = false;
        QVERIFY(!policy.isHumanTurnNow());
        csa->m_client->m_isMyTurn = true;
        csa->m_playerType = CsaGameCoordinator::PlayerType::Engine;
        QVERIFY(!policy.isHumanTurnNow());
        csa->m_playerType = CsaGameCoordinator::PlayerType::Human;
        csa->m_gameState = CsaGameCoordinator::GameState::GameOver;
        QVERIFY(!policy.isHumanTurnNow());
        csaWiring.setCoordinator(nullptr);
    }

    void startDialogCancellationKeepsGame()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy cleanup(p.gameStart, &GameStartCoordinator::requestPreStartCleanup);
        m_acceptDialog = false;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        p.gameSession->startGameFromDialog();
        QVERIFY(m_dialogSeen);
        QCOMPARE(cleanup.count(), 0);
        QVERIFY(!p.match->hasStarted());
        QVERIFY(!p.match->gameOverState().isOver);
    }

    // 手合割（平手）から始めるとき、開始前の整理は1回だけ行い、平手の局面から始める
    void presetStartCleansUpOnce()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        StartSignalRecorder recorder;
        connect(p.gameStart, &GameStartCoordinator::requestPreStartCleanup,
                &recorder, &StartSignalRecorder::onPreStartCleanup);
        startFromDialog(window);
        QCOMPARE(recorder.events, QStringList({QStringLiteral("cleanup")}));
        QVERIFY(p.match->hasStarted() && p.match->players().humanVsHuman());
        QCOMPARE(p.match->record().startSfen, SfenUtils::hirateSfen());
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::DuringGame);
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
        humanVsHumanSettings(5);
        MainWindow window;
        auto& p = window.composer().parts();
        p.replayController->setReplayMode(replay);
        startFromDialog(window);
        ShogiClock* clock = p.timeController->clock();
        QCOMPARE(p.match->clock(), clock);
        QVERIFY(clock->remainingMs(Turn::Black) > 0);
        // 棋譜再生から開始した場合も、新しい対局の時計を動かす。
        QVERIFY(clock->isRunning());
        QVERIFY(p.match->hasStarted() && p.match->players().humanVsHuman());
        QCOMPARE(p.match->record().startSfen, SfenUtils::hirateSfen());
        p.gameSession->breakOff();
    }
};

QTEST_MAIN(TestAppGameSession)
#include "tst_app_game_session.moc"
