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
// 柿木形式の KIF の省略名（対局者名が無いときに盤の名前に使う）
inline const QString kBlackShortName = QStringLiteral("先手省略名");
inline const QString kWhiteShortName = QStringLiteral("後手省略名");

inline const QString kHandicap = QStringLiteral("手合割");
inline const QString kTimeControl = QStringLiteral("持ち時間");
inline const QString kEvent = QStringLiteral("棋戦");
inline const QString kSite = QStringLiteral("場所");
inline const QString kNote = QStringLiteral("備考");
inline const QString kOpening = QStringLiteral("戦型");
inline const QString kMaxMoves = QStringLiteral("最大手数");
inline const QString kJishogi = QStringLiteral("持将棋");
inline const QString kJishogiPoints = QStringLiteral("持将棋点数");

// CSA 形式の棋譜から読んだ項目（$TIME・$TIME+・$TIME- の「秒+秒読み+加算」と V 行）
inline const QString kCsaTime = QStringLiteral("持ち時間(秒/加算)");
inline const QString kCsaBlackTime = QStringLiteral("先手:持ち時間(秒/加算)");
inline const QString kCsaWhiteTime = QStringLiteral("後手:持ち時間(秒/加算)");
inline const QString kCsaVersion = QStringLiteral("バージョン");

} // namespace GameInfoKeys

#endif // GAMEINFOKEYS_H
