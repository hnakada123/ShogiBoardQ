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

struct BoardThemePreset {
    QString id;
    QString name;
    BoardColors colors;
};

/// 詳細設定で使う全体テーマ・共通配色と、駒セットの表示名。
class BoardColorPresets
{
    Q_DECLARE_TR_FUNCTIONS(BoardColorPresets)
public:
    static QList<BoardThemePreset> themes();
    static QList<BoardColorPreset> palettes();
    static QString pieceStyleName(const QString& style);
};

#endif // BOARDCOLORPRESETS_H
