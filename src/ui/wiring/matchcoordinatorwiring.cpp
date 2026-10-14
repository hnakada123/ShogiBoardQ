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
}

void MatchCoordinatorWiring::setSignalTargets(const SignalTargets& targets)
{
    m_targets = targets;
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

    connectMatchSignals();
}

void MatchCoordinatorWiring::connectMatchSignals()
{
    MatchCoordinator* mc = m_match.get();

    // 終局手の追記（GameStateController）→ ライブセッションの確定（SessionLifecycleCoordinator）の順。
    // 確定は追記した終局手を含めて行う
    if (auto* gsc = m_targets.gameStateController) {
        connect(mc, &MatchCoordinator::requestAppendGameOverMove,
                gsc, &GameStateController::onRequestAppendGameOverMove);
        connect(mc, &MatchCoordinator::gameOverStateChanged,
                gsc, &GameStateController::onGameOverStateChanged);
    }
    if (auto* slc = m_targets.sessionLifecycle) {
        connect(mc, &MatchCoordinator::requestAppendGameOverMove,
                slc, &SessionLifecycleCoordinator::commitLiveGameSessionIfActive);
        connect(mc, &MatchCoordinator::gameEnded,
                slc, &SessionLifecycleCoordinator::handleGameEnded);
        connect(mc, &MatchCoordinator::gameEndProcessed,
                slc, &SessionLifecycleCoordinator::handleGameEndProcessed);
    }
    // 終局の記録（handleGameEnded）のあとで、UI状態を「アイドル」に戻す。
    // 検討モード・詰み探索の終了でも「アイドル」に戻す
    if (auto* uiPolicy = m_targets.uiStatePolicy) {
        connect(mc, &MatchCoordinator::gameEnded,
                uiPolicy, &UiStatePolicyManager::transitionToIdle);
        connect(mc, &MatchCoordinator::considerationModeEnded,
                uiPolicy, &UiStatePolicyManager::transitionToIdle);
        connect(mc, &MatchCoordinator::tsumeSearchModeEnded,
                uiPolicy, &UiStatePolicyManager::transitionToIdle);
    }
    if (auto* appearance = m_targets.appearance) {
        connect(mc, &MatchCoordinator::boardFlipped,
                appearance, &MainWindowAppearanceController::onBoardFlipped);
    }
    if (auto* gameSession = m_targets.gameSession) {
        connect(mc, &MatchCoordinator::gameInterrupted,
                gameSession, &GameSessionOrchestrator::onGameInterrupted);
    }
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

    connectGameStartSignals();
}

void MatchCoordinatorWiring::connectGameStartSignals()
{
    GameStartCoordinator* gsc = m_menuGameStart;

    // 対局開始の通知は、UI状態を「対局中」にする → 開始局面とモードの反映 → 中断した対局の破棄 の順。
    // UiStatePolicyManager は、モードを MainWindow 側の状態ではなく司令塔から読む
    if (auto* uiPolicy = m_targets.uiStatePolicy) {
        connect(gsc, &GameStartCoordinator::started,
                uiPolicy, &UiStatePolicyManager::transitionToDuringGame);
    }
    if (auto* slc = m_targets.sessionLifecycle) {
        connect(gsc, &GameStartCoordinator::started,
                slc, &SessionLifecycleCoordinator::handleGameStarted);
        connect(gsc, &GameStartCoordinator::requestPreStartCleanup,
                slc, &SessionLifecycleCoordinator::performPreStartCleanup);
        connect(gsc, &GameStartCoordinator::requestApplyTimeControl,
                slc, &SessionLifecycleCoordinator::applyTimeControl);
    }
    if (auto* gameSession = m_targets.gameSession) {
        connect(gsc, &GameStartCoordinator::started,
                gameSession, &GameSessionOrchestrator::discardInterruptedGame);
    }
    if (auto* playerInfo = m_targets.playerInfo) {
        connect(gsc, &GameStartCoordinator::playerNamesResolved,
                playerInfo, &PlayerInfoWiring::onMenuPlayerNamesResolved);
    }
    if (auto* consecutive = m_targets.consecutiveGamesController) {
        connect(gsc, &GameStartCoordinator::consecutiveGamesConfigured,
                consecutive, &ConsecutiveGamesController::configure);
    }
    if (auto* kifuNav = m_targets.kifuNav) {
        connect(gsc, &GameStartCoordinator::requestSelectKifuRow,
                kifuNav, &KifuNavigationCoordinator::selectKifuRow);
    }
    if (auto* branchNav = m_targets.branchNav) {
        connect(gsc, &GameStartCoordinator::requestBranchTreeResetForNewGame,
                branchNav, &BranchNavigationWiring::onBranchTreeResetForNewGame);
    }
}
