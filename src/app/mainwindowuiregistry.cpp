/// @file mainwindowuiregistry.cpp
/// @brief UI系（ウィジェット・プレゼンター・ビュー・ドック・メニュー・通知）の ensure* 実装
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "ui_mainwindow.h"

#include "commentcoordinator.h"
#include "dialogcoordinator.h"
#include "dialogcoordinatorwiring.h"
#include "dockcreationservice.h"
#include "docklayoutmanager.h"
#include "evaluationgraphcontroller.h"
#include "evaluationchartwidget.h"
#include "gamerecordpresenter.h"
#include "kifunavigationcoordinator.h"
#include "languagecontroller.h"
#include "matchruntimequeryservice.h"
#include "mainwindowappearancecontroller.h"
#include "menuwindowwiring.h"
#include "playerinfocontroller.h"
#include "playerinfowiring.h"
#include "recordnavigationwiring.h"
#include "shogiview.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "logcategories.h"

// ---------------------------------------------------------------------------
// 評価値グラフ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureEvaluationGraphController()
{
    if (m_mw.m_evalGraphController) return;

    m_mw.m_evalGraphController = std::make_unique<EvaluationGraphController>();
    m_mw.m_evalGraphController->setEvalChart(m_mw.m_evalChart);
    m_mw.m_evalGraphController->setMatchCoordinator(m_mw.m_match);
    m_mw.m_evalGraphController->setSfenRecord(m_mw.m_queryService->sfenRecord());
    m_mw.m_evalGraphController->setEngine1Name(m_mw.m_player.engineName1);
    m_mw.m_evalGraphController->setEngine2Name(m_mw.m_player.engineName2);

    // PlayerInfoControllerが既に存在する場合は、EvalGraphControllerを設定
    if (m_mw.m_playerInfoController) {
        m_mw.m_playerInfoController->setEvalGraphController(m_mw.m_evalGraphController.get());
    }
}

// ---------------------------------------------------------------------------
// 棋譜表示プレゼンター
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureRecordPresenter()
{
    if (m_mw.m_recordPresenter) return;

    GameRecordPresenter::Deps d;
    d.model      = m_mw.m_models.kifuRecord;
    d.recordPane = m_mw.m_recordPane;

    m_mw.m_recordPresenter = new GameRecordPresenter(d, &m_mw);

    ensureCommentCoordinator();
    QObject::connect(
        m_mw.m_recordPresenter,
        &GameRecordPresenter::currentRowChanged,
        m_mw.m_commentCoordinator,
        &CommentCoordinator::handleRecordRowChangeRequest,
        Qt::UniqueConnection
        );
}

// ---------------------------------------------------------------------------
// プレイヤー情報コントローラ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensurePlayerInfoController()
{
    if (m_mw.m_playerInfoController) return;
    ensurePlayerInfoWiring();
    m_mw.m_compositionRoot->ensurePlayerInfoController(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_playerInfoController);
}

