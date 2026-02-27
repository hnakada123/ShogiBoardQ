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
#include "mainwindowruntimerefs.h"

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
#include <QTimer>
#ifdef QT_DEBUG
#include "debugscreenshotwiring.h"
#endif

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
    m_pipeline->runShutdown();
    QCoreApplication::quit();
}

// GUIを初期画面表示に戻す（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::resetToInitialState()
{
    m_registry->ensureSessionLifecycleCoordinator();
    m_sessionLifecycle->resetToInitialState();
}

// ゲーム状態変数のリセット（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::resetGameState()
{
    m_registry->ensureSessionLifecycleCoordinator();
    m_sessionLifecycle->resetGameState();
}

// `displayGameRecord`: Game Record を表示する（GameRecordLoadService に委譲）。
void MainWindow::displayGameRecord(const QList<KifDisplayItem>& disp)
{
    if (!m_models.kifuRecord) return;

    m_registry->ensureGameRecordLoadService();
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

void MainWindow::onMoveRequested(const QPoint& from, const QPoint& to)
{
    m_registry->handleMoveRequested(from, to);
}

// 再生モードの切替を ReplayController へ委譲
void MainWindow::setReplayMode(bool on)
{
    m_registry->ensureReplayController();
    if (m_replayController) {
        m_replayController->setReplayMode(on);
    }
}

void MainWindow::loadBoardFromSfen(const QString& sfen)
{
    m_registry->ensureBoardLoadService();
    m_boardLoadService->loadFromSfen(sfen);
}

void MainWindow::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    m_registry->ensureBoardLoadService();
    m_boardLoadService->loadWithHighlights(currentSfen, prevSfen);
}

void MainWindow::onMoveCommitted(ShogiGameController::Player mover, int ply)
{
    m_registry->handleMoveCommitted(static_cast<int>(mover), ply);
}

// `buildRuntimeRefs`: 現在のメンバ変数から MainWindowRuntimeRefs スナップショットを構築する。
MainWindowRuntimeRefs MainWindow::buildRuntimeRefs()
{
    MainWindowRuntimeRefs refs;

    // --- UI 参照 ---
    refs.parentWidget = this;
    refs.mainWindow = this;
    refs.statusBar = ui->statusbar;

    // --- サービス参照 ---
    refs.match = m_match;
    refs.gameController = m_gameController;
    refs.shogiView = m_shogiView;
    refs.kifuLoadCoordinator = m_kifuLoadCoordinator;
    refs.csaGameCoordinator = m_csaGameCoordinator;

    // --- モデル参照 ---
    refs.kifuRecordModel = m_models.kifuRecord;
    refs.considerationModel = &m_models.consideration;

    // --- 状態参照（ポインタ） ---
    refs.playMode = &m_state.playMode;
    refs.currentMoveIndex = &m_state.currentMoveIndex;
    refs.currentSfenStr = &m_state.currentSfenStr;
    refs.startSfenStr = &m_state.startSfenStr;
    refs.skipBoardSyncForBranchNav = &m_state.skipBoardSyncForBranchNav;

    // --- 棋譜参照 ---
    refs.sfenRecord = m_queryService->sfenRecord();
    refs.gameMoves = &m_kifu.gameMoves;
    refs.gameUsiMoves = &m_kifu.gameUsiMoves;
    refs.moveRecords = &m_kifu.moveRecords;
    refs.positionStrList = &m_kifu.positionStrList;
    refs.activePly = &m_kifu.activePly;
    refs.currentSelectedPly = &m_kifu.currentSelectedPly;
    refs.saveFileName = &m_kifu.saveFileName;

    // --- 分岐ナビゲーション参照 ---
    refs.branchTree = m_branchNav.branchTree;
    refs.navState = m_branchNav.navState;
    refs.displayCoordinator = m_branchNav.displayCoordinator;

    // --- コントローラ / Wiring 参照 ---
    refs.recordPane = m_recordPane;
    refs.recordPresenter = m_recordPresenter;
    refs.replayController = m_replayController;
    refs.timeController = m_timeController;
    refs.boardController = m_boardController;
    refs.positionEditController = m_posEdit;
    refs.dialogCoordinator = m_dialogCoordinator;
    refs.playerInfoWiring = m_playerInfoWiring;
    refs.boardSync = m_boardSync;
    refs.uiStatePolicy = m_uiStatePolicy;
    refs.gameStateController = m_gameStateController;
    refs.consecutiveGamesController = m_consecutiveGamesController;

    // --- モデル参照（追加） ---
    refs.thinking1 = m_models.thinking1;
    refs.thinking2 = m_models.thinking2;
    refs.consideration = m_models.consideration;
    refs.commLog1 = m_models.commLog1;
    refs.commLog2 = m_models.commLog2;
    refs.gameRecordModel = m_models.gameRecord;

    // --- UI フォーム ---
    refs.uiForm = ui.get();

    // --- 状態参照（追加） ---
    refs.commentsByRow = &m_kifu.commentsByRow;
    refs.resumeSfenStr = &m_state.resumeSfenStr;
    refs.onMainRowGuard = &m_kifu.onMainRowGuard;

    // --- 対局者名参照 ---
    refs.humanName1 = &m_player.humanName1;
    refs.humanName2 = &m_player.humanName2;
    refs.engineName1 = &m_player.engineName1;
    refs.engineName2 = &m_player.engineName2;

    // --- その他参照 ---
    refs.gameInfoController = m_gameInfoController;
    refs.evalChart = m_evalChart;
    refs.evalGraphController = m_evalGraphController.get();
    refs.analysisPresenter = m_analysisPresenter.get();
    refs.analysisTab = m_analysisTab;

    // PlayModePolicyService の依存を最新状態に更新
    if (m_playModePolicy) {
        PlayModePolicyService::Deps policyDeps;
        policyDeps.playMode = &m_state.playMode;
        policyDeps.gameController = m_gameController;
        policyDeps.match = m_match;
        policyDeps.csaGameCoordinator = m_csaGameCoordinator;
        m_playModePolicy->updateDeps(policyDeps);
    }

    return refs;
}

// KifuExportControllerに依存を設定するヘルパー
void MainWindow::updateKifuExportDependencies()
{
    KifuExportDepsAssembler::assemble(m_kifuExportController.get(), buildRuntimeRefs());
}

// `onRecordPaneMainRowChanged`: Record Pane Main Row Changed のイベント受信時処理を行う。
void MainWindow::onRecordPaneMainRowChanged(int row)
{
    m_registry->ensureRecordNavigationHandler();
    m_recordNavWiring->handler()->onMainRowChanged(row);
}
