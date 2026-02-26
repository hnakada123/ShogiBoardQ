/// @file mainwindow.cpp
/// @brief MainWindow — UIルートの Facade 実装
///
/// コンストラクタ/デストラクタは MainWindowLifecyclePipeline に委譲し、
/// 各スロットは controller/coordinator への1行転送のみ行う。
/// 業務ロジック・UI詳細ロジック・依存生成ロジックは
/// MainWindowServiceRegistry または専用クラスに移譲済み。

#include "mainwindow.h"
#include "ui_mainwindow.h"

// --- Pipeline / Registry ---
#include "mainwindowlifecyclepipeline.h"
#include "mainwindowserviceregistry.h"
#include "mainwindowruntimerefsfactory.h"
#include "mainwindowruntimerefs.h"

// --- unique_ptr メンバの完全型（デストラクタ用） ---
#include "mainwindowmatchadapter.h"
#include "mainwindowcoreinitcoordinator.h"
#include "livegamesessionupdater.h"

// --- Controllers / Services（スロット転送先） ---
#include "shogigamecontroller.h"
#include "boardloadservice.h"
#include "gamerecordloadservice.h"
#include "turnstatesyncservice.h"
#include "undoflowservice.h"
#include "replaycontroller.h"
#include "recordnavigationwiring.h"
#include "recordnavigationhandler.h"
#include "sessionlifecyclecoordinator.h"
#include "kifuexportdepsassembler.h"

// --- 型依存（MOC / 値型） ---
#include "matchcoordinator.h"

#include <QApplication>

// MainWindow を初期化し、主要コンポーネントを構築する。
// 起動フローの詳細は MainWindowLifecyclePipeline::runStartup() を参照。
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
    , m_pipeline(std::make_unique<MainWindowLifecyclePipeline>(*this))
{
    m_pipeline->runStartup();
}

// 終了フローの詳細は MainWindowLifecyclePipeline::runShutdown() を参照。
MainWindow::~MainWindow()
{
    m_pipeline->runShutdown();
}



// 待ったボタンを押すと、2手戻る。
void MainWindow::undoLastTwoMoves()
{
    ensureUndoFlowService();
    m_undoFlowService->undoLastTwoMoves();
}

void MainWindow::updateJosekiWindow()
{
    m_registry->updateJosekiWindow();
}

// TurnManager::changed を受けて UI/Clock を更新（＋手番を GameController に同期）
void MainWindow::onTurnManagerChanged(ShogiGameController::Player now)
{
    ensureTurnStateSyncService();
    m_turnStateSync->onTurnManagerChanged(now);
}

// 現在の手番を設定する。
void MainWindow::setCurrentTurn()
{
    ensureTurnStateSyncService();
    m_turnStateSync->setCurrentTurn();
}

// 設定ファイルにGUI全体のウィンドウサイズを書き込む。
// また、将棋盤のマスサイズも書き込む。その後、GUIを終了する。
void MainWindow::saveSettingsAndClose()
{
    m_pipeline->runShutdown();
    QCoreApplication::quit();
}

// GUIを初期画面表示に戻す（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::resetToInitialState()
{
    ensureSessionLifecycleCoordinator();
    m_sessionLifecycle->resetToInitialState();
}

// ゲーム状態変数のリセット（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::resetGameState()
{
    ensureSessionLifecycleCoordinator();
    m_sessionLifecycle->resetGameState();
}

// `displayGameRecord`: Game Record を表示する（GameRecordLoadService に委譲）。
void MainWindow::displayGameRecord(const QList<KifDisplayItem> disp)
{
    if (!m_models.kifuRecord) return;

    ensureGameRecordLoadService();
    m_gameRecordLoadService->loadGameRecord(disp);
}

// `closeEvent`: 終了時に設定保存とエンジン停止を行ってから親クラス処理へ委譲する。
void MainWindow::closeEvent(QCloseEvent* e)
{
    m_pipeline->runShutdown();
    QMainWindow::closeEvent(e);
}



void MainWindow::beginPositionEditing()
{
    m_registry->handleBeginPositionEditing();
}

void MainWindow::finishPositionEditing()
{
    m_registry->handleFinishPositionEditing();
}

void MainWindow::ensureBranchNavigationWiring()
{
    m_registry->ensureBranchNavigationWiring();
}

void MainWindow::ensureMatchCoordinatorWiring()
{
    m_registry->ensureMatchCoordinatorWiring();
}

void MainWindow::ensureTimeController()
{
    m_registry->ensureTimeController();
}

void MainWindow::onMoveRequested(const QPoint& from, const QPoint& to)
{
    m_registry->handleMoveRequested(from, to);
}

// 再生モードの切替を ReplayController へ委譲
void MainWindow::setReplayMode(bool on)
{
    ensureReplayController();
    if (m_replayController) {
        m_replayController->setReplayMode(on);
    }
}



void MainWindow::loadBoardFromSfen(const QString& sfen)
{
    ensureBoardLoadService();
    m_boardLoadService->loadFromSfen(sfen);
}

