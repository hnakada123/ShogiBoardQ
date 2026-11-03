#ifndef PIECERULES_H
#define PIECERULES_H

/// @file piecerules.h
/// @brief 駒の規則（1組の駒の枚数・行き所のない駒・成れる段・持ち駒の書き出し順）を1か所に置く

#include <array>

#include "shogitypes.h"

namespace PieceRules {

/**
 * @brief 1組の駒（先手・後手の合計）の種類ごとの枚数（歩18・香4・桂4・銀4・金4・角2・飛2・玉2）
 *
 * 添字は成る前の駒の PieceKind（歩〜玉）。成駒は成る前の駒として数える。駒箱（Position::pieceBox）・
 * 局面の駒の数の上限（PositionValidator）・詰将棋の候補局面の駒（TsumeshogiPositionGenerator）で同じ表を使う。
 */
inline constexpr std::array<int, kHandKindCount> kSetCounts = {18, 4, 4, 4, 4, 2, 2, 2};

/// 1組の駒の、成る前の種類 kind（歩〜玉）の枚数
constexpr int setCount(PieceKind kind)
{
    return kSetCounts[static_cast<std::size_t>(kind)];
}

/// 敵陣（成れる段）か。段は 1〜9（先手から見た段）
constexpr bool inPromotionZone(Turn side, int rank)
{
    return side == Turn::Black ? rank <= 3 : rank >= 7;
}

/**
 * @brief 行き所のない段か（その段に置くと、それ以上動けない駒）
 *
 * 先手の歩・香は1段目、桂は1・2段目。後手は逆。成駒・ほかの駒は false。
 * 盤上の駒の必成（成らずに動けない）、駒打ちの禁止、局面図の検査で同じ規則を使う。
 */
constexpr bool isDeadSquare(Piece piece, int rank)
{
    if (piece == Piece::None) return false;
    const PieceKind kind = pieceKind(piece);
    // 先手から見た「奥からの段」（先手なら段そのもの、後手なら 10 − 段）
    const int depth = isBlackPiece(piece) ? rank : 10 - rank;
    switch (kind) {
    case PieceKind::Pawn:
    case PieceKind::Lance:
        return depth <= 1;
    case PieceKind::Knight:
        return depth <= 2;
    default:
        return false;
    }
}

/**
 * @brief 成ることもできた手か（成れる駒が、敵陣に入る・敵陣から出る・敵陣の中で動く）
 * @param moving 動かした駒（成る前の駒。色で敵陣を決める）
 *
 * 盤上の移動だけを対象にする。成らなかった手が true なら、KIF・KI2 では「不成」と書く。
 */
constexpr bool couldPromote(Piece moving, int fromRank, int toRank)
{
    if (!isPromotable(moving)) return false;
    const Turn side = pieceSide(moving);
    return inPromotionZone(side, fromRank) || inPromotionZone(side, toRank);
}

/// 持ち駒を書き出す順（SFEN・CSA・KIF の局面図・駒台。飛・角・金・銀・桂・香・歩）
inline constexpr std::array<PieceKind, 7> kHandOrder = {
    PieceKind::Rook, PieceKind::Bishop, PieceKind::Gold, PieceKind::Silver,
    PieceKind::Knight, PieceKind::Lance, PieceKind::Pawn,
};

} // namespace PieceRules

#endif // PIECERULES_H
