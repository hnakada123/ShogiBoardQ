/// @file timecontrolutil.cpp
/// @brief 対局の時間の決まりを将棋時計へ適用する

#include "timecontrolutil.h"
#include "logcategories.h"
#include "shogiclock.h"

namespace TimeControlUtil {

void applyToClock(ShogiClock& clock, const MatchTimeSettings& settings, Turn firstPlayer)
{
    // 決まりに値がなければ消費時間だけを数える時計になる
    const TimeControl rules = settings.rules.normalized();
    const bool limited = !rules.isUnlimited();
    qCDebug(lcUi).noquote()
        << "applyToClock:" << " limited=" << limited
        << " P1{mainMs=" << rules.black.mainMs << " byoMs=" << rules.black.byoyomiMs << " incMs=" << rules.black.incrementMs << "}"
        << " P2{mainMs=" << rules.white.mainMs << " byoMs=" << rules.white.byoyomiMs << " incMs=" << rules.white.incrementMs << "}";

    clock.setLoseOnTimeout(limited && settings.loseOnTimeout);
    // 初手にも加算時間を与える処理は ShogiClock::setTimeControl が行う
    clock.setTimeControl(rules);
    clock.setCurrentPlayer(firstPlayer);
}

} // namespace TimeControlUtil
