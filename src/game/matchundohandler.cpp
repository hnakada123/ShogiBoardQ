/// @file matchundohandler.cpp
/// @brief 対局中の2手UNDO（待った）処理ハンドラの実装

#include "matchundohandler.h"

#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "boardinteractioncontroller.h"
#include "kifurecordlistmodel.h"

#include <QMetaObject>
#include <QMetaMethod>

// ============================================================
// 静的ヘルパ
// ============================================================

bool MatchUndoHandler::isStandardStartposSfen(const QString& sfen)
{
    const QString canon = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    return (!sfen.isEmpty() && sfen.trimmed() == canon);
}

// 直前のフル position 文字列から、先頭 handCount 手だけ残したベースを作る。
// 例）prev="position startpos moves 7g7f 3c3d 2g2f 8c8d", handCount=2
//   → "position startpos moves 7g7f 3c3d"
QString MatchUndoHandler::buildBasePositionUpToHands(const QString& prevFull,
                                                     int handCount,
                                                     const QString& startSfenHint)
{
    QString head;  // "position startpos" or "position sfen <...>"
    QStringList moves;

    // prevFull を優先して head/moves を抽出
    if (!prevFull.isEmpty()) {
        const QString trimmed = prevFull.trimmed();

        if (trimmed.startsWith(QStringLiteral("position startpos"))) {
            head = QStringLiteral("position startpos");
        } else if (trimmed.startsWith(QStringLiteral("position sfen "))) {
            const qsizetype idxMoves = trimmed.indexOf(QStringLiteral(" moves "));
            head = (idxMoves >= 0) ? trimmed.left(idxMoves) : trimmed;
        }

        // moves の抽出
        const qsizetype idxMoves2 = trimmed.indexOf(QStringLiteral(" moves "));
        if (idxMoves2 >= 0) {
            const QString after = trimmed.mid(idxMoves2 + 7); // 7 = strlen(" moves ")
            const QStringList toks = after.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString& t : toks) {
                if (!t.isEmpty()) moves.append(t);
            }
        }
    }

    // prevFull から head が取れなかった場合：SFENヒントで head を決める
    if (head.isEmpty()) {
        if (!startSfenHint.isEmpty() && !isStandardStartposSfen(startSfenHint)) {
            head = QStringLiteral("position sfen %1").arg(startSfenHint);
        } else {
            head = QStringLiteral("position startpos");
        }
    }

    // handCount でトリミング
    if (handCount <= 0) {
        return head;
    }

    if (moves.isEmpty()) {
        return head;
    }

    const qsizetype take = qMin(qsizetype(handCount), moves.size());
    QStringList headMoves = moves.mid(0, take);
    return QStringLiteral("%1 moves %2").arg(head, headMoves.join(QLatin1Char(' ')));
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
    if (!m_refs.gc) return false;

    // --- ロールバック前のフル position を退避 ---
    QString prevFullPosition;
    if (u_.positionStrList && !u_.positionStrList->isEmpty()) {
        prevFullPosition = u_.positionStrList->last();
    } else if (m_refs.positionStrHistory && !m_refs.positionStrHistory->isEmpty()) {
        prevFullPosition = m_refs.positionStrHistory->constLast();
    }

    // --- SFEN履歴の現在位置を把握 ---
    QStringList* srec = m_refs.sfenHistory;
    int curSfenIdx = -1;

    if (srec && !srec->isEmpty()) {
        curSfenIdx = static_cast<int>(srec->size() - 1);
    } else if (u_.currentMoveIndex) {
        curSfenIdx = *u_.currentMoveIndex + 1;
    } else if (m_refs.gameMoves) {
        curSfenIdx = static_cast<int>(m_refs.gameMoves->size() + 1);
    }

    // 2手戻し後の SFEN 添字（= 残すべき手数）
    const int targetSfenIdx = qMax(0, curSfenIdx - 2);
    const int remainHands   = qMax(0, targetSfenIdx);

    // --- 盤面を targetSfenIdx に復元 ---
    if (srec && targetSfenIdx < srec->size()) {
        const QString sfen = srec->at(targetSfenIdx);
        if (m_refs.gc->board()) {
            m_refs.gc->board()->setSfen(sfen);
        }
        const bool sideToMoveIsBlack = sfen.contains(QStringLiteral(" b "));
        m_refs.gc->setCurrentPlayer(sideToMoveIsBlack ? ShogiGameController::Player1
                                                      : ShogiGameController::Player2);
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
    const QString startSfen0 = (srec && !srec->isEmpty()) ? srec->first() : QString();
    const QString nextBase   = buildBasePositionUpToHands(prevFullPosition, remainHands, startSfen0);

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
    const int targetMoveRow = qMax(0, targetSfenIdx - 1);
    if (u_.currentMoveIndex) {
        *u_.currentMoveIndex = targetMoveRow;
    }

    // --- 表示/ハイライトの同期 ---
    if (u_.boardCtl) u_.boardCtl->clearAllHighlights();
    if (h_.updateHighlightsForPly) h_.updateHighlightsForPly(targetSfenIdx);
    if (h_.updateTurnAndTimekeepingDisplay) h_.updateTurnAndTimekeepingDisplay();

    // --- 入力許可（人間手番なら盤クリックOK） ---
    const auto stm = m_refs.gc->currentPlayer();
    const bool humanNow = h_.isHumanSide ? h_.isHumanSide(stm) : false;
    if (m_refs.view) m_refs.view->setMouseClickMode(humanNow);

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
