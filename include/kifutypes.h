#ifndef KIFUTYPES_H
#define KIFUTYPES_H

#include <QString>
#include <QVector>

#include "kifdisplayitem.h"
#include "shogimove.h"

struct ResolvedRow {
    int startPly = 1;
    int parent   = -1;                 // ★追加：親行。Main は -1
    QList<KifDisplayItem> disp;
    QStringList sfen;
    QVector<ShogiMove> gm;
    int varIndex = -1;                 // 本譜 = -1
};

#endif // KIFUTYPES_H
