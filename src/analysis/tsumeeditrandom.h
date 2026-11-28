#ifndef TSUMEEDITRANDOM_H
#define TSUMEEDITRANDOM_H

/// @file tsumeeditrandom.h
/// @brief 詰将棋の候補局面を作るときの乱数の選び方（逆算と盤面編集で共通）

#include <types.h>

#include <algorithm>
#include <iterator>
#include <random>
#include <vector>

namespace TsumeEditRandom {

/// lo〜hi（両端を含む）の整数。lo >= hi なら lo
inline int randInt(std::mt19937_64& rng, int lo, int hi)
{
    if (lo >= hi) return lo;
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

/// percent % の確率で true
inline bool randChance(std::mt19937_64& rng, int percent)
{
    return randInt(rng, 0, 99) < percent;
}

template <typename T>
void shuffleList(std::vector<T>& items, std::mt19937_64& rng)
{
    std::shuffle(items.begin(), items.end(), rng);
}

/// ランダムに駒種（歩〜飛）を選ぶ（詰将棋で使いやすい駒にやや重み付け）
inline shogi::PieceType randomUsefulPiece(std::mt19937_64& rng)
{
    using shogi::PieceType;
    static constexpr PieceType kWeighted[] = {
        PieceType::Gold, PieceType::Gold, PieceType::Gold,
        PieceType::Silver, PieceType::Silver, PieceType::Silver,
        PieceType::Rook, PieceType::Rook,
        PieceType::Bishop, PieceType::Bishop,
        PieceType::Knight, PieceType::Knight,
        PieceType::Lance, PieceType::Lance,
        PieceType::Pawn, PieceType::Pawn
    };
    return kWeighted[static_cast<std::size_t>(randInt(rng, 0, static_cast<int>(std::size(kWeighted)) - 1))];
}

} // namespace TsumeEditRandom

#endif // TSUMEEDITRANDOM_H
