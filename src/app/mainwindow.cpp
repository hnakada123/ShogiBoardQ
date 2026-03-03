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

// --- unique_ptr メンバの完全型（デストラクタ用） ---
#include "mainwindowcompositionroot.h"       // IWYU pragma: keep
#include "mainwindowmatchadapter.h"
#include "mainwindowcoreinitcoordinator.h"
#include "livegamesessionupdater.h"
#include "mainwindowsignalrouter.h"
#include "mainwindowappearancecontroller.h"
#include "evaluationgraphcontroller.h"       // IWYU pragma: keep
#include "jishogiscoredialogcontroller.h"    // IWYU pragma: keep
#include "nyugyokudeclarationhandler.h"
#include "languagecontroller.h"
#include "docklayoutmanager.h"
#include "dockcreationservice.h"              // IWYU pragma: keep
#include "usicommandcontroller.h"             // IWYU pragma: keep
#include "branchnavigationwiring.h"
#include "kifunavigationcoordinator.h"       // IWYU pragma: keep
#include "gamerecordupdateservice.h"         // IWYU pragma: keep
#include "analysisresultspresenter.h"        // IWYU pragma: keep
#include "kifuexportcontroller.h"
#include "csagamewiring.h"
#include "josekiwindowwiring.h"
#include "matchcoordinatorwiring.h"
#include "playmodepolicyservice.h"
#include "matchruntimequeryservice.h"
#include <QCloseEvent>
#include <QTimer>
#ifdef QT_DEBUG
#include "debugscreenshotwiring.h"
#endif

// --- QPointer<T> の完全型（QPointer<T> -> T* 変換に必要） ---
#include "boardinteractioncontroller.h"    // IWYU pragma: keep
#include "boardsyncpresenter.h"            // IWYU pragma: keep
#include "kifuloadcoordinator.h"           // IWYU pragma: keep
#include "positioneditcontroller.h"        // IWYU pragma: keep

// --- Controllers / Services（スロット転送先） ---
#include "gamerecordloadservice.h"
#include "turnstatesyncservice.h"
#include "undoflowservice.h"

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
    m_registry->ensureUndoFlowService();
    m_undoFlowService->undoLastTwoMoves();
}

void MainWindow::updateJosekiWindow()
{
    m_registry->updateJosekiWindow();
}

// TurnManager::changed を受けて UI/Clock を更新（＋手番を GameController に同期）
void MainWindow::onTurnManagerChanged(ShogiGameController::Player now)
{
    m_registry->ensureTurnStateSyncService();
    m_turnStateSync->onTurnManagerChanged(now);
}

// 現在の手番を設定する。
void MainWindow::setCurrentTurn()
{
    m_registry->ensureTurnStateSyncService();
    m_turnStateSync->setCurrentTurn();
}

// 設定ファイルにGUI全体のウィンドウサイズを書き込む。
// また、将棋盤のマスサイズも書き込む。その後、GUIを終了する。
void MainWindow::saveSettingsAndClose()
{
    m_pipeline->shutdownAndQuit();
}

// GUIを初期画面表示に戻す（ServiceRegistryへ委譲）。
void MainWindow::resetToInitialState()
{
    m_registry->resetToInitialState();
}

// ゲーム状態変数のリセット（ServiceRegistryへ委譲）。
void MainWindow::resetGameState()
{
    m_registry->resetGameState();
}

// `displayGameRecord`: Game Record を表示する（GameRecordLoadService に委譲）。
void MainWindow::displayGameRecord(const QList<KifDisplayItem>& disp)
{
    if (!m_models.kifuRecord) return;

    m_registry->ensureGameRecordLoadService();
    m_gameRecordLoadService->loadGameRecord(disp);
}

// `closeEvent`: 親クラスでイベント受理を確認してから shutdown を実行する。
void MainWindow::closeEvent(QCloseEvent* e)
{
    QMainWindow::closeEvent(e);
    if (!e->isAccepted())
        return;
    m_pipeline->runShutdown();
}

