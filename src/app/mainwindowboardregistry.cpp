/// @file mainwindowboardregistry.cpp
/// @brief 共通/Board系（盤面操作・リセット・横断的操作）の ensure* 実装
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowresetservice.h"
#include "ui_mainwindow.h"

#include "boardsetupcontroller.h"
#include "branchnavigationwiring.h"
#include "branchtreeeditcontroller.h"
#include "boardsyncpresenter.h"             // IWYU pragma: keep (QPointer の完全型)
#include "positioneditcontroller.h"         // IWYU pragma: keep (QPointer の完全型)
#include "positioneditcoordinator.h"
#include "boardloadservice.h"
#include "kifuloadcoordinator.h"            // IWYU pragma: keep (QPointer の完全型)
#include "boardinteractioncontroller.h"
#include "appsettings.h"
#include "commentcoordinator.h"
#include "evaluationgraphcontroller.h"
#include "gamerecordupdateservice.h"
#include "gamesessionorchestrator.h"
#include "kifunavigationcoordinator.h"
#include "playmodepolicyservice.h"
#include "playerinfowiring.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"
#include "logcategories.h"

#include <QScopedValueRollback>

// ---------------------------------------------------------------------------
// 盤面セットアップ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureBoardSetupController()
{
    if (m_mw.m_isShuttingDown) return;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController = new BoardSetupController(&m_mw);
    }
    BoardSetupController* controller = m_mw.m_boardSetupController;
    controller->setShogiView(m_mw.m_shogiView);
    controller->setGameController(m_mw.m_gameController);
    controller->setMatchCoordinator(m_mw.m_match);
    controller->setPositionEditController(m_mw.m_posEdit);
    controller->setPlayMode(m_mw.m_state.playMode);
    controller->setSfenRecord(&m_mw.m_kifu.sfenRecord);
    controller->setGameMoves(&m_mw.m_kifu.gameMoves);

    controller->setEnsurePositionEditCallback([this]() { m_foundation->ensurePositionEditController(); });
    controller->setRedrawEngine1GraphCallback([this](int ply) {
        m_foundation->ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine1Graph(ply);
    });
    controller->setRedrawEngine2GraphCallback([this](int ply) {
        m_foundation->ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine2Graph(ply);
    });
    controller->setVariationInputCallbacks(
        [this]() { return canRecordVariationMove(); },
        [this](QPoint& from, QPoint& to) { return recordVariationMove(from, to); });
}

// ---------------------------------------------------------------------------
// 局面編集コーディネーター
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensurePositionEditCoordinator()
{
    if (m_mw.m_isShuttingDown) return;

    m_foundation->ensureUiStatePolicyManager();

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_registryParts.posEditCoordinator) {
        m_mw.m_registryParts.posEditCoordinator = new PositionEditCoordinator(&m_mw);
    }
    PositionEditCoordinator* coordinator = m_mw.m_registryParts.posEditCoordinator;
    coordinator->setShogiView(m_mw.m_shogiView);
    coordinator->setGameController(m_mw.m_gameController);
    coordinator->setBoardController(m_mw.m_boardController);
    coordinator->setPositionEditController(m_mw.m_posEdit);
    coordinator->setMatchCoordinator(m_mw.m_match);
    coordinator->setReplayController(m_mw.m_replayController);
    coordinator->setNavigationState(m_mw.m_branchNav.navState);
    coordinator->setStartSfenStr(&m_mw.m_state.startSfenStr);
    coordinator->setCurrentSfenStr(&m_mw.m_state.currentSfenStr);
    coordinator->setOnMainRowGuard(&m_mw.m_kifu.onMainRowGuard);

    coordinator->setApplyEditMenuStateCallback([this](bool editing) {
        if (editing) m_mw.m_uiStatePolicy->transitionToDuringPositionEdit();
        else m_mw.m_uiStatePolicy->transitionToIdle();
    });
    coordinator->setEnsurePositionEditCallback([this]() {
        m_foundation->ensurePositionEditController();
        if (m_mw.m_registryParts.posEditCoordinator) m_mw.m_registryParts.posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
    });

    PositionEditCoordinator::EditActions actions;
    actions.actionReturnAllPiecesToStand = m_mw.ui->actionReturnAllPiecesToStand;
    actions.actionSetHiratePosition = m_mw.ui->actionSetHiratePosition;
    actions.actionSetTsumePosition = m_mw.ui->actionSetTsumePosition;
    actions.actionChangeTurn = m_mw.ui->actionChangeTurn;
    coordinator->setEditActions(actions);

    connect(m_mw.m_registryParts.posEditCoordinator, &PositionEditCoordinator::positionEditingStarted,
            this, &MainWindowServiceRegistry::resetRecordForEditedPosition, Qt::UniqueConnection);
    connect(m_mw.m_registryParts.posEditCoordinator, &PositionEditCoordinator::positionEditingStarted,
            this, &MainWindowServiceRegistry::clearEvalState, Qt::UniqueConnection);
    connect(m_mw.m_registryParts.posEditCoordinator, &PositionEditCoordinator::positionEditingFinished,
            this, &MainWindowServiceRegistry::resetRecordForEditedPosition, Qt::UniqueConnection);
}

