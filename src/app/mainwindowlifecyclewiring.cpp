/// @file mainwindowlifecyclewiring.cpp
/// @brief 起動/終了パイプラインへの依存注入と UI 初期化の配線

#include "mainwindowlifecyclepipeline.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

// Foundation objects
#include "mainwindowfoundationregistry.h"
#include "mainwindowserviceregistry.h"
#include "shogienginethinkingmodel.h"
#include "kifusubregistry.h"
#include "mainwindowsignalrouter.h"
#include "usicommlogmodel.h"

// UI skeleton
#include "mainwindowappearancecontroller.h"
#include "settingsresetcontroller.h"

// QPointer<T> の完全型
#include "boardinteractioncontroller.h"    // IWYU pragma: keep

// Core components
#include "mainwindowcoreinitcoordinator.h"

// Early services
#include "playmodepolicyservice.h"
#include "playmodepolicyservice.h"
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
#include <utility>

#ifdef QT_DEBUG
#include "debugscreenshotwiring.h"
#endif

// 起動・終了時に必要な操作を配線する。実行順序と終了判断は Pipeline が担う。
std::unique_ptr<MainWindowLifecyclePipeline> MainWindowServiceRegistry::createLifecyclePipeline()
{
    MainWindowLifecyclePipeline::Deps deps;
    auto& startup = deps.startup;
    // 1. 基盤オブジェクト生成（UI セットアップ含む）
    startup.createFoundationObjects = [this]() { createFoundationObjectsForLifecycle(); };
    // 2. UI 骨格（central/toolbar）
    startup.setupUiSkeleton = [this]() { setupUiSkeletonForLifecycle(); };
    // 3. コア部品（GameController/View/Clock）
    startup.initializeCoreComponents = [this]() { initializeCoreComponentsForLifecycle(); };
    // 4. 早期サービス（PolicyService/TimePresenter/TimeController）
    startup.initializeEarlyServices = [this]() { initializeEarlyServicesForLifecycle(); };
    // 5. 画面骨格（棋譜/分岐/レイアウト/タブ/中央表示）
    startup.buildGamePanels = [this]() { buildGamePanels(); };
    // 6. ウィンドウ設定の復元（位置/サイズなど）
    startup.restoreWindowAndSync = [this]() { restoreWindowAndSync(); };
    // 7. シグナル配線
    startup.connectSignals = [this]() { connectSignalsForLifecycle(); };
    // 8. コーディネータの最終初期化と追加機能設定
    startup.finalizeAndConfigureUi = [this]() { finalizeAndConfigureUiForLifecycle(); };

    auto& shutdown = deps.shutdown;
    shutdown.beginShutdown = [this]() {
        m_mw.m_isShuttingDown = true;
    };
    // 設定保存
    shutdown.saveSettings = [this]() {
        AppSettings::saveWindowAndBoard(&m_mw, m_mw.m_shogiView ? m_mw.m_shogiView->squareSize() : 0);
        ensureDockLayoutManager();
        if (m_mw.m_dockLayoutManager) {
            m_mw.m_dockLayoutManager->saveDockStates();
        }
    };
    // エンジンが起動していれば終了する（quit コマンドを送信してプロセスを停止）
    shutdown.destroyEngines = [this]() {
        if (m_mw.m_match) {
            m_mw.m_match->destroyEngines();
        }
    };
    // リソース解放（unique_ptr がデストラクタで自動解放するため明示 delete は不要）
    shutdown.releaseOwnedResources = [this]() {
        m_mw.m_playModePolicy.reset();
    };

    deps.isShuttingDown = [this]() { return m_mw.m_isShuttingDown; };
    deps.confirmDiscardUnsavedKifu = [this]() { return confirmDiscardUnsavedKifu(); };
    deps.confirmCloseJoseki = [this]() { return confirmCloseJoseki(); };
    deps.closeWindow = [this]() { return m_mw.close(); };
    deps.quitApplication = []() { QCoreApplication::quit(); };
    return std::make_unique<MainWindowLifecyclePipeline>(std::move(deps));
}

void MainWindowServiceRegistry::createFoundationObjectsForLifecycle()
{
    m_mw.m_signalRouter = std::make_unique<MainWindowSignalRouter>();

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once at startup, never recreated
    m_mw.m_models.commLog1 = new UsiCommLogModel(&m_mw);
    m_mw.m_models.commLog2 = new UsiCommLogModel(&m_mw);
    // 検討タブの思考情報モデル。検討・読み筋表示・矢印が同じものを使うので最初に作る
    m_mw.m_models.consideration = new ShogiEngineThinkingModel(&m_mw);

    m_mw.ui->setupUi(&m_mw);
}

void MainWindowServiceRegistry::setupUiSkeletonForLifecycle()
{
    // ドックネスティングを有効化（同じエリア内でドックを左右に分割可能）
    // setDockNestingEnabled() は QMainWindow で定義された関数
    m_mw.setDockNestingEnabled(true);

    // UI外観コントローラを生成
    m_mw.m_appearanceController = std::make_unique<MainWindowAppearanceController>();

    // セントラルウィジェットとツールバーを設定（外観コントローラへ委譲）
    m_mw.m_appearanceController->setupCentralWidgetContainer(m_mw.ui->centralwidget);
    m_mw.m_central = m_mw.m_appearanceController->central();
    m_mw.m_appearanceController->configureToolBarFromUi(m_mw.ui->toolBar, m_mw.ui->actionToolBar);

    // 外観コントローラの実行時依存（盤・時間表示・司令塔）は、それらを作った後に
    // refreshAppearanceDeps() で設定する（盤と時間表示はステップ 3-4、司令塔はステップ 5）
}

