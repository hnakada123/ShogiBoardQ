#ifndef INTERRUPTEDGAMESTATE_H
#define INTERRUPTEDGAMESTATE_H

#include "shogiclock.h"
#include "shogimove.h"
#include "startoptions.h"
#include "timecontrol.h"
#include <QStringList>

/// アプリ起動中だけ保持する、中断したローカル対局の状態。
struct InterruptedGameState {
    MatchStartOptions options;
    MatchTimeControl timeControl;
    ShogiClock::Snapshot clock;
    QStringList sfens;
    QList<ShogiMove> moves;
    QString position;
    QStringList positionHistory;
    int previousFile = 0;
    int previousRank = 0;
};

#endif
