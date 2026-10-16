/// アプリと同じ配線を使い、終局通知時に棋譜・セッション・UIが確定済みか確認する。
#include <QtTest>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QTimer>
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "gamestartoptionsbuilder.h"
#include "kifudocument.h"
#include "kifubranchtree.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "gamesession.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "uistatepolicymanager.h"
#include "shogiclock.h"
#include "matchtimerecord.h"
#include "consecutivegameseries.h"
#include "boardmovecontroller.h"
#include "playerinfopresenter.h"
#include "kifunavigationstate.h"

class TestWiringContracts : public QObject {
    Q_OBJECT

    /// 対局開始ダイアログで選んだのと同じ経路（計画を作って GameSession が始める）で、表示中の手から対局を始める
    static bool startGame(MainWindowComponents& p, const StartGameDialogData& data, QString* error)
    {
        GameStartRequest request;
        request.dialogData = data;
        request.recordLine = p.navState->currentRecordLine();
        request.startNodeId = p.navState->currentNode()->nodeId();
        const std::optional<GameStartPlan> plan = GameStartOptionsBuilder::plan(request, error);
        return plan && p.gameSession->startGame(*plan, /*beginSeries=*/true);
    }

    QTemporaryDir m_config;
    MainWindowComponents* m_parts = nullptr;
    QTimer m_messageCloser;
    bool m_endWasCommitted = false;
    int m_consistentMoves = 0;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        // 終局の知らせ（メッセージボックス）は出たらすぐ閉じる
        m_messageCloser.setInterval(10);
        connect(&m_messageCloser, &QTimer::timeout, this, &TestWiringContracts::closeMessageBoxes);
        m_messageCloser.start();
    }
public:
    void closeMessageBoxes()
    {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->accept();
    }
    void observeMove(Turn, int ply)
    {
        const auto* node = m_parts->liveGameSession->liveNode();
        if (node && node->ply() == ply && m_parts->kifuRecord->rowCount() == ply + 1
            && m_parts->navState->currentNode() == node) ++m_consistentMoves;
    }
    void observeEnd()
    {
        m_endWasCommitted = m_parts->branchTree->mainLine().last()->isTerminal()
            && !m_parts->liveGameSession->isActive()
            && m_parts->uiStatePolicy->currentState() == UiStatePolicyManager::AppState::Idle;
    }
private slots:
    void humanMoveAndUndoPublishConsistentRecord()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        auto& state = window.composer().state();
        m_parts = &p;
        m_consistentMoves = 0;
        // 名前の通知だけでは活動を開始しない。
        MatchPlayers humanEngine;
        humanEngine.whiteIsEngine = true;
        p.playerInfo->showPlayers(humanEngine);
        QCOMPARE(state.activity.activity(), AppActivityState::Activity::Idle);
        QCOMPARE(p.document->players(), MatchPlayers());
        StartGameDialogData data;
        data.startingPositionNumber = 1;
        data.isHuman1 = data.isHuman2 = true;
        QString error;
        QVERIFY2(startGame(p, data, &error), qPrintable(error));
        connect(p.gameController, &ShogiGameController::moveCommitted, this, &TestWiringContracts::observeMove);
        p.boardMoves->submit(QPoint(7, 7), QPoint(7, 6));
        p.boardMoves->submit(QPoint(3, 3), QPoint(3, 4));
        QCOMPARE(m_consistentMoves, 2);
        QCOMPARE(p.match->record().moveCount(), 2);
        QCOMPARE(p.navState->currentSfen(), p.match->record().lastSfen());
        p.gameSession->undoTwoMoves();
        QCOMPARE(p.match->record().moveCount(), 0);
        QCOMPARE(p.liveGameSession->liveNode()->ply(), 0);
        QCOMPARE(p.navState->currentPly(), 0);
        QCOMPARE(p.kifuRecord->rowCount(), 1);
        QCOMPARE(p.navState->currentSfen(), p.match->record().startSfen);
        QVERIFY(p.document->isModified());
        p.document->markSaved(QString());
        m_parts = nullptr;
    }
    void startAndEndUseRealReceivers()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        auto& state = window.composer().state();
        m_parts = &p;
        m_endWasCommitted = false;
        connect(p.match, &MatchCoordinator::gameEnded, this, &TestWiringContracts::observeEnd);
        StartGameDialogData data;
        data.startingPositionNumber = 1;
        data.isHuman1 = data.isHuman2 = true;
        data.basicTimeMinutes1 = 3;
        data.humanName1 = QStringLiteral("太郎");
        data.humanName2 = QStringLiteral("花子");
        QString error;
        QVERIFY2(startGame(p, data, &error), qPrintable(error));
        QCOMPARE(p.uiStatePolicy->currentState(), UiStatePolicyManager::AppState::DuringGame);
        QCOMPARE(state.activity.activity(), AppActivityState::Activity::DuringGame);
        QVERIFY(p.document->players().humanVsHuman());
        QCOMPARE(p.document->players().name(Turn::Black), QStringLiteral("太郎"));
        QCOMPARE(p.document->players().name(Turn::White), QStringLiteral("花子"));
        QVERIFY(p.liveGameSession->isActive());
        QCOMPARE(p.match->clock()->timeControl().black.mainMs, 180000);
        QCOMPARE(p.document->timeRecord().timeControl().black.mainMs, 180000);
        QVERIFY(p.document->timeRecord().startDateTime().isValid());

        p.gameSession->resign();
        QVERIFY(m_endWasCommitted);
        QCOMPARE(p.kifuRecord->rowCount(), 2);
        QVERIFY(p.document->timeRecord().endDateTime().isValid());
        p.gameSession->resign();
        QCOMPARE(p.kifuRecord->rowCount(), 2);
        p.document->markSaved(QString());
    }
    void resetDoesNotStartLiveSession()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        p.liveGameSession->startFromRoot();
        QVERIFY(p.liveGameSession->isActive());
        p.gameSession->resetToInitialState();
        QVERIFY(!p.liveGameSession->isActive());
        QCOMPARE(p.branchTree->nodeCount(), 1);
        QCOMPARE(p.kifuRecord->rowCount(), 1);
        QCOMPARE(p.uiStatePolicy->currentState(), UiStatePolicyManager::AppState::Idle);
    }
    void consecutiveSettingsReachController()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        StartGameDialogData data;
        data.startingPositionNumber = 1;
        data.isHuman1 = data.isHuman2 = true;
        data.consecutiveGames = 3;
        data.isSwitchTurnEachGame = true;
        QString error;
        QVERIFY2(startGame(p, data, &error), qPrintable(error));
        QVERIFY(p.consecutiveGames->shouldStartNextGame());
        QCOMPARE(p.consecutiveGames->totalGames(), 3);
        p.match->handleBreakOff();

        data.consecutiveGames = 1;
        QVERIFY2(startGame(p, data, &error), qPrintable(error));
        QVERIFY(!p.consecutiveGames->shouldStartNextGame());
        p.match->handleBreakOff();
        p.document->markSaved(QString());
    }
};
QTEST_MAIN(TestWiringContracts)
#include "tst_wiring_contracts.moc"
