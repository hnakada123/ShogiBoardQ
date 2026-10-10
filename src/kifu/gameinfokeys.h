#ifndef GAMEINFOKEYS_H
#define GAMEINFOKEYS_H

/// @file gameinfokeys.h
/// @brief 対局情報テーブルで使用する内部キー定数

#include <QString>

namespace GameInfoKeys {

// 対局情報テーブルのキーは永続化/出力処理でも参照されるため、翻訳しない内部定数として扱う。
inline const QString kGameDate = QStringLiteral("対局日");
inline const QString kStartDateTime = QStringLiteral("開始日時");
inline const QString kEndDateTime = QStringLiteral("終了日時");
inline const QString kBlackPlayer = QStringLiteral("先手");
inline const QString kWhitePlayer = QStringLiteral("後手");
// 駒落ちの対局者（柿木形式の KIF と同じく、下手が先に並ぶ先手側、上手が後手側）
inline const QString kShitatePlayer = QStringLiteral("下手");
inline const QString kUwatePlayer = QStringLiteral("上手");
inline const QString kHandicap = QStringLiteral("手合割");
inline const QString kTimeControl = QStringLiteral("持ち時間");
inline const QString kEvent = QStringLiteral("棋戦");
inline const QString kSite = QStringLiteral("場所");
inline const QString kNote = QStringLiteral("備考");

} // namespace GameInfoKeys

#endif // GAMEINFOKEYS_H
