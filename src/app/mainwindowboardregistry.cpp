/// @file mainwindowboardregistry.cpp
/// @brief 共通/Board系（盤面操作・リセット・横断的操作）の ensure* 実装
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowresetservice.h"
#include "ui_mainwindow.h"

#include "boardsetupcontroller.h"
#include "positioneditcoordinator.h"
#include "boardloadservice.h"
#include "boardinteractioncontroller.h"
#include "commentcoordinator.h"
#include "evaluationgraphcontroller.h"
#include "gamerecordupdateservice.h"
#include "kifunavigationcoordinator.h"
#include "matchruntimequeryservice.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"
#include "logcategories.h"

// ---------------------------------------------------------------------------
// 盤面セットアップ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureBoardSetupController()
{
    if (m_mw.m_boardSetupController) return;

    MainWindowDepsFactory::BoardSetupControllerCallbacks cbs;
    cbs.ensurePositionEdit = [this]() { m_foundation->ensurePositionEditController(); };
    cbs.ensureTimeController = [this]() { ensureTimeController(); };
    cbs.updateGameRecord = [this](const QString& moveText, const QString& elapsed) {
        ensureGameRecordUpdateService();
        if (m_mw.m_gameRecordUpdateService) {
            m_mw.m_gameRecordUpdateService->updateGameRecord(moveText, elapsed);
        }
    };
    cbs.redrawEngine1Graph = [this](int ply) {
        m_foundation->ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine1Graph(ply);
    };
    cbs.redrawEngine2Graph = [this](int ply) {
        m_foundation->ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine2Graph(ply);
    };

    m_mw.m_compositionRoot->ensureBoardSetupController(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_boardSetupController);
}

// ---------------------------------------------------------------------------
// 局面編集コーディネーター
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensurePositionEditCoordinator()
{
    if (m_mw.m_posEditCoordinator) return;

    m_foundation->ensureUiStatePolicyManager();

    MainWindowDepsFactory::PositionEditCoordinatorCallbacks cbs;
    cbs.applyEditMenuState = [this](bool editing) {
        if (editing) m_mw.m_uiStatePolicy->transitionToDuringPositionEdit();
        else m_mw.m_uiStatePolicy->transitionToIdle();
    };
    cbs.ensurePositionEdit = [this]() {
        m_foundation->ensurePositionEditController();
        if (m_mw.m_posEditCoordinator) m_mw.m_posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
    };
    cbs.actionReturnAllPiecesToStand = m_mw.ui->actionReturnAllPiecesToStand;
    cbs.actionSetHiratePosition = m_mw.ui->actionSetHiratePosition;
    cbs.actionSetTsumePosition = m_mw.ui->actionSetTsumePosition;
    cbs.actionChangeTurn = m_mw.ui->actionChangeTurn;

    m_mw.m_compositionRoot->ensurePositionEditCoordinator(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_posEditCoordinator);
}

// ---------------------------------------------------------------------------
// 盤面読込サービス
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureBoardLoadService()
{
    if (!m_mw.m_boardLoadService) {
        m_mw.m_boardLoadService = new BoardLoadService(&m_mw);
    }

    m_foundation->ensureBoardSyncPresenter();

    BoardLoadService::Deps deps;
    deps.gc = m_mw.m_gameController;
    deps.view = m_mw.m_shogiView;
    deps.boardSync = m_mw.m_boardSync;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    deps.ensureBoardSyncPresenter = std::bind(&MainWindowFoundationRegistry::ensureBoardSyncPresenter, m_foundation);
    deps.beginBranchNavGuard = [this]() {
        m_foundation->ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->beginBranchNavGuard();
    };
    m_mw.m_boardLoadService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// BoardInteractionController構築
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::setupBoardInteractionController()
{
    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->setupBoardInteractionController();
        m_mw.m_boardController = m_mw.m_boardSetupController->boardController();

        if (m_mw.m_boardController) {
            m_mw.m_boardController->setIsHumanTurnCallback([this]() -> bool {
                return m_mw.m_queryService->isHumanTurnNow();
            });
        }
    }
}

