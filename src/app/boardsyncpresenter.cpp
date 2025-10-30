#include "boardsyncpresenter.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "boardinteractioncontroller.h"
#include "shogiboard.h"
#include "shogimove.h"
#include <QDebug>

BoardSyncPresenter::BoardSyncPresenter(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_view(d.view)
    , m_bic(d.bic)
    , m_sfenRecord(d.sfenRecord)
    , m_gameMoves(d.gameMoves)
{
}

void BoardSyncPresenter::applySfenAtPly(int ply) const
{
    if (!m_sfenRecord || m_sfenRecord->isEmpty() || !m_gc || !m_gc->board()) return;

    const int idx = qBound(0, ply, m_sfenRecord->size() - 1);
    const QString sfen = m_sfenRecord->at(idx);

    qDebug().noquote() << "[PRESENTER] applySfenAtPly idx=" << idx << " sfen=" << sfen;

    m_gc->board()->setSfen(sfen);

    if (m_view) {
        // GC の board をそのまま適用→再描画
        m_view->applyBoardAndRender(m_gc->board());
    }
}

void BoardSyncPresenter::syncBoardAndHighlightsAtRow(int ply) const
{
    if (!m_sfenRecord || !m_gc || !m_gc->board()) return;

    const int maxPly = m_sfenRecord->size() - 1;
    if (maxPly < 0) return;

    const int safePly = qBound(0, ply, maxPly);

    // 1) 盤面の適用
    applySfenAtPly(safePly);

    // 2) 最終手のハイライト（ply==0 は初期局面で手なし）
    if (!m_bic || !m_gameMoves || safePly <= 0) return;

    const int mvIdx = safePly - 1;
    if (mvIdx < 0 || mvIdx >= m_gameMoves->size()) return;

    const ShogiMove& last = m_gameMoves->at(mvIdx);

    // ドロップ対策：from が無効座標なら「移動元ハイライト無し」
    const bool hasFrom = (last.fromSquare.x() >= 0 && last.fromSquare.y() >= 0);
    const QPoint to1   = toOne(last.toSquare);

    if (hasFrom) {
        const QPoint from1 = toOne(last.fromSquare);
        m_bic->showMoveHighlights(from1, to1);
    } else {
        m_bic->showMoveHighlights(QPoint(), to1);
    }
}

void BoardSyncPresenter::clearHighlights() const
{
    if (m_bic) m_bic->clearAllHighlights();
}
