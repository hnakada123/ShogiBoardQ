/// @file ki2tosfenconverter.cpp
/// @brief KI2形式棋譜コンバータクラスの実装

#include "ki2tosfenconverter.h"

#include "kiftextparser.h"

KifuParseOutcome Ki2ToSfenConverter::parse(QStringView text, const ParseOptions& options)
{
    return KifTextParser::parse(text, KifTextParser::Dialect::Ki2, options);
}