// ---------------------------------------------------------------------------
// 着手リクエスト処理
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleMoveRequested(const QPoint& from, const QPoint& to)
{
    qCDebug(lcApp).noquote() << "onMoveRequested_: from=" << from << " to=" << to
                       << " m_state.playMode=" << static_cast<int>(m_mw.m_state.playMode)
                       << " m_match=" << (m_mw.m_match ? "valid" : "null")
                       << " matchMode=" << (m_mw.m_match ? static_cast<int>(m_mw.m_match->playMode()) : -1);

    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->setPlayMode(m_mw.m_state.playMode);
        m_mw.m_boardSetupController->setMatchCoordinator(m_mw.m_match);
        m_mw.m_boardSetupController->setSfenRecord(m_mw.m_queryService->sfenRecord());
        m_mw.m_boardSetupController->setPositionEditController(m_mw.m_posEdit);
        m_mw.m_boardSetupController->setTimeController(m_mw.m_timeController);
        m_mw.m_boardSetupController->onMoveRequested(from, to);
    }
}

// ---------------------------------------------------------------------------
// 着手確定処理
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleMoveCommitted(int mover, int ply)
{
    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->setPlayMode(m_mw.m_state.playMode);
        m_mw.m_boardSetupController->onMoveCommitted(
            static_cast<ShogiGameController::Player>(mover), ply);
    }

    ensureGameRecordUpdateService();
    m_mw.m_gameRecordUpdateService->recordUsiMoveAndUpdateSfen();

    updateJosekiWindow();
}

// ---------------------------------------------------------------------------
// 局面編集開始
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleBeginPositionEditing()
{
    ensurePositionEditCoordinator();
    if (m_mw.m_posEditCoordinator) {
        m_mw.m_posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
        m_mw.m_posEditCoordinator->setBoardController(m_mw.m_boardController);
        m_mw.m_posEditCoordinator->setMatchCoordinator(m_mw.m_match);
        m_mw.m_posEditCoordinator->beginPositionEditing();
    }
}

// ---------------------------------------------------------------------------
// 局面編集終了
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleFinishPositionEditing()
{
    ensurePositionEditCoordinator();
    if (m_mw.m_posEditCoordinator) {
        m_mw.m_posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
        m_mw.m_posEditCoordinator->setBoardController(m_mw.m_boardController);
        m_mw.m_posEditCoordinator->finishPositionEditing();
    }
}

// ---------------------------------------------------------------------------
// モデルリセット
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::resetModels(const QString& hirateStartSfen)
{
    m_mw.m_kifu.moveRecords.clear();

    MainWindowResetService::ModelResetDeps deps;
    deps.navState = m_mw.m_branchNav.navState;
    deps.recordPresenter = m_mw.m_recordPresenter;
    deps.displayCoordinator = m_mw.m_branchNav.displayCoordinator;
    deps.boardController = m_mw.m_boardController;
    deps.gameRecord = m_mw.m_models.gameRecord;
    deps.evalGraphController = m_mw.m_evalGraphController.get();
    deps.timeController = m_mw.m_timeController;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    deps.gameInfoController = m_mw.m_gameInfoController;
    deps.kifuLoadCoordinator = m_mw.m_kifuLoadCoordinator;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;

    const MainWindowResetService resetService;
    resetService.resetModels(deps, hirateStartSfen);
}

// ---------------------------------------------------------------------------
// UI状態リセット
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::resetUiState(const QString& hirateStartSfen)
{
    MainWindowResetService::UiResetDeps deps;
    deps.gameController = m_mw.m_gameController;
    deps.shogiView = m_mw.m_shogiView;
    deps.uiStatePolicy = m_mw.m_uiStatePolicy;
    deps.updateJosekiWindow = [this]() {
        updateJosekiWindow();
    };

    const MainWindowResetService resetService;
    resetService.resetUiState(deps, hirateStartSfen);
}

// ---------------------------------------------------------------------------
// セッション依存UIクリア
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::clearSessionDependentUi()
{
    m_foundation->ensureCommentCoordinator();

    MainWindowResetService::SessionUiDeps deps;
    deps.commLog1 = m_mw.m_models.commLog1;
    deps.commLog2 = m_mw.m_models.commLog2;
    deps.thinking1 = m_mw.m_models.thinking1;
    deps.thinking2 = m_mw.m_models.thinking2;
    deps.consideration = m_mw.m_models.consideration;
    deps.analysisTab = m_mw.m_analysisTab;
    deps.analysisModel = m_mw.m_models.analysis;
    deps.evalChart = m_mw.m_evalChart;
    deps.evalGraphController = m_mw.m_evalGraphController.get();
    deps.broadcastComment = std::bind(&CommentCoordinator::broadcastComment,
                                       m_mw.m_commentCoordinator,
                                       std::placeholders::_1, std::placeholders::_2);

    const MainWindowResetService resetService;
    resetService.clearSessionDependentUi(deps);
}
