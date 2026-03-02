/// @file mainwindowmatchwiringdepsservice.cpp
/// @brief MainWindow の MatchCoordinatorWiring::Deps 構築処理サービス実装

#include "mainwindowmatchwiringdepsservice.h"

MatchCoordinatorWiring::Deps MainWindowMatchWiringDepsService::buildDeps(const Inputs& in) const
{
    MatchCoordinatorWiring::BuilderInputs builder;

    builder.hookDeps.evalGraphController = in.evalGraphController;
    builder.hookDeps.onTurnChanged = in.onTurnChanged;
    builder.hookDeps.sendGo = in.sendGo;
    builder.hookDeps.sendStop = in.sendStop;
    builder.hookDeps.sendRaw = in.sendRaw;
    builder.hookDeps.initializeNewGame = in.initializeNewGame;
    builder.hookDeps.renderBoard = in.renderBoard;
    builder.hookDeps.showMoveHighlights = in.showMoveHighlights;
    builder.hookDeps.appendKifuLine = in.appendKifuLine;
    builder.hookDeps.showGameOverDialog = in.showGameOverDialog;
    builder.hookDeps.remainingMsFor = in.remainingMsFor;
    builder.hookDeps.incrementMsFor = in.incrementMsFor;
    builder.hookDeps.byoyomiMs = in.byoyomiMs;
    builder.hookDeps.setPlayersNames = in.setPlayersNames;
    builder.hookDeps.setEngineNames = in.setEngineNames;
    builder.hookDeps.autoSaveKifu = in.autoSaveKifu;

    builder.undoDeps.updateHighlightsForPly = in.updateHighlightsForPly;
    builder.undoDeps.updateTurnAndTimekeepingDisplay = in.updateTurnAndTimekeepingDisplay;
    builder.undoDeps.isHumanSide = in.isHumanSide;
    builder.undoDeps.setMouseClickMode = in.setMouseClickMode;

    builder.gc = in.gc;
    builder.view = in.view;
    builder.usi1 = in.usi1;
    builder.usi2 = in.usi2;
    builder.comm1 = in.comm1;
    builder.think1 = in.think1;
    builder.comm2 = in.comm2;
    builder.think2 = in.think2;
    builder.sfenRecord = in.sfenRecord;
    builder.playMode = in.playMode;
    builder.timePresenter = in.timePresenter;
    builder.boardController = in.boardController;
    builder.kifuRecordModel = in.kifuRecordModel;
    builder.gameMoves = in.gameMoves;
    builder.positionStrList = in.positionStrList;
    builder.currentMoveIndex = in.currentMoveIndex;

    builder.ensureTimeController = in.ensureTimeController;
    builder.ensureEvaluationGraphController = in.ensureEvaluationGraphController;
    builder.ensurePlayerInfoWiring = in.ensurePlayerInfoWiring;
    builder.ensureUsiCommandController = in.ensureUsiCommandController;
    builder.ensureUiStatePolicyManager = in.ensureUiStatePolicyManager;
    builder.connectBoardClicks = in.connectBoardClicks;
    builder.connectMoveRequested = in.connectMoveRequested;

    builder.getClock = in.getClock;
    builder.getTimeController = in.getTimeController;
    builder.getEvalGraphController = in.getEvalGraphController;
    builder.getUiStatePolicy = in.getUiStatePolicy;

    return MatchCoordinatorWiring::buildDeps(builder);
}