void MainWindowServiceRegistry::refreshAppearanceDeps()
{
    if (!m_mw.m_appearanceController) return;
    MainWindowAppearanceController::Deps appDeps;
    appDeps.shogiView = m_mw.m_shogiView;
    appDeps.timePresenter = m_mw.m_timePresenter;
    appDeps.match = m_mw.m_match;
    appDeps.bottomSide = &m_mw.m_player.bottomSide;
    m_mw.m_appearanceController->updateDeps(appDeps);
}

void MainWindowServiceRegistry::initializeCoreComponentsForLifecycle()
{
    // コア部品（GC, View, 盤モデル etc.）をコーディネータで初期化
    ensureCoreInitCoordinator();
    m_mw.m_coreInit->initialize();
}

void MainWindowServiceRegistry::initializeEarlyServicesForLifecycle()
{
    // プレイモード判定サービスを生成し、初期依存を設定
    m_mw.m_playModePolicy = std::make_unique<PlayModePolicyService>();
    m_mw.refreshPlayModePolicyDeps();

    if (!m_mw.m_timePresenter) {
        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once at startup, never recreated
        m_mw.m_timePresenter = new TimeDisplayPresenter(m_mw.m_shogiView, &m_mw);
    }

    // TimeControlController を初期化して TimeDisplayPresenter に設定
    ensureTimeController();

    if (m_mw.m_timePresenter && m_mw.m_timeController) {
        m_mw.m_timePresenter->setClock(m_mw.m_timeController->clock());
    }

    // 盤（ステップ 3）と時間表示ができたので外観コントローラに渡す。setupBoardInCenter() が
    // buildGamePanels()（ステップ 5）で使う
    refreshAppearanceDeps();
}

void MainWindowServiceRegistry::connectSignalsForLifecycle()
{
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
        d.getDialogCoordinatorWiring = [this]() {
            return m_mw.m_registryParts.dialogCoordinatorWiring;
        };
        d.getDialogLaunchWiring = [this]() {
            return m_mw.m_dialogLaunchWiring;
        };
        d.getKifuFileController = [this]() {
            return m_mw.m_kifuFileController;
        };
        d.getGameSessionOrchestrator = [this]() {
            return m_mw.m_gameSessionOrchestrator;
        };
        d.getNotificationService = [this]() {
            return m_mw.m_notificationService;
        };
        d.getBoardSetupController = [this]() {
            return m_mw.m_boardSetupController;
        };
        d.getActionsWiring = [this]() {
            return m_mw.m_registryParts.actionsWiring;
        };
        d.setActionsWiring = [this](UiActionsWiring* wiring) {
            m_mw.m_registryParts.actionsWiring = wiring;
        };
        d.initializeDialogLaunchWiring = [this]() {
            initializeDialogLaunchWiring();
        };
        d.ensureDialogCoordinator = [this]() {
            ensureDialogCoordinator();
        };
        d.ensureKifuFileController = [this]() {
            kifu()->ensureKifuFileController();
        };
        d.ensureGameSessionOrchestrator = [this]() {
            ensureGameSessionOrchestrator();
        };
        d.ensureUiNotificationService = [this]() {
            foundation()->ensureUiNotificationService();
        };
        d.ensureBoardSetupController = [this]() {
            ensureBoardSetupController();
        };
        d.getKifuExportController = [this]() -> KifuExportController* {
            kifu()->ensureKifuExportController();
            return m_mw.m_kifuExportController.get();
        };
        m_mw.m_signalRouter->updateDeps(d);
    }

    // シグナル配線を一括実行（ダイアログ起動・メニューアクション・コアシグナル）
    m_mw.m_signalRouter->connectAll();

    // ツールチップをコンパクト表示へ（外観コントローラへ委譲）
    m_mw.m_appearanceController->installAppToolTips(&m_mw);
}

void MainWindowServiceRegistry::finalizeAndConfigureUiForLifecycle()
{
    // 司令塔やUIフォント/位置編集コントローラの最終初期化
    finalizeCoordinators();

    // UI状態ポリシーマネージャを初期化し、アイドル状態を適用
    foundation()->ensureUiStatePolicyManager();
    m_mw.m_uiStatePolicy->applyState(UiStatePolicyManager::AppState::Idle);

    // 言語メニューをグループ化（相互排他）して現在の設定を反映
    foundation()->ensureLanguageController();

    auto* settingsReset = new SettingsResetController(&m_mw);
    QObject::connect(m_mw.ui->actionResetSettings, &QAction::triggered,
                     settingsReset, &SettingsResetController::confirmAndQuit);

    // 駒音プレイヤーを生成し、メニュー「駒音」のチェック状態を設定と同期
    foundation()->ensurePieceSoundPlayer();

    // ドックレイアウト関連のメニュー配線を DockLayoutManager へ移譲
    ensureDockLayoutManager();

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
