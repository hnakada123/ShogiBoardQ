/// @file tst_app_game_session.cpp
/// @brief 実際の対局サービス（本体の合成ルートで組み立てた GameSession）を使うUI操作フローの回帰テスト
#include <QtTest>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QDir>
#include <memory>
// 通信対局の司令塔の手番の状態は、サーバーとのやり取りでしか変わらないため、テストでは直接設定する
// （CsaGameCoordinator が TestAppGameSession を friend にしている）
#include "appstate.h"
#include "csagamecoordinator.h"
#include "csagamesession.h"
#include "boardchromewidget.h"
#include "shogiview.h"
#include "boardmovecontroller.h"
#include "elidelabel.h"
#include "gamechannel.h"
#include "uistatepolicymanager.h"
#include "livegamesession.h"
#include "gamesession.h"
#include "evaluationgraphpresenter.h"
#include "evaluationseries.h"
#include "kifudocument.h"
#include "kifunavigationstate.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "matchcoordinator.h"
#include "humanturnpolicy.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
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

    /// 棋譜解析・詰み探索・検討の間は、盤で指せない（盤の操作の行き先が受け付けない）
    void busyStatesRejectBoardInput()
    {
        ShogiGameController gc;
        AppActivityState activity;
        HumanTurnPolicy::Deps deps;
        deps.activity = &activity;
        deps.gameController = &gc;
        HumanTurnPolicy policy(deps);
        for (const auto state : {AppActivityState::Activity::DuringAnalysis, AppActivityState::Activity::DuringTsumeSearch,
                                 AppActivityState::Activity::DuringConsideration}) {
            activity.setActivity(state);
            QVERIFY(!policy.isHumanTurnNow());
        }
    }

    void startDialogCancellationKeepsGame()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy treeReset(p.branchTree, &KifuBranchTree::reset);
        QSignalSpy appended(p.branchTree, &KifuBranchTree::nodeAppended);
        const int rootId = p.branchTree->root()->nodeId();
        m_acceptDialog = false;
        m_dialogSeen = false;
        QTimer::singleShot(0, this, &TestAppGameSession::respondToStartDialog);
        p.gameSession->startGameFromDialog();
        QVERIFY(m_dialogSeen);
        // 棋譜は作り直さない
        QCOMPARE(treeReset.count(), 0);
        QCOMPARE(appended.count(), 0);
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
        QSignalSpy treeReset(p.branchTree, &KifuBranchTree::reset);
        startFromDialog(window);
        QCOMPARE(treeReset.count(), 1);
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
        KifuBranchNode* first = TestTree::addMove(*p.branchTree, p.branchTree->root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
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

    /// 通信対局を始めると、前の対局・棋譜解析の評価値グラフの点を消す（ローカル対局の開始と同じ）
    void csaGameStartClearsPreviousEvaluations()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        p.evalGraph->beginAnalysis(QStringLiteral("Engine"), 0);
        p.evalGraph->plotAnalysisScore(1, UsiScore::cp(120));
        QCOMPARE(p.evalGraph->series()->count(0), 1);
        emit p.csaGame->gameStarted(QStringLiteral("Sente"), QStringLiteral("Gote"), {});
        QCOMPARE(p.evalGraph->series()->count(0), 0);
        emit p.csaGame->gameEnded(CsaClient::GameResult::Win, CsaClient::GameEndCause::Resign, 0);
        p.document->markSaved(QString());
    }

    /// 通信対局の開始もローカル対局と同じく対局者の表示を通す（サーバーが知らせた名前を盤と対局情報に出す）
    void csaGameStartShowsPlayersLikeLocalGame()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        p.csaGame->m_progress.summary.blackName = QStringLiteral("Sente");
        p.csaGame->m_progress.summary.whiteName = QStringLiteral("Gote");
        emit p.csaGame->gameStarted(QStringLiteral("Sente"), QStringLiteral("Gote"), {});
        QCOMPARE(p.shogiView->chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲Sente"));
        QCOMPARE(p.shogiView->chrome()->card(Turn::White)->nameLabel()->fullText(), QStringLiteral("▽Gote"));
        QCOMPARE(p.document->headerPlayerName(Turn::Black), QStringLiteral("Sente"));
        QCOMPARE(p.document->players().name(Turn::White), QStringLiteral("Gote"));
        emit p.csaGame->gameEnded(CsaClient::GameResult::Win, CsaClient::GameEndCause::Resign, 0);
        p.document->markSaved(QString());
    }

    /// 対局中の操作の送り先は、動作状態で決まる1つの窓口（対局していなければ無い）
    void activeChannelFollowsActivity()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        auto& activity = window.composer().state().activity;
        QVERIFY(!p.gameSession->activeChannel());
        startFromDialog(window);
        GameChannel* local = p.gameSession->activeChannel();
        QVERIFY(local);
        QVERIFY(local->judgesDeclaration());
        QVERIFY(local->isHumanToMove());
        QCOMPARE(local->sideToMove(), Turn::Black);
        activity.setActivity(AppActivityState::Activity::DuringCsaGame);
        GameChannel* csa = p.gameSession->activeChannel();
        QVERIFY(csa && csa != local);
        QVERIFY(!csa->judgesDeclaration());
        activity.setActivity(AppActivityState::Activity::DuringGame);
        p.gameSession->breakOff();
        p.document->markSaved(QString());
    }

    /// ローカル対局も通信対局も、終局した棋譜を GameSession が自動保存する（中断は保存しない）
    void finishedGamesAreAutoSaved()
    {
        humanVsHumanSettings();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/isAutoSaveKifu", true);
        settings.setValue("GameSettings/kifuSaveDir", dir.path());
        settings.sync();
        MainWindow window;
        auto& p = window.composer().parts();
        const auto savedFiles = [&dir]() { return QDir(dir.path()).entryList(QDir::Files); };

        startFromDialog(window);
        p.gameSession->breakOff();
        QVERIFY(savedFiles().isEmpty());
        p.document->markSaved(QString());
        startFromDialog(window);
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::DuringGame);
        p.gameSession->resign();
        QCOMPARE(savedFiles().size(), 1);
        QVERIFY(savedFiles().first().endsWith(QStringLiteral(".kifu")));

        // 通信対局は接続ダイアログで選んだ形式
        p.csaGame->m_options.autoSave = {true, dir.path(), QStringLiteral("csa")};
        emit p.csaGame->gameStarted(QStringLiteral("Sente"), QStringLiteral("Gote"), {});
        emit p.csaGame->gameEnded(CsaClient::GameResult::Win, CsaClient::GameEndCause::Resign, 0);
        QCOMPARE(savedFiles().size(), 2);
        QVERIFY(savedFiles().filter(QStringLiteral(".csa")).size() == 1);
        settings.setValue("GameSettings/isAutoSaveKifu", false);
        settings.sync();
    }

    /// 中断した対局の状態は司令塔だけが持ち、「再開」の有効・無効も司令塔に問い合わせる
    void interruptedGameResumesFromCoordinatorState()
    {
        humanVsHumanSettings();
        MainWindow window;
        auto& p = window.composer().parts();
        startFromDialog(window);
        p.boardMoves->submit(QPoint(7, 7), QPoint(7, 6));
        QCOMPARE(p.match->record().moveCount(), 1);
        p.gameSession->breakOff();
        QVERIFY(p.match->canResumeInterruptedGame());
        QVERIFY(p.uiStatePolicy->isEnabled(UiStatePolicyManager::UiElement::GameResume));
        const int interruptNode = p.match->interruptedGame()->terminalNodeId;
        QVERIFY(p.branchTree->nodeAt(interruptNode));

        p.gameSession->resumeInterruptedGame();
        QVERIFY(!p.match->canResumeInterruptedGame());
        QVERIFY(!p.uiStatePolicy->isEnabled(UiStatePolicyManager::UiElement::GameResume));
        QCOMPARE(window.composer().state().activity.activity(), AppActivityState::Activity::DuringGame);
        QVERIFY(!p.branchTree->nodeAt(interruptNode));

        // 中断の後に棋譜を変えたら再開できない
        p.gameSession->breakOff();
        QVERIFY(p.match->canResumeInterruptedGame());
        TestTree::addMove(*p.branchTree, p.branchTree->root(), ShogiMove(), QStringLiteral("▲２六歩(27)"),
                              QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/7P1/PPPPPPP1P/1B5R1/LNSGKGSNL w - 2"));
        QVERIFY(!p.match->canResumeInterruptedGame());
        QVERIFY(!p.uiStatePolicy->isEnabled(UiStatePolicyManager::UiElement::GameResume));
        p.document->markSaved(QString());
    }
};

QTEST_MAIN(TestAppGameSession)
#include "tst_app_game_session.moc"
