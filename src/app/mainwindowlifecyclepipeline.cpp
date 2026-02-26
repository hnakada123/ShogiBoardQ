/// @file mainwindowlifecyclepipeline.cpp
/// @brief MainWindow の起動/終了フローの実装

#include "mainwindowlifecyclepipeline.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

// Foundation objects
#include "mainwindowcompositionroot.h"
#include "mainwindowserviceregistry.h"
#include "mainwindowsignalrouter.h"
#include "usicommlogmodel.h"

// UI skeleton
#include "mainwindowappearancecontroller.h"

// Core components
#include "mainwindowcoreinitcoordinator.h"

// Early services
#include "playmodepolicyservice.h"
#include "matchruntimequeryservice.h"
#include "timedisplaypresenter.h"
#include "timecontrolcontroller.h"
#include "shogiclock.h"

// UI boot
#include "mainwindowuibootstrapper.h"

// Finalization
#include "uistatepolicymanager.h"

// Shutdown
#include "settingsservice.h"
#include "docklayoutmanager.h"
#include "matchcoordinator.h"

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
    // 1. 基盤オブジェクト生成（UI セットアップ含む）
    createFoundationObjects();

    // 2. UI 骨格（central/toolbar）
    setupUiSkeleton();

    // 3. コア部品（GameController/View/Clock）
    initializeCoreComponents();

    // 4. 早期サービス（PolicyService/TimePresenter/QueryService）
    initializeEarlyServices();

    // 5. 画面骨格（棋譜/分岐/レイアウト/タブ/中央表示）
    buildGamePanels();

    // 6. ウィンドウ設定の復元（位置/サイズなど）
    restoreWindowAndSync();

    // 7. シグナル配線
    connectSignals();

    // 8. コーディネータの最終初期化と追加機能設定
    finalizeAndConfigureUi();
}

// 終了手順を一括実行する。二重実行防止付き。
void MainWindowLifecyclePipeline::runShutdown()
{
    if (m_shutdownDone) return;
    m_shutdownDone = true;

    // 設定保存
    SettingsService::saveWindowAndBoard(&m_mw, m_mw.m_shogiView);
    m_mw.ensureDockLayoutManager();
    if (m_mw.m_dockLayoutManager) {
        m_mw.m_dockLayoutManager->saveDockStates();
    }

    // エンジンが起動していれば終了する（quit コマンドを送信してプロセスを停止）
    if (m_mw.m_match) {
        m_mw.m_match->destroyEngines();
    }

    // リソース解放
    delete m_mw.m_queryService;
    m_mw.m_queryService = nullptr;
    delete m_mw.m_playModePolicy;
    m_mw.m_playModePolicy = nullptr;
}

void MainWindowLifecyclePipeline::createFoundationObjects()
{
    m_mw.m_compositionRoot = new MainWindowCompositionRoot(&m_mw);
    m_mw.m_registry = new MainWindowServiceRegistry(m_mw, &m_mw);
    m_mw.m_signalRouter = new MainWindowSignalRouter(m_mw, &m_mw);

    m_mw.m_models.commLog1 = new UsiCommLogModel(&m_mw);
    m_mw.m_models.commLog2 = new UsiCommLogModel(&m_mw);

    // 対局実行時クエリサービスを早期に生成（deps は initializeEarlyServices で設定）
    // buildRuntimeRefs() が initializeCoreComponents() 内で参照するため、
    // UI セットアップ前に存在させる必要がある。
    m_mw.m_queryService = new MatchRuntimeQueryService();

    m_mw.ui->setupUi(&m_mw);
}

