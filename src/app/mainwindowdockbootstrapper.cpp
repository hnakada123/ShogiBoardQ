/// @file mainwindowdockbootstrapper.cpp
/// @brief ドック生成・パネル構築の実装
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "gamesubregistry.h"
#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"

#include "analysistabwiring.h"
#include "branchnavigationwiring.h"
#include "commentcoordinator.h"             // IWYU pragma: keep (QPointer の完全型)
#include "dockcreationservice.h"
#include "engineanalysistab.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "josekiwindowwiring.h"
#include "kifubranchlistmodel.h"
#include "kifurecordlistmodel.h"
#include "playerinfocontroller.h"
#include "playerinfowiring.h"
#include "recordpanewiring.h"
#include "shogienginethinkingmodel.h"

#include "logcategories.h"

void MainWindowServiceRegistry::setupRecordPane()
{
    // モデルの用意（従来どおり）
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once at startup, never recreated
    if (!m_mw.m_models.kifuRecord) m_mw.m_models.kifuRecord = new KifuRecordListModel(&m_mw);
    if (!m_mw.m_models.kifuBranch) m_mw.m_models.kifuBranch = new KifuBranchListModel(&m_mw);

    // Wiring の生成
    if (!m_mw.m_recordPaneWiring) {
        m_foundation->ensureCommentCoordinator();
        RecordPaneWiring::Deps d;
        d.parent             = m_mw.m_central;                               // 親ウィジェット
        d.ctx                = &m_mw;                                        // RecordPane::mainRowChanged の受け先
        d.recordModel        = m_mw.m_models.kifuRecord;
        d.branchModel        = m_mw.m_models.kifuBranch;
        d.commentCoordinator = m_mw.m_commentCoordinator;

        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once at startup, never recreated
        m_mw.m_recordPaneWiring = new RecordPaneWiring(d, &m_mw);
    }

    // RecordPane の構築と配線
    m_mw.m_recordPaneWiring->buildUiAndWire();

    // 生成物の取得
    m_mw.m_recordPane = m_mw.m_recordPaneWiring->pane();
}

void MainWindowServiceRegistry::setupEngineAnalysisTab()
{
    // 既に配線クラスがあれば再利用し、タブ取得だけを行う
    if (!m_mw.m_analysisWiring) {
        AnalysisTabWiring::Deps d;
        d.centralParent = m_mw.m_central;         // 既存の central エリア
        d.log1          = m_mw.m_models.commLog1;  // USIログ(先手)
        d.log2          = m_mw.m_models.commLog2;  // USIログ(後手)

        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once at startup, never recreated
        m_mw.m_analysisWiring = new AnalysisTabWiring(d, &m_mw);
        m_mw.m_analysisWiring->buildUiAndWire();
    }

    // 配線クラスから出来上がった部品を受け取る（MainWindow の既存フィールドへ反映）
    m_mw.m_analysisTab    = m_mw.m_analysisWiring->analysisTab();
    m_mw.m_tab            = m_mw.m_analysisWiring->tab();
    m_mw.m_models.thinking1 = m_mw.m_analysisWiring->thinking1();
    m_mw.m_models.thinking2 = m_mw.m_analysisWiring->thinking2();

    if (Q_UNLIKELY(!m_mw.m_analysisTab) || Q_UNLIKELY(!m_mw.m_models.thinking1) || Q_UNLIKELY(!m_mw.m_models.thinking2)) {
        qCWarning(lcApp, "ensureAnalysisTabWiring: analysis wiring produced null components "
                  "(tab=%p, thinking1=%p, thinking2=%p)",
                  static_cast<void*>(m_mw.m_analysisTab),
                  static_cast<void*>(m_mw.m_models.thinking1),
                  static_cast<void*>(m_mw.m_models.thinking2));
        return;
    }

    // 外部シグナル接続を AnalysisTabWiring に委譲
    m_foundation->ensureCommentCoordinator();
    ensureUsiCommandController();
    ensureConsiderationWiring();
    ensurePvClickController();
    m_kifu->ensureBranchNavigationWiring();
    AnalysisTabWiring::ExternalSignalDeps extDeps;
    extDeps.branchNavigationWiring = m_mw.m_branchNavWiring.get();
    extDeps.pvClickController = m_mw.m_pvClickController;
    extDeps.commentCoordinator = m_mw.m_commentCoordinator;
    extDeps.usiCommandController = m_mw.m_usiCommandController.get();
    extDeps.considerationWiring = m_mw.m_considerationWiring;
    m_mw.m_analysisWiring->wireExternalSignals(extDeps);

    configureAnalysisTabDependencies();
}

void MainWindowServiceRegistry::configureAnalysisTabDependencies()
{
    // PlayerInfoControllerにもm_analysisTabを設定
    if (m_mw.m_playerInfoController) {
        m_mw.m_playerInfoController->setAnalysisTab(m_mw.m_analysisTab);
    }

    // PlayerInfoWiringにも検討タブを設定
    m_foundation->ensurePlayerInfoWiring();
    if (m_mw.m_playerInfoWiring) {
        m_mw.m_playerInfoWiring->setAnalysisTab(m_mw.m_analysisTab);
        // 起動時に対局情報タブを追加
        if (m_mw.m_tab) {
            m_mw.m_playerInfoWiring->setTabWidget(m_mw.m_tab);
        }
        m_mw.m_playerInfoWiring->addGameInfoTabAtStartup();
        m_mw.m_gameInfoController = m_mw.m_playerInfoWiring->gameInfoController();
    }

    if (!m_mw.m_models.consideration) {
        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once at startup, never recreated
        m_mw.m_models.consideration = new ShogiEngineThinkingModel(&m_mw);
    }
    if (m_mw.m_analysisTab) {
        m_mw.m_analysisTab->setConsiderationThinkingModel(m_mw.m_models.consideration);
    }
}

