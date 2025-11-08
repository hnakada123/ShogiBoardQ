#include "timecontrolutil.h"
#include "gamestartcoordinator.h"
#include "shogiclock.h"
#include <QTime>

namespace TimeControlUtil {

static inline int msToSecFloor(qint64 ms) { return (ms <= 0) ? 0 : int(ms / 1000); }

// 置き換え：TimeControlUtil::applyToClock
void applyToClock(ShogiClock* clock,
                                   const GameStartCoordinator::TimeControl& tc,
                                   const QString& startSfenStr,
                                   const QString& currentSfenStr)
{
    if (!clock) {
        qDebug() << "[TC] applyToClock: clock=null";
        return;
    }

    // 一旦停止
    clock->stopClock();

    auto msToSecFloor = [](qint64 ms)->int { return (ms <= 0) ? 0 : int(ms / 1000); };

    // tc.enabled が false でも、どれかに値が入っていれば limited を true に補正
    const bool hasAny =
        (tc.p1.baseMs > 0) || (tc.p2.baseMs > 0) ||
        (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0) ||
        (tc.p1.incrementMs > 0) || (tc.p2.incrementMs > 0);

    const bool limited = tc.enabled || hasAny;

    const int p1BaseSec = limited ? msToSecFloor(tc.p1.baseMs) : 0;
    const int p2BaseSec = limited ? msToSecFloor(tc.p2.baseMs) : 0;

    const int byo1Sec   = msToSecFloor(tc.p1.byoyomiMs);
    const int byo2Sec   = msToSecFloor(tc.p2.byoyomiMs);
    const int inc1Sec   = msToSecFloor(tc.p1.incrementMs);
    const int inc2Sec   = msToSecFloor(tc.p2.incrementMs);

    // byoyomi 優先（両立指定時は秒読みを優先）
    const bool useByoyomi = (byo1Sec > 0) || (byo2Sec > 0);
    const int finalByo1 = useByoyomi ? byo1Sec : 0;
    const int finalByo2 = useByoyomi ? byo2Sec : 0;
    const int finalInc1 = useByoyomi ? 0       : inc1Sec;
    const int finalInc2 = useByoyomi ? 0       : inc2Sec;

    qDebug().noquote()
        << "[TC] applyToClock:"
        << " limited=" << limited
        << " P1{baseSec=" << p1BaseSec << " byoSec=" << finalByo1 << " incSec=" << finalInc1 << "}"
        << " P2{baseSec=" << p2BaseSec << " byoSec=" << finalByo2 << " incSec=" << finalInc2 << "}";

    clock->setLoseOnTimeout(limited);
    clock->setPlayerTimes(p1BaseSec, p2BaseSec, finalByo1, finalByo2,
                          finalInc1, finalInc2, /*isLimitedTime=*/limited);

    // SFEN から初期手番を推定（既存仕様）
    auto sideFromSfen = [](const QString& sfen)->bool {
        const int b = sfen.indexOf(QLatin1String(" b "));
        const int w = sfen.indexOf(QLatin1String(" w "));
        if (b >= 0 && w < 0) return true;   // 先手
        if (w >= 0 && b < 0) return false;  // 後手
        return true; // fallback
    };
    const QString sfenForStart =
        !startSfenStr.isEmpty() ? startSfenStr :
            (!currentSfenStr.isEmpty() ? currentSfenStr : QStringLiteral("startpos b - 1"));

    clock->setCurrentPlayer(sideFromSfen(sfenForStart));

    qDebug().noquote()
        << "[TC] after setPlayerTimes:"
        << " P1ms=" << clock->getPlayer1TimeIntMs()
        << " P2ms=" << clock->getPlayer2TimeIntMs()
        << " byoMs=" << clock->getCommonByoyomiMs()
        << " inc(b/w)Ms=" << clock->getBincMs() << "/" << clock->getWincMs();
}

} // namespace TimeControlUtil
