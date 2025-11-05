#include "timecontrolutil.h"
#include "gamestartcoordinator.h"
#include "shogiclock.h"
#include <QTime>

namespace TimeControlUtil {

static inline int msToSecFloor(qint64 ms) { return (ms <= 0) ? 0 : int(ms / 1000); }

void applyToClock(ShogiClock* clock,
                  const GameStartCoordinator::TimeControl& tc,
                  const QString& startSfenStr,
                  const QString& currentSfenStr)
{
    if (!clock) return;

    // 一旦停止
    clock->stopClock();

    const bool limited = tc.enabled;

    const int p1BaseSec = limited ? msToSecFloor(tc.p1.baseMs) : 0;
    const int p2BaseSec = limited ? msToSecFloor(tc.p2.baseMs) : 0;

    const int byo1Sec   = msToSecFloor(tc.p1.byoyomiMs);
    const int byo2Sec   = msToSecFloor(tc.p2.byoyomiMs);
    const int inc1Sec   = msToSecFloor(tc.p1.incrementMs);
    const int inc2Sec   = msToSecFloor(tc.p2.incrementMs);

    // byoyomi と increment は排他（byoyomi 優先）
    const bool useByoyomi = (byo1Sec > 0) || (byo2Sec > 0);
    const int finalByo1 = useByoyomi ? byo1Sec : 0;
    const int finalByo2 = useByoyomi ? byo2Sec : 0;
    const int finalInc1 = useByoyomi ? 0       : inc1Sec;
    const int finalInc2 = useByoyomi ? 0       : inc2Sec;

    clock->setLoseOnTimeout(limited);
    clock->setPlayerTimes(p1BaseSec, p2BaseSec, finalByo1, finalByo2,
                          finalInc1, finalInc2, /*isLimitedTime=*/limited);

    // SFEN から初期手番を推定
    auto sideFromSfen = [](const QString& sfen)->bool {
        const int b = sfen.indexOf(QLatin1String(" b "));
        const int w = sfen.indexOf(QLatin1String(" w "));
        if (b >= 0 && w < 0) return true;   // 先手
        if (w >= 0 && b < 0) return false;  // 後手
        return true; // フォールバック＝先手
    };
    const QString sfenForStart =
        !startSfenStr.isEmpty() ? startSfenStr :
            (!currentSfenStr.isEmpty() ? currentSfenStr : QStringLiteral("startpos b - 1"));
    clock->setCurrentPlayer(sideFromSfen(sfenForStart));
}

} // namespace TimeControlUtil
