/// @file matchundohandler.cpp
/// @brief 対局中の2手UNDO（待った）処理ハンドラの実装

#include "matchundohandler.h"

#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "boardinteractioncontroller.h"
#include "sfenutils.h"
#include "sfendiffutils.h"
#include "shogiclock.h"

#include <QSignalBlocker>

// ============================================================
// Refs / Bindings
// ============================================================

void MatchUndoHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void MatchUndoHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

void MatchUndoHandler::setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks)
{
    m_uiRefs = refs;
    m_undoHooks = hooks;
}

// ============================================================
// UNDO 処理
// ============================================================

bool MatchUndoHandler::undoTwoPlies()
{
    // 開始局面を残すために2手分の履歴が必須。
    // エンジン応答待ちの入れ子イベントループからは巻き戻さない。
    QStringList* srec = m_refs.sfenHistory;
    if (!m_refs.gc || !m_refs.gc->board() || !srec || srec->size() < 3) return false;
    if (m_undoHooks.isHumanSide && !m_undoHooks.isHumanSide(m_refs.gc->currentPlayer())) return false;

    if (m_undoHooks.canUndoRecord && !m_undoHooks.canUndoRecord()) return false;
    const auto update = m_refs.gc->positionUpdate();
    ShogiClock* clock = m_hooks.clockProvider ? m_hooks.clockProvider() : nullptr;
    const QSignalBlocker clockNotifications(clock);

    const int targetSfenIdx = static_cast<int>(srec->size()) - 3;
    const QString targetSfen = srec->at(targetSfenIdx);

    // --- 盤面と手番を復元 ---
    m_refs.gc->board()->setSfen(targetSfen);
    const bool sideToMoveIsBlack = targetSfen.contains(QStringLiteral(" b "));
    m_refs.gc->setCurrentPlayer(sideToMoveIsBlack ? Turn::Black
                                                : Turn::White);
    m_refs.gc->setForcedPromotion(false);

    // 「同」表記の基準も戻す。戻した局面とその1つ前の局面の差分から移動先を求める
    // （局面列だけで決まるので、指し手列の長さに依存しない）。
    m_refs.gc->setPreviousMoveDestination(QPoint());
    SfenDiffUtils::DiffResult diff;
    if (targetSfenIdx > 0
        && SfenDiffUtils::diffBoards(srec->at(targetSfenIdx - 1), targetSfen, diff)
        && diff.isSingleMovePattern()) {
        m_refs.gc->setPreviousMoveDestination(diff.to + QPoint(1, 1));
    }

    // --- 棋譜/モデル/履歴を末尾2件ずつ削除 ---
    if (m_refs.gameMoves && m_refs.gameMoves->size() >= 2) {
        m_refs.gameMoves->remove(m_refs.gameMoves->size() - 2, 2);
    }
    if (srec && srec->size() >= 2) {
        srec->remove(srec->size() - 2, 2);
    }

    // --- 時計を取り消した自分の手を指し始めたときへ戻し、考慮時間は今から測り直す ---
    if (clock) {
        clock->undo();
    }
    if (m_hooks.restartHumanTurnTimer) m_hooks.restartHumanTurnTimer();

    if (m_undoHooks.commitRecordUndo) m_undoHooks.commitRecordUndo();

    // --- 表示/ハイライトの同期 ---
    if (m_uiRefs.boardCtl) {
        m_uiRefs.boardCtl->cancelPendingClick();
        m_uiRefs.boardCtl->clearAllHighlights();
    }
    if (m_undoHooks.updateHighlightsForPly) m_undoHooks.updateHighlightsForPly(targetSfenIdx);
    if (m_undoHooks.updateTurnAndTimekeepingDisplay) m_undoHooks.updateTurnAndTimekeepingDisplay();

    // --- 入力許可（人間手番なら盤クリックOK） ---
    const auto stm = m_refs.gc->currentPlayer();
    const bool humanNow = m_undoHooks.isHumanSide ? m_undoHooks.isHumanSide(stm) : false;
    if (m_undoHooks.setMouseClickMode) m_undoHooks.setMouseClickMode(humanNow);

    return true;
}
