#ifndef RECORDLINE_H
#define RECORDLINE_H

/// @file recordline.h
/// @brief 1本の手順（開始局面と、指した手の行の並び）

#include <QList>
#include <QString>
#include <QStringList>

#include "recordentry.h"
#include "sfenutils.h"
#include "usimove.h"

/**
 * @brief 1本の手順
 *
 * 開始局面と、指した手の行（RecordEntry。各行はその手の後の局面を持つ）の並び。終局の行は含めない。
 * 分岐ツリーで表示中の手順のスナップショット（対局の開始・検討・解析に渡す）と、
 * 対局中の手順（司令塔が持つ）の両方に使う。
 */
struct RecordLine {
    QString startSfen;          ///< 開始局面（完全な SFEN）
    QList<RecordEntry> entries; ///< entries[i] は i+1 手目
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
    int moveCount() const { return static_cast<int>(entries.size()); }

    /// ply 手目の後の局面（0 は開始局面。範囲外は空）
    QString sfenAt(int ply) const
    {
        if (ply == 0) return startSfen;
        return (ply >= 1 && ply <= moveCount()) ? entries.at(ply - 1).sfen : QString();
    }
    /// 最後の局面（手が無ければ開始局面）
    QString lastSfen() const { return entries.isEmpty() ? startSfen : entries.last().sfen; }

    /// 開始局面と各手の後の局面（moveCount() + 1 個）
    QStringList sfens() const
    {
        QStringList out;
        if (isEmpty()) return out;
        out.reserve(entries.size() + 1);
        out << startSfen;
        for (const RecordEntry& entry : entries) out << entry.sfen;
        return out;
    }
    QList<ShogiMove> moves() const
    {
        QList<ShogiMove> out;
        out.reserve(entries.size());
        for (const RecordEntry& entry : entries) out << entry.move;
        return out;
    }
    QStringList displayTexts() const
    {
        QStringList out;
        out.reserve(entries.size());
        for (const RecordEntry& entry : entries) out << entry.displayText;
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

    /// ply 手目までの手順（currentPly も ply に合わせる）
    RecordLine upTo(int ply) const
    {
        RecordLine out;
        out.startSfen = startSfen;
        out.entries = entries.mid(0, qBound(0, ply, moveCount()));
        out.currentPly = out.moveCount();
        return out;
    }

    QString positionCommand(int ply) const
    {
        if (isEmpty()) return {};
        const QString initial = SfenUtils::normalizePositionLikeSfen(startSfen);
        const QString start = initial == SfenUtils::hirateSfen()
            ? QStringLiteral("position startpos")
            : QStringLiteral("position sfen ") + initial;
        const QStringList played = usiMoves(qMax(0, ply));
        return played.isEmpty() ? start : start + QStringLiteral(" moves ") + played.join(QLatin1Char(' '));
    }
};

#endif // RECORDLINE_H
