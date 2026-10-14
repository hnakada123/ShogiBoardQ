/// @file mainwindowresetservice.cpp
/// @brief MainWindow の状態初期化ロジックの実装

#include "mainwindowresetservice.h"
#include "appstate.h"
#include "commentcoordinator.h"
#include "josekiwindowwiring.h"
#include "playerinfowiring.h"

#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "engineanalysistab.h"
#include "considerationtabmanager.h"
#include "engineinfowidget.h"
#include "kifuanalysislistmodel.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "kifunavigationstate.h"
#include "kifudisplaycoordinator.h"
#include "boardinteractioncontroller.h"
#include "gamerecordmodel.h"
#include "timecontrolcontroller.h"
#include "gameinfopanecontroller.h"
#include "kifuloadcoordinator.h"
#include "kifubranchtree.h"
#include "livegamesession.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"

#include <QStringList>

void MainWindowResetService::clearSessionDependentUi(const SessionUiDeps& deps) const
{
    if (deps.commLog1) {
        deps.commLog1->clear();
    }
    if (deps.commLog2) {
        deps.commLog2->clear();
    }
    if (deps.thinking1) {
        deps.thinking1->clearAllItems();
    }
    if (deps.thinking2) {
        deps.thinking2->clearAllItems();
    }
    if (deps.consideration) {
        deps.consideration->clearAllItems();
    }

    if (deps.analysisTab) {
        deps.analysisTab->clearUsiLog();
        deps.analysisTab->clearCsaLog();

        if (deps.analysisTab->info1()) {
            deps.analysisTab->info1()->setDisplayNameFallback(QString());
        }
        if (deps.analysisTab->info2()) {
            deps.analysisTab->info2()->setDisplayNameFallback(QString());
        }
        if (deps.analysisTab->considerationTabManager()->considerationInfo()) {
            deps.analysisTab->considerationTabManager()->considerationInfo()->setModel(nullptr);
        }

        deps.analysisTab->considerationTabManager()->setConsiderationEngineName(QString());
        deps.analysisTab->considerationTabManager()->resetElapsedTimer();
        deps.analysisTab->considerationTabManager()->setConsiderationRunning(false);
    }

    if (deps.analysisModel) {
        deps.analysisModel->clearAllItems();
    }
}

void MainWindowResetService::clearUiBeforeKifuLoad(const SessionUiDeps& deps) const
{
    clearSessionDependentUi(deps);

    if (deps.evalChart) {
        deps.evalChart->clearAll();
    }
    if (deps.evalGraphController) {
        deps.evalGraphController->clearScores();
    }
    if (deps.commentCoordinator) {
        deps.commentCoordinator->broadcastComment(QString(), true);
    }
}

void MainWindowResetService::resetModels(const ModelResetDeps& deps,
                                         const QString& hirateStartSfen) const
{
    clearPresentationState(deps);
    clearGameDataModels(deps, hirateStartSfen);
    resetBranchTreeForNewState(deps, hirateStartSfen);
}

void MainWindowResetService::clearPresentationState(const ModelResetDeps& deps) const
{
    if (deps.navState) {
        deps.navState->clearLineSelectionMemory();
    }

    if (deps.displayCoordinator) {
        deps.displayCoordinator->resetTracking();
    }

    if (deps.boardController) {
        deps.boardController->cancelPendingClick();
    }
}

void MainWindowResetService::clearGameDataModels(const ModelResetDeps& deps,
                                                 const QString& hirateStartSfen) const
{
    if (deps.gameRecord) {
        deps.gameRecord->clear();
    }

    if (deps.evalGraphController) {
        deps.evalGraphController->setEngine1Name({});
        deps.evalGraphController->setEngine2Name({});
    }

    if (deps.timeController) {
        deps.timeController->clearGameEndTime();
    }

    if (deps.sfenRecord) {
        deps.sfenRecord->clear();
        deps.sfenRecord->append(hirateStartSfen);
    }

    if (deps.gameInfoController) {
        deps.gameInfoController->resetGameInfo();
    }
}

void MainWindowResetService::resetBranchTreeForNewState(const ModelResetDeps& deps,
                                                        const QString& hirateStartSfen) const
{
    if (deps.liveGameSession && deps.liveGameSession->isActive()) {
        deps.liveGameSession->discard();
    }

    // 読み込み途中の棋譜は捨てる（分岐ツリーはこの下で作り直す）
    if (deps.kifuLoadCoordinator) {
        deps.kifuLoadCoordinator->cancelLoad();
    }
    if (deps.branchTree) {
        if (deps.navState) {
            deps.navState->setCurrentNode(nullptr);
            deps.navState->resetPreferredLineIndex();
        }

        deps.branchTree->setRootSfen(hirateStartSfen);

        if (deps.navState) {
            deps.navState->goToRoot();
        }
    }
}

void MainWindowResetService::resetUiState(const UiResetDeps& deps,
                                          const QString& hirateStartSfen) const
{
    if (deps.gameController && deps.gameController->board() && deps.shogiView) {
        deps.gameController->board()->setSfen(hirateStartSfen);
        deps.shogiView->applyBoardAndRender(deps.gameController->board());
    }

    if (deps.shogiView) {
        deps.shogiView->updateTurnIndicator(Turn::Black);
    }

    // 前の対局者名を残さない（対局情報の初期値「先手」「後手」を表示する）
    if (deps.playerInfoWiring) {
        deps.playerInfoWiring->syncBoardNamesWithGameInfo();
    }

    if (deps.uiStatePolicy) {
        deps.uiStatePolicy->transitionToIdle();
    }

    if (deps.josekiWiring) {
        deps.josekiWiring->updateJosekiWindow();
    }
}

void MainWindowResetService::clearGameStateFields(AppState& state) const
{
    state.errorOccurred = false;
    state.humanName1.clear();
    state.humanName2.clear();
    state.engineName1.clear();
    state.engineName2.clear();
    state.saveFileName.clear();
    state.skipBoardSyncForBranchNav = false;
    state.onMainRowGuard = false;
    state.gameMoves.clear();
}
