/// @file boardsyncpresenter.cpp
/// @brief 盤面同期プレゼンタクラスの実装

#include "boardsyncpresenter.h"
#include "boardcoordinates.h"
#include "boardinteractioncontroller.h"
#include "kifunavigationstate.h"
#include "logcategories.h"
#include "sfendiffutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

BoardSyncPresenter::BoardSyncPresenter(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_view(d.view)
    , m_bic(d.bic)
    , m_navState(d.navState)
{
}

void BoardSyncPresenter::syncBoardAndHighlightsAtRow(int ply) const
{
    const RecordLine line = m_navState ? m_navState->currentRecordLine() : RecordLine{};
    if (line.isEmpty() || !m_gc || !m_gc->board()) {
        qCDebug(lcUi) << "syncBoardAndHighlightsAtRow ABORT: ply=" << ply;
        return;
    }

    // 終局（投了など）の行は局面が増えないので、最後の局面を描いてハイライトを消す
    const int maxIdx = line.moveCount();
    const bool isTerminalRow = ply > maxIdx;
    const int idx = qBound(0, ply, maxIdx);
    qCDebug(lcUi).noquote() << QStringLiteral("syncBoardAndHighlightsAtRow ply=%1 idx=%2 size=%3")
                                   .arg(ply).arg(idx).arg(maxIdx + 1);

    applyBoard(line.sfenAt(idx));

    if (!m_bic) return;
    if (idx <= 0 || isTerminalRow) {
        m_bic->clearAllHighlights();
        return;
    }
    showMoveHighlight(line.sfenAt(idx - 1), line.sfenAt(idx));
}

void BoardSyncPresenter::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen) const
{
    if (currentSfen.isEmpty()) {
        qCWarning(lcUi) << "loadBoardWithHighlights: empty currentSfen, skipping";
        return;
    }

    applyBoard(currentSfen);

    if (!m_bic) return;
    // 開始局面（prevSfenが空）の場合はハイライトをクリア
    if (prevSfen.isEmpty()) {
        m_bic->clearAllHighlights();
        return;
    }
    showMoveHighlight(prevSfen, currentSfen);
}

void BoardSyncPresenter::applyBoard(const QString& sfen) const
{
    if (!m_gc || !m_gc->board()) return;
    m_gc->board()->setSfen(sfen);
    if (m_view) {
        m_view->applyBoardAndRender(m_gc->board());
    }
}

void BoardSyncPresenter::showMoveHighlight(const QString& prevSfen, const QString& currentSfen) const
{
    // 2つの局面の差分から移動元・移動先を求める。駒打ちは打った駒の駒台の位置を移動元にする
    SfenDiffUtils::DiffResult diff;
    if (!SfenDiffUtils::diffBoards(prevSfen, currentSfen, diff) || !diff.isSingleMovePattern()) {
        qCDebug(lcUi) << "showMoveHighlight: not a single move, clearing highlights";
        m_bic->clearAllHighlights();
        return;
    }

    const QPoint to(diff.to.x() + 1, diff.to.y() + 1);
    QPoint from(-1, -1);
    if (diff.hasFrom()) {
        from = QPoint(diff.from.x() + 1, diff.from.y() + 1);
    } else {
        const QPoint stand = BoardCoordinates::standSquareOf(diff.droppedPiece);
        if (!stand.isNull()) from = stand;
    }
    m_bic->showMoveHighlights(from, to);
}
