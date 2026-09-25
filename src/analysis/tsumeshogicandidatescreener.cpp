/// @file tsumeshogicandidatescreener.cpp
/// @brief 外部エンジンに送る前に候補局面を内蔵探索で選別するクラスの実装

#include "tsumeshogicandidatescreener.h"

#include <position.h>
#include <tsume.h>

#include <algorithm>

namespace {
/// Hayanagi の有限深さ探索が扱える最大手数
constexpr int kMaxSearchPlies = 63;
}

TsumeshogiCandidateScreener::Verdict TsumeshogiCandidateScreener::screen(
    const QString& sfen, int targetMoves, const Limits& limits, const std::atomic_bool& stop)
{
    if (targetMoves < 1 || targetMoves > kMaxSearchPlies || targetMoves % 2 == 0) return Verdict::Unknown;

    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) return Verdict::Unknown;
    const auto attacker = position.side_to_move();

    shogi::TsumeSearch search;
    const auto result = search.solve(position, attacker, targetMoves, limits.timeLimitMs, stop);
    switch (result.status) {
    case shogi::TsumeStatus::Mate:
        // 目標より短く詰む局面はエンジンも短いPVを返すので候補にしない
        if (result.plies != targetMoves) return Verdict::NotMateInTarget;
        break;
    case shogi::TsumeStatus::NoMate:
    case shogi::TsumeStatus::Limit:
        // Limit は目標手数以内の詰みがないことを表す（探索は深さ内で網羅的）
        return Verdict::NotMateInTarget;
    case shogi::TsumeStatus::Timeout:
    case shogi::TsumeStatus::Cancelled:
        return Verdict::Unknown;
    }

    // 目標手数（＋上乗せ）以内で詰む初手を数える。2つ見つかれば余詰として除外する
    int mating = 0;
    const int depth = std::min(kMaxSearchPlies, targetMoves - 1 + limits.alternativeExtraPlies);
    for (const auto& move : position.generate_checking_moves()) {
        auto child = position;
        child.do_move(move);
        const auto reply = search.solve(child, attacker, depth, limits.alternativeTimeLimitMs, stop);
        if (reply.status == shogi::TsumeStatus::Cancelled) return Verdict::Unknown;
        if (reply.status == shogi::TsumeStatus::Mate && ++mating >= 2) return Verdict::MultipleFirstMoves;
    }
    return Verdict::Candidate;
}

TsumeshogiCandidateScreener::Batch TsumeshogiCandidateScreener::generateBatch(
    const TsumeshogiPositionGenerator::Settings& settings, int targetMoves, int count, int maxGenerated,
    const Limits& limits, const CancelFlag& cancelFlag)
{
    Batch batch;
    TsumeshogiPositionGenerator generator;
    generator.setSettings(settings);
    const std::atomic_bool neverStop{false};
    const std::atomic_bool& stop = cancelFlag ? *cancelFlag : neverStop;

    while (batch.candidates.size() < count && batch.generated < maxGenerated && !stop.load()) {
        const QString sfen = generator.generate();
        if (sfen.isEmpty()) continue;
        ++batch.generated;
        switch (screen(sfen, targetMoves, limits, stop)) {
        case Verdict::Unknown:
            ++batch.unknown;
            batch.candidates.append(sfen);
            break;
        case Verdict::Candidate:
            batch.candidates.append(sfen);
            break;
        case Verdict::MultipleFirstMoves:
            ++batch.multipleFirstMoves;
            break;
        case Verdict::NotMateInTarget:
            break;
        }
    }
    return batch;
}
