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
    deps.gameRecord = refs.gameRecordModel;
    deps.kifuRecordModel = refs.kifuRecordModel;
    deps.gameInfoController = refs.gameInfoController;
    deps.timeController = refs.timeController;
    deps.kifuLoadCoordinator = refs.kifuLoadCoordinator;
    deps.recordPresenter = refs.recordPresenter;
    deps.match = refs.match;
    deps.replayController = refs.replayController;
    deps.gameController = refs.gameController;
    deps.statusBar = refs.statusBar;
    deps.sfenRecord = refs.sfenRecord;
    deps.usiMoves = refs.gameUsiMoves;
    deps.resolvedRows = nullptr;  // KifuBranchTree を優先使用
    deps.commentsByRow = refs.commentsByRow;
    deps.startSfenStr = refs.startSfenStr ? *refs.startSfenStr : QString();
    deps.playMode = refs.playMode ? *refs.playMode : PlayMode::NotStarted;
    deps.humanName1 = refs.humanName1 ? *refs.humanName1 : QString();
    deps.humanName2 = refs.humanName2 ? *refs.humanName2 : QString();
    deps.engineName1 = refs.engineName1 ? *refs.engineName1 : QString();
    deps.engineName2 = refs.engineName2 ? *refs.engineName2 : QString();
    deps.activeResolvedRow = (refs.navState != nullptr) ? refs.navState->currentLineIndex() : 0;
    deps.currentMoveIndex = refs.currentMoveIndex ? *refs.currentMoveIndex : 0;
    deps.activePly = refs.activePly ? *refs.activePly : 0;
    deps.currentSelectedPly = refs.currentSelectedPly ? *refs.currentSelectedPly : 0;

    controller->setDependencies(deps);
}
