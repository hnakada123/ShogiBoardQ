/// @file timecontrolutil.cpp
/// @brief 持ち時間設定をShogiClockへ適用するユーティリティの実装

#include "timecontrolutil.h"
#include "gamestartcoordinator.h"
#include "loggingcategory.h"
#include "shogiclock.h"
#include <QTime>

namespace TimeControlUtil {

// ============================================================
// 適用 API
// ============================================================

void applyToClock(
    ShogiClock* clock,
    const GameStartCoordinator::TimeControl& tc,
    const QString& startSfenStr,
    const QString& currentSfenStr)
{
    if (!clock) return;

    auto msToSecFloor = [](qint64 ms)->int { return (ms <= 0) ? 0 : int(ms / 1000); };

    // tc.enabled が false でも、どれかに値が入っていれば limited を true 扱い
    const bool hasAny =
        (tc.p1.baseMs > 0) || (tc.p2.baseMs > 0) ||
        (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0) ||
        (tc.p1.incrementMs > 0) || (tc.p2.incrementMs > 0);

    const bool limited = tc.enabled || hasAny;

    const int p1BaseSec = limited ? msToSecFloor(tc.p1.baseMs) : 0;
    const int p2BaseSec = limited ? msToSecFloor(tc.p2.baseMs) : 0;

    // byoyomi 指定があれば byoyomi 優先（increment は 0 扱い）
    const int byo1Sec = msToSecFloor(tc.p1.byoyomiMs);
    const int byo2Sec = msToSecFloor(tc.p2.byoyomiMs);
    const int inc1Sec = msToSecFloor(tc.p1.incrementMs);
    const int inc2Sec = msToSecFloor(tc.p2.incrementMs);

    const bool useByoyomi = (byo1Sec > 0) || (byo2Sec > 0);
    const int finalByo1 = useByoyomi ? byo1Sec : 0;
    const int finalByo2 = useByoyomi ? byo2Sec : 0;
    const int finalInc1 = useByoyomi ? 0       : inc1Sec;
    const int finalInc2 = useByoyomi ? 0       : inc2Sec;

    qCDebug(lcUi).noquote()
        << "applyToClock:"
        << " limited=" << limited
        << " P1{baseSec=" << p1BaseSec << " byoSec=" << finalByo1 << " incSec=" << finalInc1 << "}"
        << " P2{baseSec=" << p2BaseSec << " byoSec=" << finalByo2 << " incSec=" << finalInc2 << "}";

    clock->setLoseOnTimeout(limited);
    clock->setPlayerTimes(
        p1BaseSec, p2BaseSec,
        finalByo1, finalByo2,
        finalInc1, finalInc2,
        /*isLimitedTime=*/limited);

    // 初期手番を SFEN から推定する（1=先手, 2=後手）
    auto sideFromSfen = [](const QString& sfen)->int {
        const qsizetype b = sfen.indexOf(QLatin1String(" b "));
        const qsizetype w = sfen.indexOf(QLatin1String(" w "));
        if (b >= 0 && w < 0) return 1;
        if (w >= 0 && b < 0) return 2;
        return 1; // fallback は先手
    };

    const QString sfenForStart =
        !startSfenStr.isEmpty() ? startSfenStr :
            (!currentSfenStr.isEmpty() ? currentSfenStr : QString());

    const int initialPlayer = sfenForStart.isEmpty()
                                  ? 1
                                  : sideFromSfen(sfenForStart);

    clock->setCurrentPlayer(initialPlayer);
}

} // namespace TimeControlUtil
