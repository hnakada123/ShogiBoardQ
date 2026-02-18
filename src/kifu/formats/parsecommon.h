#ifndef PARSECOMMON_H
#define PARSECOMMON_H

#include <QChar>
#include <QString>
#include <array>

namespace KifuParseCommon {

int asciiDigitToInt(QChar c);
int zenkakuDigitToInt(QChar c);
int flexDigitToIntNoDetach(QChar c);
int flexDigitsToIntNoDetach(const QString& text);

const std::array<QString, 16>& terminalWords();
bool isTerminalWordExact(const QString& text, QString* normalized = nullptr);
bool isTerminalWordContains(const QString& text, QString* normalized = nullptr);

bool mapKanjiPiece(const QString& text, QChar& base, bool& promoted);

} // namespace KifuParseCommon

#endif // PARSECOMMON_H