// ---------------------------------------------------------------------------
// プレイヤー情報配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensurePlayerInfoWiring()
{
    if (m_mw.m_playerInfoWiring) return;

    PlayerInfoWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.tabWidget = m_mw.m_tab;
    deps.shogiView = m_mw.m_shogiView;
    deps.playMode = &m_mw.m_state.playMode;
    deps.humanName1 = &m_mw.m_player.humanName1;
    deps.humanName2 = &m_mw.m_player.humanName2;
    deps.engineName1 = &m_mw.m_player.engineName1;
    deps.engineName2 = &m_mw.m_player.engineName2;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.timeControllerRef = &m_mw.m_timeController;

    m_mw.m_playerInfoWiring = new PlayerInfoWiring(deps, &m_mw);

    // 検討タブが既に作成済みなら設定
    if (m_mw.m_analysisTab) {
        m_mw.m_playerInfoWiring->setAnalysisTab(m_mw.m_analysisTab);
    }

    // PlayerInfoWiringからのシグナルを外観コントローラに接続
    connect(m_mw.m_playerInfoWiring, &PlayerInfoWiring::tabCurrentChanged,
            m_mw.m_appearanceController.get(), &MainWindowAppearanceController::onTabCurrentChanged);

    // PlayerInfoControllerも同期
    m_mw.m_playerInfoController = m_mw.m_playerInfoWiring->playerInfoController();

    qCDebug(lcApp).noquote() << "ensurePlayerInfoWiring_: created and connected";
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
    callbacks.getUiStatePolicyManager = [this]() { ensureUiStatePolicyManager(); return m_mw.m_uiStatePolicy; };
    ensureKifuNavigationCoordinator();
    callbacks.navigateKifuViewToRow = std::bind(&KifuNavigationCoordinator::navigateToRow, m_mw.m_kifuNavCoordinator.get(), std::placeholders::_1);

    m_mw.m_compositionRoot->ensureDialogCoordinator(refs, callbacks, &m_mw,
        m_mw.m_dialogCoordinatorWiring, m_mw.m_dialogCoordinator);
}

// ---------------------------------------------------------------------------
// メニューウィンドウ配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureMenuWiring()
{
    if (m_mw.m_menuWiring) return;

    MenuWindowWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.menuBar = m_mw.menuBar();

    m_mw.m_menuWiring = new MenuWindowWiring(deps, &m_mw);

    qCDebug(lcApp).noquote() << "ensureMenuWiring_: created and connected";
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
// ドック生成サービス
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureDockCreationService()
{
    if (m_mw.m_dockCreationService) return;

    m_mw.m_dockCreationService = std::make_unique<DockCreationService>(&m_mw);
    m_mw.m_dockCreationService->setDisplayMenu(m_mw.ui->Display);
}

// ---------------------------------------------------------------------------
// UI状態ポリシー
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureUiStatePolicyManager()
{
    m_mw.m_compositionRoot->ensureUiStatePolicyManager(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_uiStatePolicy);
}

// ---------------------------------------------------------------------------
// UI通知サービス
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureUiNotificationService()
{
    if (m_mw.m_notificationService) return;

    m_mw.m_notificationService = new UiNotificationService(&m_mw);

    UiNotificationService::Deps deps;
    deps.errorOccurred = &m_mw.m_state.errorOccurred;
    deps.shogiView = m_mw.m_shogiView;
    deps.parentWidget = &m_mw;
    m_mw.m_notificationService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 棋譜ナビUI配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureRecordNavigationHandler()
{
    ensureKifuNavigationCoordinator();
    ensureUiStatePolicyManager();
    ensureConsiderationPositionService();

    RecordNavigationWiring::WiringTargets targets;
    targets.kifuNav = m_mw.m_kifuNavCoordinator.get();
    targets.uiStatePolicy = m_mw.m_uiStatePolicy;
    targets.considerationPosition = m_mw.m_considerationPositionService;

    auto refs = m_mw.buildRuntimeRefs();
    m_mw.m_compositionRoot->ensureRecordNavigationWiring(refs, targets, &m_mw, m_mw.m_recordNavWiring);
}

// ---------------------------------------------------------------------------
// 言語コントローラ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureLanguageController()
{
    if (m_mw.m_languageController) return;

    m_mw.m_languageController = std::make_unique<LanguageController>();
    m_mw.m_languageController->setParentWidget(&m_mw);
    m_mw.m_languageController->setActions(
        m_mw.ui->actionLanguageSystem,
        m_mw.ui->actionLanguageJapanese,
        m_mw.ui->actionLanguageEnglish);
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
    ensureEvaluationGraphController();
    if (m_mw.m_evalGraphController) {
        m_mw.m_evalGraphController->clearScores();
    }
    if (m_mw.m_recordPresenter) {
        m_mw.m_recordPresenter->clearLiveDisp();
    }
}
