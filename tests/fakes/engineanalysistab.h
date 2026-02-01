#ifndef ENGINEANALYSISTAB_H
#define ENGINEANALYSISTAB_H

#include <QList>
#include <QStringList>
#include <QVector>

#include "kifdisplayitem.h"

class EngineAnalysisTab
{
public:
    struct ResolvedRowLite {
        int startPly = 1;
        int parent   = -1;
        QList<KifDisplayItem> disp;
        QStringList sfen;
    };

    void setBranchTreeRows(const QVector<ResolvedRowLite>&) {}
    void highlightBranchTreeAt(int, int, bool = false) {}
};

#endif // ENGINEANALYSISTAB_H