void MainWindow::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    ensureBoardLoadService();
    m_boardLoadService->loadWithHighlights(currentSfen, prevSfen);
}

void MainWindow::onMoveCommitted(ShogiGameController::Player mover, int ply)
{
    m_registry->handleMoveCommitted(static_cast<int>(mover), ply);
}

// `ensureReplayController`: Replay Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureReplayController()
{
    m_registry->ensureReplayController();
}

// `buildRuntimeRefs`: 現在のメンバ変数から MainWindowRuntimeRefs スナップショットを構築する。
MainWindowRuntimeRefs MainWindow::buildRuntimeRefs()
{
    return MainWindowRuntimeRefsFactory::build(*this);
}

void MainWindow::ensureDialogCoordinator()
{
    m_registry->ensureDialogCoordinator();
}

void MainWindow::ensureKifuFileController()
{
    m_registry->ensureKifuFileController();
}

void MainWindow::ensureKifuExportController()
{
    m_registry->ensureKifuExportController();
}

// KifuExportControllerに依存を設定するヘルパー
void MainWindow::updateKifuExportDependencies()
{
    KifuExportDepsAssembler::assemble(*this);
}

void MainWindow::ensureGameStateController()
{
    m_registry->ensureGameStateController();
}



void MainWindow::ensureBoardSetupController()
{
    m_registry->ensureBoardSetupController();
}

void MainWindow::ensurePvClickController()
{
    m_registry->ensurePvClickController();
}

void MainWindow::ensurePositionEditCoordinator()
{
    m_registry->ensurePositionEditCoordinator();
}


void MainWindow::ensureMenuWiring()
{
    m_registry->ensureMenuWiring();
}

void MainWindow::ensurePlayerInfoWiring()
{
    m_registry->ensurePlayerInfoWiring();
}

void MainWindow::ensurePreStartCleanupHandler()
{
    m_registry->ensurePreStartCleanupHandler();
}

void MainWindow::ensureTurnSyncBridge()
{
    m_registry->ensureTurnSyncBridge();
}

void MainWindow::ensurePositionEditController()
{
    m_registry->ensurePositionEditController();
}

void MainWindow::ensureBoardSyncPresenter()
{
    m_registry->ensureBoardSyncPresenter();
}

void MainWindow::ensureBoardLoadService()
{
    m_registry->ensureBoardLoadService();
}

void MainWindow::ensureConsiderationPositionService()
{
    m_registry->ensureConsiderationPositionService();
}

void MainWindow::ensureAnalysisPresenter()
{
    m_registry->ensureAnalysisPresenter();
}

void MainWindow::ensureGameStartCoordinator()
{
    m_registry->ensureGameStartCoordinator();
}

// `ensureRecordPresenter`: Record Presenter を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureRecordPresenter()
{
    m_registry->ensureRecordPresenter();
}

void MainWindow::ensureLiveGameSessionStarted()
{
    m_registry->ensureLiveGameSessionStarted();
}

void MainWindow::ensureLiveGameSessionUpdater()
{
    m_registry->ensureLiveGameSessionUpdater();
}

void MainWindow::ensureGameRecordUpdateService()
{
    m_registry->ensureGameRecordUpdateService();
}

void MainWindow::ensureUndoFlowService()
{
    m_registry->ensureUndoFlowService();
}

void MainWindow::ensureGameRecordLoadService()
{
    m_registry->ensureGameRecordLoadService();
}

void MainWindow::ensureTurnStateSyncService()
{
    m_registry->ensureTurnStateSyncService();
}

// `onRecordPaneMainRowChanged`: Record Pane Main Row Changed のイベント受信時処理を行う。
void MainWindow::onRecordPaneMainRowChanged(int row)
{
    ensureRecordNavigationHandler();
    m_recordNavWiring->handler()->onMainRowChanged(row);
}

void MainWindow::ensureGameRecordModel()
{
    m_registry->ensureGameRecordModel();
}

void MainWindow::ensureConsecutiveGamesController()
{
    m_registry->ensureConsecutiveGamesController();
}

void MainWindow::ensureLanguageController()
{
    m_registry->ensureLanguageController();
}

void MainWindow::ensureDockLayoutManager()
{
    m_registry->ensureDockLayoutManager();
}

void MainWindow::ensureDockCreationService()
{
    m_registry->ensureDockCreationService();
}

void MainWindow::ensureCommentCoordinator()
{
    m_registry->ensureCommentCoordinator();
}

void MainWindow::ensureRecordNavigationHandler()
{
    m_registry->ensureRecordNavigationHandler();
}

void MainWindow::ensureUiStatePolicyManager()
{
    m_registry->ensureUiStatePolicyManager();
}

void MainWindow::ensureKifuNavigationCoordinator()
{
    m_registry->ensureKifuNavigationCoordinator();
}

void MainWindow::ensureSessionLifecycleCoordinator()
{
    m_registry->ensureSessionLifecycleCoordinator();
}
