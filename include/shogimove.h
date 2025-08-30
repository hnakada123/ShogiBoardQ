#ifndef SHOGIMOVE_H
#define SHOGIMOVE_H

#include <QPoint>
#include <iostream>
#include <QVector>
#include <QDebug>

struct ShogiMove {
    // 駒の移動元のマス
    QPoint fromSquare;

    // 駒の移動先のマス
    QPoint toSquare;

    // 指した自分の駒情報
    QChar movingPiece;

    // 捕獲した駒情報（駒が無ければ、空文字（' '）で初期化）
    QChar capturedPiece;

    // 駒が成るかどうかを示す情報（成る: true、不成: false）
    bool isPromotion;

    // デフォルトコンストラクタ
    ShogiMove();

    // コンストラクタ
    ShogiMove(const QPoint &from, const QPoint &to, QChar moving, QChar captured, bool promotion);

    // 構造体ShogiMoveの比較演算子定義
    bool operator==(const ShogiMove& other) const;

    // 構造体ShogiMoveのデバッグプリント用演算子"<<"の定義
    friend std::ostream& operator<<(std::ostream& os, const ShogiMove& move);
};

QDebug operator<<(QDebug dbg, const ShogiMove& move);

#endif // SHOGIMOVE_H
