#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

/// @file kiftosfenconverter.h
/// @brief KIF形式棋譜コンバータクラスの定義

#include <QStringView>

#include "kifparsetypes.h"

/**
 * @brief KIF形式の棋譜を読む
 *
 * 棋譜（本譜と各変化）・開始局面・手合割の名前・対局情報を1回の解析で返す。
 * 行の分類・変化の区切り・指し手の文法は KI2 と共有する（KifTextParser）。
 * ファイルは KifuLoadParser が文字コードを判別して読み、テキストを渡す。
 */
class KifToSfenConverter
{
public:
    /// 棋譜を読む。手合割の名前は局面図なら "BOD"、手合割の行がなければ "平手(既定)"
    static KifuParseOutcome parse(QStringView text, const ParseOptions& options = {});
};

#endif // KIFTOSFENCONVERTER_H
