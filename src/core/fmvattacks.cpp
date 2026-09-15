/// @file fmvattacks.cpp
/// @brief 事前計算した利き・間のマスと占有ビットボードによる利き判定

#include "fmvattacks.h"

namespace fmv {

namespace {

enum StepPattern {
    PawnStep, KnightStep, SilverStep, GoldStep, KingStep, HorseStep, DragonStep,
    StepPatternNb
};

struct AttackTables {
    Bitboard81 step[2][kSquareNb][StepPatternNb]{};
    Bitboard81 rook[kSquareNb]{};
    Bitboard81 bishop[kSquareNb]{};
    Bitboard81 lance[2][kSquareNb]{};
    Bitboard81 between[kSquareNb][kSquareNb]{};
};

constexpr bool inBoard(int file, int rank) noexcept
{
    return file >= 0 && file < kBoardSize && rank >= 0 && rank < kBoardSize;
}

constexpr AttackTables makeAttackTables() noexcept
{
    AttackTables tables;
    for (int from = 0; from < kSquareNb; ++from) {
        const int file = squareFile(static_cast<Square>(from));
        const int rank = squareRank(static_cast<Square>(from));
        for (int ci = 0; ci < 2; ++ci) {
            const int forward = ci == 0 ? -1 : 1;
            for (int df = -1; df <= 1; ++df) {
                for (int dr = -1; dr <= 1; ++dr) {
                    if ((df == 0 && dr == 0) || !inBoard(file + df, rank + dr)) {
                        continue;
                    }
                    const Square to = toSquare(file + df, rank + dr);
                    tables.step[ci][from][KingStep].set(to);
                    if (df == 0 && dr == forward) {
                        tables.step[ci][from][PawnStep].set(to);
                    }
                    if (dr == forward || (df != 0 && dr == -forward)) {
                        tables.step[ci][from][SilverStep].set(to);
                    }
                    if (dr == forward || dr == 0 || (df == 0 && dr == -forward)) {
                        tables.step[ci][from][GoldStep].set(to);
                    }
                    if (df == 0 || dr == 0) {
                        tables.step[ci][from][HorseStep].set(to);
                    } else {
                        tables.step[ci][from][DragonStep].set(to);
                    }
                }
            }
            for (int df = -1; df <= 1; df += 2) {
                if (inBoard(file + df, rank + 2 * forward)) {
                    tables.step[ci][from][KnightStep].set(toSquare(file + df, rank + 2 * forward));
                }
            }
        }

        for (int df = -1; df <= 1; ++df) {
            for (int dr = -1; dr <= 1; ++dr) {
                if (df == 0 && dr == 0) {
                    continue;
                }
                Bitboard81 between;
                for (int f = file + df, r = rank + dr; inBoard(f, r); f += df, r += dr) {
                    const Square to = toSquare(f, r);
                    // 両端は含めない。移動先の駒は障害物ではなく捕獲対象になり得る。
                    tables.between[from][to] = between;
                    between.set(to);
                    if (df == 0 || dr == 0) {
                        tables.rook[from].set(to);
                    } else {
                        tables.bishop[from].set(to);
                    }
                    if (df == 0) {
                        tables.lance[dr < 0 ? 0 : 1][from].set(to);
                    }
                }
            }
        }
    }
    return tables;
}

// コンパイル時に構築し、初回のGUI操作でもテーブル生成を行わない。
constexpr AttackTables kAttackTables = makeAttackTables();

Bitboard81 stepAttackersTo(const EnginePosition& pos, Square sq, Color attacker) noexcept
{
    const auto& pieces = pos.pieceOcc[static_cast<int>(attacker)];
    // 移動先から移動元を探すため、反対色の移動方向を使う。
    const auto& steps = kAttackTables.step[static_cast<int>(opposite(attacker))][sq];
    const Bitboard81 golds = pieces[static_cast<int>(PieceType::Gold)]
                            | pieces[static_cast<int>(PieceType::ProPawn)]
                            | pieces[static_cast<int>(PieceType::ProLance)]
                            | pieces[static_cast<int>(PieceType::ProKnight)]
                            | pieces[static_cast<int>(PieceType::ProSilver)];
    return (steps[PawnStep] & pieces[static_cast<int>(PieceType::Pawn)])
           | (steps[KnightStep] & pieces[static_cast<int>(PieceType::Knight)])
           | (steps[SilverStep] & pieces[static_cast<int>(PieceType::Silver)])
           | (steps[GoldStep] & golds)
           | (steps[KingStep] & pieces[static_cast<int>(PieceType::King)])
           | (steps[HorseStep] & pieces[static_cast<int>(PieceType::Horse)])
           | (steps[DragonStep] & pieces[static_cast<int>(PieceType::Dragon)]);
}

template<bool StopAfterFirst>
Bitboard81 collectAttackers(const EnginePosition& pos, Square sq, Color attacker) noexcept
{
    if (sq >= kSquareNb || attacker >= Color::ColorNb) {
        return {};
    }

    Bitboard81 result = stepAttackersTo(pos, sq, attacker);
    if constexpr (StopAfterFirst) {
        if (result.any()) {
            return result;
        }
    }

    const auto& pieces = pos.pieceOcc[static_cast<int>(attacker)];
    Bitboard81 sliders = (kAttackTables.rook[sq]
                          & (pieces[static_cast<int>(PieceType::Rook)]
                             | pieces[static_cast<int>(PieceType::Dragon)]))
                         | (kAttackTables.bishop[sq]
                            & (pieces[static_cast<int>(PieceType::Bishop)]
                               | pieces[static_cast<int>(PieceType::Horse)]))
                         | (kAttackTables.lance[static_cast<int>(opposite(attacker))][sq]
                            & pieces[static_cast<int>(PieceType::Lance)]);
    while (sliders.any()) {
        const Square from = sliders.popFirst();
        if ((kAttackTables.between[sq][from] & pos.occupied).none()) {
            result.set(from);
            if constexpr (StopAfterFirst) {
                return result;
            }
        }
    }
    return result;
}

} // namespace

Bitboard81 attackersTo(const EnginePosition& pos, Square sq, Color attacker) noexcept
{
    return collectAttackers<false>(pos, sq, attacker);
}

bool isSquareAttacked(const EnginePosition& pos, Square sq, Color attacker) noexcept
{
    return collectAttackers<true>(pos, sq, attacker).any();
}

bool pieceAttacksSquare(const EnginePosition& pos, Color side, PieceType piece,
                        Square from, Square to) noexcept
{
    if (from >= kSquareNb || to >= kSquareNb || side >= Color::ColorNb) {
        return false;
    }
    const auto& steps = kAttackTables.step[static_cast<int>(side)][from];
    Bitboard81 rays;
    switch (piece) {
    case PieceType::Pawn:
        return steps[PawnStep].test(to);
    case PieceType::Knight:
        return steps[KnightStep].test(to);
    case PieceType::Silver:
        return steps[SilverStep].test(to);
    case PieceType::Gold:
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
        return steps[GoldStep].test(to);
    case PieceType::King:
        return steps[KingStep].test(to);
    case PieceType::Lance:
        rays = kAttackTables.lance[static_cast<int>(side)][from];
        break;
    case PieceType::Horse:
        if (steps[HorseStep].test(to)) {
            return true;
        }
        [[fallthrough]];
    case PieceType::Bishop:
        rays = kAttackTables.bishop[from];
        break;
    case PieceType::Dragon:
        if (steps[DragonStep].test(to)) {
            return true;
        }
        [[fallthrough]];
    case PieceType::Rook:
        rays = kAttackTables.rook[from];
        break;
    default:
        return false;
    }
    return rays.test(to) && (kAttackTables.between[from][to] & pos.occupied).none();
}

} // namespace fmv
