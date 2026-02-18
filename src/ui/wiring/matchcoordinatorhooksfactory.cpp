#include "matchcoordinatorhooksfactory.h"

#include "evaluationgraphcontroller.h"

MatchCoordinator::Hooks MatchCoordinatorHooksFactory::buildHooks(const HookDeps& deps)
{
    MatchCoordinator::Hooks hooks;

    hooks.appendEvalP1 = [graph = deps.evalGraphController]() {
        if (graph) graph->redrawEngine1Graph(-1);
    };
    hooks.appendEvalP2 = [graph = deps.evalGraphController]() {
        if (graph) graph->redrawEngine2Graph(-1);
    };
    hooks.updateTurnDisplay = [onTurnChanged = deps.onTurnChanged](MatchCoordinator::Player cur) {
        if (!onTurnChanged) return;
        const auto now = (cur == MatchCoordinator::P2)
                             ? ShogiGameController::Player2
                             : ShogiGameController::Player1;
        onTurnChanged(now);
    };

    hooks.sendGoToEngine = deps.sendGo;
    hooks.sendStopToEngine = deps.sendStop;
    hooks.sendRawToEngine = deps.sendRaw;
    hooks.initializeNewGame = deps.initializeNewGame;
    hooks.renderBoardFromGc = deps.renderBoard;
    hooks.showMoveHighlights = deps.showMoveHighlights;
    hooks.appendKifuLine = deps.appendKifuLine;
    hooks.showGameOverDialog = deps.showGameOverDialog;
    hooks.remainingMsFor = deps.remainingMsFor;
    hooks.incrementMsFor = deps.incrementMsFor;
    hooks.byoyomiMs = deps.byoyomiMs;
    hooks.setPlayersNames = deps.setPlayersNames;
    hooks.setEngineNames = deps.setEngineNames;
    hooks.autoSaveKifu = deps.autoSaveKifu;

    return hooks;
}

MatchCoordinator::UndoHooks MatchCoordinatorHooksFactory::buildUndoHooks(const UndoDeps& deps)
{
    MatchCoordinator::UndoHooks undoHooks;
    undoHooks.getMainRowGuard = deps.getMainRowGuard;
    undoHooks.setMainRowGuard = deps.setMainRowGuard;
    undoHooks.updateHighlightsForPly = deps.updateHighlightsForPly;
    undoHooks.updateTurnAndTimekeepingDisplay = deps.updateTurnAndTimekeepingDisplay;
    undoHooks.isHumanSide = deps.isHumanSide;
    undoHooks.isHvH = deps.isHvH;
    return undoHooks;
}