void MainWindow::beginPositionEditing()
{
    m_registry->handleBeginPositionEditing();
}

void MainWindow::finishPositionEditing()
{
    m_registry->handleFinishPositionEditing();
}

void MainWindow::onMoveRequested(const QPoint& from, const QPoint& to)
{
    m_registry->handleMoveRequested(from, to);
}

// 再生モードの切替を ServiceRegistry へ委譲
void MainWindow::setReplayMode(bool on)
{
    m_registry->setReplayMode(on);
}

void MainWindow::loadBoardFromSfen(const QString& sfen)
{
    m_registry->loadBoardFromSfen(sfen);
}

void MainWindow::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    m_registry->loadBoardWithHighlights(currentSfen, prevSfen);
}

void MainWindow::onMoveCommitted(ShogiGameController::Player mover, int ply)
{
    m_registry->handleMoveCommitted(static_cast<int>(mover), ply);
}

// `buildRuntimeRefs`: 現在のメンバ変数から MainWindowRuntimeRefs スナップショットを構築する。
MainWindowRuntimeRefs MainWindow::buildRuntimeRefs()
{
    // 契約:
    // - 本関数は参照スナップショットの構築のみ行い、副作用を持たない。
    // - QueryService 等の遅延生成依存が null でも安全に呼び出せる。
    MainWindowRuntimeRefs refs;
    buildUiRefs(refs);
    buildModelRefs(refs);
    buildKifuRefs(refs);
    buildStateRefs(refs);
    buildBranchNavRefs(refs);
    buildPlayerRefs(refs);
    buildGameServiceRefs(refs);
    buildKifuServiceRefs(refs);
    buildUiControllerRefs(refs);
    buildAnalysisRefs(refs);
    buildGameControllerRefs(refs);
    return refs;
}

void MainWindow::buildUiRefs(MainWindowRuntimeRefs& refs)
{
    refs.ui.parentWidget = this;
    refs.ui.mainWindow = this;
    refs.ui.statusBar = ui->statusbar;
    refs.ui.uiForm = ui.get();
    refs.ui.recordPane = m_recordPane;
    refs.ui.evalChart = m_evalChart;
    refs.ui.analysisTab = m_analysisTab;
}

void MainWindow::buildModelRefs(MainWindowRuntimeRefs& refs)
{
    refs.models.kifuRecordModel = m_models.kifuRecord;
    refs.models.considerationModel = &m_models.consideration;
    refs.models.thinking1 = m_models.thinking1;
    refs.models.thinking2 = m_models.thinking2;
    refs.models.consideration = m_models.consideration;
    refs.models.commLog1 = m_models.commLog1;
    refs.models.commLog2 = m_models.commLog2;
    refs.models.gameRecordModel = m_models.gameRecord;
}

void MainWindow::buildKifuRefs(MainWindowRuntimeRefs& refs)
{
    // null 許容ポリシー:
    // - m_queryService は shutdown 中に null / 無効化される可能性がある。
    // - 呼び出し側は sfenRecord が null の場合を許容して処理する。
    refs.kifu.sfenRecord = m_queryService ? m_queryService->sfenRecord() : nullptr;
    refs.kifu.gameMoves = &m_kifu.gameMoves;
    refs.kifu.gameUsiMoves = &m_kifu.gameUsiMoves;
    refs.kifu.moveRecords = &m_kifu.moveRecords;
    refs.kifu.positionStrList = &m_kifu.positionStrList;
    refs.kifu.activePly = &m_kifu.activePly;
    refs.kifu.currentSelectedPly = &m_kifu.currentSelectedPly;
    refs.kifu.saveFileName = &m_kifu.saveFileName;
    refs.kifu.commentsByRow = &m_kifu.commentsByRow;
    refs.kifu.onMainRowGuard = &m_kifu.onMainRowGuard;
}

