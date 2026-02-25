/// @file mainwindowuibootstrapper.cpp
/// @brief MainWindow の UI ブート手順の実装

#include "mainwindowuibootstrapper.h"
#include "mainwindow.h"
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
    // 1) 記録ペイン（RecordPane）など UI 部の初期化
    m_mw.setupRecordPane();

    // 2) 棋譜欄をQDockWidgetとして作成
    m_mw.createRecordPaneDock();

    // 3) 将棋盤・駒台の初期化（従来順序を維持）
    m_mw.startNewShogiGame(m_mw.m_state.startSfenStr);

    // 4) 将棋盤をQDockWidgetとして作成
    m_mw.setupBoardInCenter();

    // 5) エンジン解析タブの構築
    m_mw.setupEngineAnalysisTab();

    // 6) 解析用ドックを作成（独立した複数ドック）
    m_mw.createAnalysisDocks();

    // 7) 分岐ナビゲーションクラスの初期化
    m_mw.initializeBranchNavigationClasses();

    // 8) 評価値グラフのQDockWidget作成
    m_mw.createEvalChartDock();

    // 9) ドックを固定アクションの初期状態を設定
    m_mw.ui->actionLockDocks->setChecked(SettingsService::docksLocked());

    // 10) メニューウィンドウのQDockWidget作成（デフォルトは非表示）
    m_mw.createMenuWindowDock();

    // 11) 定跡ウィンドウのQDockWidget作成（デフォルトは非表示）
    m_mw.createJosekiWindowDock();

    // 12) 棋譜解析結果のQDockWidget作成（初期状態は非表示）
    m_mw.createAnalysisResultsDock();

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
    m_mw.loadWindowSettings();

    // 起動時のカスタムレイアウトがあれば復元
    m_mw.ensureDockLayoutManager();
    if (m_mw.m_dockLayoutManager) {
        m_mw.m_dockLayoutManager->restoreStartupLayoutIfSet();
    }
}

void MainWindowUiBootstrapper::finalizeCoordinators()
{
    m_mw.initMatchCoordinator();
    m_mw.setupNameAndClockFonts();
    m_mw.ensurePositionEditController();
    m_mw.ensureBoardSyncPresenter();
    m_mw.ensureAnalysisPresenter();
}
