#include "boardcolorpresets.h"

namespace {
BoardColorPreset preset(const QString& name, QRgb background, QRgb board, QRgb stand, QRgb grid)
{
    return {name, {QColor(background), QColor(board), QColor(stand), QColor(grid)}};
}
}

QList<BoardColorPreset> BoardColorPresets::forPieceStyle(const QString& style)
{
    if (style == QLatin1String("clear")) {
        return {
            preset(tr("若葉"),       0x536f66, 0xb6c9bb, 0x87a798, 0x3c554a),
            preset(tr("青灰"),       0x536679, 0xb9cbd6, 0x8da6b7, 0x3d5262),
            preset(tr("蜂蜜"),       0x89734e, 0xdabb7c, 0xb89a60, 0x58452d),
            preset(tr("藤色"),       0x655e75, 0xc8c0d5, 0xa69ab7, 0x51465f),
            preset(tr("夕暮れ"),     0x252e35, 0x94a6ae, 0x647f89, 0x344b55)};
    }
    if (style == QLatin1String("wood")) {
        return {
            preset(tr("苔庭"),       0x52604a, 0xabb28b, 0x818d67, 0x3e4c36),
            preset(tr("胡桃"),       0x443831, 0xb49578, 0x876a53, 0x4b392c),
            preset(tr("生成り"),     0x9b9081, 0xd9cfbb, 0xb7a88e, 0x665541),
            preset(tr("深い森"),     0x243d36, 0x9aac92, 0x687f6a, 0x354e40),
            preset(tr("石庭"),       0x606565, 0xbcc2ba, 0x949e95, 0x46534c)};
    }
    if (style == QLatin1String("ivory")) {
        return {
            preset(tr("藍夜"),       0x172d42, 0x718a9e, 0x48647c, 0x233c51),
            preset(tr("青磁の海"),   0x264d59, 0x91b5ba, 0x638f97, 0x305762),
            preset(tr("セージ"),     0x485d51, 0xa5baa3, 0x78947d, 0x3d5743),
            preset(tr("ラベンダー"), 0x49455f, 0xa9a2bd, 0x7b7194, 0x4c425f),
            preset(tr("墨色"),       0x24282f, 0x9199a4, 0x626e7d, 0x333f4f)};
    }
    if (style == QLatin1String("dark")) {
        return {
            preset(tr("金屏風"),     0x3d3530, 0xdfc48b, 0xb59a65, 0x655335),
            preset(tr("銀鼠"),       0x48515b, 0xd1d6da, 0xa5afb7, 0x4f5d68),
            preset(tr("青磁"),       0x365550, 0xbfd6c7, 0x8caf9b, 0x42675c),
            preset(tr("桜霞"),       0x6c5159, 0xe3c9c8, 0xba989b, 0x76535d),
            preset(tr("砂紋"),       0x665d4e, 0xddd2b4, 0xb4a783, 0x655d46)};
    }
    return {
        preset(tr("畳と榧"),     0xb9bc91, 0xdab86b, 0xbd944f, 0x5b452b),
        preset(tr("薄茶"),       0x7b6854, 0xd8b681, 0xba986a, 0x514334),
        preset(tr("若竹"),       0x64796a, 0xd8d9b4, 0xa8b28e, 0x435844),
        preset(tr("暖かな灰色"), 0x7a7772, 0xded2b9, 0xb5aa93, 0x555048),
        preset(tr("藍染"),       0x293a4d, 0xd6bf8a, 0xad986f, 0x4b4c46)};
}

QString BoardColorPresets::pieceStyleName(const QString& style)
{
    if (style == QLatin1String("clear"))
        return QCoreApplication::translate("MainWindow", "見やすい駒（太字）");
    if (style == QLatin1String("wood"))
        return QCoreApplication::translate("MainWindow", "木目の駒（明朝）");
    if (style == QLatin1String("ivory"))
        return QCoreApplication::translate("MainWindow", "白い駒（ゴシック）");
    if (style == QLatin1String("dark"))
        return QCoreApplication::translate("MainWindow", "黒い駒（金文字）");
    return QCoreApplication::translate("MainWindow", "標準の駒");
}
