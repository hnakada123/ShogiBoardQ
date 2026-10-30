/// @file gameinfo.cpp
/// @brief 対局情報の規則の実装

#include "gameinfo.h"

#include "gameinfokeys.h"
#include "handicaps.h"

QString GameInfo::handicapLabel(const QString& startSfen)
{
    // 駒落ちの初期配置（持駒なし・上手の手番）は手合割の名前。それ以外は局面図で表す「その他」
    const Handicaps::Handicap* handicap = Handicaps::ofStart(startSfen);
    return handicap ? Handicaps::labelOf(*handicap) : GameInfoKeys::kHandicapOther;
}

bool GameInfo::isHandicap(const QString& startSfen)
{
    const Handicaps::Handicap* handicap = Handicaps::ofStart(startSfen);
    return handicap && handicap != &Handicaps::kHirate;
}

const QString& GameInfo::playerKey(Turn side, bool handicapNames)
{
    if (side == Turn::Black) return handicapNames ? GameInfoKeys::kShitatePlayer : GameInfoKeys::kBlackPlayer;
    return handicapNames ? GameInfoKeys::kUwatePlayer : GameInfoKeys::kWhitePlayer;
}

std::optional<Turn> GameInfo::playerSide(const QString& key)
{
    const QString trimmed = key.trimmed();
    for (const Turn side : {Turn::Black, Turn::White}) {
        if (trimmed == playerKey(side, false) || trimmed == playerKey(side, true)) return side;
    }
    return std::nullopt;
}

QString GameInfo::playerName(const QList<KifGameInfoItem>& header, Turn side)
{
    const QString& shortKey = side == Turn::Black ? GameInfoKeys::kBlackShortName : GameInfoKeys::kWhiteShortName;
    QString full, handicap, shortName;
    for (const auto& item : header) {
        const QString value = item.value.trimmed();
        if (item.key == playerKey(side, false)) full = value;
        else if (item.key == playerKey(side, true)) handicap = value;
        else if (item.key == shortKey) shortName = value;
    }
    return !full.isEmpty() ? full : !handicap.isEmpty() ? handicap : shortName;
}

bool GameInfo::usesHandicapNames(const QList<KifGameInfoItem>& header, const QString& startSfen)
{
    // 読み込んだ棋譜の見出しが先手・後手なら、駒落ちでもその呼び方にそろえる（1つの棋譜で呼び方を混ぜない）
    for (const auto& item : header) {
        if (const std::optional<Turn> side = playerSide(item.key))
            return item.key.trimmed() == playerKey(*side, true);
    }
    return isHandicap(startSfen);
}
