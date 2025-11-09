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

// デバッグ用：SFENリストのダンプ（必要なら使う）
// ※ std::min(int, qsizetype) の型衝突を避けるため、自前で件数を算出
static inline void DBG_DUMP_SFEN_LIST(const QStringList* rec, const char* tag, int maxItems = 8) {
    if (!rec) { qDebug().noquote() << tag << " (rec=null)"; return; }

    const qsizetype sz = rec->size();
    qDebug().noquote() << tag << " size=" << sz << " ptr=" << static_cast<const void*>(rec);

    const int n = (sz > static_cast<qsizetype>(maxItems))
                      ? maxItems
                      : static_cast<int>(sz);

    for (int i = 0; i < n; ++i) {
        qDebug().noquote() << "  [" << i << "] " << rec->at(i);
    }
    if (sz > n) {
        qDebug().noquote() << "  ... (+" << static_cast<int>(sz - n) << " more)";
    }
}

void BoardSyncPresenter::applySfenAtPly(int ply) const
{
    if (!m_sfenRecord || m_sfenRecord->isEmpty() || !m_gc || !m_gc->board()) {
        qDebug() << "[PRESENTER] applySfenAtPly guard failed:"
                 << "rec=" << static_cast<const void*>(m_sfenRecord)
                 << "isEmpty?=" << (m_sfenRecord ? m_sfenRecord->isEmpty() : true)
                 << "gc=" << m_gc << "board?=" << (m_gc ? m_gc->board() : nullptr);
        return;
    }

    const int size = static_cast<int>(m_sfenRecord->size());
    const int idx  = qBound(0, ply, size - 1);
    const QString sfen = m_sfenRecord->at(idx);

    qDebug().noquote() << "[PRESENTER] applySfenAtPly reqPly=" << ply
                       << " idx=" << idx
                       << " size=" << size
                       << " rec*=" << static_cast<const void*>(m_sfenRecord);

    if (size > 0) {
        qDebug().noquote() << "[PRESENTER] head[0]=" << m_sfenRecord->first();
        qDebug().noquote() << "[PRESENTER] tail[last]=" << m_sfenRecord->last();
    }
    qDebug().noquote() << "[PRESENTER] pick[" << idx << "]=" << sfen;

    // 4フィールド分解
    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts);
    if (parts.size() == 4) {
        qDebug().noquote() << "[PRESENTER] fields board=" << parts[0]
                           << " turn=" << parts[1]
                           << " stand=" << parts[2]
                           << " move=" << parts[3];

        if (idx == 0) {
            if (parts[3] != QLatin1String("1") ||
                (parts[1] != QLatin1String("b") && parts[1] != QLatin1String("w"))) {
                qDebug().noquote() << "[WARN][PRESENTER] head looks suspicious: " << m_sfenRecord->first();
                // 必要ならダンプ：
                // DBG_DUMP_SFEN_LIST(m_sfenRecord, "[PRESENTER] dump", 20);
            }
        }
    } else {
        qDebug().noquote() << "[PRESENTER] fields malformed: " << sfen;
    }

    // 実適用
    m_gc->board()->setSfen(sfen);
    if (m_view) {
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
