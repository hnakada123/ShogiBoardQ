/// @file timecontrolvalidator.cpp
/// @brief 対局開始前の時間設定の検証の実装

#include "timecontrolvalidator.h"

namespace TimeControlValidator {

namespace {

bool hasTime(const PlayerTime& p, bool useByoyomi)
{
    if (p.hours > 0 || p.minutes > 0) return true;
    // 秒読みと加算は排他。どちらかに秒読みがあれば加算は対局に使わない（GameStartOptionsBuilder と同じ）。
    return useByoyomi ? p.byoyomiSec > 0 : p.incrementSec > 0;
}

} // namespace

Issue validate(const PlayerTime& p1, const PlayerTime& p2, bool hasEngine)
{
    const bool useByoyomi = p1.byoyomiSec > 0 || p2.byoyomiSec > 0;
    const bool limited1 = hasTime(p1, useByoyomi);
    const bool limited2 = hasTime(p2, useByoyomi);

    if (limited1 && limited2) return Issue::None;
    if (limited1) return Issue::Player2HasNoTime;
    if (limited2) return Issue::Player1HasNoTime;
    // エンジンは残り0秒の指定を受け取ると、ほとんど考えずに指す。
    return hasEngine ? Issue::EngineWithoutLimit : Issue::None;
}

} // namespace TimeControlValidator
