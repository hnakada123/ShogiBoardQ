#ifndef MATCHPLAYERS_H
#define MATCHPLAYERS_H

/// @file matchplayers.h
/// @brief 対局者（先手・後手を人間とエンジンのどちらが指すかと、その名前）

#include <QMetaType>
#include <QString>
#include <array>
#include <utility>

#include "shogitypes.h"

/**
 * @brief 対局者（先手・後手を人間とエンジンのどちらが指すかと、その名前）
 *
 * 駒落ちでは先手が下手、後手が上手。名前は対局開始ダイアログで選んだ人間の名前とエンジンの名前を
 * 両方持ち、手番ごとに指す側の名前を name() で返す。1人分の値は seat(Turn) で引く。
 */
struct MatchPlayers {
    /// 対局者1人（先手か後手）
    struct Seat {
        bool engine = false;   ///< エンジンが指す
        QString humanName;     ///< 人間の名前
        QString engineName;    ///< エンジンの名前

        /// 指す側の名前（エンジンならエンジンの名前）。未入力なら空
        QString name() const { return engine ? engineName : humanName; }

        friend bool operator==(const Seat& a, const Seat& b)
        {
            return a.engine == b.engine && a.humanName == b.humanName && a.engineName == b.engineName;
        }
        friend bool operator!=(const Seat& a, const Seat& b) { return !(a == b); }
    };

    std::array<Seat, 2> seats;   ///< [0] が先手（下手）、[1] が後手（上手）

    Seat& seat(Turn side) { return seats[side == Turn::Black ? 0 : 1]; }
    const Seat& seat(Turn side) const { return seats[side == Turn::Black ? 0 : 1]; }

    bool isEngine(Turn side) const { return seat(side).engine; }
    bool isHuman(Turn side) const { return !isEngine(side); }
    bool hasEngine() const { return seats[0].engine || seats[1].engine; }
    bool hasHuman() const { return !seats[0].engine || !seats[1].engine; }
    bool engineVsEngine() const { return seats[0].engine && seats[1].engine; }
    bool humanVsHuman() const { return !seats[0].engine && !seats[1].engine; }

    QString humanName(Turn side) const { return seat(side).humanName; }
    QString engineName(Turn side) const { return seat(side).engineName; }
    /// side を指す側の名前（エンジンならエンジンの名前）。未入力なら空
    QString name(Turn side) const { return seat(side).name(); }

    /// 先手と後手を入れ替える（連続対局で1局ごとに手番を替える）
    void swapSides() { std::swap(seats[0], seats[1]); }

    friend bool operator==(const MatchPlayers& a, const MatchPlayers& b) { return a.seats == b.seats; }
    friend bool operator!=(const MatchPlayers& a, const MatchPlayers& b) { return !(a == b); }
};
Q_DECLARE_METATYPE(MatchPlayers)

#endif // MATCHPLAYERS_H