void MainWindowServiceRegistry::createEvalChartDock()
{
    // 評価値グラフウィジェットを作成
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once at startup, never recreated
    m_mw.m_evalChart = new EvaluationChartWidget(&m_mw);

    // 既存のEvaluationGraphControllerがあれば、評価値グラフを設定
    if (m_mw.m_evalGraphController) {
        m_mw.m_evalGraphController->setEvalChart(m_mw.m_evalChart);
    }

    // DockCreationServiceに委譲
    m_foundation->ensureDockCreationService();
    m_mw.m_dockCreationService->setEvalChart(m_mw.m_evalChart);
    m_mw.m_docks.evalChart = m_mw.m_dockCreationService->createEvalChartDock();
}

void MainWindowServiceRegistry::createRecordPaneDock()
{
    if (!m_mw.m_recordPane) {
        qCWarning(lcApp) << "createRecordPaneDock: m_recordPane is null!";
        return;
    }

    // DockCreationServiceに委譲
    m_foundation->ensureDockCreationService();
    m_mw.m_dockCreationService->setRecordPane(m_mw.m_recordPane);
    m_mw.m_docks.recordPane = m_mw.m_dockCreationService->createRecordPaneDock();
}

void MainWindowServiceRegistry::createAnalysisDocks()
{
    if (!m_mw.m_analysisTab) {
        qCWarning(lcApp) << "createAnalysisDocks: m_analysisTab is null!";
        return;
    }

    // 対局情報コントローラを準備し、デフォルト値を設定
    m_foundation->ensurePlayerInfoWiring();
    if (m_mw.m_playerInfoWiring) {
        m_mw.m_playerInfoWiring->ensureGameInfoController();
        m_mw.m_gameInfoController = m_mw.m_playerInfoWiring->gameInfoController();
    }

    // DockCreationServiceに委譲
    m_foundation->ensureDockCreationService();
    m_mw.m_dockCreationService->setAnalysisTab(m_mw.m_analysisTab);
    m_mw.m_dockCreationService->setGameInfoController(m_mw.m_gameInfoController);
    m_mw.m_dockCreationService->setModels(m_mw.m_models.thinking1, m_mw.m_models.thinking2, m_mw.m_models.commLog1, m_mw.m_models.commLog2);
    m_mw.m_dockCreationService->createAnalysisDocks();

    // 作成されたドックへの参照を取得
    m_mw.m_docks.gameInfo = m_mw.m_dockCreationService->gameInfoDock();
    m_mw.m_docks.thinking = m_mw.m_dockCreationService->thinkingDock();
    m_mw.m_docks.consideration = m_mw.m_dockCreationService->considerationDock();
    m_mw.m_docks.usiLog = m_mw.m_dockCreationService->usiLogDock();
    m_mw.m_docks.csaLog = m_mw.m_dockCreationService->csaLogDock();
    m_mw.m_docks.comment = m_mw.m_dockCreationService->commentDock();
    m_mw.m_docks.branchTree = m_mw.m_dockCreationService->branchTreeDock();
}

void MainWindowServiceRegistry::createMenuWindowDock()
{
    createMenuWindowDockImpl();
}

void MainWindowServiceRegistry::createMenuWindowDockImpl()
{
    // MenuWindowWiringを確保
    m_foundation->ensureMenuWiring();
    if (!m_mw.m_menuWiring) {
        qCWarning(lcApp) << "createMenuWindowDock: MenuWindowWiring is null!";
        return;
    }

    // DockCreationServiceに委譲
    m_foundation->ensureDockCreationService();
    m_mw.m_dockCreationService->setMenuWiring(m_mw.m_menuWiring);
    m_mw.m_docks.menuWindow = m_mw.m_dockCreationService->createMenuWindowDock();
}

void MainWindowServiceRegistry::createJosekiWindowDock()
{
    // JosekiWindowWiringを確保
    m_kifu->ensureJosekiWiring();
    if (!m_mw.m_josekiWiring) {
        qCWarning(lcApp) << "createJosekiWindowDock: JosekiWindowWiring is null!";
        return;
    }

    // DockCreationServiceに委譲
    m_foundation->ensureDockCreationService();
    m_mw.m_dockCreationService->setJosekiWiring(m_mw.m_josekiWiring.get());
    m_mw.m_docks.josekiWindow = m_mw.m_dockCreationService->createJosekiWindowDock();

    // ドックが表示されたときに定跡ウィンドウを更新する
    if (m_mw.m_docks.josekiWindow) {
        QObject::connect(m_mw.m_docks.josekiWindow, &QDockWidget::visibilityChanged,
                         &m_mw, &MainWindow::updateJosekiWindow);
    }
}

void MainWindowServiceRegistry::createAnalysisResultsDock()
{
    // AnalysisResultsPresenterを確保
    m_foundation->ensureAnalysisPresenter();
    if (!m_mw.m_analysisPresenter) {
        qCWarning(lcApp) << "createAnalysisResultsDock: AnalysisResultsPresenter is null!";
        return;
    }

    // DockCreationServiceに委譲
    m_foundation->ensureDockCreationService();
    m_mw.m_dockCreationService->setAnalysisPresenter(m_mw.m_analysisPresenter.get());
    m_mw.m_docks.analysisResults = m_mw.m_dockCreationService->createAnalysisResultsDock();
}

void MainWindowServiceRegistry::initializeBranchNavigationClasses()
{
    m_kifu->ensureBranchNavigationWiring();
    m_mw.m_branchNavWiring->initialize();
}
