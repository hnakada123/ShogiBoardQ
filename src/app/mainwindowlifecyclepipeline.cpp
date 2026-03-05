/// @file mainwindowlifecyclepipeline.cpp
/// @brief MainWindow の起動/終了フローの実装

#include "mainwindowlifecyclepipeline.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

// Foundation objects
#include "mainwindowcompositionroot.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowserviceregistry.h"
#include "mainwindowserviceregistryfacades.h"
#include "gamesessionsubregistry.h"
#include "gamesubregistry.h"
#include "kifusubregistry.h"
#include "mainwindowsignalrouter.h"
#include "usicommlogmodel.h"

// UI skeleton
#include "mainwindowappearancecontroller.h"

// QPointer<T> の完全型
#include "boardinteractioncontroller.h"    // IWYU pragma: keep

// Core components
#include "mainwindowcoreinitcoordinator.h"

// Early services
#include "playmodepolicyservice.h"
#include "matchruntimequeryservice.h"
#include "timedisplaypresenter.h"
#include "timecontrolcontroller.h"
#include "shogiclock.h"

// Signal wiring
#include "kifuexportcontroller.h"

// Finalization
#include "uistatepolicymanager.h"

// Shutdown
#include "appsettings.h"
#include "docklayoutmanager.h"
#include "matchcoordinator.h"
#include "shogiview.h"

#include <QCoreApplication>
#include <QTimer>

#ifdef QT_DEBUG
#include "debugscreenshotwiring.h"
#endif

MainWindowLifecyclePipeline::MainWindowLifecyclePipeline(MainWindow& mw)
    : m_mw(mw)
{
}

// 起動手順を依存順に実行する。
// 各ステップ間には順序依存があるため、並び替え禁止。
void MainWindowLifecyclePipeline::runStartup()
{
    m_mw.runLifecycleStartupInternal();
}

// 終了手順を一括実行する。二重実行防止付き。
void MainWindowLifecyclePipeline::runShutdown()
{
    m_mw.runLifecycleShutdownInternal(m_shutdownDone);
}

void MainWindowLifecyclePipeline::shutdownAndQuit()
{
    runShutdown();
    QCoreApplication::quit();
}

// 起動手順を依存順に実行する（MainWindow 側の内部実装）。
void MainWindow::runLifecycleStartupInternal()
{
    // 1. 基盤オブジェクト生成（UI セットアップ含む）
    createFoundationObjectsForLifecycle();

    // 2. UI 骨格（central/toolbar）
    setupUiSkeletonForLifecycle();

    // 3. コア部品（GameController/View/Clock）
    initializeCoreComponentsForLifecycle();

    // 4. 早期サービス（PolicyService/TimePresenter/QueryService）
    initializeEarlyServicesForLifecycle();

    // 5. 画面骨格（棋譜/分岐/レイアウト/タブ/中央表示）
    m_registry->ui()->buildGamePanels();

    // 6. ウィンドウ設定の復元（位置/サイズなど）
    m_registry->ui()->restoreWindowAndSync();

    // 7. シグナル配線
    connectSignalsForLifecycle();

    // 8. コーディネータの最終初期化と追加機能設定
    finalizeAndConfigureUiForLifecycle();
}

// 終了手順を一括実行する。二重実行防止フラグは呼び出し側が保持する。
void MainWindow::runLifecycleShutdownInternal(bool& shutdownDone)
{
    if (shutdownDone) return;
    shutdownDone = true;
    m_isShuttingDown = true;

    // 設定保存
    AppSettings::saveWindowAndBoard(this, m_shogiView ? m_shogiView->squareSize() : 0);
    m_registry->ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->saveDockStates();
    }

    // エンジンが起動していれば終了する（quit コマンドを送信してプロセスを停止）
    if (m_match) {
        m_match->destroyEngines();
    }

    // m_queryService は各所の std::bind 経由で参照されるため、
    // ここで破棄せず依存だけ無効化して終了時アクセスを安全化する。
    if (m_queryService) {
        MatchRuntimeQueryService::Deps nullDeps;
        m_queryService->updateDeps(nullDeps);
    }

    // リソース解放（unique_ptr がデストラクタで自動解放するため明示 delete は不要）
    m_playModePolicy.reset();
}

