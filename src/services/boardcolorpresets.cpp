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
    ink.background = QColor("#303b38");
    ink.board = QColor("#e1c68e");
    ink.stand = QColor("#86715a");
    ink.cardBackground = QColor("#3e4b44");
    ink.cardBorder = QColor("#58665b");
    ink.activeCardBorder = ink.turnBackground = ink.turnBorder = QColor("#7d9878");
    ink.nameText = ink.clockText = QColor("#e5e7d9");
    ink.clockWarningText = QColor("#f2cf83");
    ink.clockCriticalText = QColor("#ffa397");
    BoardColors amber;
    amber.background = QColor("#39352f");
    amber.board = QColor("#e3b161");
    amber.stand = QColor("#a26a37");
    amber.cardBackground = QColor("#eee4cf");
    amber.cardBorder = QColor("#bdab87");
    amber.activeCardBorder = amber.turnBackground = amber.turnBorder = QColor("#8c6937");
    amber.nameText = amber.clockText = QColor("#483c29");
    return {{QStringLiteral("kaya"), tr("榧と畳"), BoardColors{}},
            {QStringLiteral("ink"), tr("墨と榧"), ink},
            {QStringLiteral("amber"), tr("琥珀"), amber}};
}

QList<BoardColorPreset> BoardColorPresets::forPieceStyle(const QString& style)
{
    Q_UNUSED(style)
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
    if (style == QLatin1String("wood_straight"))
        return QCoreApplication::translate("MainWindow", "糸柾");
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
    return QCoreApplication::translate("MainWindow", "標準の駒");
}
