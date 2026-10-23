#ifndef KIFUPRESENTATION_H
#define KIFUPRESENTATION_H

/// @file kifupresentation.h
/// @brief 画面に出す棋譜の表記（指し手・読み筋・終局語・対局情報の見出しと値）を表示言語と表記の設定に合わせる

#include <QString>
#include "notationstyle.h"
#include "shogimove.h"

class PvText;
struct RecordEntry;

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
/// 局面 beforeSfen の上の1手 usi の表記（style の表記。駒のない移動元や読めない手は usi のまま）
QString move(const QString& beforeSfen, const QString& usi, Options style,
             bool includeSide = true);
/**
 * @brief 読み筋の表記（fullOrigin なら英語表記で移動元をすべて付ける）
 *
 * 日本語表記は棋譜と同じ KIF の表記を続けて書き、同じマスへの手は「同」と書く（開始局面に至った手も見る）。
 * 英語表記は1手ずつ空白で区切る。終局の宣言（resign・win）は手番の印と終局語、末尾の注記はそのまま付ける。
 * 局面と合う手が1つも無ければ、エンジンが送った語のまま返す。
 */
QString pv(const PvText& line, bool fullOrigin = false);
/// 読み筋の ply 手目（1から）の表記（日本語表記は「同」も付ける）
QString pvMove(const PvText& line, int ply);
/// 1手の表記（手数・分岐の印は付けない）。日本語表記なら棋譜の表記 kifText、英語表記なら指す前の局面
/// beforeSfen と指し手 usi から作る（usi が無ければ kifText のまま）
QString moveLabel(const QString& kifText, const QString& beforeSfen, const QString& usi, bool fullOrigin = false);
/**
 * @brief 棋譜の1行（指した手・終局）の表記（手数・分岐の印は付けない）
 * @param beforeSfen 指す前の局面（英語表記で使う）
 *
 * 指した手は moveLabel と同じ。終局の行は手番の印と、表示言語に合わせた終局語（entry.terminal から作る）。
 * 指し手を読めなかった行は棋譜の表記のまま。
 */
QString entryLabel(const RecordEntry& entry, const QString& beforeSfen, bool fullOrigin = false);
/// 棋譜欄の1行（手数を4桁に右寄せして前に付け、分岐がある手には branchMark を付ける）
QString numberedLabel(int ply, const QString& label, bool branch);
/// 分岐がある手に付ける印（日本語表記は "+"、英語表記は " [+]"）
QString branchMark();
/// 棋譜解析で、読み筋の代わりに出す定跡の印（表示言語に合わせる）
QString bookLabel();

QString infoKey(const QString& canonical);
QString infoValue(const QString& key, const QString& raw);
}

#endif
