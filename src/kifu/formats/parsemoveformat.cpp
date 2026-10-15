/// @file parsemoveformat.cpp
/// @brief USI/USEN共通の指し手フォーマット変換関数の実装

#include "parsemoveformat.h"
#include "parsecommon.h"
#include "kifmovetext.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "usimove.h"

namespace KifuParseCommon {

KifDisplayItem createUsiTerminalDisplayItem(int ply, TerminalType terminal,
                                           const QString& baseSfen, qsizetype moveCount)
{
    const bool blackToMove = (moveCount % 2 == 0) != baseSfen.contains(QStringLiteral(" w "));
    return createTerminalDisplayItem(ply, terminal, blackToMove ? Turn::Black : Turn::White);
}

} // namespace KifuParseCommon
