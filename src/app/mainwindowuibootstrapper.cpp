/// @file mainwindowuibootstrapper.cpp
/// @brief UiBootstrapper系メソッドの実装（ServiceRegistry統合版）
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。
/// 旧 MainWindowUiBootstrapper の3メソッドを ServiceRegistry のメソッドとして提供する。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowappearancecontroller.h"
#include "mainwindowfoundationregistry.h"
#include "ui_mainwindow.h"
#include "docksettings.h"
#include "dockuicoordinator.h"
#include "dockcreationservice.h"
#include "docklayoutmanager.h"
#include "gamesessionorchestrator.h"
#include "appsettings.h"

void MainWindowServiceRegistry::buildGamePanels()
{
    // 1) 記録ペイン（RecordPane）など UI 部の初期化
    setupRecordPane();

    // 2) 棋譜欄をQDockWidgetとして作成
    createRecordPaneDock();

    // 3) 将棋盤・駒台の初期化（従来順序を維持）
    ensureGameSessionOrchestrator();
    m_mw.m_gameSessionOrchestrator->startNewShogiGame();

    // 4) 将棋盤をセントラルウィジェットに配置（外観コントローラへ委譲）
    m_mw.m_appearanceController->setupBoardInCenter();

    // 5) エンジン解析タブの構築
    setupEngineAnalysisTab();

    // 6) 解析用ドックを作成（独立した複数ドック）
    createAnalysisDocks();

    // 7) 分岐ナビゲーションクラスの初期化
    initializeBranchNavigationClasses();

    // 8) 評価値グラフのQDockWidget作成
    createEvalChartDock();

    // 9) ドックを固定アクションの初期状態を設定
    m_mw.ui->actionLockDocks->setChecked(DockSettings::docksLocked());

    // 10) メニューウィンドウのQDockWidget作成（デフォルトは非表示）
    createMenuWindowDock();

    // 11) 定跡ウィンドウのQDockWidget作成（デフォルトは非表示）
    createJosekiWindowDock();

    // 12) 棋譜解析結果のQDockWidget作成（初期状態は非表示）
    createAnalysisResultsDock();

    // 13) アクティブタブを設定（全ドック作成後に実行）
    if (m_mw.m_dockCreationService) {
        if (m_mw.m_dockCreationService->thinkingDock())
            m_mw.m_dockCreationService->thinkingDock()->raise();
        if (m_mw.m_dockCreationService->recordPaneDock())
            m_mw.m_dockCreationService->recordPaneDock()->raise();
    }

    // 14) ドックレイアウト関連アクションをDockLayoutManagerに配線
    ensureDockLayoutManager();
    if (m_mw.m_dockLayoutManager) {
        DockUiCoordinator::wireLayoutMenu(
            m_mw.m_dockLayoutManager.get(),
            m_mw.ui->actionResetDockLayout,
            m_mw.ui->actionSaveDockLayout,
            m_mw.ui->actionLockDocks,
            m_mw.ui->menuSavedLayouts);
    }
}

void MainWindowServiceRegistry::restoreWindowAndSync()
{
    AppSettings::loadWindowSize(&m_mw);

    // 起動時のカスタムレイアウトがあれば復元
    ensureDockLayoutManager();
    if (m_mw.m_dockLayoutManager) {
        m_mw.m_dockLayoutManager->restoreStartupLayoutIfSet();
    }
}

void MainWindowServiceRegistry::finalizeCoordinators()
{
    initMatchCoordinator();
    m_mw.m_appearanceController->setupNameAndClockFonts();
    m_foundation->ensurePositionEditController();
    m_foundation->ensureBoardSyncPresenter();
    m_foundation->ensureAnalysisPresenter();
}
