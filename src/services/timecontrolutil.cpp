/// @file timecontrolutil.cpp
/// @brief 持ち時間設定をShogiClockへ適用するユーティリティの実装

#include "timecontrolutil.h"
#include "gamestartcoordinator.h"
#include "logcategories.h"
#include "shogiclock.h"
#include <QTime>

namespace TimeControlUtil {

// ============================================================
// 適用 API
// ============================================================

void applyToClock(
    ShogiClock* clock,
    const GameStartCoordinator::TimeSettings& tc,
    const QString& startSfenStr,
    const QString& currentSfenStr)
{
    if (!clock) return;

    // 決まりに値がなければ消費時間だけを数える時計になる（tc.enabled に関係なく決まりで判断する）
    const TimeControl rules = tc.rules.normalized();
    const bool limited = !rules.isUnlimited();
    qCDebug(lcUi).noquote()
        << "applyToClock:"
        << " limited=" << limited
        << " P1{mainMs=" << rules.black.mainMs << " byoMs=" << rules.black.byoyomiMs << " incMs=" << rules.black.incrementMs << "}"
        << " P2{mainMs=" << rules.white.mainMs << " byoMs=" << rules.white.byoyomiMs << " incMs=" << rules.white.incrementMs << "}";

    clock->setLoseOnTimeout(limited && tc.loseOnTimeout);
    // 初手にも加算時間を与える処理は ShogiClock::setTimeControl が行う
    clock->setTimeControl(rules);

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
