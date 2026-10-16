#ifndef KIFUPRESENTATION_H
#define KIFUPRESENTATION_H

/// @file kifupresentation.h
/// @brief 画面に出す棋譜の表記（指し手・読み筋・終局語・対局情報の見出しと値）を表示言語と表記の設定に合わせる

#include <QString>
#include "notationstyle.h"
#include "shogimove.h"

// Presentation only. Never use these strings in a file writer or a protocol.
// 駒名・筋・段の表は core の ShogiNotation、日本語の指し手は KifMoveText、英語表記の移動元の要否は
// KI2 と同じ判定（Ki2MoveNotation::annotate の sources）、手合割の名前は core の Handicaps の表を使う。
namespace KifuPresentation {
using Notation = NotationStyle::Notation;
using Options = NotationStyle::Options;

inline QString resolveLanguage(const QString& setting, const QString& systemLocale)
{
    return NotationStyle::resolveLanguage(setting, systemLocale);
}
inline void configure(const QString& language, const QString& notation, bool alwaysOrigin)
{
    NotationStyle::configure(language, notation, alwaysOrigin);
}
inline Options options() { return NotationStyle::options(); }
/// 表記の設定（configure）を変えるたびに増える番号。表示用の文字列を覚えた側は、番号が変わったら作り直す
inline int revision() { return NotationStyle::revision(); }
QString rankLabel(int rank);
QString usiMove(const ShogiMove& move);
QString move(const QString& beforeSfen, const QString& usi, Options style,
             bool includeSide = true);
QString pv(const QString& beforeSfen, const QString& usiMoves, const QString& japanese,
           bool fullOrigin = false);
QString label(const QString& canonical, const QString& beforeSfen = {},
              const QString& usi = {}, bool fullOrigin = false);
/// 1手の表記（手数・分岐の印を含まない）。日本語表記なら棋譜の表記（終局語は表示言語に合わせる）、
/// 英語表記なら指す前の局面と指し手から作る
QString moveLabel(const QString& canonical, const QString& beforeSfen, const QString& usi, bool fullOrigin = false);
/// 分岐がある手に付ける印（日本語表記は "+"、英語表記は " [+]"）
QString branchMark();

QString status(const QString& canonical);
QString infoKey(const QString& canonical);
QString infoValue(const QString& key, const QString& raw);
}

#endif
