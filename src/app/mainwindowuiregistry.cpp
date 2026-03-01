/// @file mainwindowuiregistry.cpp
/// @brief UI系（ウィジェット・プレゼンター・ビュー・ドック・メニュー・通知）の ensure* 実装
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。
/// 共通基盤メソッドは MainWindowFoundationRegistry に移動済み。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowfoundationregistry.h"
#include "ui_mainwindow.h"

#include "commentcoordinator.h"
#include "dialogcoordinator.h"
#include "dialogcoordinatorwiring.h"
#include "docklayoutmanager.h"
#include "evaluationgraphcontroller.h"
#include "evaluationchartwidget.h"
#include "gamerecordpresenter.h"
#include "kifunavigationcoordinator.h"
#include "recordnavigationhandler.h"
#include "recordnavigationwiring.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"
#include "logcategories.h"

// ---------------------------------------------------------------------------
// 棋譜表示プレゼンター
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureRecordPresenter()
{
    if (m_mw.m_recordPresenter) return;

    GameRecordPresenter::Deps d;
    d.model      = m_mw.m_models.kifuRecord;
    d.recordPane = m_mw.m_recordPane;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_recordPresenter = new GameRecordPresenter(d, &m_mw);

    m_foundation->ensureCommentCoordinator();
    QObject::connect(
        m_mw.m_recordPresenter,
        &GameRecordPresenter::currentRowChanged,
        m_mw.m_commentCoordinator,
        &CommentCoordinator::handleRecordRowChangeRequest,
        Qt::UniqueConnection
        );
}

// ---------------------------------------------------------------------------
// ダイアログコーディネーター
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureDialogCoordinator()
{
    if (m_mw.m_dialogCoordinator) return;

    auto refs = m_mw.buildRuntimeRefs();

    MainWindowDepsFactory::DialogCoordinatorCallbacks callbacks;
    callbacks.getBoardFlipped = [this]() { return m_mw.m_shogiView ? m_mw.m_shogiView->getFlipMode() : false; };
    callbacks.getConsiderationWiring = [this]() { ensureConsiderationWiring(); return m_mw.m_considerationWiring; };
    callbacks.getUiStatePolicyManager = [this]() { m_foundation->ensureUiStatePolicyManager(); return m_mw.m_uiStatePolicy; };
    m_foundation->ensureKifuNavigationCoordinator();
    callbacks.navigateKifuViewToRow = std::bind(&KifuNavigationCoordinator::navigateToRow, m_mw.m_kifuNavCoordinator.get(), std::placeholders::_1);

    m_mw.m_compositionRoot->ensureDialogCoordinator(refs, callbacks, &m_mw,
        m_mw.m_registryParts.dialogCoordinatorWiring, m_mw.m_dialogCoordinator);
}

// ---------------------------------------------------------------------------
// ドック配置管理
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureDockLayoutManager()
{
    if (m_mw.m_dockLayoutManager) return;

    m_mw.m_dockLayoutManager = std::make_unique<DockLayoutManager>(&m_mw);

    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Menu, m_mw.m_docks.menuWindow);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Joseki, m_mw.m_docks.josekiWindow);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Record, m_mw.m_docks.recordPane);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::GameInfo, m_mw.m_docks.gameInfo);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Thinking, m_mw.m_docks.thinking);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Consideration, m_mw.m_docks.consideration);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::UsiLog, m_mw.m_docks.usiLog);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::CsaLog, m_mw.m_docks.csaLog);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Comment, m_mw.m_docks.comment);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::BranchTree, m_mw.m_docks.branchTree);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::EvalChart, m_mw.m_docks.evalChart);

    m_mw.m_dockLayoutManager->setSavedLayoutsMenu(m_mw.ui->menuSavedLayouts);
}

// ---------------------------------------------------------------------------
// 棋譜ナビUI配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureRecordNavigationHandler()
{
    m_foundation->ensureKifuNavigationCoordinator();
    m_foundation->ensureUiStatePolicyManager();
    ensureConsiderationPositionService();

    RecordNavigationWiring::WiringTargets targets;
    targets.kifuNav = m_mw.m_kifuNavCoordinator.get();
    targets.uiStatePolicy = m_mw.m_uiStatePolicy;
    targets.considerationPosition = m_mw.m_considerationPositionService;

    auto refs = m_mw.buildRuntimeRefs();
    m_mw.m_compositionRoot->ensureRecordNavigationWiring(refs, targets, &m_mw, m_mw.m_recordNavWiring);
}

// ---------------------------------------------------------------------------
// 終局スタイル解除
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::unlockGameOverStyle()
{
    if (m_mw.m_shogiView) {
        m_mw.m_shogiView->setGameOverStyleLock(false);
    }
}

// ---------------------------------------------------------------------------
// 評価値状態クリア
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::clearEvalState()
{
    if (m_mw.m_evalChart) {
        m_mw.m_evalChart->clearAll();
    }
    m_foundation->ensureEvaluationGraphController();
    if (m_mw.m_evalGraphController) {
        m_mw.m_evalGraphController->clearScores();
    }
    if (m_mw.m_recordPresenter) {
        m_mw.m_recordPresenter->clearLiveDisp();
    }
}

// ---------------------------------------------------------------------------
// MainWindow スロットからの転送（UI系）
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::onRecordPaneMainRowChanged(int row)
{
    ensureRecordNavigationHandler();
    m_mw.m_recordNavWiring->handler()->onMainRowChanged(row);
}
