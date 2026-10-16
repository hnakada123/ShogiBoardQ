#ifndef CSAMOVECONVERTER_H
#define CSAMOVECONVERTER_H

/// @file csamoveconverter.h
/// @brief CSA の指し手（"+7776FU"）と USI の指し手の変換

#include <QString>
#include <optional>

#include "shogitypes.h"
#include "usimove.h"

class ShogiBoard;

/**
 * @brief CSA の指し手と USI の指し手の変換（盤は読むだけで動かさない）
 *
 * CSA の駒種は指した後の駒（成る手は成った駒）なので、どちらの向きも指す前の盤の駒を読む。
 * 駒の名前・CSA の駒種の対応は ShogiNotation の表を使う。
 */
namespace CsaMoveConverter {

/// 指し手の CSA の表記（"+7776FU"・"-0055KA"）。指す前の盤で駒を読む。動かす駒がなければ空
QString toCsa(const UsiMove& move, Turn mover, const ShogiBoard& board);

/// CSA の指し手を、指す前の盤での USI の指し手にする（成りは盤の駒と CSA の駒種を比べて決める）。
/// 書式・手番・駒が盤と合わなければ nullopt
std::optional<UsiMove> toUsi(const QString& csaMove, const ShogiBoard& board);

} // namespace CsaMoveConverter

#endif // CSAMOVECONVERTER_H
