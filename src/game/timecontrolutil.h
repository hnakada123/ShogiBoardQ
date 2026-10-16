#ifndef TIMECONTROLUTIL_H
#define TIMECONTROLUTIL_H

/// @file timecontrolutil.h
/// @brief 対局の時間の決まりを将棋時計へ適用する

#include "matchtimesettings.h"
#include "shogitypes.h"

class ShogiClock;

namespace TimeControlUtil {

/// 時間の決まりを時計に設定し、時計の手番を最初に指す側にする（時計はまだ動かさない）
void applyToClock(ShogiClock& clock, const MatchTimeSettings& settings, Turn firstPlayer);

} // namespace TimeControlUtil

#endif // TIMECONTROLUTIL_H
