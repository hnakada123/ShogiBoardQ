#ifndef CORE_TIMECONTROL_H
#define CORE_TIMECONTROL_H

/// @file timecontrol.h
/// @brief 持ち時間の決まり（持ち時間・秒読み・加算・遅延）の値型

#include <QtGlobal>

#include "shogitypes.h"

/// 片方の対局者の持ち時間の決まり（ミリ秒）
struct TimeControlSide {
    qint64 mainMs = 0;       ///< 持ち時間の初期値
    qint64 byoyomiMs = 0;    ///< 秒読み（持ち時間を使い切った後、1手ごとに与える時間）
    qint64 incrementMs = 0;  ///< 加算（手番が来るたびに持ち時間へ足す時間）
    qint64 delayMs = 0;      ///< 遅延（手番の始めから、この時間までは消費しない。CSA の Delay）

    /// 何らかの時間制限があるか
    bool hasLimit() const { return mainMs > 0 || byoyomiMs > 0 || incrementMs > 0 || delayMs > 0; }

    friend bool operator==(const TimeControlSide& a, const TimeControlSide& b)
    {
        return a.mainMs == b.mainMs && a.byoyomiMs == b.byoyomiMs
            && a.incrementMs == b.incrementMs && a.delayMs == b.delayMs;
    }
    friend bool operator!=(const TimeControlSide& a, const TimeControlSide& b) { return !(a == b); }
};

/// 先手・後手の持ち時間の決まり
struct TimeControl {
    TimeControlSide black;  ///< 先手（下手）
    TimeControlSide white;  ///< 後手（上手）

    const TimeControlSide& side(Turn t) const { return t == Turn::Black ? black : white; }
    TimeControlSide& side(Turn t) { return t == Turn::Black ? black : white; }

    /// 双方とも時間制限がない（消費時間だけを数える）
    bool isUnlimited() const { return !black.hasLimit() && !white.hasLimit(); }

    /// どちらかに秒読みがある（USI では秒読みと加算を同時に伝えられないので、秒読みを優先する）
    bool usesByoyomi() const { return black.byoyomiMs > 0 || white.byoyomiMs > 0; }

    /// 秒読みと加算が両方あるときは秒読みを残し、加算を外した決まりを返す
    TimeControl normalized() const
    {
        TimeControl tc = *this;
        if (tc.usesByoyomi()) {
            tc.black.incrementMs = 0;
            tc.white.incrementMs = 0;
        }
        return tc;
    }

    friend bool operator==(const TimeControl& a, const TimeControl& b)
    {
        return a.black == b.black && a.white == b.white;
    }
    friend bool operator!=(const TimeControl& a, const TimeControl& b) { return !(a == b); }
};

#endif // CORE_TIMECONTROL_H
