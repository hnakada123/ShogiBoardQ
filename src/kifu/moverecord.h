#ifndef MOVERECORD_H
#define MOVERECORD_H

/// @file moverecord.h
/// @brief 指す前の局面と指し手から棋譜の1行を作る（棋譜の読み込みと対局の着手で共通）

#include <QString>
#include <optional>

#include "kifmovetext.h"
#include "recordentry.h"
#include "shogiposition.h"
#include "usimove.h"

namespace MoveRecord {

/**
 * @brief position で手番の側が move を指し、指した手の棋譜の1行（RecordEntry）を作る
 *
 * 動かす駒を局面から決め（Position::describe）、指し（position は指した後の局面になる）、表記を
 * KifMoveText::format で作る（「同」は previousTo、KI2 の修飾は notes）。指した側は局面の手番。
 * 合法手かは確かめない（rule は持ち駒の扱い）。指せなければ position を変えずに nullopt を返し、
 * error に理由（英語。棋譜の読み込みの警告に使う）を入れる。
 * 棋譜の読み込み（KifLineBuilder::addMove）と対局の着手（GameBoard）が使う。
 */
inline std::optional<RecordEntry> play(Position& position, const UsiMove& move, BoardSquare previousTo,
                                       const RecordTime& time = {}, const KifMoveText::Notes& notes = {},
                                       Position::DropRule rule = Position::DropRule::RequireHand,
                                       QString* error = nullptr)
{
    const Turn mover = position.sideToMove();
    const ShogiMove described = position.describe(move);
    if (described.movingPiece == Piece::None) {
        if (error) *error = QStringLiteral("Source square is empty: %1").arg(move.toString());
        return std::nullopt;
    }
    // 相手の駒を動かす手は、反則手としても棋譜に残せない（指した側と動いた駒が食い違い、以降の手番がずれる）
    if (!move.isDrop() && pieceSide(described.movingPiece) != mover) {
        if (error) *error = QStringLiteral("The piece belongs to the side not to move: %1").arg(move.toString());
        return std::nullopt;
    }
    if (!position.apply(move, rule)) {
        if (error) *error = QStringLiteral("Move cannot be applied: %1").arg(move.toString());
        return std::nullopt;
    }
    RecordEntry entry = RecordEntry::played(described, KifMoveText::format(mover, described.movingPiece, move, previousTo, notes),
                                            position.toSfen(), time);
    entry.mover = mover;
    return entry;
}

} // namespace MoveRecord

#endif // MOVERECORD_H
