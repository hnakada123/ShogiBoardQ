#ifndef JKFFORMATTER_H
#define JKFFORMATTER_H

/// @file jkfformatter.h
/// @brief JKF形式の変換ヘルパ関数の定義

#include <QJsonObject>
#include <QString>
#include "recordtime.h"

#include "kifubranchnode.h"
#include "shogitypes.h"
#include "usimove.h"

class Position;

/**
 * @brief JKFエクスポート用フォーマット変換ヘルパ関数群
 */
namespace JkfFormatter {

/// 消費時間（RecordTime）を JKF の time（now は分・秒、total は時・分・秒）にする。無効なら空
QJsonObject timeToJkf(const RecordTime& time);

/// 開始局面の SFEN から JKF の initial.data（board・color・hands）を作る。読めない SFEN は空
QJsonObject sfenToJkfData(const QString& sfen);

/**
 * @brief 局面と指し手から JKF の move を作る
 * @param board 指す前の局面
 * @param mover 指す側（color）
 * @param move 指し手
 * @param previousTo 直前の手の移動先。同じマスなら same を付ける
 *
 * piece は動かす前の駒、capture は取る駒（成駒ならその種類）。promote は成る手で true、
 * 成れるのに成らない手で false、成れない手では書かない。relative は KI2 の修飾語・「打」を記号にしたもので、
 * 同じ駒で同じマスへ動ける駒がほかにあるときだけ書く。
 */
QJsonObject moveToJkf(const Position& board, Turn mover, const UsiMove& move, BoardSquare previousTo);

} // namespace JkfFormatter

#endif // JKFFORMATTER_H
