#ifndef BOARDCOORDINATES_H
#define BOARDCOORDINATES_H

/// @file boardcoordinates.h
/// @brief 盤面表示の座標（1始まり、駒台は筋10・11）と駒・USI の指し手との対応

#include <QPoint>

#include "boardconstants.h"
#include "shogiposition.h"
#include "shogitypes.h"
#include "usimove.h"

/**
 * @brief 盤面表示とクリック操作で使う座標の対応表
 *
 * 盤上は筋・段とも1〜9。駒台は筋 10（先手）・11（後手）で、段が駒の並びを表す
 * （先手は歩1・香2・桂3・銀4・金5・角6・飛7・玉8、後手は玉2・飛3・角4・金5・銀6・桂7・香8・歩9）。
 * 駒箱（筋12）は先手の駒台と同じ並び。この対応はここだけで決める。
 */
namespace BoardCoordinates {

/// 駒台・駒箱の座標にある駒（並びに無い段は None）
inline Piece standPieceAt(int file, int rank)
{
    if (file == BoardConstants::kBlackStandFile || file == BoardConstants::kPieceBoxFile) {
        switch (rank) {
        case 1: return Piece::BlackPawn;
        case 2: return Piece::BlackLance;
        case 3: return Piece::BlackKnight;
        case 4: return Piece::BlackSilver;
        case 5: return Piece::BlackGold;
        case 6: return Piece::BlackBishop;
        case 7: return Piece::BlackRook;
        case 8: return Piece::BlackKing;
        default: return Piece::None;
        }
    }
    if (file == BoardConstants::kWhiteStandFile) {
        switch (rank) {
        case 2: return Piece::WhiteKing;
        case 3: return Piece::WhiteRook;
        case 4: return Piece::WhiteBishop;
        case 5: return Piece::WhiteGold;
        case 6: return Piece::WhiteSilver;
        case 7: return Piece::WhiteKnight;
        case 8: return Piece::WhiteLance;
        case 9: return Piece::WhitePawn;
        default: return Piece::None;
        }
    }
    return Piece::None;
}

/// 表示の座標の駒（盤上はマスの駒、駒台・駒箱はそのセルに置く駒の種類。範囲外は None）
inline Piece pieceAt(const Position& position, int file, int rank)
{
    if (file >= 1 && file <= BoardConstants::kBoardSize)
        return (rank >= 1 && rank <= BoardConstants::kBoardSize) ? position.pieceAt(file, rank) : Piece::None;
    return standPieceAt(file, rank);
}

/// 駒台・駒箱の座標 file に source の駒が1枚以上あるか（盤上の筋は常に true。駒箱は盤と駒台に無い駒）
inline bool isAvailable(const Position& position, Piece source, int file)
{
    if (file == BoardConstants::kPieceBoxFile) return position.pieceBox().value(toBlack(demote(source)), 0) > 0;
    if (file == BoardConstants::kBlackStandFile || file == BoardConstants::kWhiteStandFile)
        return position.handCount(source) > 0;
    return true;
}

/// 駒台の座標（成駒は生駒の位置）。駒台に置けない駒は QPoint()
inline QPoint standSquareOf(Piece piece)
{
    const Piece base = demote(piece);
    const bool black = isBlackPiece(base);
    const int file = black ? BoardConstants::kBlackStandFile : BoardConstants::kWhiteStandFile;
    for (int rank = 1; rank <= 9; ++rank) {
        if (standPieceAt(file, rank) == base) return QPoint(file, rank);
    }
    return QPoint();
}

/// 指し手の移動元の表示座標（駒打ちは指す側の駒台）
inline QPoint fromSquare(const UsiMove& move, Turn mover)
{
    if (move.isDrop()) return standSquareOf(move.dropPieceFor(mover));
    return QPoint(move.from.file, move.from.rank);
}

/// 指し手の移動先の表示座標
inline QPoint toSquare(const UsiMove& move)
{
    return QPoint(move.to.file, move.to.rank);
}

/// 表示座標の操作（盤上の移動・駒台からの駒打ち）を USI の指し手にする。盤外・駒台への移動は nullopt
inline std::optional<UsiMove> usiMoveFromSquares(const QPoint& from, const QPoint& to, bool promote)
{
    UsiMove move;
    move.to = BoardSquare{to.x(), to.y()};
    if (from.x() >= 1 && from.x() <= BoardConstants::kBoardSize) {
        move.from = BoardSquare{from.x(), from.y()};
        move.promote = promote;
    } else if (from.x() == BoardConstants::kBlackStandFile || from.x() == BoardConstants::kWhiteStandFile) {
        move.drop = toBlack(standPieceAt(from.x(), from.y()));
    } else {
        return std::nullopt;
    }
    if (!move.isValid()) return std::nullopt;
    return move;
}

} // namespace BoardCoordinates

#endif // BOARDCOORDINATES_H
