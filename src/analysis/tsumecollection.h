#ifndef TSUMECOLLECTION_H
#define TSUMECOLLECTION_H

#include <QStringList>
#include <QList>

struct TsumeProblem {
    QString sfen;
    QStringList referenceMoves; // 参考手順。開始局面に適用したり正解判定には使わない。
    int lineNumber = 0;
};

namespace TsumeCollection {
struct Result {
    QList<TsumeProblem> problems;
    QList<int> invalidLines;
};
Result parse(const QString& text);
}

#endif
