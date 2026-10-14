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

QString BoardColorPresets::pieceStyleName(const QString& style)
{
    if (style == QLatin1String("torafu_light"))
        return QCoreApplication::translate("MainWindow", "淡虎斑");
    if (style == QLatin1String("torafu_silk"))
        return QCoreApplication::translate("MainWindow", "絹虎斑");
    if (style == QLatin1String("torafu_amber"))
        return QCoreApplication::translate("MainWindow", "飴虎斑");
    if (style == QLatin1String("torafu_red"))
        return QCoreApplication::translate("MainWindow", "紅虎斑");
    if (style == QLatin1String("torafu_gold"))
        return QCoreApplication::translate("MainWindow", "山吹虎斑");
    if (style == QLatin1String("wood_pale"))
        return QCoreApplication::translate("MainWindow", "白木");
    if (style == QLatin1String("wood_classic"))
        return QCoreApplication::translate("MainWindow", "木目");
    if (style == QLatin1String("wood_amber"))
        return QCoreApplication::translate("MainWindow", "飴柾");
    if (style == QLatin1String("wood_bamboo"))
        return QCoreApplication::translate("MainWindow", "笹杢");
    if (style == QLatin1String("wood_walnut"))
        return QCoreApplication::translate("MainWindow", "胡桃");
    if (style == QLatin1String("tint_linen"))
        return QCoreApplication::translate("MainWindow", "生成り");
    if (style == QLatin1String("tint_sakura"))
        return QCoreApplication::translate("MainWindow", "薄桜");
    if (style == QLatin1String("tint_celadon"))
        return QCoreApplication::translate("MainWindow", "青磁");
    if (style == QLatin1String("tint_moon"))
        return QCoreApplication::translate("MainWindow", "月白");
    if (style == QLatin1String("tint_wisteria"))
        return QCoreApplication::translate("MainWindow", "藤鼠");
    if (style == QLatin1String("deep_ebony"))
        return QCoreApplication::translate("MainWindow", "黒檀");
    if (style == QLatin1String("deep_navy"))
        return QCoreApplication::translate("MainWindow", "鉄紺");
    if (style == QLatin1String("deep_green"))
        return QCoreApplication::translate("MainWindow", "深緑");
    if (style == QLatin1String("deep_grape"))
        return QCoreApplication::translate("MainWindow", "葡萄");
    if (style == QLatin1String("deep_gold"))
        return QCoreApplication::translate("MainWindow", "墨金");
    if (style == QLatin1String("sengoku"))
        return QCoreApplication::translate("MainWindow", "戦国文字");
    if (style == QLatin1String("chess_facet_wood"))
        return QCoreApplication::translate("MainWindow", "Facet（木肌）");
    if (style == QLatin1String("chess_facet_paper"))
        return QCoreApplication::translate("MainWindow", "Facet（白）");
    if (style == QLatin1String("chess_facet_slate"))
        return QCoreApplication::translate("MainWindow", "Facet（墨）");
    if (style == QLatin1String("chess_atelier_wood"))
        return QCoreApplication::translate("MainWindow", "Atelier（木肌）");
    if (style == QLatin1String("chess_atelier_paper"))
        return QCoreApplication::translate("MainWindow", "Atelier（白）");
    if (style == QLatin1String("chess_atelier_slate"))
        return QCoreApplication::translate("MainWindow", "Atelier（墨）");
    if (style == QLatin1String("chess_ribbon_wood"))
        return QCoreApplication::translate("MainWindow", "Ribbon（木肌）");
    if (style == QLatin1String("chess_ribbon_paper"))
        return QCoreApplication::translate("MainWindow", "Ribbon（白）");
    if (style == QLatin1String("chess_ribbon_slate"))
        return QCoreApplication::translate("MainWindow", "Ribbon（墨）");
    if (style == QLatin1String("alphabet_sei_wood"))
        return QCoreApplication::translate("MainWindow", "端正（木肌）");
    if (style == QLatin1String("alphabet_sei_paper"))
        return QCoreApplication::translate("MainWindow", "端正（白）");
    if (style == QLatin1String("alphabet_sei_slate"))
        return QCoreApplication::translate("MainWindow", "端正（墨）");
    if (style == QLatin1String("alphabet_rin_wood"))
        return QCoreApplication::translate("MainWindow", "凛（木肌）");
    if (style == QLatin1String("alphabet_rin_paper"))
        return QCoreApplication::translate("MainWindow", "凛（白）");
    if (style == QLatin1String("alphabet_rin_slate"))
        return QCoreApplication::translate("MainWindow", "凛（墨）");
    if (style == QLatin1String("alphabet_sumi_wood"))
        return QCoreApplication::translate("MainWindow", "墨（木肌）");
    if (style == QLatin1String("alphabet_sumi_paper"))
        return QCoreApplication::translate("MainWindow", "墨（白）");
    if (style == QLatin1String("alphabet_sumi_slate"))
        return QCoreApplication::translate("MainWindow", "墨（墨）");
    return QCoreApplication::translate("MainWindow", "標準の駒");
}
