/// @file kifusubregistry.cpp
/// @brief Kifu系（棋譜・ナビゲーション・ファイルI/O・コメント・定跡）の ensure* 実装
///
/// MainWindowServiceRegistry から抽出した Kifu 系メソッドの実装。

#include "kifusubregistry.h"
#include "playmodepolicyservice.h"
#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
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
#include "evaluationgraphcontroller.h"
#include "josekiwindowwiring.h"
#include "kifunavigationstate.h"
#include "josekiwindow.h"
#include "kifuloadcoordinatorfactory.h"
#include "kifuloadcoordinator.h"
#include "livegamesessionupdater.h"
#include "playmodepolicyservice.h"
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
    // ツリー選択時の棋譜欄更新はシグナルを抑止するため、グラフへも明示的に通知する。
    m_foundation->ensureEvaluationGraphController();
    connect(m_mw.m_branchNavWiring.get(), &BranchNavigationWiring::currentPlyChanged,
            m_mw.m_evalGraphController.get(), &EvaluationGraphController::setCurrentPly);
}

void KifuSubRegistry::refreshBranchNavWiringDeps()
{
    if (!m_mw.m_branchNavWiring) return;

    BranchNavigationWiring::Deps deps;
    deps.branchTree = &m_mw.m_branchNav.branchTree;
    deps.navState = &m_mw.m_branchNav.navState;
    deps.kifuNavController = &m_mw.m_branchNav.kifuNavController;
    deps.displayCoordinator = &m_mw.m_branchNav.displayCoordinator;
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
    deps.markGameRecordDirty = [this]() {
        // 起動直後に盤上で指した最初の手では、棋譜モデルがまだ無いことがある
        ensureGameRecordModel();
        if (m_mw.m_models.gameRecord) m_mw.m_models.gameRecord->markDirty();
    };
    m_mw.m_branchNavWiring->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 棋譜ファイルコントローラ
// ---------------------------------------------------------------------------

bool KifuSubRegistry::confirmDiscardUnsavedKifu()
{
    ensureKifuFileController();
    return m_mw.m_kifuFileController->confirmDiscardUnsaved();
}

void KifuSubRegistry::ensureKifuFileController()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_kifuFileController) {
        m_mw.m_kifuFileController = new KifuFileController(&m_mw);
    }

    KifuFileController::Deps deps;
    deps.parentWidget = &m_mw;
    deps.statusBar = m_mw.ui ? m_mw.ui->statusbar : nullptr;
    deps.gameInfo = m_mw.m_gameInfoController;
    deps.saveFileName = &m_mw.m_kifu.saveFileName;

    // 準備コールバック
    deps.clearUiBeforeKifuLoad = [this]() {
        clearUiBeforeKifuLoad();
    };
    deps.setReplayMode = [this](bool replayMode) {
        m_mw.setReplayMode(replayMode);
    };
    deps.ensurePlayerInfoAndGameInfo = [this]() {
        m_foundation->ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->ensureGameInfoController();
            m_mw.m_gameInfoController = m_mw.m_playerInfoWiring->gameInfoController();
        }
    };
    deps.ensureGameRecordModel = [this]() {
        ensureGameRecordModel();
    };
    deps.ensureKifuExportController = [this]() {
        ensureKifuExportController();
    };
    deps.updateKifuExportDependencies = [this]() {
        updateKifuExportDeps();
    };
    deps.ensureKifuLoadCoordinator = [this]() -> KifuLoadCoordinator* {
        ensureKifuLoadCoordinator();
        return m_mw.m_kifuLoadCoordinator;
    };

    // ゲッター
    deps.getGameRecordModel = [this]() { return m_mw.m_models.gameRecord; };
    deps.getKifuExportController = [this]() { return m_mw.m_kifuExportController.get(); };

    m_mw.m_kifuFileController->updateDeps(deps);
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
        m_registry->ensureLiveGameSessionUpdater();
        return m_mw.m_liveGameSessionUpdater.get();
    };
    deps.markGameRecordDirty = [this]() {
        ensureGameRecordModel();
        m_mw.m_models.gameRecord->markDirty();
    };
    deps.match = m_mw.m_match;
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.sfenRecord = &m_mw.m_kifu.sfenRecord;
    m_mw.m_gameRecordUpdateService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 棋譜データ読み込みサービス
