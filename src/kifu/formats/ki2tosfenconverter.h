#ifndef KI2TOSFENCONVERTER_H
#define KI2TOSFENCONVERTER_H

/// @file ki2tosfenconverter.h
/// @brief KI2形式棋譜コンバータクラスの定義

#include <QStringView>

#include "kifparsetypes.h"

/**
 * @brief KI2形式の棋譜を読む
 *
 * KI2 は移動元を書かないので、局面を追いながら合法手の中から指し手を決める（Ki2MoveNotation）。
 * それ以外の行の読み方は KIF と同じ（KifTextParser）。
 */
class Ki2ToSfenConverter
{
public:
    /// 棋譜を読む。手合割の名前は局面図なら "BOD"、手合割の行がなければ "平手(既定)"
    static KifuParseOutcome parse(QStringView text, const ParseOptions& options = {});
};

#endif // KI2TOSFENCONVERTER_H
