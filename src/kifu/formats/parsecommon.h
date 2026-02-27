#ifndef PARSECOMMON_H
#define PARSECOMMON_H

#include <QChar>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <array>
#include <optional>
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
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

// KIF/KI2共通：コメント行判定（先頭が '*' or '＊'）
bool isKifCommentLine(const QString& s);

// KIF/KI2共通：しおり行判定（先頭が '&'）
bool isBookmarkLine(const QString& s);

// CSA/JKF共通：CSA形式終局コード→日本語ラベル
// %プレフィックスの有無を自動判別（%TORYO / TORYO 両対応）
// マッチしない場合は入力文字列をそのまま返す
QString csaSpecialToJapanese(const QString& code);

// CSA/JKF共通：mm:ss形式の時間フォーマット（ミリ秒入力）
QString formatTimeMS(qint64 ms);

// CSA/JKF共通：HH:MM:SS形式の時間フォーマット（ミリ秒入力）
QString formatTimeHMS(qint64 ms);

// CSA/JKF共通：mm:ss/HH:MM:SS形式の複合時間フォーマット
QString formatTimeText(qint64 moveMs, qint64 cumMs);

// KIF/KI2/JKF共通：extractGameInfo結果をQMap変換
QMap<QString, QString> toGameInfoMap(const QList<KifGameInfoItem>& items);

} // namespace KifuParseCommon

#endif // PARSECOMMON_H
