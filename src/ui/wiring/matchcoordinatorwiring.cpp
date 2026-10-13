/// @file matchcoordinatorwiring.cpp
/// @brief MatchCoordinator配線クラスの実装

#include "matchcoordinatorwiring.h"
#include "gamesessionorchestrator.h"
#include "sessionlifecyclecoordinator.h"
#include "gamestatecontroller.h"
#include "consecutivegamescontroller.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfowiring.h"

#include "gamestartcoordinator.h"
#include "gamestartoptionsbuilder.h"
#include "evaluationgraphcontroller.h"
#include "timecontrolcontroller.h"
#include "timedisplaypresenter.h"
#include "uistatepolicymanager.h"
#include "boardinteractioncontroller.h"
#include "shogiview.h"
#include "candidatearrowcontroller.h"
#include "kifurecordlistmodel.h"
#include "shogiclock.h"
#include "kifunavigationcoordinator.h"
#include "branchnavigationwiring.h"

MatchCoordinatorWiring::MatchCoordinatorWiring(QObject* parent)
    : QObject(parent)
{
}

MatchCoordinatorWiring::~MatchCoordinatorWiring() = default;

void MatchCoordinatorWiring::updateDeps(const Deps& deps)
{
    m_gc = deps.gc;
    m_view = deps.view;
    m_comm1 = deps.comm1;
    m_think1 = deps.think1;
    m_comm2 = deps.comm2;
    m_think2 = deps.think2;
    m_sfenRecord = deps.sfenRecord;
    m_playMode = deps.playMode;

    m_timePresenter = deps.timePresenter;
    m_boardController = deps.boardController;

    m_kifuRecordModel = deps.kifuRecordModel;
    m_gameMoves = deps.gameMoves;

    m_hooks = deps.hooks;
    m_undoHooks = deps.undoHooks;

    if (deps.ensureTimeController)
        m_ensureTimeController = deps.ensureTimeController;
    if (deps.ensureEvaluationGraphController)
        m_ensureEvaluationGraphController = deps.ensureEvaluationGraphController;
    if (deps.ensureUsiCommandController)
        m_ensureUsiCommandController = deps.ensureUsiCommandController;
    if (deps.ensureUiStatePolicyManager)
        m_ensureUiStatePolicyManager = deps.ensureUiStatePolicyManager;
    if (deps.connectBoardClicks)
        m_connectBoardClicks = deps.connectBoardClicks;
    if (deps.connectMoveRequested)
        m_connectMoveRequested = deps.connectMoveRequested;

    if (deps.getClock)
        m_getClock = deps.getClock;
    if (deps.getTimeController)
        m_getTimeController = deps.getTimeController;
    if (deps.getEvalGraphController)
        m_getEvalGraphController = deps.getEvalGraphController;
    if (deps.getUiStatePolicy)
        m_getUiStatePolicy = deps.getUiStatePolicy;
}

bool MatchCoordinatorWiring::initializeSession()
{
    // 依存が揃っていない場合は何もしない
    if (!m_gc || !m_view) return false;

    // まず時計を用意（nullでも可だが、あれば渡す）
    if (m_ensureTimeController) m_ensureTimeController();

    // 司令塔は初回だけ作る。2回目以降（起動の最終段など）は作り直さず、実行時の依存を結び直し、
    // 起動途中の仮の対局開始で残った局面の履歴を捨てる
    if (!m_match) {
        createMatchCoordinator();
    } else {
        m_match->setEngineModels(m_comm1, m_think1, m_comm2, m_think2);
        m_match->resetPositionHistory();
    }
    bindRuntimeDependencies();
    return true;
}

