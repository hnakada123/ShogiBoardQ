/// @file mainwindowfoundationregistry.cpp
/// @brief 共通基盤レジストリの ensure* 実装
///
/// MainWindowServiceRegistry から抽出した Tier 0/1 メソッドの実装。
/// 各メソッドは他の ensure* を呼ばない（ensurePlayerInfoController のみ
/// ensurePlayerInfoWiring を内部呼出し）。

#include "mainwindowfoundationregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowserviceregistry.h"
#include "kifusubregistry.h"
#include "ui_mainwindow.h"

#include "analysisresultspresenter.h"
#include "boardinteractioncontroller.h"
#include "boardsyncpresenter.h"
#include "commentcoordinator.h"
#include "considerationpositionservice.h"
#include "dockcreationservice.h"
#include "evaluationgraphcontroller.h"
#include "evaluationchartwidget.h"
#include "jishogiscoredialogcontroller.h"
#include "josekiwindowwiring.h"
#include "kifunavigationcoordinator.h"
#include "kifunavigationdepsfactory.h"
#include "languagecontroller.h"
#include "mainwindowappearancecontroller.h"
#include "matchruntimequeryservice.h"
#include "menuwindowwiring.h"
#include "nyugyokudeclarationhandler.h"
#include "playerinfocontroller.h"
#include "playerinfowiring.h"
#include "positioneditcontroller.h"
#include "shogiview.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "logcategories.h"

