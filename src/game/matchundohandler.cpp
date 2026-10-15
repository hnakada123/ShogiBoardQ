/// @file matchundohandler.cpp
/// @brief 対局中の2手UNDO（待った）処理ハンドラの実装

#include "matchundohandler.h"

#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "boardinteractioncontroller.h"
#include "kifurecordlistmodel.h"
#include "sfenutils.h"
#include "sfendiffutils.h"
#include "shogiclock.h"

#include <QMetaObject>
#include <QMetaMethod>

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
    u_ = refs;
    h_ = hooks;
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
    if (h_.isHumanSide && !h_.isHumanSide(m_refs.gc->currentPlayer())) return false;

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
    if (u_.recordModel) tryRemoveLastItems(u_.recordModel, 2);
    if (m_refs.gameMoves && m_refs.gameMoves->size() >= 2) {
        m_refs.gameMoves->remove(m_refs.gameMoves->size() - 2, 2);
    }
    if (srec && srec->size() >= 2) {
        srec->remove(srec->size() - 2, 2);
    }

    // --- 時計を取り消した自分の手を指し始めたときへ戻し、考慮時間は今から測り直す ---
    if (ShogiClock* clock = m_hooks.clockProvider ? m_hooks.clockProvider() : nullptr) {
        clock->undo();
    }
    if (m_hooks.restartHumanTurnTimer) m_hooks.restartHumanTurnTimer();

    // --- 表示/ハイライトの同期 ---
    if (u_.recordModel) u_.recordModel->setCurrentHighlightRow(targetSfenIdx);
    if (u_.boardCtl) {
        u_.boardCtl->cancelPendingClick();
        u_.boardCtl->clearAllHighlights();
    }
    if (h_.updateHighlightsForPly) h_.updateHighlightsForPly(targetSfenIdx);
    if (h_.updateTurnAndTimekeepingDisplay) h_.updateTurnAndTimekeepingDisplay();

    // --- 入力許可（人間手番なら盤クリックOK） ---
    const auto stm = m_refs.gc->currentPlayer();
    const bool humanNow = h_.isHumanSide ? h_.isHumanSide(stm) : false;
    if (h_.setMouseClickMode) h_.setMouseClickMode(humanNow);

    return true;
}

bool MatchUndoHandler::tryRemoveLastItems(QObject* model, int n)
{
    if (!model) return false;

    if (auto* km = qobject_cast<KifuRecordListModel*>(model)) {
        return km->removeLastItems(n);
    }

    const QMetaObject* mo = model->metaObject();
    const int idx = mo->indexOfMethod("removeLastItems(int)");
    if (idx < 0) return false;

    bool ok = QMetaObject::invokeMethod(model, "removeLastItems", Q_ARG(int, n));
    return ok;
}
