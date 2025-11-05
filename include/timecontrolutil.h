#ifndef TIMECONTROLUTIL_H
#define TIMECONTROLUTIL_H

#include "gamestartcoordinator.h"
#include <QString>

class ShogiClock;
class GameStartCoordinator;

namespace TimeControlUtil {

// GameStartCoordinator::TimeControl を ShogiClock へ適用。
// startSfenStr / currentSfenStr は「現在手番の推定」にのみ利用。
void applyToClock(ShogiClock* clock,
                  const GameStartCoordinator::TimeControl& tc,
                  const QString& startSfenStr,
                  const QString& currentSfenStr);

} // namespace TimeControlUtil

#endif // TIMECONTROLUTIL_H
