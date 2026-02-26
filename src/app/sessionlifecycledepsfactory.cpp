/// @file sessionlifecycledepsfactory.cpp
/// @brief SessionLifecycleCoordinator 用 Deps を生成するファクトリの実装

#include "sessionlifecycledepsfactory.h"

SessionLifecycleCoordinator::Deps SessionLifecycleDepsFactory::createDeps(
    const MainWindowRuntimeRefs& refs,
    const Callbacks& callbacks)
{
    SessionLifecycleCoordinator::Deps deps;

    // コントローラ
    deps.replayController = refs.replayController;
    deps.gameController = refs.gameController;
    deps.gameStateController = refs.gameStateController;

    // 状態ポインタ
    deps.playMode = refs.playMode;
    deps.startSfenStr = refs.startSfenStr;
    deps.currentSfenStr = refs.currentSfenStr;
    deps.currentSelectedPly = refs.currentSelectedPly;

    // コールバック（リセット処理用）
    deps.clearGameStateFields = callbacks.clearGameStateFields;
    deps.resetEngineState = callbacks.resetEngineState;
    deps.onPreStartCleanupRequested = callbacks.onPreStartCleanupRequested;
    deps.resetModels = callbacks.resetModels;
    deps.resetUiState = callbacks.resetUiState;

    // コールバック（startNewGame用）
    deps.clearEvalState = callbacks.clearEvalState;
    deps.unlockGameOverStyle = callbacks.unlockGameOverStyle;
    deps.startGame = callbacks.startGame;

    // handleGameEnded 用
    deps.timeController = refs.timeController;
    deps.consecutiveGamesController = refs.consecutiveGamesController;
    deps.updateEndTime = callbacks.updateEndTime;
    deps.startNextConsecutiveGame = callbacks.startNextConsecutiveGame;

    // applyTimeControl 用
    deps.match = refs.match;
    deps.shogiView = refs.shogiView;
    deps.lastTimeControl = callbacks.lastTimeControl;
    deps.updateGameInfoWithTimeControl = callbacks.updateGameInfoWithTimeControl;

    return deps;
}
