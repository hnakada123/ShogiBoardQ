#ifndef PIECEMOVERULES_H
#define PIECEMOVERULES_H

/// @file piecemoverules.h
/// @brief 駒の移動ルール検証（禁じ手チェック・強制成り判定）

#include "shogitypes.h"

class ShogiBoard;

/**
 * @brief 駒の移動ルールに関するスタティックユーティリティ
 *
 * ShogiGameController から局面編集時の禁じ手チェック・強制成り判定を分離。
 * すべてのメソッドは状態を持たない純粋関数として実装。
 */
class PieceMoveRules
{
public:
    /// 移動の総合チェック（二歩・味方駒・駒台在庫・駒台間移動・玉取りを検証）
    static bool checkMovePiece(ShogiBoard* board,
                               Piece source, Piece dest,
                               int fileFrom, int fileTo);

    /// 二歩チェック
    static bool checkTwoPawn(ShogiBoard* board,
                             Piece source, int fileFrom, int fileTo);

    /// 味方駒への移動チェック
    static bool checkWhetherAllyPiece(Piece source, Piece dest,
                                      int fileFrom, int fileTo);

    /// 駒台の在庫チェック
    static bool checkNumberStandPiece(ShogiBoard* board,
                                      Piece source, int fileFrom);

    /// 駒台間移動の妥当性チェック
    static bool checkFromPieceStandToPieceStand(Piece source, Piece dest,
                                                int fileFrom, int fileTo);

    /// 相手の玉を取る移動のチェック
    static bool checkGetKingOpponentPiece(Piece source, Piece dest);

    /// 行き所のない駒に対する強制成り判定
    static bool shouldAutoPromote(int fileTo, int rankTo, Piece source);
};

#endif // PIECEMOVERULES_H
