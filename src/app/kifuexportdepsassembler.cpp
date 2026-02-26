/// @file kifuexportdepsassembler.cpp
/// @brief KifuExportController の依存収集ロジック

#include "kifuexportdepsassembler.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "kifuexportcontroller.h"
#include "kifunavigationstate.h"
#include "matchruntimequeryservice.h"

void KifuExportDepsAssembler::assemble(MainWindow& mw)
{
    if (!mw.m_kifuExportController) return;

    KifuExportController::Dependencies deps;
    deps.gameRecord = mw.m_models.gameRecord;
    deps.kifuRecordModel = mw.m_models.kifuRecord;
    deps.gameInfoController = mw.m_gameInfoController;
    deps.timeController = mw.m_timeController;
    deps.kifuLoadCoordinator = mw.m_kifuLoadCoordinator;
    deps.recordPresenter = mw.m_recordPresenter;
    deps.match = mw.m_match;
    deps.replayController = mw.m_replayController;
    deps.gameController = mw.m_gameController;
    deps.statusBar = mw.ui ? mw.ui->statusbar : nullptr;
    deps.sfenRecord = mw.m_queryService->sfenRecord();
    deps.usiMoves = &mw.m_kifu.gameUsiMoves;
    deps.resolvedRows = nullptr;  // KifuBranchTree を優先使用
    deps.commentsByRow = &mw.m_kifu.commentsByRow;
    deps.startSfenStr = mw.m_state.startSfenStr;
    deps.playMode = mw.m_state.playMode;
    deps.humanName1 = mw.m_player.humanName1;
    deps.humanName2 = mw.m_player.humanName2;
    deps.engineName1 = mw.m_player.engineName1;
    deps.engineName2 = mw.m_player.engineName2;
    deps.activeResolvedRow = (mw.m_branchNav.navState != nullptr) ? mw.m_branchNav.navState->currentLineIndex() : 0;
    deps.currentMoveIndex = mw.m_state.currentMoveIndex;
    deps.activePly = mw.m_kifu.activePly;
    deps.currentSelectedPly = mw.m_kifu.currentSelectedPly;

    mw.m_kifuExportController->setDependencies(deps);
}
