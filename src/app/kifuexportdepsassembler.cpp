/// @file kifuexportdepsassembler.cpp
/// @brief KifuExportController の依存収集ロジック

#include "kifuexportdepsassembler.h"
#include "mainwindowruntimerefs.h"
#include "kifuexportcontroller.h"
#include "kifunavigationstate.h"

void KifuExportDepsAssembler::assemble(KifuExportController* controller, const MainWindowRuntimeRefs& refs)
{
    if (!controller) return;

    KifuExportController::Dependencies deps;
    deps.gameRecord = refs.models.gameRecordModel;
    deps.kifuRecordModel = refs.models.kifuRecordModel;
    deps.gameInfoController = refs.analysis.gameInfoController;
    deps.timeController = refs.uiController.timeController;
    deps.kifuLoadCoordinator = refs.kifuService.kifuLoadCoordinator;
    deps.recordPresenter = refs.kifuService.recordPresenter;
    deps.match = refs.gameService.match;
    deps.replayController = refs.kifuService.replayController;
    deps.gameController = refs.gameService.gameController;
    deps.statusBar = refs.ui.statusBar;
    deps.sfenRecord = refs.kifu.sfenRecord;
    deps.usiMoves = refs.kifu.gameUsiMoves;
    deps.resolvedRows = nullptr;  // KifuBranchTree を優先使用
    deps.commentsByRow = refs.kifu.commentsByRow;
    deps.startSfenStr = refs.state.startSfenStr ? *refs.state.startSfenStr : QString();
    deps.playMode = refs.state.playMode ? *refs.state.playMode : PlayMode::NotStarted;
    deps.humanName1 = refs.player.humanName1 ? *refs.player.humanName1 : QString();
    deps.humanName2 = refs.player.humanName2 ? *refs.player.humanName2 : QString();
    deps.engineName1 = refs.player.engineName1 ? *refs.player.engineName1 : QString();
    deps.engineName2 = refs.player.engineName2 ? *refs.player.engineName2 : QString();
    deps.activeResolvedRow = (refs.branchNav.navState != nullptr) ? refs.branchNav.navState->currentLineIndex() : 0;
    deps.currentMoveIndex = refs.state.currentMoveIndex ? *refs.state.currentMoveIndex : 0;
    deps.activePly = refs.kifu.activePly ? *refs.kifu.activePly : 0;
    deps.currentSelectedPly = refs.kifu.currentSelectedPly ? *refs.kifu.currentSelectedPly : 0;

    controller->setDependencies(deps);
}