void MainWindow::createFoundationObjectsForLifecycle()
{
    MainWindow& m_mw = *this;
    m_mw.m_compositionRoot = std::make_unique<MainWindowCompositionRoot>();
    m_mw.m_registry = std::make_unique<MainWindowServiceRegistry>(m_mw);
    m_mw.m_signalRouter = std::make_unique<MainWindowSignalRouter>();

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once at startup, never recreated
    m_mw.m_models.commLog1 = new UsiCommLogModel(&m_mw);
    m_mw.m_models.commLog2 = new UsiCommLogModel(&m_mw);

    // 対局実行時クエリサービスを早期に生成（deps は initializeEarlyServices で設定）
    // buildRuntimeRefs() が initializeCoreComponents() 内で参照するため、
    // UI セットアップ前に存在させる必要がある。
    m_mw.m_queryService = std::make_unique<MatchRuntimeQueryService>();

    m_mw.ui->setupUi(&m_mw);
}

void MainWindow::setupUiSkeletonForLifecycle()
{
    MainWindow& m_mw = *this;
    // ドックネスティングを有効化（同じエリア内でドックを左右に分割可能）
    // setDockNestingEnabled() は QMainWindow で定義された関数
    m_mw.setDockNestingEnabled(true);

    // UI外観コントローラを生成
    m_mw.m_appearanceController = std::make_unique<MainWindowAppearanceController>();

    // セントラルウィジェットとツールバーを設定（外観コントローラへ委譲）
    m_mw.m_appearanceController->setupCentralWidgetContainer(m_mw.ui->centralwidget);
    m_mw.m_central = m_mw.m_appearanceController->central();
    m_mw.m_appearanceController->configureToolBarFromUi(m_mw.ui->toolBar, m_mw.ui->actionToolBar);

    // 外観コントローラに実行時依存を設定（ダブルポインタのためアドレスは常に有効）
    // setupBoardInCenter() が buildGamePanels() (Step 5) で使うため、ここで設定する。
    {
        MainWindowAppearanceController::Deps appDeps;
        appDeps.shogiView = &m_mw.m_shogiView;
        appDeps.timePresenter = &m_mw.m_timePresenter;
        appDeps.match = &m_mw.m_match;
        appDeps.bottomIsP1 = &m_mw.m_player.bottomIsP1;
        appDeps.lastP1Turn = &m_mw.m_player.lastP1Turn;
        appDeps.lastP1Ms = &m_mw.m_player.lastP1Ms;
        appDeps.lastP2Ms = &m_mw.m_player.lastP2Ms;
        m_mw.m_appearanceController->updateDeps(appDeps);
    }
}

void MainWindow::initializeCoreComponentsForLifecycle()
{
    MainWindow& m_mw = *this;
    // コア部品（GC, View, 盤モデル etc.）をコーディネータで初期化
    m_mw.m_registry->game()->session()->ensureCoreInitCoordinator();
    m_mw.m_coreInit->initialize();
}

void MainWindow::initializeEarlyServicesForLifecycle()
{
    MainWindow& m_mw = *this;
    // プレイモード判定サービスを生成し、初期依存を設定
    m_mw.m_playModePolicy = std::make_unique<PlayModePolicyService>();
    m_mw.refreshPlayModePolicyDeps();

    if (!m_mw.m_timePresenter) {
        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once at startup, never recreated
        m_mw.m_timePresenter = new TimeDisplayPresenter(m_mw.m_shogiView, &m_mw);
    }

    // TimeControlController を初期化して TimeDisplayPresenter に設定
    m_mw.m_registry->game()->ensureTimeController();

    // 対局実行時クエリサービスの依存を設定（生成は createFoundationObjects で実施済み）
    {
        MatchRuntimeQueryService::Deps qsDeps;
        qsDeps.playModePolicy = m_mw.m_playModePolicy.get();
        qsDeps.timeController = m_mw.m_timeController;
        qsDeps.match = &m_mw.m_match;
        m_mw.m_queryService->updateDeps(qsDeps);
    }

    if (m_mw.m_timePresenter && m_mw.m_timeController) {
        m_mw.m_timePresenter->setClock(m_mw.m_timeController->clock());
    }
}

