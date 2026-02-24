#ifndef PARSECOMMON_H
#define PARSECOMMON_H

#include <QChar>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <array>
#include <optional>
#include "kifdisplayitem.h"
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

// 改行区切りで追記（buf が空でなければ '\n' を挿入してから text を追加）
void appendLine(QString& buf, const QString& text);

// 直前の手（items.last()）にコメントを付与し buf をクリア
// items.size() > minSize のときのみ付与する
void flushCommentToLastItem(QString& buf, QList<KifDisplayItem>& items, int minSize = 1);

// 開始局面用 KifDisplayItem を生成
KifDisplayItem createOpeningDisplayItem(const QString& comment, const QString& bookmark);

// KIF/KI2共通：盤面枠行・ヘッダ行を判定（BODの数字行、罫線行、持駒見出し等）
bool isBoardHeaderOrFrame(const QString& line);

// KIF/KI2共通：終局語を含むか判定
bool containsAnyTerminal(const QString& s, QString* matched = nullptr);

// KIF/KI2共通：スキップ可能なヘッダ行かどうか判定
bool isKifSkippableHeaderLine(const QString& line);

} // namespace KifuParseCommon

#endif // PARSECOMMON_H
