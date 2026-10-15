#ifndef PARSEMOVEFORMAT_H
#define PARSEMOVEFORMAT_H

/// @file parsemoveformat.h
/// @brief USI/USEN共通の指し手フォーマット変換関数の定義

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

#include "terminaltype.h"

struct KifDisplayItem;

namespace KifuParseCommon {

// 任意の開始手番と、分岐内の実手数から終局側を決める。
KifDisplayItem createUsiTerminalDisplayItem(int ply, TerminalType terminal,
                                           const QString& baseSfen, qsizetype moveCount);

} // namespace KifuParseCommon

#endif // PARSEMOVEFORMAT_H
