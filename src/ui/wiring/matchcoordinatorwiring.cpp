/// @file matchcoordinatorwiring.cpp
/// @brief MatchCoordinator配線クラスの実装

#include "matchcoordinatorwiring.h"

#include "gamestartcoordinator.h"
#include "evaluationgraphcontroller.h"
#include "timecontrolcontroller.h"
#include "timedisplaypresenter.h"
#include "uistatepolicymanager.h"
#include "boardinteractioncontroller.h"
#include "shogiclock.h"

MatchCoordinatorWiring::MatchCoordinatorWiring(QObject* parent)
    : QObject(parent)
{
}

void MatchCoordinatorWiring::updateDeps(const Deps& deps)
{
    m_gc = deps.gc;
    m_view = deps.view;
    m_usi1 = deps.usi1;
    m_usi2 = deps.usi2;
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
    m_positionStrList = deps.positionStrList;
    m_currentMoveIndex = deps.currentMoveIndex;

    m_hooks = deps.hooks;
    m_undoHooks = deps.undoHooks;

    if (deps.ensureTimeController)
        m_ensureTimeController = deps.ensureTimeController;
    if (deps.ensureEvaluationGraphController)
        m_ensureEvaluationGraphController = deps.ensureEvaluationGraphController;
    if (deps.ensurePlayerInfoWiring)
        m_ensurePlayerInfoWiring = deps.ensurePlayerInfoWiring;
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

void MatchCoordinatorWiring::wireConnections()
{
    // 依存が揃っていない場合は何もしない
    if (!m_gc || !m_view) return;

    // まず時計を用意（nullでも可だが、あれば渡す）
    if (m_ensureTimeController) m_ensureTimeController();

    // --- MatchCoordinator::Deps を構築 ---
    MatchCoordinator::Deps d;
    d.gc    = m_gc;
    d.clock = m_getClock ? m_getClock() : nullptr;
    d.view  = m_view;
    d.usi1  = m_usi1;
    d.usi2  = m_usi2;
    d.comm1  = m_comm1;
    d.think1 = m_think1;
    d.comm2  = m_comm2;
    d.think2 = m_think2;
    d.sfenRecord = m_sfenRecord;
    d.gameMoves  = m_gameMoves;
    d.hooks = m_hooks;

    // --- GameStartCoordinator の確保（1 回だけ） ---
    ensureGameStartCoordinator();

    // --- MatchCoordinator（司令塔）の生成＆初期配線 ---
    m_match = m_gameStartCoordinator->createAndWireMatch(d, parent());

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
        evalCtl->setMatchCoordinator(m_match);
        evalCtl->setSfenRecord(m_sfenRecord);
    }

    // UndoBindings の設定
    if (m_match) {
        MatchCoordinator::UndoRefs u;
        u.recordModel      = m_kifuRecordModel;
        u.positionStrList  = m_positionStrList;
        u.currentMoveIndex = m_currentMoveIndex;
        u.boardCtl         = m_boardController;

        m_match->setUndoBindings(u, m_undoHooks);

        // 検討モード終了・詰み探索終了時にUI状態を「アイドル」に遷移
        if (m_ensureUiStatePolicyManager) m_ensureUiStatePolicyManager();
        auto* uiPolicy = m_getUiStatePolicy ? m_getUiStatePolicy() : nullptr;
        if (uiPolicy) {
            connect(m_match, &MatchCoordinator::considerationModeEnded,
                    uiPolicy, &UiStatePolicyManager::transitionToIdle,
                    Qt::UniqueConnection);
            connect(m_match, &MatchCoordinator::tsumeSearchModeEnded,
                    uiPolicy, &UiStatePolicyManager::transitionToIdle,
                    Qt::UniqueConnection);
        }
    }

    // TimeController へ MatchCoordinator を反映
    auto* timeCtl = m_getTimeController ? m_getTimeController() : nullptr;
    if (timeCtl) {
        timeCtl->setMatchCoordinator(m_match);
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

void MatchCoordinatorWiring::ensureGameStartCoordinator()
{
    if (m_gameStartCoordinator) return;

    GameStartCoordinator::Deps gd;
    gd.match = nullptr;
    gd.clock = m_getClock ? m_getClock() : nullptr;
    gd.gc    = m_gc;
    gd.view  = m_view;

    m_gameStartCoordinator = new GameStartCoordinator(gd, this);

    // timeUpdated → TimeDisplayPresenter
    if (m_timeConn) { QObject::disconnect(m_timeConn); m_timeConn = {}; }
    if (m_timePresenter) {
        m_timeConn = connect(
            m_gameStartCoordinator, &GameStartCoordinator::timeUpdated,
            m_timePresenter, &TimeDisplayPresenter::onMatchTimeUpdated,
            Qt::UniqueConnection);
    }

    // シグナル転送: GameStartCoordinator → MatchCoordinatorWiring
    connect(m_gameStartCoordinator, &GameStartCoordinator::requestAppendGameOverMove,
            this,                   &MatchCoordinatorWiring::requestAppendGameOverMove,
            Qt::UniqueConnection);

    connect(m_gameStartCoordinator, &GameStartCoordinator::boardFlipped,
            this,                   &MatchCoordinatorWiring::boardFlipped,
            Qt::UniqueConnection);

    connect(m_gameStartCoordinator, &GameStartCoordinator::gameOverStateChanged,
            this,                   &MatchCoordinatorWiring::gameOverStateChanged,
            Qt::UniqueConnection);

    connect(m_gameStartCoordinator, &GameStartCoordinator::matchGameEnded,
            this,                   &MatchCoordinatorWiring::matchGameEnded,
            Qt::UniqueConnection);

    // 対局終了時にUI状態を「アイドル」に遷移
    if (m_ensureUiStatePolicyManager) m_ensureUiStatePolicyManager();
    auto* uiPolicy = m_getUiStatePolicy ? m_getUiStatePolicy() : nullptr;
    if (uiPolicy) {
        connect(m_gameStartCoordinator, &GameStartCoordinator::matchGameEnded,
                uiPolicy, &UiStatePolicyManager::transitionToIdle,
                Qt::UniqueConnection);
    }
}
