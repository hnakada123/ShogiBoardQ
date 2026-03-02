/// @file gamestatecontroller.cpp
/// @brief 終局処理・投了/中断・人間/エンジン判定の実装

#include "gamestatecontroller.h"

#include <QDebug>

#include "logcategories.h"
#include "matchcoordinator.h"
#include "replaycontroller.h"
#include "timecontrolcontroller.h"
#include "kifuloadcoordinator.h"
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

void GameStateController::setReplayController(ReplayController* replay)
{
    m_replayController = replay;
}

void GameStateController::setTimeController(TimeControlController* tc)
{
    m_timeController = tc;
}

void GameStateController::setKifuLoadCoordinator(KifuLoadCoordinator* kc)
{
    m_kifuLoadCoordinator = kc;
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

void GameStateController::setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne)
{
    if (!m_match || !m_match->gameOverState().isOver) return;

    m_match->appendGameOverLineAndMark(
        cause,
        loserIsPlayerOne ? MatchCoordinator::P1 : MatchCoordinator::P2);

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
    if (m_replayController) {
        m_replayController->exitLiveAppendMode();
    }
}

// ============================================================
// 人間/エンジン判定
// ============================================================

bool GameStateController::isHvH() const
{
    return (m_playMode == PlayMode::HumanVsHuman);
}

bool GameStateController::isHumanSide(ShogiGameController::Player p) const
{
    switch (m_playMode) {
    case PlayMode::HumanVsHuman:
        return true;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        return (p == ShogiGameController::Player1);
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        return (p == ShogiGameController::Player2);
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
        << " loser=" << ((info.loser == MatchCoordinator::P1) ? "P1" : "P2");

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
    const bool loserIsP1 = (info.loser == MatchCoordinator::P1);
    setGameOverMove(info.cause, loserIsP1);

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
    // 1. 投了行の有無チェック
    // 2. ライブ追記モード終了・分岐リセット
    // 3. 手数状態の更新
    // 4. UI遷移（閲覧モードへ）・ハイライト消去

    if (!st.isOver) return;

    // 投了行がまだ追加されていない場合は何もしない
    if (!st.moveAppended) {
        return;
    }

    // ライブ追記モードを終了
    if (m_replayController) {
        m_replayController->exitLiveAppendMode();
    }

    // 分岐コンテキストをリセット
    if (m_kifuLoadCoordinator) {
        m_kifuLoadCoordinator->resetBranchContext();
    }

    // 分岐ツリーを再構築
    if (m_hooks.refreshBranchTree) {
        m_hooks.refreshBranchTree();
    }

    // 対局終了時に現在の手数を設定
    if (m_kifuRecordModel) {
        const int rows = m_kifuRecordModel->rowCount();
        if (rows > 0) {
            const int lastRow = rows - 1;
            if (m_hooks.updatePlyState) {
                m_hooks.updatePlyState(lastRow, lastRow, lastRow);
            }
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
    const bool loserIsP1 = (info.loser == MatchCoordinator::P1);
    setGameOverMove(info.cause, loserIsP1);
}
