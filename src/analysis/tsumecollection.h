#ifndef TSUMECOLLECTION_H
#define TSUMECOLLECTION_H

#include <QStringList>
#include <QList>
#include <QByteArray>

struct TsumeProblem {
    QString sfen;
    QStringList referenceMoves; // 開始局面には適用しない。同梱監査との一致時だけ検証済み手順として扱う。
    int lineNumber = 0;
};

namespace TsumeCollection {
struct Result {
    QList<TsumeProblem> problems;
    QList<int> invalidLines;
    QStringList positionIds; // problems と同順。読込時の局面解析から作り、履歴照会で再解析しない。
};
Result parse(const QString& text);
/// contents を parse() した結果を渡す。同梱監査とファイル全体が一致すれば最短手数、その他は0。
int verifiedMateLength(const QByteArray& contents, const Result& parsed);
/// 手数カウンタ、ファイル名、参考手順に依存しない局面識別子。
QString positionId(const QString& sfen);
bool validMateLine(const QString& sfen, const QStringList& pv);
}

#endif
