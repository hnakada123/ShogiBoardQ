#ifndef TIMECONTROL_TEST_HELPER_H
#define TIMECONTROL_TEST_HELPER_H

/// @file timecontrol_test_helper.h
/// @brief テスト用: 秒で持ち時間の決まりを作る

#include "timecontrol.h"
#include "usitimingparams.h"

namespace TestTime {

/// 先手の持ち時間・後手の持ち時間・先手/後手の秒読み・先手/後手の加算（すべて秒）
inline TimeControl rules(int main1Sec, int main2Sec, int byo1Sec, int byo2Sec, int inc1Sec, int inc2Sec)
{
    TimeControl tc;
    tc.black.mainMs = main1Sec * 1000LL;
    tc.white.mainMs = main2Sec * 1000LL;
    tc.black.byoyomiMs = byo1Sec * 1000LL;
    tc.white.byoyomiMs = byo2Sec * 1000LL;
    tc.black.incrementMs = inc1Sec * 1000LL;
    tc.white.incrementMs = inc2Sec * 1000LL;
    return tc;
}

/// 時計をつながないエンジン向けの条件: 両者同じ秒読みと、USI の意味の残り時間（ミリ秒）
inline UsiTimingParams byoyomiTiming(qint64 byoyomiMs, qint64 btimeMs, qint64 wtimeMs)
{
    UsiTimingParams timing;
    timing.timeControl.black.byoyomiMs = byoyomiMs;
    timing.timeControl.white.byoyomiMs = byoyomiMs;
    timing.btimeMs = btimeMs;
    timing.wtimeMs = wtimeMs;
    return timing;
}

} // namespace TestTime

#endif // TIMECONTROL_TEST_HELPER_H
