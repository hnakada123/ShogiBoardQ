#ifndef RECORDLINE_H
#define RECORDLINE_H

/// @file recordline.h
/// @brief 1本の手順（開始局面と、指した手の行の並び。末尾に終局の行を持てる）

#include <QList>
#include <QString>
#include <QStringList>

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

    /// 局面列（[0] が開始局面、[i] が i 手目の後）と指し手列から作る。
    /// 指し手が局面より多ければ、局面のある手までにする
    static RecordLine fromPositions(const QStringList& sfens, const QList<ShogiMove>& moves)
    {
        RecordLine line;
        line.startSfen = sfens.value(0);
        const qsizetype count = qMin(moves.size(), sfens.size() - 1);
        for (qsizetype i = 0; i < count; ++i)
            line.entries << RecordEntry::played(moves.at(i), {}, sfens.at(i + 1));
        line.currentPly = line.moveCount();
        return line;
    }

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
    QStringList displayTexts() const
    {
        QStringList out;
        out.reserve(moveCount());
        for (int ply = 1; ply <= moveCount(); ++ply) out << entries.at(ply - 1).displayText;
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

    /// ply 手目の後の局面を、開始局面からの手順としてエンジンに送る値（手順が無ければ空）
    UsiPosition usiPosition(int ply) const
    {
        if (isEmpty()) return {};
        const int last = qBound(0, ply, moveCount());
        QList<UsiMove> played;
        played.reserve(last);
        for (int i = 1; i <= last; ++i) played << usiMove(i);
        return UsiPosition::fromMoves(startSfen, played).value_or(UsiPosition{});
    }
};

#endif // RECORDLINE_H
