#ifndef SHOGIMOVE_H
#define SHOGIMOVE_H

/// @file shogimove.h
/// @brief 将棋の指し手データ構造体の定義

#include <QPoint>
#include <QList>
#include <QDebug>
#include "shogitypes.h"

/**
 * @brief 1手分の指し手情報を保持する構造体
 *
 * 移動元・移動先の座標、動かした駒、取った駒、成りの有無を記録する。
 * 座標は0-indexed（0〜8が盤上、9/10が先手/後手の駒台）。
 *
 */
struct ShogiMove {
    QPoint fromSquare{0, 0};   ///< 移動元の座標（盤上:0-8、駒台:9=先手,10=後手）
    QPoint toSquare{0, 0};     ///< 移動先の座標（0-8）
    Piece movingPiece = Piece::None;   ///< 動かした自分の駒（SFEN表記）
    Piece capturedPiece = Piece::None; ///< 取った相手の駒（なければ None）
    bool isPromotion = false;  ///< 成りフラグ

    ShogiMove();
    ShogiMove(const QPoint &from, const QPoint &to, Piece moving, Piece captured, bool promotion);

    bool operator==(const ShogiMove& other) const;
};

QDebug operator<<(QDebug dbg, const ShogiMove& move);

#endif // SHOGIMOVE_H
