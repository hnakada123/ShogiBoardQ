#ifndef PARSECOMMON_H
#define PARSECOMMON_H

#include <QChar>
#include <QString>
#include <array>
#include <optional>
#include "shogitypes.h"

namespace KifuParseCommon {

int asciiDigitToInt(QChar c);
int zenkakuDigitToInt(QChar c);
int flexDigitToIntNoDetach(QChar c);
int flexDigitsToIntNoDetach(const QString& text);

const std::array<QString, 16>& terminalWords();
bool isTerminalWordExact(const QString& text, QString* normalized = nullptr);
bool isTerminalWordContains(const QString& text, QString* normalized = nullptr);

bool mapKanjiPiece(const QString& text, Piece& base, bool& promoted);

std::optional<int> parseFileChar(QChar ch);   // '1'〜'9' → 1〜9
std::optional<int> parseRankChar(QChar ch);   // 'a'〜'i' → 1〜9
std::optional<int> parseDigit(QChar ch);      // '0'〜'9' → 0〜9

} // namespace KifuParseCommon

#endif // PARSECOMMON_H
