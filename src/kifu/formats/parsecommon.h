#ifndef PARSECOMMON_H
#define PARSECOMMON_H

#include <QChar>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <functional>
#include "kifparsetypes.h"

namespace KifuParseCommon {

// 改行区切りで追記（buf が空でなければ '\n' を挿入してから text を追加）
void appendLine(QString& buf, const QString& text);

// KIF/KI2共通：盤面枠行・ヘッダ行を判定（BODの数字行、罫線行、持駒見出し等）
bool isBoardHeaderOrFrame(QStringView line);

// KIF/KI2共通：スキップ可能なヘッダ行かどうか判定
bool isKifSkippableHeaderLine(QStringView line);

// KIF/KI2共通：コメント行判定（先頭が '*' or '＊'）
bool isKifCommentLine(QStringView s);

// KIF/KI2共通：しおり行判定（先頭が '&'）
bool isBookmarkLine(QStringView s);

// KIF/KI2共通：BOD持駒行判定（先手の持駒/後手の持駒/持ち駒）
bool isBodHandsLine(QStringView line);

// KIF/KI2共通：ヘッダ行から対局情報を抽出
// isMoveLine: 指し手行を検出したら true を返す（形式により判定方法が異なる）
QList<KifGameInfoItem> extractHeaderGameInfo(
    const QStringList& lines,
    const std::function<bool(const QString&)>& isMoveLine);

} // namespace KifuParseCommon

#endif // PARSECOMMON_H