void MainWindow::connectSignalsForLifecycle()
{
    MainWindow& m_mw = *this;
    // SignalRouter の依存を設定（ステップ 1-6 で生成済みのオブジェクトを渡す）
    {
        MainWindowSignalRouter::Deps d;
        d.mainWindow = &m_mw;
        d.ui = m_mw.ui.get();
        d.appearanceController = m_mw.m_appearanceController.get();
        d.shogiView = m_mw.m_shogiView;
        d.evalChart = m_mw.m_evalChart;
        d.gameController = m_mw.m_gameController;
        d.boardController = m_mw.m_boardController;
        d.getDialogCoordinatorWiring = [this, &m_mw]() {
            return m_mw.m_registryParts.dialogCoordinatorWiring;
        };
        d.getDialogLaunchWiring = [this, &m_mw]() {
            return m_mw.m_dialogLaunchWiring;
        };
        d.getKifuFileController = [this, &m_mw]() {
            return m_mw.m_kifuFileController;
        };
        d.getGameSessionOrchestrator = [this, &m_mw]() {
            return m_mw.m_gameSessionOrchestrator;
        };
        d.getNotificationService = [this, &m_mw]() {
            return m_mw.m_notificationService;
        };
        d.getBoardSetupController = [this, &m_mw]() {
            return m_mw.m_boardSetupController;
        };
        d.getActionsWiring = [this, &m_mw]() {
            return m_mw.m_registryParts.actionsWiring;
        };
        d.setActionsWiring = [this, &m_mw](UiActionsWiring* wiring) {
            m_mw.m_registryParts.actionsWiring = wiring;
        };
        d.initializeDialogLaunchWiring = [this, &m_mw]() {
            m_mw.m_registry->ui()->initializeDialogLaunchWiring();
        };
        d.ensureDialogCoordinator = [this, &m_mw]() {
            m_mw.m_registry->ui()->ensureDialogCoordinator();
        };
        d.ensureKifuFileController = [this, &m_mw]() {
            m_mw.m_registry->kifu()->ensureKifuFileController();
        };
        d.ensureGameSessionOrchestrator = [this, &m_mw]() {
            m_mw.m_registry->game()->session()->ensureGameSessionOrchestrator();
        };
        d.ensureUiNotificationService = [this, &m_mw]() {
            m_mw.m_registry->foundation()->ensureUiNotificationService();
        };
        d.ensureBoardSetupController = [this, &m_mw]() {
            m_mw.m_registry->board()->ensureBoardSetupController();
        };
        d.getKifuExportController = [this, &m_mw]() -> KifuExportController* {
            m_mw.m_registry->kifu()->ensureKifuExportController();
            return m_mw.m_kifuExportController.get();
        };
        m_mw.m_signalRouter->updateDeps(d);
    }

    // シグナル配線を一括実行（ダイアログ起動・メニューアクション・コアシグナル）
    m_mw.m_signalRouter->connectAll();

    // ツールチップをコンパクト表示へ（外観コントローラへ委譲）
    m_mw.m_appearanceController->installAppToolTips(&m_mw);
}

void MainWindow::finalizeAndConfigureUiForLifecycle()
{
    MainWindow& m_mw = *this;
    // 司令塔やUIフォント/位置編集コントローラの最終初期化
    m_mw.m_registry->ui()->finalizeCoordinators();

    // UI状態ポリシーマネージャを初期化し、アイドル状態を適用
    m_mw.m_registry->foundation()->ensureUiStatePolicyManager();
    m_mw.m_uiStatePolicy->applyState(UiStatePolicyManager::AppState::Idle);

    // 言語メニューをグループ化（相互排他）して現在の設定を反映
    m_mw.m_registry->foundation()->ensureLanguageController();

    // ドックレイアウト関連のメニュー配線を DockLayoutManager へ移譲
    m_mw.m_registry->ui()->ensureDockLayoutManager();

#ifdef QT_DEBUG
    // デバッグ用スクリーンショット機能（F12キーで /tmp/shogiboardq-debug/ にPNG保存）
    m_mw.m_debugScreenshotWiring = std::make_unique<DebugScreenshotWiring>(&m_mw);
#endif

    // 評価値グラフ高さ調整用タイマーを初期化（デバウンス処理用）
    m_mw.m_evalChartResizeTimer = std::make_unique<QTimer>();
    m_mw.m_evalChartResizeTimer->setSingleShot(true);
    QObject::connect(m_mw.m_evalChartResizeTimer.get(), &QTimer::timeout,
                     m_mw.m_appearanceController.get(), &MainWindowAppearanceController::performDeferredEvalChartResize);

}
