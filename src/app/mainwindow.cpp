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
#include "gamesessionsubregistry.h"
#include "gamesubregistry.h"
#include "kifusubregistry.h"

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

// --- QPointer<T> の完全型（buildRuntimeRefs 用） ---
#include "boardinteractioncontroller.h"    // IWYU pragma: keep
#include "boardsyncpresenter.h"            // IWYU pragma: keep
#include "kifuloadcoordinator.h"           // IWYU pragma: keep
#include "positioneditcontroller.h"        // IWYU pragma: keep

// --- Controllers / Services（スロット転送先） ---
#include "gamerecordloadservice.h"
#include "turnstatesyncservice.h"
#include "undoflowservice.h"

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
    m_registry->game()->ensureUndoFlowService();
    m_undoFlowService->undoLastTwoMoves();
}

void MainWindow::updateJosekiWindow()
{
    m_registry->kifu()->updateJosekiWindow();
}

// TurnManager::changed を受けて UI/Clock を更新（＋手番を GameController に同期）
void MainWindow::onTurnManagerChanged(ShogiGameController::Player now)
{
    m_registry->game()->ensureTurnStateSyncService();
    m_turnStateSync->onTurnManagerChanged(now);
}

// 現在の手番を設定する。
void MainWindow::setCurrentTurn()
{
    m_registry->game()->ensureTurnStateSyncService();
    m_turnStateSync->setCurrentTurn();
}

// 設定ファイルにGUI全体のウィンドウサイズを書き込む。
// また、将棋盤のマスサイズも書き込む。その後、GUIを終了する。
void MainWindow::saveSettingsAndClose()
{
    m_pipeline->runShutdown();
    QCoreApplication::quit();
}

// GUIを初期画面表示に戻す（GameSessionSubRegistryへ委譲）。
void MainWindow::resetToInitialState()
{
    m_registry->game()->session()->resetToInitialState();
}

// ゲーム状態変数のリセット（GameSessionSubRegistryへ委譲）。
void MainWindow::resetGameState()
{
    m_registry->game()->session()->resetGameState();
}

// `displayGameRecord`: Game Record を表示する（GameRecordLoadService に委譲）。
void MainWindow::displayGameRecord(const QList<KifDisplayItem>& disp)
{
    if (!m_models.kifuRecord) return;

    m_registry->kifu()->ensureGameRecordLoadService();
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

// 再生モードの切替を GameSubRegistry へ委譲
void MainWindow::setReplayMode(bool on)
{
    m_registry->game()->setReplayMode(on);
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
    MainWindowRuntimeRefs refs;

    // --- UI 参照 ---
    refs.ui.parentWidget = this;
    refs.ui.mainWindow = this;
    refs.ui.statusBar = ui->statusbar;
    refs.ui.uiForm = ui.get();
    refs.ui.recordPane = m_recordPane;
    refs.ui.evalChart = m_evalChart;
    refs.ui.analysisTab = m_analysisTab;

    // --- モデル参照 ---
    refs.models.kifuRecordModel = m_models.kifuRecord;
    refs.models.considerationModel = &m_models.consideration;
    refs.models.thinking1 = m_models.thinking1;
    refs.models.thinking2 = m_models.thinking2;
    refs.models.consideration = m_models.consideration;
    refs.models.commLog1 = m_models.commLog1;
    refs.models.commLog2 = m_models.commLog2;
    refs.models.gameRecordModel = m_models.gameRecord;

    // --- 棋譜データ参照 ---
    refs.kifu.sfenRecord = m_queryService->sfenRecord();
    refs.kifu.gameMoves = &m_kifu.gameMoves;
    refs.kifu.gameUsiMoves = &m_kifu.gameUsiMoves;
    refs.kifu.moveRecords = &m_kifu.moveRecords;
    refs.kifu.positionStrList = &m_kifu.positionStrList;
    refs.kifu.activePly = &m_kifu.activePly;
    refs.kifu.currentSelectedPly = &m_kifu.currentSelectedPly;
    refs.kifu.saveFileName = &m_kifu.saveFileName;
    refs.kifu.commentsByRow = &m_kifu.commentsByRow;
    refs.kifu.onMainRowGuard = &m_kifu.onMainRowGuard;

    // --- 可変状態参照 ---
    refs.state.playMode = &m_state.playMode;
    refs.state.currentMoveIndex = &m_state.currentMoveIndex;
    refs.state.currentSfenStr = &m_state.currentSfenStr;
    refs.state.startSfenStr = &m_state.startSfenStr;
    refs.state.skipBoardSyncForBranchNav = &m_state.skipBoardSyncForBranchNav;
    refs.state.resumeSfenStr = &m_state.resumeSfenStr;

    // --- 分岐ナビゲーション参照 ---
    refs.branchNav.branchTree = m_branchNav.branchTree;
    refs.branchNav.navState = m_branchNav.navState;
    refs.branchNav.displayCoordinator = m_branchNav.displayCoordinator;

    // --- 対局者名参照 ---
    refs.player.humanName1 = &m_player.humanName1;
    refs.player.humanName2 = &m_player.humanName2;
    refs.player.engineName1 = &m_player.engineName1;
    refs.player.engineName2 = &m_player.engineName2;

    // --- サービス参照 ---
    refs.match = m_match;
    refs.gameController = m_gameController;
    refs.shogiView = m_shogiView;
    refs.kifuLoadCoordinator = m_kifuLoadCoordinator;
    refs.csaGameCoordinator = m_csaGameCoordinator;

    // --- コントローラ / Wiring 参照 ---
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

    // --- その他参照 ---
    refs.gameInfoController = m_gameInfoController;
    refs.evalGraphController = m_evalGraphController.get();
    refs.analysisPresenter = m_analysisPresenter.get();

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

// `onRecordPaneMainRowChanged`: Record Pane Main Row Changed のイベント受信時処理を行う。
void MainWindow::onRecordPaneMainRowChanged(int row)
{
    m_registry->onRecordPaneMainRowChanged(row);
}
