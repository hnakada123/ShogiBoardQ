#ifndef TSUMEEVALUATION_H
#define TSUMEEVALUATION_H

#include <QStringList>
#include <QMetaType>

struct TsumeEvaluation {
    enum class Status { Unknown, Mate, NoMate };
    Status status = Status::Unknown;
    int plies = 0; // Mate のときのみ、最短の攻手・最長の応手による確定手数。
    QStringList pv;
    QString detail;
};
Q_DECLARE_METATYPE(TsumeEvaluation)

#endif