// ---------------------------------------------------------------------------

void KifuSubRegistry::displayGameRecord(const QList<KifDisplayItem>& disp)
{
    if (m_mw.m_isShuttingDown || !m_mw.m_models.kifuRecord) return;

    ensureGameRecordLoadService();
    m_mw.m_gameRecordLoadService->loadGameRecord(disp);
}

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
    deps.recordPane = m_mw.m_recordPane;
    deps.ensureRecordPresenter = [this]() -> GameRecordPresenter* {
        m_registry->ensureRecordPresenter();
        return m_mw.m_recordPresenter;
    };
    deps.ensureGameRecordModel = [this]() -> GameRecordModel* {
        ensureGameRecordModel();
        return m_mw.m_models.gameRecord;
    };
    m_mw.m_gameRecordLoadService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// ライブ棋譜ロード
// ---------------------------------------------------------------------------

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

bool KifuSubRegistry::confirmCloseJoseki()
{
    const auto *wiring = m_mw.m_josekiWiring.get();
    return !wiring || !wiring->josekiWindow() || wiring->josekiWindow()->confirmClose();
}

void KifuSubRegistry::ensureJosekiWiring()
{
    if (m_mw.m_josekiWiring) return;

    JosekiWindowWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.gameController = m_mw.m_gameController;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.currentLine = [this]() {
        return m_mw.m_branchNav.navState ? m_mw.m_branchNav.navState->currentRecordLine() : RecordLine{};
    };
    deps.isHumanTurnNow = [this]() {
        return m_mw.m_playModePolicy && m_mw.m_playModePolicy->isHumanTurnNow();
    };

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
    deps.broadcastComment = [this](const QString& comment, int row) {
        m_mw.m_commentCoordinator->broadcastComment(comment, row);
    };

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
// KifuLoadCoordinator（初回だけ生成）
// ---------------------------------------------------------------------------

void KifuSubRegistry::ensureKifuLoadCoordinator()
{
    // 一度作ったら作り直さない。読み込み中の別の読み込みは KifuLoadCoordinator 自身が打ち切る。
    // 対局情報の表を作成時に受け取るので、初回の読み込み（対局情報の表ができた後）に作る
    if (m_mw.m_kifuLoadCoordinator) return;

    // 配線先の遅延初期化
    ensureBranchNavigationWiring();
    m_foundation->ensureKifuNavigationCoordinator();
    m_foundation->ensureUiStatePolicyManager();
    m_foundation->ensurePlayerInfoWiring();
    m_foundation->ensureUiNotificationService();

    KifuLoadCoordinatorFactory::Params p;
    p.gameInfoController = m_mw.m_gameInfoController;
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
    KifuExportController* controller = m_mw.m_kifuExportController.get();
    if (!controller) return;

    KifuExportController::Dependencies deps;
    deps.gameRecord = m_mw.m_models.gameRecord;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.gameInfoController = m_mw.m_gameInfoController;
    deps.timeController = m_mw.m_timeController;
    deps.gameController = m_mw.m_gameController;
    deps.resolvedRows = nullptr;  // KifuBranchTree を優先使用
    deps.startSfenStr = m_mw.m_state.startSfenStr;
    deps.playMode = m_mw.m_state.playMode;
    deps.humanName1 = m_mw.m_player.humanName1;
    deps.humanName2 = m_mw.m_player.humanName2;
    deps.engineName1 = m_mw.m_player.engineName1;
    deps.engineName2 = m_mw.m_player.engineName2;
    const KifuNavigationState* nav = m_mw.m_branchNav.navState;
    deps.activeResolvedRow = nav ? nav->currentLineIndex() : 0;
    deps.currentMoveIndex = nav ? nav->currentPly() : 0;
    controller->setDependencies(deps);
}
