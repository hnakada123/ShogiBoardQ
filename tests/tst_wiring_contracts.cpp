/// アプリと同じ配線を使い、終局通知時に棋譜・セッション・UIが確定済みか確認する。
#include <QtTest>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "gamestartcoordinator.h"
#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "sessionlifecyclecoordinator.h"
#include "uistatepolicymanager.h"
#include "shogiclock.h"
#include "timecontrolcontroller.h"
#include "consecutivegamescontroller.h"
#include "boardsetupcontroller.h"
#include "undoflowservice.h"
#include "playerinfowiring.h"
#include "kifunavigationstate.h"

class TestWiringContracts : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    MainWindowComponents* m_parts = nullptr;
    bool m_endWasCommitted = false;
    int m_consistentMoves = 0;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
public:
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
        p.playerInfoWiring->onPlayerNamesResolved({}, {}, {}, {}, int(PlayMode::EvenHumanVsEngine));
        QCOMPARE(state.playMode, PlayMode::NotStarted);
        QCOMPARE(p.match->playMode(), PlayMode::NotStarted);
        GameStartCoordinator::Ctx ctx;
        ctx.dialogData.startingPositionNumber = 1;
        ctx.dialogData.isHuman1 = ctx.dialogData.isHuman2 = true;
        ctx.startSfenStr = &state.startSfenStr;
        ctx.currentSfenStr = &state.currentSfenStr;
        QString error;
        QVERIFY2(p.gameStart->initializeGame(ctx, error), qPrintable(error));
        connect(p.gameController, &ShogiGameController::moveCommitted, this, &TestWiringContracts::observeMove);
        p.boardSetupController->onMoveRequested(QPoint(7, 7), QPoint(7, 6));
        p.boardSetupController->onMoveRequested(QPoint(3, 3), QPoint(3, 4));
        QCOMPARE(m_consistentMoves, 2);
        QCOMPARE(state.sfenRecord.size(), 3);
        p.undoFlowService->undoLastTwoMoves();
        QCOMPARE(state.sfenRecord.size(), 1);
        QCOMPARE(p.liveGameSession->totalPly(), 0);
        QCOMPARE(p.navState->currentPly(), 0);
        QCOMPARE(p.kifuRecord->rowCount(), 1);
        QCOMPARE(state.currentSfenStr, state.sfenRecord.first());
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
        GameStartCoordinator::Ctx ctx;
        ctx.dialogData.startingPositionNumber = 1;
        ctx.dialogData.isHuman1 = ctx.dialogData.isHuman2 = true;
        ctx.dialogData.basicTimeMinutes1 = 3;
        ctx.dialogData.humanName1 = QStringLiteral("太郎");
        ctx.dialogData.humanName2 = QStringLiteral("花子");
        ctx.startSfenStr = &state.startSfenStr;
        ctx.currentSfenStr = &state.currentSfenStr;
        QString error;
        QVERIFY2(p.gameStart->initializeGame(ctx, error), qPrintable(error));
        QCOMPARE(p.uiStatePolicy->currentState(), UiStatePolicyManager::AppState::DuringGame);
        QCOMPARE(state.playMode, PlayMode::HumanVsHuman);
        QVERIFY(p.liveGameSession->isActive());
        QCOMPARE(p.match->clock()->timeControl().black.mainMs, 180000);
        QCOMPARE(p.timeController->timeControl().black.mainMs, 180000);
        QVERIFY(p.timeController->gameStartDateTime().isValid());

        p.match->setGameOver({MatchCause::Resignation, Turn::Black});
        QVERIFY(m_endWasCommitted);
        QCOMPARE(p.kifuRecord->rowCount(), 2);
        QVERIFY(p.timeController->gameEndDateTime().isValid());
        p.match->setGameOver({MatchCause::Resignation, Turn::Black});
        QCOMPARE(p.kifuRecord->rowCount(), 2);
        p.gameRecord->clearDirty();
    }
    void resetDoesNotStartLiveSession()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        p.liveGameSession->startFromRoot();
        QVERIFY(p.liveGameSession->isActive());
        p.sessionLifecycle->resetToInitialState();
        QVERIFY(!p.liveGameSession->isActive());
        QCOMPARE(p.branchTree->nodeCount(), 1);
        QCOMPARE(p.kifuRecord->rowCount(), 1);
        QCOMPARE(p.uiStatePolicy->currentState(), UiStatePolicyManager::AppState::Idle);
    }
    void consecutiveSettingsReachController()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        emit p.gameStart->consecutiveGamesConfigured(3, true);
        QVERIFY(p.consecutiveGamesController->shouldStartNextGame());
        emit p.gameStart->consecutiveGamesConfigured(1, false);
        QVERIFY(!p.consecutiveGamesController->shouldStartNextGame());
    }
};
QTEST_MAIN(TestWiringContracts)
#include "tst_wiring_contracts.moc"
