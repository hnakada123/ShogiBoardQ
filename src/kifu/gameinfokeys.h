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
inline const QString kHandicap = QStringLiteral("手合割");
inline const QString kTimeControl = QStringLiteral("持ち時間");

} // namespace GameInfoKeys

#endif // GAMEINFOKEYS_H
