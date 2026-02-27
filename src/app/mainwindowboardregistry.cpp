/// @file mainwindowboardregistry.cpp
/// @brief 共通/Board系の ensure* 実装

#include "mainwindowboardregistry.h"
#include "mainwindow.h"
#include "mainwindowserviceregistry.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowresetservice.h"
#include "ui_mainwindow.h"

#include "boardsetupcontroller.h"
#include "positioneditcoordinator.h"
#include "positioneditcontroller.h"
#include "boardsyncpresenter.h"
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

MainWindowBoardRegistry::MainWindowBoardRegistry(MainWindow& mw,
                                                   MainWindowServiceRegistry& registry,
                                                   QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_registry(registry)
{
}

// ---------------------------------------------------------------------------
// 盤面セットアップ
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::ensureBoardSetupController()
{
    if (m_mw.m_boardSetupController) return;

    MainWindowDepsFactory::BoardSetupControllerCallbacks cbs;
    cbs.ensurePositionEdit = [this]() { ensurePositionEditController(); };
    cbs.ensureTimeController = [this]() { m_registry.ensureTimeController(); };
    cbs.updateGameRecord = [this](const QString& moveText, const QString& elapsed) {
        m_registry.ensureGameRecordUpdateService();
        if (m_mw.m_gameRecordUpdateService) {
            m_mw.m_gameRecordUpdateService->updateGameRecord(moveText, elapsed);
        }
    };
    cbs.redrawEngine1Graph = [this](int ply) {
        m_registry.ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine1Graph(ply);
    };
    cbs.redrawEngine2Graph = [this](int ply) {
        m_registry.ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine2Graph(ply);
    };

    m_mw.m_compositionRoot->ensureBoardSetupController(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_boardSetupController);
}

// ---------------------------------------------------------------------------
// 局面編集コーディネーター
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::ensurePositionEditCoordinator()
{
    if (m_mw.m_posEditCoordinator) return;

    m_registry.ensureUiStatePolicyManager();

    MainWindowDepsFactory::PositionEditCoordinatorCallbacks cbs;
    cbs.applyEditMenuState = [this](bool editing) {
        if (editing) m_mw.m_uiStatePolicy->transitionToDuringPositionEdit();
        else m_mw.m_uiStatePolicy->transitionToIdle();
    };
    cbs.ensurePositionEdit = [this]() {
        ensurePositionEditController();
        if (m_mw.m_posEditCoordinator) m_mw.m_posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
    };
    cbs.actionReturnAllPiecesToStand = m_mw.ui->actionReturnAllPiecesToStand;
    cbs.actionSetHiratePosition = m_mw.ui->actionSetHiratePosition;
    cbs.actionSetTsumePosition = m_mw.ui->actionSetTsumePosition;
    cbs.actionChangeTurn = m_mw.ui->actionChangeTurn;

    m_mw.m_compositionRoot->ensurePositionEditCoordinator(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_posEditCoordinator);
}

// ---------------------------------------------------------------------------
// 局面編集コントローラ
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::ensurePositionEditController()
{
    if (m_mw.m_posEdit) return;
    m_mw.m_posEdit = new PositionEditController(&m_mw);
}

// ---------------------------------------------------------------------------
// 盤面同期プレゼンター
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::ensureBoardSyncPresenter()
{
    if (m_mw.m_boardSync) {
        // sfenRecord ポインタが変わっている場合は更新する
        const QStringList* current = m_mw.m_queryService->sfenRecord();
        m_mw.m_boardSync->setSfenRecord(current);
        return;
    }

    qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: creating BoardSyncPresenter"
                       << "sfenRecord()*=" << static_cast<const void*>(m_mw.m_queryService->sfenRecord())
                       << "sfenRecord().size=" << (m_mw.m_queryService->sfenRecord() ? m_mw.m_queryService->sfenRecord()->size() : -1);

    BoardSyncPresenter::Deps d;
    d.gc         = m_mw.m_gameController;
    d.view       = m_mw.m_shogiView;
    d.bic        = m_mw.m_boardController;
    d.sfenRecord = m_mw.m_queryService->sfenRecord();
    d.gameMoves  = &m_mw.m_kifu.gameMoves;

    m_mw.m_boardSync = new BoardSyncPresenter(d, &m_mw);

    qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: created m_boardSync*=" << static_cast<const void*>(m_mw.m_boardSync);
}

// ---------------------------------------------------------------------------
// 盤面読込サービス
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::ensureBoardLoadService()
{
    if (!m_mw.m_boardLoadService) {
        m_mw.m_boardLoadService = new BoardLoadService(&m_mw);
    }

    ensureBoardSyncPresenter();

    BoardLoadService::Deps deps;
    deps.gc = m_mw.m_gameController;
    deps.view = m_mw.m_shogiView;
    deps.boardSync = m_mw.m_boardSync;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    deps.ensureBoardSyncPresenter = std::bind(&MainWindowBoardRegistry::ensureBoardSyncPresenter, this);
    deps.beginBranchNavGuard = [this]() {
        m_registry.ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->beginBranchNavGuard();
    };
    m_mw.m_boardLoadService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// BoardInteractionController構築
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::setupBoardInteractionController()
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

void MainWindowBoardRegistry::handleMoveRequested(const QPoint& from, const QPoint& to)
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

void MainWindowBoardRegistry::handleMoveCommitted(int mover, int ply)
{
    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->setPlayMode(m_mw.m_state.playMode);
        m_mw.m_boardSetupController->onMoveCommitted(
            static_cast<ShogiGameController::Player>(mover), ply);
    }

    m_registry.ensureGameRecordUpdateService();
    m_mw.m_gameRecordUpdateService->recordUsiMoveAndUpdateSfen();

    m_registry.updateJosekiWindow();
}

// ---------------------------------------------------------------------------
// 局面編集開始
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::handleBeginPositionEditing()
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

void MainWindowBoardRegistry::handleFinishPositionEditing()
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

void MainWindowBoardRegistry::resetModels(const QString& hirateStartSfen)
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

void MainWindowBoardRegistry::resetUiState(const QString& hirateStartSfen)
{
    MainWindowResetService::UiResetDeps deps;
    deps.gameController = m_mw.m_gameController;
    deps.shogiView = m_mw.m_shogiView;
    deps.uiStatePolicy = m_mw.m_uiStatePolicy;
    deps.updateJosekiWindow = [this]() {
        m_registry.updateJosekiWindow();
    };

    const MainWindowResetService resetService;
    resetService.resetUiState(deps, hirateStartSfen);
}

// ---------------------------------------------------------------------------
// セッション依存UIクリア
// ---------------------------------------------------------------------------

void MainWindowBoardRegistry::clearSessionDependentUi()
{
    m_registry.ensureCommentCoordinator();

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