void MatchCoordinatorWiring::bindRuntimeDependencies()
{
    if (auto* arrows = CandidateArrowController::forView(m_view))
        arrows->setMatchCoordinator(m_match.get());

    // USIコマンドコントローラへ司令塔を反映
    if (m_ensureUsiCommandController) m_ensureUsiCommandController();

    // PlayMode を司令塔へ伝達
    if (m_match && m_playMode) {
        m_match->setPlayMode(*m_playMode);
    }

    // 評価値グラフコントローラへの反映
    if (m_ensureEvaluationGraphController) m_ensureEvaluationGraphController();
    auto* evalCtl = m_getEvalGraphController ? m_getEvalGraphController() : nullptr;
    if (evalCtl) {
        evalCtl->setMatchCoordinator(m_match.get());
        // m_sfenRecord は MC 生成前に取得されるため nullptr になり得る。
        // MC 生成後は MC が保持する sfenRecord を使う。
        QStringList* sfenPtr = (m_match ? m_match->sfenRecordPtr() : m_sfenRecord);
        evalCtl->setSfenRecord(sfenPtr);
    }

    // UndoBindings の設定
    if (m_match) {
        MatchCoordinator::UndoRefs u;
        u.recordModel      = m_kifuRecordModel;
        u.boardCtl         = m_boardController;

        m_match->setUndoBindings(u, m_undoHooks);

        // 検討モード終了・詰み探索終了時にUI状態を「アイドル」に遷移
        if (m_ensureUiStatePolicyManager) m_ensureUiStatePolicyManager();
        auto* uiPolicy = m_getUiStatePolicy ? m_getUiStatePolicy() : nullptr;
        if (uiPolicy) {
            connect(m_match.get(), &MatchCoordinator::considerationModeEnded,
                    uiPolicy, &UiStatePolicyManager::transitionToIdle,
                    Qt::UniqueConnection);
            connect(m_match.get(), &MatchCoordinator::tsumeSearchModeEnded,
                    uiPolicy, &UiStatePolicyManager::transitionToIdle,
                    Qt::UniqueConnection);
            // 対局終了時にUI状態を「アイドル」に遷移（終局処理への転送より後に接続する）
            connect(m_match.get(), &MatchCoordinator::gameEnded,
                    uiPolicy, &UiStatePolicyManager::transitionToIdle,
                    Qt::UniqueConnection);
        }

        // timeUpdated → TimeDisplayPresenter（直接配線）。時間表示は起動の途中で作られるので、
        // 結び直すたびに接続する（同じ組の接続は UniqueConnection で1本に保つ）
        if (m_timePresenter) {
            connect(m_match.get(), &MatchCoordinator::timeUpdated,
                    m_timePresenter, &TimeDisplayPresenter::onMatchTimeUpdated,
                    Qt::UniqueConnection);
        }
    }

    // TimeController へ MatchCoordinator を反映
    auto* timeCtl = m_getTimeController ? m_getTimeController() : nullptr;
    if (timeCtl) {
        timeCtl->setMatchCoordinator(m_match.get());
    }

    // Clock → 投了シグナルの配線
    ShogiClock* clock = m_getClock ? m_getClock() : nullptr;
    if (clock) {
        if (m_clockConn) { QObject::disconnect(m_clockConn); m_clockConn = {}; }
        m_clockConn = connect(clock, &ShogiClock::resignationTriggered,
                              this, &MatchCoordinatorWiring::resignationTriggered,
                              Qt::UniqueConnection);
    }

    // 盤クリックの配線
    if (m_connectBoardClicks) m_connectBoardClicks();

    // 人間操作 → 合法判定＆適用の配線
    if (m_connectMoveRequested) m_connectMoveRequested();

    // 既定モード（必要に応じて開始時に上書き）
    if (m_boardController)
        m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
}

