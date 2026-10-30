#ifndef GAMEINFOKEYS_H
#define GAMEINFOKEYS_H

/// @file gameinfokeys.h
/// @brief 対局情報の見出し（KIF の「見出し：値」の見出し）の語

#include <QString>
#include <QtGlobal>

namespace GameInfoKeys {

// 見出しはファイルに書く語なので、値は訳さない。画面では KifuPresentation::infoKey が kTranslated の見出しを訳す
// （訳す見出しは QT_TRANSLATE_NOOP で訳の一覧に載せる）
inline const QString kGameDate = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "対局日"));
inline const QString kStartDateTime = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "開始日時"));
inline const QString kEndDateTime = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "終了日時"));
inline const QString kBlackPlayer = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "先手"));
inline const QString kWhitePlayer = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "後手"));
// 駒落ちの対局者（柿木形式の KIF と同じく、下手が先に並ぶ先手側、上手が後手側）
inline const QString kShitatePlayer = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "下手"));
inline const QString kUwatePlayer = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "上手"));
// 柿木形式の KIF の省略名（対局者名が無いときに盤の名前に使う）
inline const QString kBlackShortName = QStringLiteral("先手省略名");
inline const QString kWhiteShortName = QStringLiteral("後手省略名");

inline const QString kHandicap = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "手合割"));
/// 手合割の値「その他」（駒落ちの初期配置でない局面。局面図で開始局面を書く）
inline const QString kHandicapOther = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "その他"));
inline const QString kConsumedTime = QStringLiteral("消費時間");
inline const QString kResult = QStringLiteral("結果");
inline const QString kTimeControl = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "持ち時間"));
inline const QString kEvent = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "棋戦"));
inline const QString kSite = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "場所"));
inline const QString kNote = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "備考"));
inline const QString kOpening = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "戦型"));
inline const QString kTitle = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "表題"));
inline const QString kMaxMoves = QStringLiteral("最大手数");
inline const QString kJishogi = QStringLiteral("持将棋");
inline const QString kJishogiPoints = QStringLiteral("持将棋点数");

// CSA 形式の棋譜から読んだ項目（$TIME・$TIME+・$TIME- の「秒+秒読み+加算」と V 行）
inline const QString kCsaTime = QStringLiteral("持ち時間(秒/加算)");
inline const QString kCsaBlackTime = QStringLiteral("先手:持ち時間(秒/加算)");
inline const QString kCsaWhiteTime = QStringLiteral("後手:持ち時間(秒/加算)");
inline const QString kCsaVersion = QStringLiteral("バージョン");

/// 画面で訳す見出し（KifuPresentation::infoKey が引く）
inline const QString* const kTranslated[] = {&kGameDate, &kStartDateTime, &kEndDateTime, &kBlackPlayer,
                                             &kWhitePlayer, &kShitatePlayer, &kUwatePlayer, &kHandicap,
                                             &kTimeControl, &kEvent, &kSite, &kNote, &kOpening, &kTitle};

} // namespace GameInfoKeys

#endif // GAMEINFOKEYS_H
