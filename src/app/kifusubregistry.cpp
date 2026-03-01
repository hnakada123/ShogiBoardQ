/// @file kifusubregistry.cpp
/// @brief Kifu系（棋譜・ナビゲーション・ファイルI/O・コメント・定跡）の ensure* 実装
///
/// MainWindowServiceRegistry から抽出した Kifu 系メソッドの実装。

#include "kifusubregistry.h"
#include "mainwindowserviceregistry.h"
#include "gamesessionsubregistry.h"
#include "gamesubregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowresetservice.h"
#include "ui_mainwindow.h"

#include "branchnavigationwiring.h"
#include "kifufilecontroller.h"
#include "kifuexportcontroller.h"
#include "gamerecordupdateservice.h"
#include "gamerecordloadservice.h"
#include "gamerecordmodelbuilder.h"
#include "gamerecordpresenter.h"
#include "commentcoordinator.h"
#include "kifunavigationcoordinator.h"
#include "josekiwindowwiring.h"
#include "kifuexportdepsassembler.h"
#include "kifuloadcoordinatorfactory.h"
#include "kifuloadcoordinator.h"
#include "livegamesessionupdater.h"
#include "matchruntimequeryservice.h"
#include "playerinfowiring.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <QStatusBar>
#include <functional>

KifuSubRegistry::KifuSubRegistry(MainWindow& mw,
                                 MainWindowServiceRegistry* registry,
                                 MainWindowFoundationRegistry* foundation,
                                 QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_registry(registry)
    , m_foundation(foundation)
{
}

// ---------------------------------------------------------------------------
// 分岐ナビゲーション
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureBranchNavigationWiring()
{
    if (!m_mw.m_branchNavWiring) {
        createBranchNavigationWiring();
    }
    refreshBranchNavWiringDeps();
}

void KifuSubRegistry::createBranchNavigationWiring()
{
    m_mw.m_branchNavWiring = std::make_unique<BranchNavigationWiring>();

    // 転送シグナルを MainWindow スロットに接続
    connect(m_mw.m_branchNavWiring.get(), &BranchNavigationWiring::boardWithHighlightsRequired,
            &m_mw, &MainWindow::loadBoardWithHighlights);
    connect(m_mw.m_branchNavWiring.get(), &BranchNavigationWiring::boardSfenChanged,
            &m_mw, &MainWindow::loadBoardFromSfen);
    m_foundation->ensureKifuNavigationCoordinator();
    connect(m_mw.m_branchNavWiring.get(), &BranchNavigationWiring::branchNodeHandled,
            m_mw.m_kifuNavCoordinator.get(), &KifuNavigationCoordinator::handleBranchNodeHandled);
}

void KifuSubRegistry::refreshBranchNavWiringDeps()
{
    if (!m_mw.m_branchNavWiring) return;

    BranchNavigationWiring::Deps deps;
    deps.branchTree = &m_mw.m_branchNav.branchTree;
    deps.navState = &m_mw.m_branchNav.navState;
    deps.kifuNavController = &m_mw.m_branchNav.kifuNavController;
    deps.displayCoordinator = &m_mw.m_branchNav.displayCoordinator;
    deps.branchTreeWidget = &m_mw.m_branchNav.branchTreeWidget;
    deps.liveGameSession = &m_mw.m_branchNav.liveGameSession;
    deps.recordPane = m_mw.m_recordPane;
    deps.analysisTab = m_mw.m_analysisTab;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.kifuBranchModel = m_mw.m_models.kifuBranch;
    deps.gameRecordModel = m_mw.m_models.gameRecord;
    deps.gameController = m_mw.m_gameController;
    deps.commentCoordinator = m_mw.m_commentCoordinator;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.ensureCommentCoordinator = [this]() { m_foundation->ensureCommentCoordinator(); };
    m_mw.m_branchNavWiring->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 棋譜ファイルコントローラ
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureKifuFileController()
{
    auto refs = m_mw.buildRuntimeRefs();

    MainWindowDepsFactory::KifuFileCallbacks callbacks;
    callbacks.clearUiBeforeKifuLoad = std::bind(&KifuSubRegistry::clearUiBeforeKifuLoad, this);
    callbacks.setReplayMode = std::bind(&MainWindow::setReplayMode, &m_mw, std::placeholders::_1);
    callbacks.ensurePlayerInfoAndGameInfo = [this]() {
        m_foundation->ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->ensureGameInfoController();
            m_mw.m_gameInfoController = m_mw.m_playerInfoWiring->gameInfoController();
        }
    };
    callbacks.ensureGameRecordModel = std::bind(&KifuSubRegistry::ensureGameRecordModel, this);
    callbacks.ensureKifuExportController = std::bind(&KifuSubRegistry::ensureKifuExportController, this);
    callbacks.updateKifuExportDependencies = std::bind(&KifuSubRegistry::updateKifuExportDeps, this);
    callbacks.createAndWireKifuLoadCoordinator = [this]() { createAndWireKifuLoadCoordinator(); };
    callbacks.ensureKifuLoadCoordinatorForLive = std::bind(&KifuSubRegistry::ensureKifuLoadCoordinatorForLive, this);
    callbacks.getKifuExportController = [this]() { return m_mw.m_kifuExportController.get(); };
    callbacks.getKifuLoadCoordinator = [this]() { return m_mw.m_kifuLoadCoordinator; };

    m_mw.m_compositionRoot->ensureKifuFileController(refs, callbacks, &m_mw,
        m_mw.m_kifuFileController);
}

// ---------------------------------------------------------------------------
// 棋譜エクスポートコントローラ
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureKifuExportController()
{
    if (m_mw.m_kifuExportController) return;

    m_mw.m_kifuExportController = std::make_unique<KifuExportController>(&m_mw);

    // 準備コールバックを設定（クリップボードコピー等の前に呼ばれる）
    m_mw.m_kifuExportController->setPrepareCallback([this]() {
        ensureGameRecordModel();
        updateKifuExportDeps();
    });

    // ステータスバーへのメッセージ転送
    connect(m_mw.m_kifuExportController.get(), &KifuExportController::statusMessage,
            m_mw.statusBar(), &QStatusBar::showMessage);
}

// ---------------------------------------------------------------------------
// 棋譜追記サービス
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureGameRecordUpdateService()
{
    if (!m_mw.m_gameRecordUpdateService) {
        createGameRecordUpdateService();
    }
    refreshGameRecordUpdateDeps();
}

void KifuSubRegistry::createGameRecordUpdateService()
{
    m_mw.m_gameRecordUpdateService = std::make_unique<GameRecordUpdateService>();
}

void KifuSubRegistry::refreshGameRecordUpdateDeps()
{
    if (!m_mw.m_gameRecordUpdateService) return;

    GameRecordUpdateService::Deps deps;
    deps.ensureRecordPresenter = [this]() -> GameRecordPresenter* {
        m_registry->ensureRecordPresenter();
        return m_mw.m_recordPresenter;
    };
    deps.ensureLiveGameSessionUpdater = [this]() -> LiveGameSessionUpdater* {
        m_registry->game()->session()->ensureLiveGameSessionUpdater();
        return m_mw.m_liveGameSessionUpdater.get();
    };
    deps.match = m_mw.m_match;
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.gameUsiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    m_mw.m_gameRecordUpdateService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 棋譜データ読み込みサービス
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureGameRecordLoadService()
{
    if (!m_mw.m_gameRecordLoadService) {
        createGameRecordLoadService();
    }
    refreshGameRecordLoadDeps();
}

void KifuSubRegistry::createGameRecordLoadService()
{
    m_mw.m_gameRecordLoadService = std::make_unique<GameRecordLoadService>();
}

void KifuSubRegistry::refreshGameRecordLoadDeps()
{
    if (!m_mw.m_gameRecordLoadService) return;

    GameRecordLoadService::Deps deps;
    deps.gameUsiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.commentsByRow = &m_mw.m_kifu.commentsByRow;
    deps.recordPane = m_mw.m_recordPane;
    deps.ensureRecordPresenter = [this]() -> GameRecordPresenter* {
        m_registry->ensureRecordPresenter();
        return m_mw.m_recordPresenter;
    };
    deps.ensureGameRecordModel = [this]() -> GameRecordModel* {
        ensureGameRecordModel();
        return m_mw.m_models.gameRecord;
    };
    deps.sfenRecord = [this]() -> const QStringList* {
        return m_mw.m_queryService->sfenRecord();
    };
    m_mw.m_gameRecordLoadService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// ライブ棋譜ロード
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureKifuLoadCoordinatorForLive()
{
    if (m_mw.m_kifuLoadCoordinator) {
        return;
    }

    createAndWireKifuLoadCoordinator();
}

// ---------------------------------------------------------------------------
// 棋譜モデル構築
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureGameRecordModel()
{
    if (m_mw.m_models.gameRecord) return;

    m_foundation->ensureCommentCoordinator();

    GameRecordModelBuilder::Deps deps;
    deps.parent = &m_mw;
    deps.recordPresenter = m_mw.m_recordPresenter;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;
    deps.commentCoordinator = m_mw.m_commentCoordinator;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;

    m_mw.m_models.gameRecord = GameRecordModelBuilder::build(deps);
}

// ---------------------------------------------------------------------------
// 定跡ウィンドウ配線
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureJosekiWiring()
{
    if (m_mw.m_josekiWiring) return;

    JosekiWindowWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.gameController = m_mw.m_gameController;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    deps.usiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.currentMoveIndex = &m_mw.m_state.currentMoveIndex;
    deps.currentSelectedPly = &m_mw.m_kifu.currentSelectedPly;
    deps.playMode = &m_mw.m_state.playMode;

    m_mw.m_josekiWiring = std::make_unique<JosekiWindowWiring>(deps);

    connect(m_mw.m_josekiWiring.get(), &JosekiWindowWiring::moveRequested,
            &m_mw, &MainWindow::onMoveRequested);
    // forcedPromotionRequested → ShogiGameController::setForcedPromotion 直接接続
    if (m_mw.m_gameController) {
        connect(m_mw.m_josekiWiring.get(), &JosekiWindowWiring::forcedPromotionRequested,
                m_mw.m_gameController, &ShogiGameController::setForcedPromotion);
    }

    qCDebug(lcApp).noquote() << "ensureJosekiWiring_: created and connected";
}

// ---------------------------------------------------------------------------
// 棋譜読込前UIクリア
// ---------------------------------------------------------------------------

void KifuSubRegistry::clearUiBeforeKifuLoad()
{
    m_foundation->ensureCommentCoordinator();

    MainWindowResetService::SessionUiDeps deps;
    deps.commLog1 = m_mw.m_models.commLog1;
    deps.commLog2 = m_mw.m_models.commLog2;
    deps.thinking1 = m_mw.m_models.thinking1;
    deps.thinking2 = m_mw.m_models.thinking2;
    deps.consideration = m_mw.m_models.consideration;
    deps.analysisTab = m_mw.m_analysisTab;
    deps.analysisModel = m_mw.m_models.analysis;
    deps.evalChart = m_mw.m_evalChart;
    deps.evalGraphController = m_mw.m_evalGraphController.get();
    deps.broadcastComment = std::bind(&CommentCoordinator::broadcastComment,
                                       m_mw.m_commentCoordinator,
                                       std::placeholders::_1, std::placeholders::_2);

    const MainWindowResetService resetService;
    resetService.clearUiBeforeKifuLoad(deps);
}

// ---------------------------------------------------------------------------
// 定跡ウィンドウ更新
// ---------------------------------------------------------------------------

void KifuSubRegistry::updateJosekiWindow()
{
    if (m_mw.m_docks.josekiWindow && !m_mw.m_docks.josekiWindow->isVisible()) {
        return;
    }
    if (m_mw.m_josekiWiring) {
        m_mw.m_josekiWiring->updateJosekiWindow();
    }
}

// ---------------------------------------------------------------------------
// KifuLoadCoordinator 生成ヘルパー
// ---------------------------------------------------------------------------

void KifuSubRegistry::createAndWireKifuLoadCoordinator()
{
    // 既存があれば遅延破棄
    if (m_mw.m_kifuLoadCoordinator) {
        m_mw.m_kifuLoadCoordinator->deleteLater();
        m_mw.m_kifuLoadCoordinator = nullptr;
    }

    // 配線先の遅延初期化
    ensureBranchNavigationWiring();
    m_foundation->ensureKifuNavigationCoordinator();
    m_foundation->ensureUiStatePolicyManager();
    m_foundation->ensurePlayerInfoWiring();
    m_foundation->ensureUiNotificationService();

    KifuLoadCoordinatorFactory::Params p;
    p.gameMoves = &m_mw.m_kifu.gameMoves;
    p.positionStrList = &m_mw.m_kifu.positionStrList;
    p.activePly = &m_mw.m_kifu.activePly;
    p.currentSelectedPly = &m_mw.m_kifu.currentSelectedPly;
    p.currentMoveIndex = &m_mw.m_state.currentMoveIndex;
    p.sfenRecord = m_mw.m_queryService->sfenRecord();
    p.gameInfoController = m_mw.m_gameInfoController;
    p.tab = m_mw.m_tab;
    p.recordPane = m_mw.m_recordPane;
    p.kifuRecordModel = m_mw.m_models.kifuRecord;
    p.kifuBranchModel = m_mw.m_models.kifuBranch;
    p.parent = &m_mw;
    p.branchTree = m_mw.m_branchNav.branchTree;
    p.navState = m_mw.m_branchNav.navState;
    p.analysisTab = m_mw.m_analysisTab;
    p.shogiView = m_mw.m_shogiView;
    p.mainWindow = &m_mw;
    p.branchNavWiring = m_mw.m_branchNavWiring.get();
    p.kifuNavCoordinator = m_mw.m_kifuNavCoordinator.get();
    p.uiStatePolicy = m_mw.m_uiStatePolicy;
    p.playerInfoWiring = m_mw.m_playerInfoWiring;
    p.notificationService = m_mw.m_notificationService;

    m_mw.m_kifuLoadCoordinator = KifuLoadCoordinatorFactory::createAndWire(p);
}

// ---------------------------------------------------------------------------
// KifuExportController 依存更新
// ---------------------------------------------------------------------------

void KifuSubRegistry::updateKifuExportDeps()
{
    KifuExportDepsAssembler::assemble(m_mw.m_kifuExportController.get(), m_mw.buildRuntimeRefs());
}