void MainWindow::buildStateRefs(MainWindowRuntimeRefs& refs)
{
    refs.state.playMode = &m_state.playMode;
    refs.state.currentMoveIndex = &m_state.currentMoveIndex;
    refs.state.currentSfenStr = &m_state.currentSfenStr;
    refs.state.startSfenStr = &m_state.startSfenStr;
    refs.state.skipBoardSyncForBranchNav = &m_state.skipBoardSyncForBranchNav;
    refs.state.resumeSfenStr = &m_state.resumeSfenStr;
}

void MainWindow::buildBranchNavRefs(MainWindowRuntimeRefs& refs)
{
    refs.branchNav.branchTree = m_branchNav.branchTree;
    refs.branchNav.navState = m_branchNav.navState;
    refs.branchNav.displayCoordinator = m_branchNav.displayCoordinator;
}

void MainWindow::buildPlayerRefs(MainWindowRuntimeRefs& refs)
{
    refs.player.humanName1 = &m_player.humanName1;
    refs.player.humanName2 = &m_player.humanName2;
    refs.player.engineName1 = &m_player.engineName1;
    refs.player.engineName2 = &m_player.engineName2;
}

void MainWindow::buildGameServiceRefs(MainWindowRuntimeRefs& refs)
{
    refs.gameService.match = m_match;
    refs.gameService.gameController = m_gameController;
    refs.gameService.csaGameCoordinator = m_csaGameCoordinator;
    refs.gameService.shogiView = m_shogiView;
}

void MainWindow::buildKifuServiceRefs(MainWindowRuntimeRefs& refs)
{
    refs.kifuService.kifuLoadCoordinator = m_kifuLoadCoordinator;
    refs.kifuService.recordPresenter = m_recordPresenter;
    refs.kifuService.replayController = m_replayController;
}

void MainWindow::buildUiControllerRefs(MainWindowRuntimeRefs& refs)
{
    refs.uiController.timeController = m_timeController;
    refs.uiController.boardController = m_boardController;
    refs.uiController.positionEditController = m_posEdit;
    refs.uiController.dialogCoordinator = m_dialogCoordinator;
    refs.uiController.uiStatePolicy = m_uiStatePolicy;
    refs.uiController.boardSync = m_boardSync;
    refs.uiController.playerInfoWiring = m_playerInfoWiring;
}

void MainWindow::buildAnalysisRefs(MainWindowRuntimeRefs& refs)
{
    refs.analysis.gameInfoController = m_gameInfoController;
    refs.analysis.evalGraphController = m_evalGraphController.get();
    refs.analysis.analysisPresenter = m_analysisPresenter.get();
}

void MainWindow::buildGameControllerRefs(MainWindowRuntimeRefs& refs)
{
    refs.gameCtrl.gameStateController = m_gameStateController;
    refs.gameCtrl.consecutiveGamesController = m_consecutiveGamesController;
}

// `refreshPlayModePolicyDeps`: PlayModePolicyService の依存を最新に更新する。
// 以下のタイミングで呼び出すこと:
//   - 起動完了直後（initializeEarlyServices）
//   - Match 再生成直後（GameWiringSubRegistry::initMatchCoordinator）
//   - CsaGameCoordinator 生成時（DialogLaunchWiring::displayCsaGameDialog）
void MainWindow::refreshPlayModePolicyDeps()
{
    if (!m_playModePolicy) return;
    PlayModePolicyService::Deps policyDeps;
    policyDeps.playMode = &m_state.playMode;
    policyDeps.gameController = m_gameController;
    policyDeps.match = m_match;
    policyDeps.csaGameCoordinator = m_csaGameCoordinator;
    m_playModePolicy->updateDeps(policyDeps);
}

// `onRecordPaneMainRowChanged`: Record Pane Main Row Changed のイベント受信時処理を行う。
void MainWindow::onRecordPaneMainRowChanged(int row)
{
    m_registry->onRecordPaneMainRowChanged(row);
}