// ---------------------------------------------------------------------------
// 盤面読込サービス
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureBoardLoadService()
{
    if (m_mw.m_isShuttingDown) return;
    if (!m_mw.m_boardLoadService) {
        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once on first use, never recreated
        m_mw.m_boardLoadService = new BoardLoadService(&m_mw);
    }

    m_foundation->ensureBoardSyncPresenter();

    BoardLoadService::Deps deps;
    deps.gc = m_mw.m_gameController;
    deps.view = m_mw.m_shogiView;
    deps.boardSync = m_mw.m_boardSync;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.setCurrentTurn = [this]() {
        m_mw.setCurrentTurn();
    };
    deps.ensureBoardSyncPresenter = [this]() {
        m_foundation->ensureBoardSyncPresenter();
    };
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
            auto* action = m_mw.ui->actionLegalMoves;
            action->setChecked(AppSettings::legalMovesVisible());
            m_mw.m_boardController->setLegalMovesVisible(action->isChecked());
            connect(action, &QAction::toggled, m_mw.m_boardController,
                    &BoardInteractionController::setLegalMovesVisible, Qt::UniqueConnection);
            m_mw.m_boardController->setIsHumanTurnCallback([this]() -> bool {
                return m_mw.m_playModePolicy && m_mw.m_playModePolicy->isHumanTurnNow();
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
        m_mw.m_boardSetupController->onMoveRequested(from, to);
    }
}

// ---------------------------------------------------------------------------
// 対局していないときの着手（棋譜の変化として記録）
// ---------------------------------------------------------------------------

bool MainWindowServiceRegistry::canRecordVariationMove() const
{
    // 待機中（対局・解析・検討・局面編集などをしていない）だけ受け付ける
    if (m_mw.m_uiStatePolicy == nullptr
        || m_mw.m_uiStatePolicy->currentState() != UiStatePolicyManager::AppState::Idle) {
        return false;
    }
    if (m_mw.m_boardController != nullptr
        && m_mw.m_boardController->mode() == BoardInteractionController::Mode::Edit) {
        return false;
    }
    BranchTreeEditController* editor = m_mw.m_branchNavWiring ? m_mw.m_branchNavWiring->editController() : nullptr;
    return editor != nullptr && editor->canRecordBoardMove();
}

bool MainWindowServiceRegistry::recordVariationMove(QPoint& from, QPoint& to)
{
    BranchTreeEditController* editor = m_mw.m_branchNavWiring ? m_mw.m_branchNavWiring->editController() : nullptr;
    return editor != nullptr && editor->recordBoardMove(from, to, m_mw.m_gameController);
}

// ---------------------------------------------------------------------------
// 着手確定処理
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleMoveCommitted(int mover, int ply)
{
    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->onMoveCommitted(
            static_cast<Turn>(mover), ply);
    }

    m_kifu->ensureGameRecordUpdateService();
    m_mw.m_gameRecordUpdateService->updateCurrentSfenAfterMove();

    m_kifu->updateJosekiWindow();
}