void MainWindowLifecyclePipeline::setupUiSkeleton()
{
    // ドックネスティングを有効化（同じエリア内でドックを左右に分割可能）
    // setDockNestingEnabled() は QMainWindow で定義された関数
    m_mw.setDockNestingEnabled(true);

    // UI外観コントローラを生成
    m_mw.m_appearanceController = new MainWindowAppearanceController(&m_mw);

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

void MainWindowLifecyclePipeline::initializeCoreComponents()
{
    // コア部品（GC, View, 盤モデル etc.）をコーディネータで初期化
    m_mw.m_registry->ensureCoreInitCoordinator();
    m_mw.m_coreInit->initialize();
}

void MainWindowLifecyclePipeline::initializeEarlyServices()
{
    // プレイモード判定サービスを生成し、初期依存を設定
    m_mw.m_playModePolicy = new PlayModePolicyService();
    {
        PlayModePolicyService::Deps policyDeps;
        policyDeps.playMode = &m_mw.m_state.playMode;
        policyDeps.gameController = m_mw.m_gameController;
        m_mw.m_playModePolicy->updateDeps(policyDeps);
    }

    if (!m_mw.m_timePresenter) {
        m_mw.m_timePresenter = new TimeDisplayPresenter(m_mw.m_shogiView, &m_mw);
    }

    // TimeControlController を初期化して TimeDisplayPresenter に設定
    m_mw.ensureTimeController();

    // 対局実行時クエリサービスの依存を設定（生成は createFoundationObjects で実施済み）
    {
        MatchRuntimeQueryService::Deps qsDeps;
        qsDeps.playModePolicy = m_mw.m_playModePolicy;
        qsDeps.timeController = m_mw.m_timeController;
        qsDeps.match = &m_mw.m_match;
        m_mw.m_queryService->updateDeps(qsDeps);
    }

    if (m_mw.m_timePresenter && m_mw.m_timeController) {
        m_mw.m_timePresenter->setClock(m_mw.m_timeController->clock());
    }
}

void MainWindowLifecyclePipeline::buildGamePanels()
{
    MainWindowUiBootstrapper bootstrapper(m_mw);
    bootstrapper.buildGamePanels();
}

void MainWindowLifecyclePipeline::restoreWindowAndSync()
{
    MainWindowUiBootstrapper bootstrapper(m_mw);
    bootstrapper.restoreWindowAndSync();
}

void MainWindowLifecyclePipeline::connectSignals()
{
    // シグナル配線を一括実行（ダイアログ起動・メニューアクション・コアシグナル）
    m_mw.m_signalRouter->connectAll();

    // ツールチップをコンパクト表示へ（外観コントローラへ委譲）
    m_mw.m_appearanceController->installAppToolTips(&m_mw);
}

void MainWindowLifecyclePipeline::finalizeAndConfigureUi()
{
    // 司令塔やUIフォント/位置編集コントローラの最終初期化
    {
        MainWindowUiBootstrapper bootstrapper(m_mw);
        bootstrapper.finalizeCoordinators();
    }

    // UI状態ポリシーマネージャを初期化し、アイドル状態を適用
    m_mw.ensureUiStatePolicyManager();
    m_mw.m_uiStatePolicy->applyState(UiStatePolicyManager::AppState::Idle);

    // 言語メニューをグループ化（相互排他）して現在の設定を反映
    m_mw.ensureLanguageController();

    // ドックレイアウト関連のメニュー配線を DockLayoutManager へ移譲
    m_mw.ensureDockLayoutManager();

#ifdef QT_DEBUG
    // デバッグ用スクリーンショット機能（F12キーで /tmp/shogiboardq-debug/ にPNG保存）
    m_mw.m_debugScreenshotWiring = new DebugScreenshotWiring(&m_mw, &m_mw);
#endif

    // 評価値グラフ高さ調整用タイマーを初期化（デバウンス処理用）
    m_mw.m_evalChartResizeTimer = new QTimer(&m_mw);
    m_mw.m_evalChartResizeTimer->setSingleShot(true);
    QObject::connect(m_mw.m_evalChartResizeTimer, &QTimer::timeout,
                     m_mw.m_appearanceController, &MainWindowAppearanceController::performDeferredEvalChartResize);

}
