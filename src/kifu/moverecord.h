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

/// 局面に合わない理由の文（英語。棋譜の読み込みの警告に使う。%1 は指し手）
inline QString mismatchText(Position::MoveMismatch mismatch)
{
    switch (mismatch) {
    case Position::MoveMismatch::InvalidMove: return QStringLiteral("Invalid move: %1");
    case Position::MoveMismatch::EmptySource: return QStringLiteral("Source square is empty: %1");
    case Position::MoveMismatch::OpponentPiece: return QStringLiteral("The piece belongs to the side not to move: %1");
    case Position::MoveMismatch::CapturesOwnPiece: return QStringLiteral("The move captures a piece of the side to move: %1");
    case Position::MoveMismatch::DropNotInHand: return QStringLiteral("The dropped piece is not in hand: %1");
    case Position::MoveMismatch::DropOnOccupied: return QStringLiteral("The piece is dropped on an occupied square: %1");
    case Position::MoveMismatch::None: break;
    }
    return QStringLiteral("Move cannot be applied: %1");
}

/**
 * @brief position で手番の側が move を指し、指した手の棋譜の1行（RecordEntry）を作る
 *
 * 動かす駒を局面から決め（Position::describe）、指し（position は指した後の局面になる）、表記を
 * KifMoveText::format で作る（「同」は previousTo、KI2 の修飾は notes）。指した側は局面の手番。
 * 合法手かは確かめず、手番側の手として局面に合うか（Position::mismatchOf。rule は持ち駒の扱い）だけを見る。
 * 合わなければ position を変えずに nullopt を返し、error に理由（mismatchText）を入れる。
 * 棋譜の読み込み（KifLineBuilder::addMove）と対局の着手（GameBoard）が使う。
 */
inline std::optional<RecordEntry> play(Position& position, const UsiMove& move, BoardSquare previousTo,
                                       const RecordTime& time = {}, const KifMoveText::Notes& notes = {},
                                       Position::DropRule rule = Position::DropRule::RequireHand,
                                       QString* error = nullptr)
{
    const Turn mover = position.sideToMove();
    // 相手の駒を動かす手・自分の駒を取る手は、反則手としても棋譜に残せない（指した側と動いた駒・取った駒が
    // 食い違い、以降の局面が崩れる）
    if (const Position::MoveMismatch mismatch = position.mismatchOf(move, rule); mismatch != Position::MoveMismatch::None) {
        if (error) *error = mismatchText(mismatch).arg(move.toString());
        return std::nullopt;
    }
    const ShogiMove described = position.describe(move);
    if (!position.apply(move, rule)) {
        if (error) *error = mismatchText(Position::MoveMismatch::None).arg(move.toString());
        return std::nullopt;
    }
    RecordEntry entry = RecordEntry::played(described, KifMoveText::format(mover, described.movingPiece, move, previousTo, notes),
                                            position.toSfen(), time);
    entry.mover = mover;
    return entry;
}

} // namespace MoveRecord

#endif // MOVERECORD_H
