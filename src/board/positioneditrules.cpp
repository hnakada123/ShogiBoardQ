/// @file positioneditrules.cpp
/// @brief 局面編集で駒を動かす規則の実装

#include "positioneditrules.h"

#include "boardconstants.h"
#include "shogiboard.h"

namespace {
using BoardConstants::kBlackStandFile;
using BoardConstants::kPieceBoxFile;
using BoardConstants::kWhiteStandFile;

bool isStand(int file)
{
    return file == kBlackStandFile || file == kWhiteStandFile;
}

/// 盤・駒台・駒箱のマスか（先手の駒台は 1〜8 段、後手の駒台は 2〜9 段、駒箱は 1〜8 段に駒種を並べる）
bool isValidSquare(const QPoint& square)
{
    const int file = square.x();
    const int rank = square.y();
    return (file >= 1 && file <= 9 && rank >= 1 && rank <= 9) || (file == kBlackStandFile && rank >= 1 && rank <= 8)
        || (file == kWhiteStandFile && rank >= 2 && rank <= 9) || (file == kPieceBoxFile && rank >= 1 && rank <= 8);
}

/// 盤の筋 fileTo に同じ側の歩があれば二歩（同じ筋の中の移動は除く）
bool makesTwoPawns(const ShogiBoard& board, Piece source, int fileFrom, int fileTo)
{
    if (fileFrom == fileTo || fileTo >= kBlackStandFile) return false;
    if (source != Piece::BlackPawn && source != Piece::WhitePawn) return false;
    for (int rank = 1; rank <= 9; ++rank)
        if (board.pieceCharacter(fileTo, rank) == source) return true;
    return false;
}
} // namespace

bool PositionEditRules::canMove(const ShogiBoard& board, Piece source, Piece dest, int fileFrom, int fileTo)
{
    const bool toBoard = fileTo < kBlackStandFile;
    // 王・玉は盤と駒箱の間で移せるが、持ち駒にはできない
    if ((source == Piece::BlackKing || source == Piece::WhiteKing) && (isStand(fileFrom) || isStand(fileTo)))
        return false;
    if (makesTwoPawns(board, source, fileFrom, fileTo)) return false;
    // 盤上の味方の駒の上には置けない
    if (toBoard && dest != Piece::None && isBlackPiece(source) == isBlackPiece(dest)) return false;
    if (!board.isPieceAvailableOnStand(source, fileFrom)) return false;
    // 駒台から駒台へは同じ駒種だけ（駒台の dest は駒種を表すセル）
    if (fileFrom == kBlackStandFile && fileTo == kWhiteStandFile && source != toBlack(dest)) return false;
    if (fileFrom == kWhiteStandFile && fileTo == kBlackStandFile && source != toWhite(dest)) return false;
    // 相手の玉は取れない
    if (toBoard && ((dest == Piece::BlackKing && isWhitePiece(source)) || (dest == Piece::WhiteKing && isBlackPiece(source))))
        return false;
    return true;
}

bool PositionEditRules::apply(ShogiBoard& board, const QPoint& from, const QPoint& to, Turn pieceBoxSide)
{
    if (!isValidSquare(from) || !isValidSquare(to) || from == to) return false;
    const int fileFrom = from.x();
    const int fileTo = to.x();
    Piece source = board.pieceCharacter(fileFrom, from.y());
    const Piece dest = board.pieceCharacter(fileTo, to.y());
    // 駒箱は先後共通の在庫。盤上に置く側を、二歩などの判定の前に決める
    if (fileFrom == kPieceBoxFile && pieceBoxSide == Turn::White) source = toWhite(source);
    if (source == Piece::None) return false;
    // 駒台・駒箱へは、その駒種のセルにだけ戻せる
    if (fileTo >= kBlackStandFile && toBlack(demote(source)) != toBlack(dest)) return false;
    if (!canMove(board, source, dest, fileFrom, fileTo)) return false;
    board.updateBoardAndPieceStand(source, dest, fileFrom, from.y(), fileTo, to.y());
    return true;
}
