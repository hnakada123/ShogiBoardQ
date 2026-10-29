/// @file kifuexportmetadata.cpp
/// @brief 棋譜形式間で共有する対局情報と対局者名の生成
#include "kifuexportmetadata.h"
#include "gameinfokeys.h"
#include "handicaps.h"

QString KifuExportMetadataBuilder::handicapLabel(const QString& startSfen)
{
    const QString trimmed = startSfen.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("startpos")) return QStringLiteral("平手");

    // 駒落ちの初期配置（持駒なし・正しい手番）なら手合割名を使う。
    // それ以外は局面図で表す「その他」にする。
    if (const Handicaps::Handicap* handicap = Handicaps::byBoard(trimmed)) {
        const QString preset = QString::fromLatin1(handicap->sfen);
        const QString hands = trimmed.section(QLatin1Char(' '), 2, 2);
        const QString turn = trimmed.section(QLatin1Char(' '), 1, 1);
        if ((hands.isEmpty() || hands == QLatin1String("-"))
            && (turn.isEmpty() || turn == preset.section(QLatin1Char(' '), 1, 1))) {
            return Handicaps::labelOf(*handicap);
        }
    }
    return GameInfoKeys::kHandicapOther;
}

bool KifuExportMetadataBuilder::isHandicap(const QString& handicapLabel)
{
    const QString label = handicapLabel.trimmed();
    return !label.isEmpty() && label != QStringLiteral("平手") && label != GameInfoKeys::kHandicapOther;
}

QString KifuExportMetadataBuilder::blackPlayerKey(const QString& handicapLabel)
{
    return isHandicap(handicapLabel) ? GameInfoKeys::kShitatePlayer : GameInfoKeys::kBlackPlayer;
}

QString KifuExportMetadataBuilder::whitePlayerKey(const QString& handicapLabel)
{
    return isHandicap(handicapLabel) ? GameInfoKeys::kUwatePlayer : GameInfoKeys::kWhitePlayer;
}

QString KifuExportMetadataBuilder::playerName(const QList<KifGameInfoItem>& header, Turn side)
{
    const QString& fullKey = side == Turn::Black ? GameInfoKeys::kBlackPlayer : GameInfoKeys::kWhitePlayer;
    const QString& handicapKey = side == Turn::Black ? GameInfoKeys::kShitatePlayer : GameInfoKeys::kUwatePlayer;
    const QString& shortKey = side == Turn::Black ? GameInfoKeys::kBlackShortName : GameInfoKeys::kWhiteShortName;
    QString full, handicap, shortName;
    for (const auto& item : header) {
        const QString value = item.value.trimmed();
        if (item.key == fullKey) full = value;
        else if (item.key == handicapKey) handicap = value;
        else if (item.key == shortKey) shortName = value;
    }
    return !full.isEmpty() ? full : !handicap.isEmpty() ? handicap : shortName;
}

bool KifuExportMetadataBuilder::usesHandicapNames(const QList<KifGameInfoItem>& header, const QString& startSfen)
{
    // 読み込んだ棋譜の見出しが先手・後手なら、駒落ちでもその呼び方にそろえる（1つの棋譜で呼び方を混ぜない）
    for (const auto& item : header) {
        const QString key = item.key.trimmed();
        if (key == GameInfoKeys::kShitatePlayer || key == GameInfoKeys::kUwatePlayer) return true;
        if (key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kWhitePlayer) return false;
    }
    return isHandicap(handicapLabel(startSfen));
}
