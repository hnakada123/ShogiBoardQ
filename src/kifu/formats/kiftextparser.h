#ifndef KIFTEXTPARSER_H
#define KIFTEXTPARSER_H

/// @file kiftextparser.h
/// @brief KIF・KI2 のテキストを読む（行の分類・変化の区切り・指し手の文法を両形式で共有する）

#include <QStringView>

#include "kifparsetypes.h"

/**
 * @brief KIF・KI2 のテキストを読む
 *
 * 両形式は、ヘッダ・局面図・コメント（*）・しおり（&）・「変化：N手」の行の書き方が同じで、
 * 指し手も同じ文法（KifMoveGrammar）で読む。KIF の手は移動元 "(xy)" を書き、KI2 は書かない
 * （局面の合法手から決める）だけが違う。行の形も、手数で始まる KIF の行と、手番の印で始まる KI2 の行を
 * どちらの形式でも読む。形式で違うのは、終局を「まで77手で先手の勝ち」の行で書くか（KI2）と、
 * 対局情報の終わり（最初の指し手の行）の見分け方と、警告の見出しだけ。
 */
namespace KifTextParser {

enum class Dialect { Kif, Ki2 };

/// 棋譜を読む（本譜・変化・開始局面・手合割の名前・対局情報・警告）
KifuParseOutcome parse(QStringView text, Dialect dialect, const ParseOptions& options = {});

/// 局面図（BOD）だけのテキストを読む（局面を positionOnly に入れる）
KifuParseOutcome parseBod(QStringView text);

} // namespace KifTextParser

#endif // KIFTEXTPARSER_H