MainWindowFoundationRegistry::MainWindowFoundationRegistry(MainWindow& mw,
                                                           MainWindowServiceRegistry* serviceRegistry,
                                                           QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_serviceRegistry(serviceRegistry)
{
}

// ===========================================================================
// 共有基盤
// ===========================================================================

// ---------------------------------------------------------------------------
// UI状態ポリシー
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureUiStatePolicyManager()
{
    m_mw.m_compositionRoot->ensureUiStatePolicyManager(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_uiStatePolicy);
}

// ---------------------------------------------------------------------------
// プレイヤー情報配線
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensurePlayerInfoWiring()
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

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
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
// プレイヤー情報コントローラ
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensurePlayerInfoController()
{
    if (m_mw.m_playerInfoController) return;
    ensurePlayerInfoWiring();
    m_mw.m_compositionRoot->ensurePlayerInfoController(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_playerInfoController);
}

// ---------------------------------------------------------------------------
// 棋譜ナビゲーションコーディネーター
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureKifuNavigationCoordinator()
{
    if (!m_mw.m_kifuNavCoordinator) {
        m_mw.m_kifuNavCoordinator = std::make_unique<KifuNavigationCoordinator>();
    }
    refreshKifuNavigationCoordinatorDeps();
}

void MainWindowFoundationRegistry::refreshKifuNavigationCoordinatorDeps()
{
    if (!m_mw.m_kifuNavCoordinator) return;

    KifuNavigationDepsFactory::Callbacks callbacks;
    callbacks.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    callbacks.updateJosekiWindow = [this]() {
        if (m_mw.m_docks.josekiWindow && !m_mw.m_docks.josekiWindow->isVisible()) {
            return;
        }
        if (m_mw.m_josekiWiring) {
            m_mw.m_josekiWiring->updateJosekiWindow();
        }
    };
    callbacks.ensureBoardSyncPresenter = [this]() { ensureBoardSyncPresenter(); };
    callbacks.isGameActivelyInProgress = std::bind(&MatchRuntimeQueryService::isGameActivelyInProgress, m_mw.m_queryService.get());
    callbacks.getSfenRecord = [this]() -> QStringList* { return m_mw.m_queryService->sfenRecord(); };

    m_mw.m_kifuNavCoordinator->updateDeps(
        KifuNavigationDepsFactory::createDeps(m_mw.buildRuntimeRefs(), callbacks));
}

// ---------------------------------------------------------------------------
// コメントコーディネーター
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureCommentCoordinator()
{
    if (m_mw.m_commentCoordinator) return;
    // QPointer<T> は T*& に直接バインドできないため、一時変数経由で渡す
    CommentCoordinator* ptr = nullptr;
    m_mw.m_compositionRoot->ensureCommentCoordinator(m_mw.buildRuntimeRefs(), &m_mw, ptr);
    m_mw.m_commentCoordinator = ptr;
    connect(m_mw.m_commentCoordinator, &CommentCoordinator::ensureGameRecordModelRequested,
            m_serviceRegistry->kifu(), &KifuSubRegistry::ensureGameRecordModel);
}

// ---------------------------------------------------------------------------
// 盤面同期プレゼンター
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureBoardSyncPresenter()
{
    if (!m_mw.m_boardSync) {
        qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: creating BoardSyncPresenter"
                           << "sfenRecord()*=" << static_cast<const void*>(m_mw.m_queryService->sfenRecord())
                           << "sfenRecord().size=" << (m_mw.m_queryService->sfenRecord() ? m_mw.m_queryService->sfenRecord()->size() : -1);

        BoardSyncPresenter::Deps d;
        d.gc         = m_mw.m_gameController;
        d.view       = m_mw.m_shogiView;
        d.bic        = m_mw.m_boardController;
        d.sfenRecord = m_mw.m_queryService->sfenRecord();
        d.gameMoves  = &m_mw.m_kifu.gameMoves;

        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once on first use, never recreated
        m_mw.m_boardSync = new BoardSyncPresenter(d, &m_mw);

        qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: created m_boardSync*=" << static_cast<const void*>(m_mw.m_boardSync);
    }
    refreshBoardSyncPresenterDeps();
}

void MainWindowFoundationRegistry::refreshBoardSyncPresenterDeps()
{
    if (!m_mw.m_boardSync) return;
    // sfenRecord ポインタが変わっている場合は更新する
    const QStringList* current = m_mw.m_queryService->sfenRecord();
    m_mw.m_boardSync->setSfenRecord(current);
}

// ---------------------------------------------------------------------------
// UI通知サービス
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureUiNotificationService()
{
    if (m_mw.m_notificationService) return;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_notificationService = new UiNotificationService(&m_mw);

    UiNotificationService::Deps deps;
    deps.errorOccurred = &m_mw.m_state.errorOccurred;
    deps.shogiView = m_mw.m_shogiView;
    deps.parentWidget = &m_mw;
    m_mw.m_notificationService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 評価値グラフ
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureEvaluationGraphController()
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

// ===========================================================================
// 単純リーフ
// ===========================================================================

// ---------------------------------------------------------------------------
// メニューウィンドウ配線
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureMenuWiring()
{
    if (m_mw.m_registryParts.menuWiring) return;

    MenuWindowWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.menuBar = m_mw.menuBar();

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_registryParts.menuWiring = new MenuWindowWiring(deps, &m_mw);

    qCDebug(lcApp).noquote() << "ensureMenuWiring_: created and connected";
}

// ---------------------------------------------------------------------------
// 言語コントローラ
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureLanguageController()
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
// ドック生成サービス
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureDockCreationService()
{
    if (m_mw.m_dockCreationService) return;

    m_mw.m_dockCreationService = std::make_unique<DockCreationService>(&m_mw);
    m_mw.m_dockCreationService->setDisplayMenu(m_mw.ui->Display);
}

// ---------------------------------------------------------------------------
// 局面編集コントローラ
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensurePositionEditController()
{
    if (m_mw.m_posEdit) return;
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_posEdit = new PositionEditController(&m_mw);
}

// ---------------------------------------------------------------------------
// 解析結果プレゼンター
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureAnalysisPresenter()
{
    if (!m_mw.m_analysisPresenter)
        m_mw.m_analysisPresenter = std::make_unique<AnalysisResultsPresenter>();
}

// ---------------------------------------------------------------------------
// 持将棋コントローラ
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureJishogiController()
{
    if (m_mw.m_jishogiController) return;
    m_mw.m_jishogiController = std::make_unique<JishogiScoreDialogController>();
}

// ---------------------------------------------------------------------------
// 入玉宣言ハンドラ
// ---------------------------------------------------------------------------

void MainWindowFoundationRegistry::ensureNyugyokuHandler()
{
    if (m_mw.m_nyugyokuHandler) return;
    m_mw.m_nyugyokuHandler = std::make_unique<NyugyokuDeclarationHandler>();
}
