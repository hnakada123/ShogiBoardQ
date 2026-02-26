/// @file mainwindowruntimerefsfactory.cpp
/// @brief MainWindowRuntimeRefs スナップショットの構築ロジック

#include "mainwindowruntimerefsfactory.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "playmodepolicyservice.h"

MainWindowRuntimeRefs MainWindowRuntimeRefsFactory::build(MainWindow& mw)
{
    MainWindowRuntimeRefs refs;

    // --- UI 参照 ---
    refs.parentWidget = &mw;
    refs.mainWindow = &mw;
    refs.statusBar = mw.ui->statusbar;

    // --- サービス参照 ---
    refs.match = mw.m_match;
    refs.gameController = mw.m_gameController;
    refs.shogiView = mw.m_shogiView;
    refs.kifuLoadCoordinator = mw.m_kifuLoadCoordinator;
    refs.csaGameCoordinator = mw.m_csaGameCoordinator;

    // --- モデル参照 ---
    refs.kifuRecordModel = mw.m_models.kifuRecord;
    refs.considerationModel = &mw.m_models.consideration;

    // --- 状態参照（ポインタ） ---
    refs.playMode = &mw.m_state.playMode;
    refs.currentMoveIndex = &mw.m_state.currentMoveIndex;
    refs.currentSfenStr = &mw.m_state.currentSfenStr;
    refs.startSfenStr = &mw.m_state.startSfenStr;
    refs.skipBoardSyncForBranchNav = &mw.m_state.skipBoardSyncForBranchNav;

    // --- 棋譜参照 ---
    refs.sfenRecord = mw.sfenRecord();
    refs.gameMoves = &mw.m_kifu.gameMoves;
    refs.gameUsiMoves = &mw.m_kifu.gameUsiMoves;
    refs.moveRecords = &mw.m_kifu.moveRecords;
    refs.positionStrList = &mw.m_kifu.positionStrList;
    refs.activePly = &mw.m_kifu.activePly;
    refs.currentSelectedPly = &mw.m_kifu.currentSelectedPly;
    refs.saveFileName = &mw.m_kifu.saveFileName;

    // --- 分岐ナビゲーション参照 ---
    refs.branchTree = mw.m_branchNav.branchTree;
    refs.navState = mw.m_branchNav.navState;
    refs.displayCoordinator = mw.m_branchNav.displayCoordinator;

    // --- コントローラ / Wiring 参照 ---
    refs.recordPane = mw.m_recordPane;
    refs.recordPresenter = mw.m_recordPresenter;
    refs.replayController = mw.m_replayController;
    refs.timeController = mw.m_timeController;
    refs.boardController = mw.m_boardController;
    refs.positionEditController = mw.m_posEdit;
    refs.dialogCoordinator = mw.m_dialogCoordinator;
    refs.playerInfoWiring = mw.m_playerInfoWiring;

    // --- モデル参照（追加） ---
    refs.thinking1 = mw.m_models.thinking1;
    refs.thinking2 = mw.m_models.thinking2;
    refs.consideration = mw.m_models.consideration;
    refs.commLog1 = mw.m_models.commLog1;
    refs.commLog2 = mw.m_models.commLog2;
    refs.gameRecordModel = mw.m_models.gameRecord;

    // --- UI フォーム ---
    refs.uiForm = mw.ui.get();

    // --- 状態参照（追加） ---
    refs.commentsByRow = &mw.m_kifu.commentsByRow;
    refs.resumeSfenStr = &mw.m_state.resumeSfenStr;
    refs.onMainRowGuard = &mw.m_kifu.onMainRowGuard;

    // --- その他参照 ---
    refs.gameInfoController = mw.m_gameInfoController;
    refs.evalChart = mw.m_evalChart;
    refs.evalGraphController = mw.m_evalGraphController;
    refs.analysisPresenter = mw.m_analysisPresenter;
    refs.analysisTab = mw.m_analysisTab;

    // PlayModePolicyService の依存を最新状態に更新
    if (mw.m_playModePolicy) {
        PlayModePolicyService::Deps policyDeps;
        policyDeps.playMode = &mw.m_state.playMode;
        policyDeps.gameController = mw.m_gameController;
        policyDeps.match = mw.m_match;
        policyDeps.csaGameCoordinator = mw.m_csaGameCoordinator;
        mw.m_playModePolicy->updateDeps(policyDeps);
    }

    return refs;
}
