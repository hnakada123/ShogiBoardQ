#ifndef KIFUIOSERVICE_H
#define KIFUIOSERVICE_H

/// @file kifuioservice.h
/// @brief 棋譜ファイルI/Oサービスの定義


#include <QString>
#include <QStringList>
#include <QDateTime>


namespace KifuIoService {

/// 保存ファイル名の既定値（日時_先手_vs_後手.拡張子。対局者の名前が無ければ 日時_Session.拡張子）。
/// 名前は対局情報の対局者名（駒落ちは下手・上手）を渡す
QString makeDefaultSaveFileName(const QString& blackName, const QString& whiteName, const QDateTime& now,
                                const QString& extension = QStringLiteral("kifu"));

/// Shift_JIS で表せない文字を、出現順・重複なしで最大 maxCount 個連結して返す。すべて表せる場合は空文字。
QString charactersNotInShiftJis(const QString& text, int maxCount = 10);

[[nodiscard]] bool writeKifuFile(const QString& filePath,
                   const QStringList& kifuLines,
                   QString* errorText,
                   bool useShiftJis = false);

} // namespace

#endif // KIFUIOSERVICE_H
