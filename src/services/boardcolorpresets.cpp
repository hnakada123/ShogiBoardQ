#include "boardcolorpresets.h"

namespace {
BoardColorPreset preset(const QString& name, QRgb background, QRgb board, QRgb stand, QRgb grid)
{
    return {name, {QColor(background), QColor(board), QColor(stand), QColor(grid)}};
}
}

QList<BoardThemePreset> BoardColorPresets::themes()
{
    BoardColors ink;
    ink.background = QColor(0x303b38);
    ink.board = QColor(0xe1c68e);
    ink.stand = QColor(0x86715a);
    ink.cardBackground = QColor(0x3e4b44);
    ink.cardBorder = QColor(0x58665b);
    ink.activeCardBorder = ink.turnBackground = ink.turnBorder = QColor(0x7d9878);
    ink.nameText = ink.clockText = QColor(0xe5e7d9);
    ink.clockWarningText = QColor(0xf2cf83);
    ink.clockCriticalText = QColor(0xffa397);
    BoardColors amber;
    amber.background = QColor(0x39352f);
    amber.board = QColor(0xe3b161);
    amber.stand = QColor(0xa26a37);
    amber.cardBackground = QColor(0xeee4cf);
    amber.cardBorder = QColor(0xbdab87);
    amber.activeCardBorder = amber.turnBackground = amber.turnBorder = QColor(0x8c6937);
    amber.nameText = amber.clockText = QColor(0x483c29);
    return {{QStringLiteral("kaya"), tr("榧と畳"), BoardColors{}},
            {QStringLiteral("ink"), tr("墨と榧"), ink},
            {QStringLiteral("amber"), tr("琥珀"), amber}};
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
