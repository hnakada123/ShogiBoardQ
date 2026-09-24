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
/// 手数カウンタ、ファイル名、参考手順に依存しない局面識別子。
QString positionId(const QString& sfen);
bool validMateLine(const QString& sfen, const QStringList& pv);
}

#endif
