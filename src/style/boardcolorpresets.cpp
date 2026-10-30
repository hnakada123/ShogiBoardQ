/// @file boardcolorpresets.cpp
/// @brief 盤・駒台・背景のおすすめ配色の表

#include "boardcolorpresets.h"

namespace {
BoardColorPreset preset(const QString& name, QRgb background, QRgb board, QRgb stand, QRgb grid)
{
    return {name, {QColor(background), QColor(board), QColor(stand), QColor(grid)}};
}
}

QList<BoardColorPreset> BoardColorPresets::palettes()
{
    return {
        preset(tr("苔庭"),       0x52604a, 0xabb28b, 0x818d67, 0x3e4c36),
        preset(tr("胡桃"),       0x443831, 0xb49578, 0x876a53, 0x4b392c),
        preset(tr("生成り"),     0x9b9081, 0xd9cfbb, 0xb7a88e, 0x665541),
        preset(tr("深い森"),     0x243d36, 0x9aac92, 0x687f6a, 0x354e40),
        preset(tr("石庭"),       0x606565, 0xbcc2ba, 0x949e95, 0x46534c)};
}
