/// @file mainwindowuibootstrapper.cpp
/// @brief MainWindow の UI ブート手順の実装

#include "mainwindowuibootstrapper.h"
#include "gamesessionorchestrator.h"
#include "mainwindow.h"
#include "mainwindowappearancecontroller.h"
#include "mainwindowdockbootstrapper.h"
#include "mainwindowserviceregistry.h"
#include "ui_mainwindow.h"
#include "settingsservice.h"
#include "dockuicoordinator.h"
#include "dockcreationservice.h"
#include "docklayoutmanager.h"

MainWindowUiBootstrapper::MainWindowUiBootstrapper(MainWindow& mw)
    : m_mw(mw)
{
}

// MainWindow が管理する主要パネルを依存順に構築する。
// RecordPane を先に用意することで、後続の分岐ナビ初期化が
// ボタン/ビュー参照を安全に取得できる。
void MainWindowUiBootstrapper::buildGamePanels()
{
    MainWindowDockBootstrapper dockBoot(m_mw);

    // 1) 記録ペイン（RecordPane）など UI 部の初期化
    dockBoot.setupRecordPane();

    // 2) 棋譜欄をQDockWidgetとして作成
    dockBoot.createRecordPaneDock();

    // 3) 将棋盤・駒台の初期化（従来順序を維持）
    m_mw.m_registry->ensureGameSessionOrchestrator();
    m_mw.m_gameSessionOrchestrator->startNewShogiGame();

    // 4) 将棋盤をセントラルウィジェットに配置（外観コントローラへ委譲）
    m_mw.m_appearanceController->setupBoardInCenter();

    // 5) エンジン解析タブの構築
    dockBoot.setupEngineAnalysisTab();

    // 6) 解析用ドックを作成（独立した複数ドック）
    dockBoot.createAnalysisDocks();

    // 7) 分岐ナビゲーションクラスの初期化
    dockBoot.initializeBranchNavigationClasses();

    // 8) 評価値グラフのQDockWidget作成
    dockBoot.createEvalChartDock();

    // 9) ドックを固定アクションの初期状態を設定
    m_mw.ui->actionLockDocks->setChecked(SettingsService::docksLocked());

    // 10) メニューウィンドウのQDockWidget作成（デフォルトは非表示）
    dockBoot.createMenuWindowDock();

    // 11) 定跡ウィンドウのQDockWidget作成（デフォルトは非表示）
    dockBoot.createJosekiWindowDock();

    // 12) 棋譜解析結果のQDockWidget作成（初期状態は非表示）
    dockBoot.createAnalysisResultsDock();

    // 13) アクティブタブを設定（全ドック作成後に実行）
    if (m_mw.m_dockCreationService) {
        if (m_mw.m_dockCreationService->thinkingDock())
            m_mw.m_dockCreationService->thinkingDock()->raise();
        if (m_mw.m_dockCreationService->recordPaneDock())
            m_mw.m_dockCreationService->recordPaneDock()->raise();
    }

    // 14) ドックレイアウト関連アクションをDockLayoutManagerに配線
    m_mw.ensureDockLayoutManager();
    if (m_mw.m_dockLayoutManager) {
        DockUiCoordinator::wireLayoutMenu(
            m_mw.m_dockLayoutManager,
            m_mw.ui->actionResetDockLayout,
            m_mw.ui->actionSaveDockLayout,
            m_mw.ui->actionLockDocks,
            m_mw.ui->menuSavedLayouts);
    }
}

void MainWindowUiBootstrapper::restoreWindowAndSync()
{
    SettingsService::loadWindowSize(&m_mw);

    // 起動時のカスタムレイアウトがあれば復元
    m_mw.ensureDockLayoutManager();
    if (m_mw.m_dockLayoutManager) {
        m_mw.m_dockLayoutManager->restoreStartupLayoutIfSet();
    }
}

void MainWindowUiBootstrapper::finalizeCoordinators()
{
    m_mw.m_registry->initMatchCoordinator();
    m_mw.m_appearanceController->setupNameAndClockFonts();
    m_mw.m_registry->ensurePositionEditController();
    m_mw.m_registry->ensureBoardSyncPresenter();
    m_mw.m_registry->ensureAnalysisPresenter();
}