// ---------------------------------------------------------------------------
// 局面編集開始
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleBeginPositionEditing()
{
    // 編集を始めると棋譜は開始局面だけになるため、未保存の棋譜は先に保存するか確認する。
    // やめたときは中断した対局も残す
    if (!confirmDiscardUnsavedKifu()) return;
    ensureGameSessionOrchestrator();
    if (m_mw.m_gameSessionOrchestrator) {
        m_mw.m_gameSessionOrchestrator->discardInterruptedGame();
    }

    ensurePositionEditCoordinator();
    if (m_mw.m_registryParts.posEditCoordinator) {
        m_mw.m_registryParts.posEditCoordinator->beginPositionEditing();
    }
}

// ---------------------------------------------------------------------------
// 局面編集終了
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleFinishPositionEditing()
{
    ensurePositionEditCoordinator();
    if (m_mw.m_registryParts.posEditCoordinator) {
        m_mw.m_registryParts.posEditCoordinator->finishPositionEditing();
    }
}

void MainWindowServiceRegistry::resetRecordForEditedPosition()
{
    if (!m_mw.m_shogiView || !m_mw.m_shogiView->board()) return;

    // 行選択の通知で編集局面が以前の棋譜に巻き戻されるのを防ぐ。
    const QScopedValueRollback<bool> guard(m_mw.m_kifu.onMainRowGuard, true);
    ShogiBoard* board = m_mw.m_shogiView->board();
    const QString sfen = QStringLiteral("%1 %2 %3 1")
                             .arg(board->convertBoardToSfen(),
                                  turnToSfen(board->currentPlayer()),
                                  board->convertStandToSfen());
    m_mw.m_state.startSfenStr = sfen;
    m_mw.m_state.currentSfenStr = sfen;
    m_mw.m_kifu.gameMoves.clear();

    MainWindowResetService::ModelResetDeps deps;
    deps.navState = m_mw.m_branchNav.navState;
    deps.displayCoordinator = m_mw.m_branchNav.displayCoordinator;
    deps.gameRecord = m_mw.m_models.gameRecord;
    deps.sfenRecord = &m_mw.m_kifu.sfenRecord;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;
    const MainWindowResetService resetService;
    resetService.resetModels(deps, sfen);

    if (m_mw.m_commentCoordinator) {
        m_mw.m_commentCoordinator->broadcastComment(QString(), false);
    }
}

// ---------------------------------------------------------------------------
// モデルリセット
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::resetModels(const QString& hirateStartSfen)
{
    MainWindowResetService::ModelResetDeps deps;
    deps.navState = m_mw.m_branchNav.navState;
    deps.displayCoordinator = m_mw.m_branchNav.displayCoordinator;
    deps.boardController = m_mw.m_boardController;
    deps.gameRecord = m_mw.m_models.gameRecord;
    deps.evalGraphController = m_mw.m_evalGraphController.get();
    deps.timeController = m_mw.m_timeController;
    deps.sfenRecord = &m_mw.m_kifu.sfenRecord;
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
        m_kifu->updateJosekiWindow();
    };
    deps.syncBoardNamesWithGameInfo = [this]() {
        if (m_mw.m_playerInfoWiring) m_mw.m_playerInfoWiring->syncBoardNamesWithGameInfo();
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
    deps.broadcastComment = [this](const QString& comment, int row) {
        m_mw.m_commentCoordinator->broadcastComment(comment, row);
    };

    const MainWindowResetService resetService;
    resetService.clearSessionDependentUi(deps);
}

// ---------------------------------------------------------------------------
// MainWindow スロットからの転送（Board系）
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::loadBoardFromSfen(const QString& sfen)
{
    ensureBoardLoadService();
    m_mw.m_boardLoadService->loadFromSfen(sfen);
}

void MainWindowServiceRegistry::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    ensureBoardLoadService();
    m_mw.m_boardLoadService->loadWithHighlights(currentSfen, prevSfen);
}
