#ifndef RECORDLINE_H
#define RECORDLINE_H

/// @file recordline.h
/// @brief 1本の手順（開始局面と、指した手の行の並び。末尾に終局の行を持てる）

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "recordentry.h"
#include "usimove.h"
#include "usiposition.h"

/**
 * @brief 1本の手順
 *
 * 開始局面と、指した手の行（RecordEntry。各行はその手の後の局面を持つ）の並び。末尾に終局の行を1つ持てる。
 * 棋譜の読み込みで作る本譜・変化、分岐ツリーで表示中の手順のスナップショット（対局の開始・検討・解析に渡す）、
 * 対局中の手順（司令塔が持つ）のすべてに使う。手数の引数（ply）は手順の最初からの手数で、
 * startPly が 1 の手順（本譜・表示中の手順・対局の手順）では棋譜の手数と同じ。
 */
struct RecordLine {
    QString startSfen;          ///< 開始局面（完全な SFEN）
    int startPly = 1;           ///< 最初の手の手数（変化は分岐した手の手数）
    QString openingComment;     ///< 開始局面のコメント（本譜だけ）
    QString openingBookmark;    ///< 開始局面のしおり（本譜だけ）
    QList<RecordEntry> entries; ///< entries[i] は i+1 手目。末尾は終局の行のことがある
    int currentPly = 0;         ///< 表示中の手数（0 は開始局面）

    bool isEmpty() const { return startSfen.isEmpty(); }
    bool endsWithTerminal() const { return !entries.isEmpty() && entries.last().isTerminal(); }
    /// 終局の行（なければ nullptr）
    const RecordEntry* terminal() const { return endsWithTerminal() ? &entries.last() : nullptr; }
    /// 指した手の数（終局の行は数えない）
    int moveCount() const { return static_cast<int>(entries.size()) - (endsWithTerminal() ? 1 : 0); }

    /// ply 手目の後の局面（0 は開始局面。範囲外は空）
    QString sfenAt(int ply) const
    {
        if (ply == 0) return startSfen;
        return (ply >= 1 && ply <= moveCount()) ? entries.at(ply - 1).sfen : QString();
    }
    /// 最後の手の後の局面（手が無ければ開始局面）
    QString lastSfen() const { return sfenAt(moveCount()); }

    /// 開始局面と各手の後の局面（moveCount() + 1 個）
    QStringList sfens() const
    {
        QStringList out;
        if (isEmpty()) return out;
        out.reserve(moveCount() + 1);
        out << startSfen;
        for (int ply = 1; ply <= moveCount(); ++ply) out << entries.at(ply - 1).sfen;
        return out;
    }
    QList<ShogiMove> moves() const
    {
        QList<ShogiMove> out;
        out.reserve(moveCount());
        for (int ply = 1; ply <= moveCount(); ++ply) out << entries.at(ply - 1).move;
        return out;
    }
    QString displayText(int ply) const
    {
        return (ply >= 1 && ply <= moveCount()) ? entries.at(ply - 1).displayText : QString();
    }
    UsiMove usiMove(int ply) const
    {
        return (ply >= 1 && ply <= moveCount()) ? entries.at(ply - 1).move.move : UsiMove{};
    }
    QStringList usiMoves(int upToPly = -1) const
    {
        const int last = (upToPly < 0) ? moveCount() : qMin(upToPly, moveCount());
        QStringList out;
        out.reserve(last);
        for (int ply = 1; ply <= last; ++ply) out << usiMove(ply).toString();
        return out;
    }
    BoardSquare destinationOf(int ply) const { return usiMove(ply).to; }

    /// ply 手目までの手順（終局の行は含めない。currentPly も ply に合わせる）
    RecordLine upTo(int ply) const
    {
        RecordLine out = *this;
        out.entries = entries.mid(0, qBound(0, ply, moveCount()));
        out.currentPly = out.moveCount();
        return out;
    }

    /// ply 手目の後の局面を、開始局面からの手順としてエンジンに送る値（手順が無い・局面を読めなければ nullopt）
    std::optional<UsiPosition> usiPosition(int ply) const
    {
        if (isEmpty()) return std::nullopt;
        const int last = qBound(0, ply, moveCount());
        QList<UsiMove> played;
        played.reserve(last);
        for (int i = 1; i <= last; ++i) played << usiMove(i);
        return UsiPosition::fromMoves(startSfen, played);
    }

    /**
     * @brief fromPly〜toPly 手目の後の各局面を、エンジンに送る値にする（[0] が fromPly。範囲は手順に収める）
     *
     * 手順を1度だけたどり、開始局面からの手順で送る（千日手の判断材料をエンジンに渡す。手の列は全部の局面で
     * 共有する）。手順をたどれない（指せない手がある）ときは各局面を局面だけで送り、読めない局面は nullopt。
     * 棋譜解析（GUI）と CLI の analyze-kifu で同じ局面を送るのに使う。
     */
    QList<std::optional<UsiPosition>> usiPositions(int fromPly, int toPly) const
    {
        QList<std::optional<UsiPosition>> positions;
        if (isEmpty()) return positions;
        const int last = qBound(0, toPly, moveCount());
        const int first = qBound(0, fromPly, last);
        positions.reserve(last - first + 1);
        QList<UsiMove> played;
        played.reserve(last);
        for (int ply = 1; ply <= last; ++ply) played << usiMove(ply);
        if (const std::optional<QList<UsiPosition>> along = UsiPosition::alongMoves(startSfen, played, first, last)) {
            for (const UsiPosition& position : *along) positions << position;
            return positions;
        }
        for (int ply = first; ply <= last; ++ply) {
            const std::optional<UsiMove> lastMove = ply > 0 ? std::optional<UsiMove>(played.at(ply - 1)) : std::nullopt;
            positions << UsiPosition::fromSfen(sfenAt(ply), lastMove);
        }
        return positions;
    }
};

#endif // RECORDLINE_H
