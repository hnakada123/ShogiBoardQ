#ifndef BOARDCOLORPRESETS_H
#define BOARDCOLORPRESETS_H

/// @file boardcolorpresets.h
/// @brief 盤・駒台・背景のおすすめ配色（外観ダイアログの「詳細」の配色のコンボ）

#include "boardcolors.h"
#include <QCoreApplication>
#include <QList>
#include <QString>

struct BoardColorPreset {
    QString name;
    BoardColors colors;
};

/// 盤・駒台・背景・罫線の配色の表（外観の全体の組み合わせは BoardAppearanceCatalog::combinations）
class BoardColorPresets
{
    Q_DECLARE_TR_FUNCTIONS(BoardColorPresets)
public:
    static QList<BoardColorPreset> palettes();
};

#endif // BOARDCOLORPRESETS_H
