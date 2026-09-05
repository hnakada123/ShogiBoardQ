#ifndef BOARDCOLORPRESETS_H
#define BOARDCOLORPRESETS_H

#include "boardcolors.h"
#include <QCoreApplication>
#include <QList>
#include <QString>

struct BoardColorPreset {
    QString name;
    BoardColors colors;
};

/// 駒の地色・文字色に合わせた配色候補。各駒セットに5種類を用意する。
class BoardColorPresets
{
    Q_DECLARE_TR_FUNCTIONS(BoardColorPresets)
public:
    static QList<BoardColorPreset> forPieceStyle(const QString& style);
    static QString pieceStyleName(const QString& style);
};

#endif // BOARDCOLORPRESETS_H
