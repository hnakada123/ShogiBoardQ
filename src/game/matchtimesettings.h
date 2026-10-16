#ifndef MATCHTIMESETTINGS_H
#define MATCHTIMESETTINGS_H

/// @file matchtimesettings.h
/// @brief 対局の時間の決まり（持ち時間・秒読み・加算と、時間切れの扱い）

#include <QMetaType>

#include "timecontrol.h"

/// 対局の時間の決まり。持ち時間がどちらにも無ければ（rules.isUnlimited()）消費時間だけを数える
struct MatchTimeSettings {
    TimeControl rules;          ///< 先手・後手の持ち時間の決まり
    bool loseOnTimeout = true;  ///< 時間切れを負けとするか
};

Q_DECLARE_METATYPE(MatchTimeSettings)

#endif // MATCHTIMESETTINGS_H
