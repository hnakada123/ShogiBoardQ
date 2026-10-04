#ifndef KIFUIOSERVICE_H
#define KIFUIOSERVICE_H

/// @file kifuioservice.h
/// @brief 棋譜ファイルI/Oサービスの定義


#include <QString>
#include <QStringList>
#include <QDateTime>

#include "playmode.h"

namespace KifuIoService {

QString makeDefaultSaveFileName(PlayMode mode,
                                const QString& human1,
                                const QString& human2,
                                const QString& engine1,
                                const QString& engine2,
                                const QDateTime& now,
                                const QString& extension = QStringLiteral("kifu"));

/// Shift_JIS で表せない文字を、出現順・重複なしで最大 maxCount 個連結して返す。すべて表せる場合は空文字。
QString charactersNotInShiftJis(const QString& text, int maxCount = 10);

[[nodiscard]] bool writeKifuFile(const QString& filePath,
                   const QStringList& kifuLines,
                   QString* errorText,
                   bool useShiftJis = false);

} // namespace

#endif // KIFUIOSERVICE_H
