#ifndef POSITIONVALIDATOR_H
#define POSITIONVALIDATOR_H

/// @file positionvalidator.h
/// @brief 局面として成り立つかの検査（駒の数・玉・手番でない側への王手）と、手番側の王手・合法手の数

#include "shogiposition.h"

/**
 * @brief 局面の検査
 *
 * SFEN の文法と行き所のない駒は Position::fromSfen が見る。ここでは盤と持ち駒として成り立つか
 * （駒の種類ごとの枚数が上限以内・持ち駒に玉が無い・玉は各側1枚まで・玉が少なくとも1枚）と、
 * 手番でない側の玉に王手がかかっていないかを見る。詰将棋の慣例に合わせ、攻方の玉が無い局面は認める。
 * 王手と合法手はビットボードの合法手生成（EngineMoveValidator）で決める。
 * CLI・自動化 API の validate-sfen、局面集、詰将棋問題一覧、対局の開始で同じ規則を使う。
 */
namespace PositionValidator {

/// 局面として成り立たない理由（盤と持ち駒の問題を先に見る）
enum class Problem {
    None,
    PieceCountOver,   ///< 駒の種類ごとの枚数（盤と持ち駒の合計。成駒は元の駒に数える）が上限を超える
    KingInHand,       ///< 持ち駒に玉がある
    DuplicatedKing,   ///< 同じ側の玉が盤に2枚以上
    NoKing,           ///< 盤に玉が1枚も無い
    OpponentInCheck,  ///< 手番でない側の玉に王手がかかっている（対局では生じない）
};

/// 検査の結果
struct Report {
    Problem problem = Problem::None;
    bool blackKing = false;      ///< 先手の玉が盤にある
    bool whiteKing = false;      ///< 後手の玉が盤にある
    bool inCheck = false;        ///< 手番側の玉に王手がかかっている
    bool opponentInCheck = false;
    int legalMoveCount = -1;     ///< 手番側の合法手の数（盤と持ち駒として成り立たなければ -1）

    bool isValid() const { return problem == Problem::None; }
    /// 盤と持ち駒として成り立つ（手番でない側への王手は問わない。棋譜の表記の説明の図のような局面）
    bool isWellFormed() const { return problem == Problem::None || problem == Problem::OpponentInCheck; }
};

Report check(const Position& position);
/// 盤と持ち駒の問題だけを見る（王手と合法手は調べないので軽い。局面の識別子を作るときなど）
Problem boardProblem(const Position& position);

} // namespace PositionValidator

#endif // POSITIONVALIDATOR_H
