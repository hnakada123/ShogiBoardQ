#ifndef TSUMEEDITBOARD_H
#define TSUMEEDITBOARD_H

/// @file tsumeeditboard.h
/// @brief 詰将棋の候補局面を組み立てる盤（片玉の盤と攻方の持駒）と、盤面の規則・利きの計算

#include "tsumeedittypes.h"

#include <QString>

#include <position.h>
#include <types.h>

#include <array>
#include <vector>

/// 逆算・盤面編集・余詰リペアが候補局面を組み立てるときに使う盤の値と、その規則。
/// 乱数は使わない（候補の選び方は TsumeRetrogradeOps・TsumePositionEdits が決める）。
/// Hayanagi の型を使うので、局面演算の実装（.cpp）からだけ include する。ほかの部品に見せる値
/// （駒数の上限 Limits・作った候補 Candidate）は tsumeedittypes.h。
namespace TsumeEditBoard {

constexpr int kBoardSquares = 81;

/// コンパクトな盤面状態（Hayanagi の符号付き駒コード・座標系と完全一致）
struct State {
    std::array<int, kBoardSquares> board{};
    std::array<int, 8> attackHand{}; ///< index 1..7 (Pawn..Rook)
};

/// SFEN を読む（後手玉が1枚・先手玉0枚でなければ false）。parsed には読んだ局面を返す
bool loadStateFromSfen(const QString& sfen, State& out, shogi::Position* parsed = nullptr);

/// 後手玉のマス（無ければ -1）
int findWhiteKing(const State& state);

/// 盤面ルール（二歩・行き所のない駒・駒枚数上限・Limits）を満たせば SFEN を構築する。満たさなければ空
QString buildSfen(const State& state, shogi::Color sideToMove, const Limits& limits,
                  bool enforceLimits = true, bool requireAttackPiece = true);

/// color の駒 pt が盤面 state 上で from から to へ移動（利き）できるか
bool pieceReaches(shogi::Color color, shogi::PieceType pt, int from, int to, const State& state);

/// 飛び駒（香・角・飛・馬・龍）か
bool isSlider(shogi::PieceType pt);

/// from と to が一直線上にあるとき、間のマス一覧を返す（直線上でなければ空）
std::vector<int> squaresBetween(int from, int to);

/// from -> to の動きが color の成条件を満たすか
bool canPromoteMove(shogi::Color color, shogi::PieceType unpromoted, int from, int to);

/// USI の指し手文字列を組み立てる
QString makeMoveUsi(int from, int to, bool promoted);
QString makeDropUsi(shogi::PieceType pt, int to);

/// State 上で後手玉に王手している攻方駒のマス一覧
std::vector<int> findCheckers(const State& state, int kingSq);

/// pieceSq に置くと kingSq の玉に王手をかけられる攻方駒の種類（復活・捨駒の逆算用）
std::vector<shogi::PieceType> checkingPieceTypesFor(int pieceSq, int kingSq, const State& state);

} // namespace TsumeEditBoard

#endif // TSUMEEDITBOARD_H
