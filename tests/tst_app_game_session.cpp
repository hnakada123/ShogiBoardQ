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
#include "csagamesession.h"
#include "livegamesession.h"
#include "gamesession.h"
#include "kifudocument.h"
#include "kifunavigationstate.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "matchcoordinator.h"
#include "humanturnpolicy.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "settingscommon.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "startgamedialog.h"

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
        // 通信対局中の投了・中断は通信対局の司令塔へ送り、ローカル対局は終わらせない
        window.composer().state().activity.setActivity(AppActivityState::Activity::DuringCsaGame);
        p.gameSession->resign();
        p.gameSession->breakOff();
        QVERIFY(!p.match->gameOverState().isOver);
    }

    /// 通信対局の盤上の着手は、司令塔の状態（対局中・人間の手番）のときだけ受け付ける
    void csaHumanTurnFollowsCoordinatorState()
    {
        ShogiGameController gc;
        AppActivityState activity;
        activity.setActivity(AppActivityState::Activity::DuringCsaGame);
        auto csa = std::make_unique<CsaGameCoordinator>();
        HumanTurnPolicy::Deps deps;
        deps.activity = &activity;
        deps.gameController = &gc;
        deps.csaGame = csa.get();
        HumanTurnPolicy policy(deps);
        QVERIFY(!policy.isHumanTurnNow());

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
    }

    void startDialogCancellationKeepsGame()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy treeChanged(p.branchTree, &KifuBranchTree::treeChanged);
        const int rootId = p.branchTree->root()->nodeId();
        m_acceptDialog = false;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        p.gameSession->startGameFromDialog();
        QVERIFY(m_dialogSeen);
        // 棋譜は作り直さない
        QCOMPARE(treeChanged.count(), 0);
        QCOMPARE(p.branchTree->root()->nodeId(), rootId);
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::Idle);
        QVERIFY(!p.match->gameOverState().isOver);
    }

    // 手合割（平手）から始めるとき、開始前の整理は1回だけ行い、平手の局面から始める
    void presetStartCleansUpOnce()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy treeChanged(p.branchTree, &KifuBranchTree::treeChanged);
        startFromDialog(window);
        QCOMPARE(treeChanged.count(), 1);
        QCOMPARE(p.branchTree->nodeCount(), 1);
        QVERIFY(p.match->players().humanVsHuman());
        QCOMPARE(p.match->record().startSfen, SfenUtils::hirateSfen());
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::DuringGame);
    }

    /// 棋譜をたどっている途中から新しい対局を始めても、対局の時計を動かす
    void newGameStartsMatchClockWhileBrowsing()
    {
        humanVsHumanSettings(5);
        MainWindow window;
        auto& p = window.composer().parts();
        KifuBranchNode* first = p.branchTree->addMove(p.branchTree->root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
                                                      QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));
        p.navState->setCurrentNode(first);
        p.document->markSaved(QString());
        startFromDialog(window);
        ShogiClock* clock = p.clock;
        QCOMPARE(p.match->clock(), clock);
        QVERIFY(clock->remainingMs(Turn::Black) > 0);
        QVERIFY(clock->isRunning());
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::DuringGame);
        QVERIFY(p.match->players().humanVsHuman());
        QCOMPARE(p.match->record().startSfen, SfenUtils::hirateSfen());
        p.gameSession->breakOff();
        p.document->markSaved(QString());
    }

    /// 通信対局が終わったら、ローカル対局と同じくライブ記録を確定する（B5）。
    /// 確定しないと、盤で変化を指せず、分岐の編集も評価値グラフの追従も止まったままになる
    void csaGameEndCommitsLiveRecord()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        // サーバーとのやり取りの代わりに、司令塔の知らせを直接出す
        emit p.csaGame->gameStarted(QStringLiteral("Sente"), QStringLiteral("Gote"), {});
        QVERIFY(p.liveGameSession->isActive());
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::DuringCsaGame);

        emit p.csaGame->gameEnded(CsaClient::GameResult::Win, CsaClient::GameEndCause::Resign, 0);
        QVERIFY(!p.liveGameSession->isActive());
        QVERIFY(p.document->timeRecord().endDateTime().isValid());
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::Idle);
        p.document->markSaved(QString());
    }
};

QTEST_MAIN(TestAppGameSession)
#include "tst_app_game_session.moc"
