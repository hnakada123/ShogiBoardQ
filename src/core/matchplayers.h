#ifndef MATCHPLAYERS_H
#define MATCHPLAYERS_H

/// @file matchplayers.h
/// @brief 対局者（先手・後手を人間とエンジンのどちらが指すかと、その名前）

#include <QMetaType>
#include <QString>

#include "shogitypes.h"

/**
 * @brief 対局者（先手・後手を人間とエンジンのどちらが指すかと、その名前）
 *
 * 駒落ちでは先手が下手、後手が上手。名前は対局開始ダイアログで選んだ人間の名前とエンジンの名前を
 * 両方持ち、手番ごとに指す側の名前を name() で返す。
 */
struct MatchPlayers {
    bool blackIsEngine = false;  ///< 先手（下手）をエンジンが指す
    bool whiteIsEngine = false;  ///< 後手（上手）をエンジンが指す
    QString blackHumanName;      ///< 先手の人間の名前
    QString whiteHumanName;      ///< 後手の人間の名前
    QString blackEngineName;     ///< 先手のエンジンの名前
    QString whiteEngineName;     ///< 後手のエンジンの名前

    bool isEngine(Turn side) const { return side == Turn::Black ? blackIsEngine : whiteIsEngine; }
    bool isHuman(Turn side) const { return !isEngine(side); }
    bool hasEngine() const { return blackIsEngine || whiteIsEngine; }
    bool hasHuman() const { return !blackIsEngine || !whiteIsEngine; }
    bool engineVsEngine() const { return blackIsEngine && whiteIsEngine; }
    bool humanVsHuman() const { return !blackIsEngine && !whiteIsEngine; }

    QString humanName(Turn side) const { return side == Turn::Black ? blackHumanName : whiteHumanName; }
    QString engineName(Turn side) const { return side == Turn::Black ? blackEngineName : whiteEngineName; }
    /// side を指す側の名前（エンジンならエンジンの名前）。未入力なら空
    QString name(Turn side) const { return isEngine(side) ? engineName(side) : humanName(side); }

    friend bool operator==(const MatchPlayers& a, const MatchPlayers& b)
    {
        return a.blackIsEngine == b.blackIsEngine && a.whiteIsEngine == b.whiteIsEngine
            && a.blackHumanName == b.blackHumanName && a.whiteHumanName == b.whiteHumanName
            && a.blackEngineName == b.blackEngineName && a.whiteEngineName == b.whiteEngineName;
    }
    friend bool operator!=(const MatchPlayers& a, const MatchPlayers& b) { return !(a == b); }
};
Q_DECLARE_METATYPE(MatchPlayers)

#endif // MATCHPLAYERS_H
