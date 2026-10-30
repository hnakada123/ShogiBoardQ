#ifndef GAMEINFO_H
#define GAMEINFO_H

/// @file gameinfo.h
/// @brief 対局情報（KIF の「見出し：値」の並び）の規則

#include <QList>
#include <QString>
#include <optional>

#include "kifparsetypes.h"
#include "shogitypes.h"

/**
 * @brief 対局情報の規則（対局者の見出し・手合割の値）
 *
 * 対局情報は KIF の見出し（GameInfoKeys）と値の並びで、ファイルに書く語のまま持つ（表示は KifuPresentation が訳す）。
 * 棋譜の文書・書き出し・対局情報の表・対局で、見出しの判定と手合割の値をここで決める。
 */
namespace GameInfo {

/// 開始局面（空・startpos は平手）の手合割の値。平手・駒落ちの初期配置は手合割の名前、それ以外は「その他」
QString handicapLabel(const QString& startSfen);
/// 開始局面が駒落ちの初期配置か（平手・その他は false）
bool isHandicap(const QString& startSfen);

/// 対局者の見出し。handicapNames なら柿木形式の KIF と同じく下手（先手側）・上手（後手側）、それ以外は先手・後手
const QString& playerKey(Turn side, bool handicapNames);
/// 見出し（前後の空白は無視する）が対局者（先手・後手・下手・上手）なら、その側
std::optional<Turn> playerSide(const QString& key);
/// 対局情報の対局者名（先手・後手。駒落ちは下手・上手。なければ省略名、それもなければ空）
QString playerName(const QList<KifGameInfoItem>& header, Turn side);
/// 対局者を下手・上手と呼ぶか（書き出す棋譜の見出し・局面図・終局行と、持ち時間の先後の書き分け）。
/// 対局情報の見出し（下手・上手か先手・後手か）に合わせ、見出しに対局者がなければ開始局面の手合割で決める
bool usesHandicapNames(const QList<KifGameInfoItem>& header, const QString& startSfen);

} // namespace GameInfo

#endif // GAMEINFO_H
