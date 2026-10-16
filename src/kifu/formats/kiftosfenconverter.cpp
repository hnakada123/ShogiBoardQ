/// @file kiftosfenconverter.cpp
/// @brief KIF形式棋譜コンバータクラスの実装

#include "kiftosfenconverter.h"

#include "kiftextparser.h"

KifuParseOutcome KifToSfenConverter::parse(QStringView text, const ParseOptions& options)
{
    return KifTextParser::parse(text, KifTextParser::Dialect::Kif, options);
}
