/// @file gamestatecontroller.cpp
/// @brief 終局処理・投了/中断・人間/エンジン判定の実装

#include "gamestatecontroller.h"

#include <QDebug>

#include "logcategories.h"
#include "matchcoordinator.h"
#include "timecontrolcontroller.h"
#include "kifurecordlistmodel.h"
#include "shogiclock.h"

// ============================================================
// 生成・破棄
// ============================================================

GameStateController::GameStateController(QObject* parent)
    : QObject(parent)
{
}

GameStateController::~GameStateController() = default;

// ============================================================
// 依存オブジェクトの設定
// ============================================================

void GameStateController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void GameStateController::setTimeController(TimeControlController* tc)
{
    m_timeController = tc;
}

void GameStateController::setKifuRecordModel(KifuRecordListModel* model)
{
    m_kifuRecordModel = model;
}

void GameStateController::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

void GameStateController::setPlayMode(PlayMode mode)
{
    m_playMode = mode;
}

// ============================================================
// 投了・中断処理
// ============================================================

void GameStateController::handleResignation()
{
    if (m_match) {
        m_match->handleResign();
    }
}

void GameStateController::handleBreakOffGame()
{
    if (!m_match || m_match->gameOverState().isOver) return;
    m_match->handleBreakOff();
}

// ============================================================
// 終局処理
// ============================================================

void GameStateController::setGameOverMove(MatchCoordinator::Cause cause, Turn loser)
{
    if (!m_match || !m_match->gameOverState().isOver) return;

    m_match->appendGameOverLineAndMark(cause, loser);

    // UI 後処理
    if (m_hooks.updateBoardView) {
        m_hooks.updateBoardView();
    }
    if (m_hooks.setKifuViewSingleSelection) {
        m_hooks.setKifuViewSingleSelection();
    }
    if (m_hooks.setReplayMode) {
        m_hooks.setReplayMode(true);
    }
}

// ============================================================
// 人間/エンジン判定
// ============================================================

bool GameStateController::isHvH() const
{
    return (m_playMode == PlayMode::HumanVsHuman);
}

bool GameStateController::isHumanSide(Turn p) const
{
    switch (m_playMode) {
    case PlayMode::HumanVsHuman:
        return true;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        return (p == Turn::Black);
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        return (p == Turn::White);
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        return false;
    default:
        return true;
    }
}

bool GameStateController::isGameOver() const
{
    return m_match && m_match->gameOverState().isOver;
}

// ============================================================
// シグナルハンドラ
// ============================================================

void GameStateController::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    // 処理フロー:
    // 1. 対局終了スタイルロック
    // 2. タイマー停止・マウス操作無効化
    // 3. 棋譜追記
    // 4. UI後処理（矢印ボタン有効化・選択モード設定）

    qCDebug(lcGame).nospace()
        << "onMatchGameEnded ENTER cause="
        << ((info.cause == MatchCoordinator::Cause::Timeout) ? "Timeout" : "Resign")
        << " loser=" << info.loser;

    // 対局終了時のスタイル維持を有効化
    if (m_hooks.setGameOverStyleLock) {
        m_hooks.setGameOverStyleLock(true);
    }

    // UI 後始末
    if (m_match) {
        m_match->disarmHumanTimerIfNeeded();
    }
    if (ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr) {
        clk->stopClock();
    }
    if (m_hooks.setMouseClickMode) {
        m_hooks.setMouseClickMode(false);
    }

    // 棋譜追記＋時間確定
    setGameOverMove(info.cause, info.loser);

    // UIの後処理
    if (m_hooks.enableArrowButtons) {
        m_hooks.enableArrowButtons();
    }
    if (m_hooks.setKifuViewSingleSelection) {
        m_hooks.setKifuViewSingleSelection();
    }

    qCDebug(lcGame) << "onMatchGameEnded LEAVE";
}

void GameStateController::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    // 処理フロー:
    // 1. 対局終了スタイルロック・投了行の有無チェック
    // 2. 分岐ツリー再構築
    // 3. 手数状態の更新
    // 4. UI遷移（閲覧モードへ）・ハイライト消去

    if (!st.isOver) return;

    // 棋譜追記に伴う閲覧モードへの遷移は gameEnded より先に起こるため、
    // 終局を検知した時点で名前・時計の手番ハイライトを保護する。
    if (m_hooks.setGameOverStyleLock) {
        m_hooks.setGameOverStyleLock(true);
    }

    // 投了行が追加されるまでは UI 後処理を待つ
    if (!st.moveAppended) {
        return;
    }

    // 対局終了時に表示中の手数を最終行（終局行）に合わせる
    if (m_kifuRecordModel) {
        const int rows = m_kifuRecordModel->rowCount();
        if (rows > 0 && m_hooks.syncCursorToPly) {
            m_hooks.syncCursorToPly(rows - 1);
        }
    }

    // UI 遷移（閲覧モードへ）
    if (m_hooks.enableArrowButtons) {
        m_hooks.enableArrowButtons();
    }
    if (m_hooks.setReplayMode) {
        m_hooks.setReplayMode(true);
    }

    // ハイライト消去
    if (m_hooks.removeHighlightAllData) {
        m_hooks.removeHighlightAllData();
    }

}

void GameStateController::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    setGameOverMove(info.cause, info.loser);
}
