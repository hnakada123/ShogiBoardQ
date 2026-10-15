/// @file parsemoveformat.cpp
/// @brief USI/USEN共通の指し手フォーマット変換関数の実装

#include "parsemoveformat.h"
#include "parsecommon.h"
#include "kifmovetext.h"
#include "sfenpositiontracer.h"
#include "usimove.h"

namespace KifuParseCommon {

KifDisplayItem createUsiTerminalDisplayItem(int ply, const QString& term,
                                           const QString& baseSfen, qsizetype moveCount)
{
    const bool blackToMove = (moveCount % 2 == 0) != baseSfen.contains(QStringLiteral(" w "));
    auto item = createTerminalDisplayItem(ply, term);
    item.prettyMove = (blackToMove ? QStringLiteral("▲") : QStringLiteral("△")) + term;
    return item;
}

} // namespace KifuParseCommon
