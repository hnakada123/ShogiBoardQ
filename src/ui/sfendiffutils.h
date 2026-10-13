#ifndef SFENDIFFUTILS_H
#define SFENDIFFUTILS_H

/// @file sfendiffutils.h
/// @brief SFEN 盤面差分の共通ユーティリティ

#include <QPoint>
#include <QString>

#include "sfenutils.h"
#include "shogiboard.h"

namespace SfenDiffUtils {

struct DiffResult
{
    QPoint from = QPoint(-1, -1);   ///< 駒がなくなったマス（筋-1, 段-1）
    QPoint to = QPoint(-1, -1);     ///< 駒が置かれた・替わったマス（筋-1, 段-1）
    Piece droppedPiece = Piece::None;  ///< 空きマスに置かれた駒（先後を含む生駒）
    int emptyCount = 0;
    int filledCount = 0;
    int changedCount = 0;

    [[nodiscard]] bool hasFrom() const
    {
        return from.x() >= 0 && from.y() >= 0;
    }

    [[nodiscard]] bool hasTo() const
    {
        return to.x() >= 0 && to.y() >= 0;
    }

    [[nodiscard]] bool isSingleMovePattern() const
    {
        // 通常移動 / 駒取り / 駒打ちのいずれか
        return (emptyCount == 1 && changedCount == 0 && filledCount == 1) ||
               (emptyCount == 1 && changedCount == 1 && filledCount == 0) ||
               (emptyCount == 0 && changedCount == 0 && filledCount == 1);
    }
};

/// 2つの局面（SFEN。"startpos" や "position ..." も読む）の盤上の差分を数える。読めない局面があれば false
inline bool diffBoards(const QString& prevSfen, const QString& currSfen, DiffResult& out)
{
    out = DiffResult{};
    if (prevSfen.isEmpty() || currSfen.isEmpty()) return false;

    ShogiBoard prev;
    ShogiBoard curr;
    if (!prev.trySetSfen(SfenUtils::normalizePositionLikeSfen(prevSfen))
        || !curr.trySetSfen(SfenUtils::normalizePositionLikeSfen(currSfen))) {
        return false;
    }

    for (int rank = 1; rank <= 9; ++rank) {
        for (int file = 1; file <= 9; ++file) {
            const Piece before = prev.pieceAt(BoardSquare{file, rank});
            const Piece after = curr.pieceAt(BoardSquare{file, rank});
            if (before == after) continue;

            const QPoint square(file - 1, rank - 1);
            if (after == Piece::None) {
                ++out.emptyCount;
                out.from = square;
            } else if (before == Piece::None) {
                ++out.filledCount;
                out.to = square;
                out.droppedPiece = demote(after);
            } else {
                ++out.changedCount;
                out.to = square;
            }
        }
    }

    return out.hasTo();
}

} // namespace SfenDiffUtils

#endif // SFENDIFFUTILS_H
