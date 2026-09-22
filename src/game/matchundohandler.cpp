/// @file matchundohandler.cpp
/// @brief 対局中の2手UNDO（待った）処理ハンドラの実装

#include "matchundohandler.h"

#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "boardinteractioncontroller.h"
#include "kifurecordlistmodel.h"
#include "sfenutils.h"
#include "sfendiffutils.h"

#include <QMetaObject>
#include <QMetaMethod>

// ============================================================
// 静的ヘルパ
// ============================================================

bool MatchUndoHandler::isStandardStartposSfen(const QString& sfen)
{
    const QString canon = SfenUtils::hirateSfen();
    return (!sfen.isEmpty() && sfen.trimmed() == canon);
}

// position の開始局面が途中局面でも、末尾の2手だけを取り除く。
QString MatchUndoHandler::buildPositionAfterUndo(const QString& prevFull,
                                                 const QString& targetSfen)
{
    const QString trimmed = prevFull.trimmed();
    const qsizetype movesIndex = trimmed.indexOf(QStringLiteral(" moves "));
    if (movesIndex >= 0) {
        QStringList moves = trimmed.mid(movesIndex + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (moves.size() >= 2) {
            moves.resize(moves.size() - 2);
            const QString head = trimmed.left(movesIndex);
            return moves.isEmpty() ? head : head + QStringLiteral(" moves ") + moves.join(QLatin1Char(' '));
        }
    }
    return isStandardStartposSfen(targetSfen)
        ? QStringLiteral("position startpos")
        : QStringLiteral("position sfen %1").arg(targetSfen);
}

// ============================================================
// Refs / Bindings
// ============================================================

void MatchUndoHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
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

    // --- ロールバック前のフル position を退避 ---
    QString prevFullPosition;
    if (m_refs.positionStr1 && !m_refs.positionStr1->isEmpty()) {
        // HvE の positionStrHistory は人間着手時の履歴なので、
        // エンジンの応手まで含む現在値を優先する。
        prevFullPosition = *m_refs.positionStr1;
    } else if (u_.positionStrList && !u_.positionStrList->isEmpty()) {
        prevFullPosition = u_.positionStrList->last();
    } else if (m_refs.positionStrHistory && !m_refs.positionStrHistory->isEmpty()) {
        prevFullPosition = m_refs.positionStrHistory->constLast();
    }

    const int targetSfenIdx = static_cast<int>(srec->size()) - 3;
    const QString targetSfen = srec->at(targetSfenIdx);

    // --- 盤面と手番を復元 ---
    m_refs.gc->board()->setSfen(targetSfen);
    const bool sideToMoveIsBlack = targetSfen.contains(QStringLiteral(" b "));
    m_refs.gc->setCurrentPlayer(sideToMoveIsBlack ? ShogiGameController::Player1
                                                : ShogiGameController::Player2);
    m_refs.gc->setPromote(false);
    m_refs.gc->setForcedPromotion(false);

    // 「同」表記の基準も戻す。途中局面から開始した場合は gameMoves に
    // 開始前の手がないことがあるため、SFEN履歴の差分から復元する。
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
    if (u_.positionStrList && u_.positionStrList->size() >= 2) {
        u_.positionStrList->remove(u_.positionStrList->size() - 2, 2);
    }
    if (srec && srec->size() >= 2) {
        srec->remove(srec->size() - 2, 2);
    }

    // --- USI用 position 履歴も2手ぶん巻き戻す ---
    if (m_refs.positionStrHistory) {
        if (m_refs.positionStrHistory->size() >= 2) {
            m_refs.positionStrHistory->remove(m_refs.positionStrHistory->size() - 2, 2);
        } else {
            m_refs.positionStrHistory->clear();
        }
    }

    // 巻き戻し後の現在ベースを厳密に再構成
    const QString nextBase = buildPositionAfterUndo(prevFullPosition, targetSfen);

    // 現在値と履歴に反映
    if (m_refs.positionStr1) *m_refs.positionStr1 = nextBase;
    if (m_refs.positionPonder1) *m_refs.positionPonder1 = nextBase;

    if (u_.positionStrList) {
        if (u_.positionStrList->isEmpty()) {
            u_.positionStrList->append(nextBase);
        } else {
            (*u_.positionStrList)[u_.positionStrList->size() - 1] = nextBase;
        }
    }

    if (m_refs.positionStrHistory) {
        if (m_refs.positionStrHistory->isEmpty() || m_refs.positionStrHistory->constLast() != nextBase) {
            m_refs.positionStrHistory->clear();
            m_refs.positionStrHistory->append(nextBase);
        }
    }

    // --- 現在行（0始まり）を同期 ---
    const int targetMoveRow = targetSfenIdx;
    if (u_.currentMoveIndex) {
        *u_.currentMoveIndex = targetMoveRow;
    }

    // --- 表示/ハイライトの同期 ---
    if (u_.recordModel) u_.recordModel->setCurrentHighlightRow(targetMoveRow);
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
