/// @file timecontrolvalidator.cpp
/// @brief 対局開始前の時間設定の検証の実装

#include "timecontrolvalidator.h"

namespace TimeControlValidator {

Issue validate(const TimeControlSide& p1, const TimeControlSide& p2, bool hasEngine)
{
    // 秒読みと加算が両方あれば加算は対局に使わない。どちらを使うかは対局と同じ TimeControl::normalized で決める
    const TimeControl rules = TimeControl{p1, p2}.normalized();
    const bool limited1 = rules.black.hasLimit();
    const bool limited2 = rules.white.hasLimit();

    if (limited1 && limited2) return Issue::None;
    if (limited1) return Issue::Player2HasNoTime;
    if (limited2) return Issue::Player1HasNoTime;
    // エンジンは残り0秒の指定を受け取ると、ほとんど考えずに指す。
    return hasEngine ? Issue::EngineWithoutLimit : Issue::None;
}

} // namespace TimeControlValidator
