#ifndef RECORDLINE_H
#define RECORDLINE_H

/// @file recordline.h
/// @brief 分岐ツリーの1本の手順を、ほかの機能が使う形にした値

#include <QList>
#include <QString>
#include <QStringList>

#include "sfenutils.h"
#include "shogimove.h"
#include "usimove.h"

/**
 * @brief 分岐ツリー（棋譜の正本）の1本の手順
 *
 * 検討・詰み探索・棋譜解析・定跡のマージ・読み筋の表示・書き出しは、局面列や指し手列を
 * 別々の一覧から探さずに、この値（KifuNavigationState::currentRecordLine）から取る。
 * 終局行（投了など）は含まない。
 */
struct RecordLine {
    QStringList sfens;          ///< 開始局面（[0]）と各手の後の局面
    QList<ShogiMove> moves;     ///< 各手（moves[i] は i+1 手目）
    QStringList displayTexts;   ///< 各手の棋譜の表記（"▲７六歩(77)"）
    int currentPly = 0;         ///< 表示中の手数（0 は開始局面）

    int moveCount() const { return static_cast<int>(moves.size()); }
    bool isEmpty() const { return sfens.isEmpty(); }
    QString startSfen() const { return sfens.value(0); }

    /// ply 手目（1始まり）の指し手。範囲外は無効な値
    UsiMove usiMove(int ply) const
    {
        return (ply >= 1 && ply <= moves.size()) ? moves.at(ply - 1).toUsiMove() : UsiMove{};
    }

    /// 開始局面から upToPly 手目までの USI の指し手（-1 なら最後まで）
    QStringList usiMoves(int upToPly = -1) const
    {
        const int last = (upToPly < 0) ? moveCount() : qMin(upToPly, moveCount());
        QStringList out;
        out.reserve(last);
        for (int ply = 1; ply <= last; ++ply) out << usiMove(ply).toString();
        return out;
    }

    /// ply 手目の移動先（ply 0 や範囲外は無効なマス）
    BoardSquare destinationOf(int ply) const { return usiMove(ply).to; }

    /// ply 手目の後の局面を表す USI の position コマンド（開始局面が平手なら startpos）
    QString positionCommand(int ply) const
    {
        if (isEmpty()) return {};
        const QString initial = SfenUtils::normalizePositionLikeSfen(startSfen());
        const QString start = initial == SfenUtils::hirateSfen()
            ? QStringLiteral("position startpos")
            : QStringLiteral("position sfen ") + initial;
        const QStringList played = usiMoves(qMax(0, ply));
        return played.isEmpty() ? start : start + QStringLiteral(" moves ") + played.join(QLatin1Char(' '));
    }
};

#endif // RECORDLINE_H