void MatchCoordinatorWiring::createMatchCoordinator()
{
    MatchCoordinator::Deps deps;
    deps.gc    = m_gc;
    deps.clock = m_getClock ? m_getClock() : nullptr;
    deps.comm1  = m_comm1;
    deps.think1 = m_think1;
    deps.comm2  = m_comm2;
    deps.think2 = m_think2;
    deps.sfenRecord = m_sfenRecord;
    deps.gameMoves  = m_gameMoves;
    deps.hooks = m_hooks;

    // Lifetime: owned by MatchCoordinatorWiring (unique_ptr)
    // Created: once (first initializeSession), never recreated
    m_match = std::make_unique<MatchCoordinator>(deps, nullptr);

    // --- MC シグナル → MCW シグナルへ直接配線 ---
    connect(m_match.get(), &MatchCoordinator::requestAppendGameOverMove,
            this,    &MatchCoordinatorWiring::requestAppendGameOverMove,
            Qt::UniqueConnection);
    connect(m_match.get(), &MatchCoordinator::gameInterrupted,
            this, &MatchCoordinatorWiring::gameInterrupted, Qt::UniqueConnection);

    connect(m_match.get(), &MatchCoordinator::boardFlipped,
            this,    &MatchCoordinatorWiring::boardFlipped,
            Qt::UniqueConnection);

    connect(m_match.get(), &MatchCoordinator::gameOverStateChanged,
            this,    &MatchCoordinatorWiring::gameOverStateChanged,
            Qt::UniqueConnection);

    connect(m_match.get(), &MatchCoordinator::gameEnded,
            this,    &MatchCoordinatorWiring::matchGameEnded,
            Qt::UniqueConnection);
    connect(m_match.get(), &MatchCoordinator::gameEndProcessed,
            this,    &MatchCoordinatorWiring::gameEndProcessed,
            Qt::UniqueConnection);
}

void MatchCoordinatorWiring::ensureMenuGameStartCoordinator()
{
    if (m_menuGameStart) return;

    GameStartCoordinator::Deps d;
    d.match = m_match.get();

    m_menuGameStart = new GameStartCoordinator(d, this);

    // ViewHooks: ShogiView 操作をコールバックで注入
    {
        GameStartCoordinator::ViewHooks vh;
        vh.initializeToFlatStartingPosition = [this]() {
            if (m_view) m_view->initializeToFlatStartingPosition();
        };
        vh.removeHighlightAllData = [this]() {
            if (m_view) m_view->removeHighlightAllData();
        };
        vh.applyResumePosition = [](ShogiGameController* gc, const QString& sfen) {
            GameStartOptionsBuilder::applyResumePositionIfAny(
                gc, sfen, {});
        };
        vh.addKifuHeaderItem = [this](const QString& move, const QString& time, bool prepend) {
            if (m_kifuRecordModel) {
                auto* item = new KifuDisplay(move, time);
                if (prepend) {
                    m_kifuRecordModel->prependItem(item);
                } else {
                    m_kifuRecordModel->appendItem(item);
                }
            }
        };
        m_menuGameStart->setViewHooks(vh);
    }

    // --- 11 connect: GameStartCoordinator → 転送シグナル / 内部配線 ---

    // 対局開始前クリーンアップ
    connect(m_menuGameStart, &GameStartCoordinator::requestPreStartCleanup,
            this, &MatchCoordinatorWiring::requestPreStartCleanup,
            Qt::UniqueConnection);

    // 時間制御の適用
    connect(m_menuGameStart, &GameStartCoordinator::requestApplyTimeControl,
            this, &MatchCoordinatorWiring::requestApplyTimeControl,
            Qt::UniqueConnection);

    // 対局者名確定
    connect(m_menuGameStart, &GameStartCoordinator::playerNamesResolved,
            this, &MatchCoordinatorWiring::menuPlayerNamesResolved,
            Qt::UniqueConnection);

    // 連続対局設定
    connect(m_menuGameStart, &GameStartCoordinator::consecutiveGamesConfigured,
            this, &MatchCoordinatorWiring::consecutiveGamesConfigured,
            Qt::UniqueConnection);

    // 対局開始時にUI状態を「対局中」に遷移
    if (m_ensureUiStatePolicyManager) m_ensureUiStatePolicyManager();
    auto* uiPolicy = m_getUiStatePolicy ? m_getUiStatePolicy() : nullptr;
    if (uiPolicy) {
        connect(m_menuGameStart, &GameStartCoordinator::started,
                uiPolicy, &UiStatePolicyManager::transitionToDuringGame,
                Qt::UniqueConnection);
    }

    // 対局開始通知を転送
    connect(m_menuGameStart, &GameStartCoordinator::started,
            this, &MatchCoordinatorWiring::gameStarted,
            Qt::UniqueConnection);

    // 盤面反転シグナルを転送
    connect(m_menuGameStart, &GameStartCoordinator::boardFlipped,
            this, &MatchCoordinatorWiring::boardFlipped,
            Qt::UniqueConnection);

    // 棋譜欄の行選択要求
    connect(m_menuGameStart, &GameStartCoordinator::requestSelectKifuRow,
            this, &MatchCoordinatorWiring::requestSelectKifuRow,
            Qt::UniqueConnection);

    // 分岐ツリーリセット要求
    connect(m_menuGameStart, &GameStartCoordinator::requestBranchTreeResetForNewGame,
            this, &MatchCoordinatorWiring::requestBranchTreeResetForNewGame,
            Qt::UniqueConnection);
}

