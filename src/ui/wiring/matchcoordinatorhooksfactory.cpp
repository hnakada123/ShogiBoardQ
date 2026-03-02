#include "matchcoordinatorhooksfactory.h"

#include "evaluationgraphcontroller.h"

MatchCoordinator::Hooks MatchCoordinatorHooksFactory::buildHooks(const HookDeps& deps)
{
    MatchCoordinator::Hooks hooks;

    // UI hooks
    hooks.ui.updateTurnDisplay = [onTurnChanged = deps.onTurnChanged](MatchCoordinator::Player cur) {
        if (!onTurnChanged) return;
        const auto now = (cur == MatchCoordinator::P2)
                             ? ShogiGameController::Player2
                             : ShogiGameController::Player1;
        onTurnChanged(now);
    };
    hooks.ui.setPlayersNames = deps.setPlayersNames;
    hooks.ui.setEngineNames = deps.setEngineNames;
    hooks.ui.renderBoardFromGc = deps.renderBoard;
    hooks.ui.showGameOverDialog = deps.showGameOverDialog;
    hooks.ui.showMoveHighlights = deps.showMoveHighlights;

    // Time hooks
    hooks.time.remainingMsFor = deps.remainingMsFor;
    hooks.time.incrementMsFor = deps.incrementMsFor;
    hooks.time.byoyomiMs = deps.byoyomiMs;

    // Engine hooks
    hooks.engine.sendGoToEngine = deps.sendGo;
    hooks.engine.sendStopToEngine = deps.sendStop;
    hooks.engine.sendRawToEngine = deps.sendRaw;

    // Game hooks
    hooks.game.initializeNewGame = deps.initializeNewGame;
    hooks.game.appendKifuLine = deps.appendKifuLine;
    hooks.game.appendEvalP1 = [graph = deps.evalGraphController]() {
        if (graph) graph->redrawEngine1Graph(-1);
    };
    hooks.game.appendEvalP2 = [graph = deps.evalGraphController]() {
        if (graph) graph->redrawEngine2Graph(-1);
    };
    hooks.game.autoSaveKifu = deps.autoSaveKifu;

    return hooks;
}

MatchCoordinator::UndoHooks MatchCoordinatorHooksFactory::buildUndoHooks(const UndoDeps& deps)
{
    MatchCoordinator::UndoHooks undoHooks;
    undoHooks.updateHighlightsForPly = deps.updateHighlightsForPly;
    undoHooks.updateTurnAndTimekeepingDisplay = deps.updateTurnAndTimekeepingDisplay;
    undoHooks.isHumanSide = deps.isHumanSide;
    undoHooks.setMouseClickMode = deps.setMouseClickMode;
    return undoHooks;
}