void MatchCoordinatorWiring::wireForwardingSignals(const ForwardingTargets& targets)
{
    // --- MatchCoordinator/Clock 転送シグナル ---
    // 終局手追記後にライブセッションを commit する必要があるため、
    // GameStateController → SessionLifecycleCoordinator の順で接続する。
    if (targets.gameStateController) {
        QObject::connect(this, &MatchCoordinatorWiring::requestAppendGameOverMove,
                         targets.gameStateController, &GameStateController::onRequestAppendGameOverMove,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::gameOverStateChanged,
                         targets.gameStateController, &GameStateController::onGameOverStateChanged,
                         Qt::UniqueConnection);
    }
    if (targets.sessionLifecycle) {
        QObject::connect(this, &MatchCoordinatorWiring::requestAppendGameOverMove,
                         targets.sessionLifecycle, &SessionLifecycleCoordinator::commitLiveGameSessionIfActive,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::matchGameEnded,
                         targets.sessionLifecycle, &SessionLifecycleCoordinator::handleGameEnded,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::gameEndProcessed,
                         targets.sessionLifecycle, &SessionLifecycleCoordinator::handleGameEndProcessed,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::requestPreStartCleanup,
                         targets.sessionLifecycle, &SessionLifecycleCoordinator::performPreStartCleanup,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::requestApplyTimeControl,
                         targets.sessionLifecycle, &SessionLifecycleCoordinator::applyTimeControl,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::gameStarted,
                         targets.sessionLifecycle, &SessionLifecycleCoordinator::handleGameStarted,
                         Qt::UniqueConnection);
    }
    if (targets.appearance) {
        QObject::connect(this, &MatchCoordinatorWiring::boardFlipped,
                         targets.appearance, &MainWindowAppearanceController::onBoardFlipped,
                         Qt::UniqueConnection);
    }
    if (targets.gameSession) {
        QObject::connect(this, &MatchCoordinatorWiring::gameInterrupted,
                         targets.gameSession, &GameSessionOrchestrator::onGameInterrupted,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::gameStarted,
                         targets.gameSession, &GameSessionOrchestrator::discardInterruptedGame,
                         Qt::UniqueConnection);
        QObject::connect(this, &MatchCoordinatorWiring::resignationTriggered,
                         targets.gameSession, &GameSessionOrchestrator::onResignationTriggered,
                         Qt::UniqueConnection);
    }

    // --- メニュー GameStartCoordinator 転送シグナル ---
    if (targets.playerInfo) {
        QObject::connect(this, &MatchCoordinatorWiring::menuPlayerNamesResolved,
                         targets.playerInfo, &PlayerInfoWiring::onMenuPlayerNamesResolved,
                         Qt::UniqueConnection);
    }
    if (targets.consecutiveGamesController) {
        QObject::connect(this, &MatchCoordinatorWiring::consecutiveGamesConfigured,
                         targets.consecutiveGamesController, &ConsecutiveGamesController::configure,
                         Qt::UniqueConnection);
    }

    // --- ナビゲーション ---
    if (targets.kifuNav) {
        QObject::connect(this, &MatchCoordinatorWiring::requestSelectKifuRow,
                         targets.kifuNav, &KifuNavigationCoordinator::selectKifuRow,
                         Qt::UniqueConnection);
    }
    if (targets.branchNav) {
        QObject::connect(this, &MatchCoordinatorWiring::requestBranchTreeResetForNewGame,
                         targets.branchNav, &BranchNavigationWiring::onBranchTreeResetForNewGame,
                         Qt::UniqueConnection);
    }
}
