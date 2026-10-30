/// @file positioneditrules.cpp
/// @brief 局面編集の規則の実装

#include "positioneditrules.h"

#include "boardconstants.h"
#include "boardcoordinates.h"
#include "piecerules.h"
#include "shogiboard.h"

#include <QList>

namespace {
using BoardConstants::kBlackStandFile;
using BoardConstants::kPieceBoxFile;
using BoardConstants::kWhiteStandFile;

bool isStand(int file)
{
    return file == kBlackStandFile || file == kWhiteStandFile;
}

bool isBoardFile(int file)
{
    return file >= 1 && file <= Position::kFiles;
}

/// 盤・駒台・駒箱のマスか（先手の駒台は 1〜8 段、後手の駒台は 2〜9 段、駒箱は 1〜8 段に駒種を並べる）
bool isValidSquare(const QPoint& square)
{
    const int file = square.x();
    const int rank = square.y();
    return (isBoardFile(file) && rank >= 1 && rank <= Position::kRanks)
        || (file == kBlackStandFile && rank >= 1 && rank <= 8)
        || (file == kWhiteStandFile && rank >= 2 && rank <= 9) || (file == kPieceBoxFile && rank >= 1 && rank <= 8);
}

/// 盤の筋 file に、同じ側の歩 pawn があるか（exceptRank の段は数えない）。移動と巡回の二歩の判定で共通
bool hasPawnOnFile(const Position& position, Piece pawn, int file, int exceptRank)
{
    if (pieceKind(pawn) != PieceKind::Pawn) return false;
    for (int rank = 1; rank <= Position::kRanks; ++rank) {
        if (rank != exceptRank && position.pieceAt(file, rank) == pawn) return true;
    }
    return false;
}
} // namespace

bool PositionEditRules::canMove(const Position& position, Piece source, Piece dest, int fileFrom, int fileTo)
{
    const bool toBoard = isBoardFile(fileTo);
    // 王・玉は盤と駒箱の間で移せるが、持ち駒にはできない
    if (pieceKind(source) == PieceKind::King && (isStand(fileFrom) || isStand(fileTo))) return false;
    // 二歩（同じ筋の中の移動は除く）
    if (toBoard && fileFrom != fileTo && hasPawnOnFile(position, source, fileTo, 0)) return false;
    // 盤上の味方の駒の上には置けない
    if (toBoard && dest != Piece::None && isBlackPiece(source) == isBlackPiece(dest)) return false;
    if (!BoardCoordinates::isAvailable(position, source, fileFrom)) return false;
    // 駒台から駒台へは同じ駒種だけ（駒台の dest は駒種を表すセル）
    if (fileFrom == kBlackStandFile && fileTo == kWhiteStandFile && source != toBlack(dest)) return false;
    if (fileFrom == kWhiteStandFile && fileTo == kBlackStandFile && source != toWhite(dest)) return false;
    // 相手の玉は取れない
    if (toBoard && ((dest == Piece::BlackKing && isWhitePiece(source)) || (dest == Piece::WhiteKing && isBlackPiece(source))))
        return false;
    return true;
}

std::optional<Position> PositionEditRules::moved(const Position& position, const QPoint& from, const QPoint& to,
                                                 Turn pieceBoxSide)
{
    if (!isValidSquare(from) || !isValidSquare(to) || from == to) return std::nullopt;
    const int fileFrom = from.x();
    const int fileTo = to.x();
    Piece source = BoardCoordinates::pieceAt(position, fileFrom, from.y());
    const Piece dest = BoardCoordinates::pieceAt(position, fileTo, to.y());
    // 駒箱は先後共通の在庫。盤上に置く側を、二歩などの判定の前に決める
    if (fileFrom == kPieceBoxFile && pieceBoxSide == Turn::White) source = toWhite(source);
    if (source == Piece::None) return std::nullopt;
    // 駒台・駒箱へは、その駒種のセルにだけ戻せる
    if (!isBoardFile(fileTo) && toBlack(demote(source)) != toBlack(dest)) return std::nullopt;
    if (!canMove(position, source, dest, fileFrom, fileTo)) return std::nullopt;

    Position next = position;
    if (isBoardFile(fileTo)) {
        // 盤上の相手の駒を取ったら、取った側の持ち駒にする
        if (dest != Piece::None) next.addToHand(capturedToHand(dest));
    } else if (fileTo != kPieceBoxFile) {
        // 駒台に移したら、その駒台の駒（セルの駒種）を1枚増やす。駒箱は盤と駒台に無い駒なので数えない
        next.addToHand(demote(dest));
    }
    if (isStand(fileFrom)) next.takeFromHand(source);
    if (isBoardFile(fileFrom)) next.setPieceAt(BoardSquare{fileFrom, from.y()}, Piece::None);
    if (isBoardFile(fileTo)) {
        // 行き所のない段に置いた歩・香・桂は成駒にする
        const Piece placed = PieceRules::isDeadSquare(source, to.y()) ? promote(source) : source;
        next.setPieceAt(BoardSquare{fileTo, to.y()}, placed);
    }
    return next;
}

std::optional<Position> PositionEditRules::cycled(const Position& position, BoardSquare square)
{
    if (!square.isValid()) return std::nullopt;
    const Piece current = position.pieceAt(square);
    if (current == Piece::None) return std::nullopt;

    // 巡回の順: 先手の生駒 → 先手の成駒 → 後手の生駒 → 後手の成駒（成れない駒は先手 → 後手）
    const Piece base = toBlack(demote(current));
    QList<Piece> cycle{base};
    if (isPromotable(base)) cycle << promote(base);
    cycle << toWhite(base);
    if (isPromotable(base)) cycle << toWhite(promote(base));

    // 行き所のない段と二歩になる形は飛ばす
    const qsizetype index = cycle.indexOf(current);
    for (qsizetype step = 1; step <= cycle.size(); ++step) {
        const Piece candidate = cycle.at((index + step) % cycle.size());
        if (PieceRules::isDeadSquare(candidate, square.rank)) continue;
        if (hasPawnOnFile(position, candidate, square.file, square.rank)) continue;
        Position next = position;
        next.setPieceAt(square, candidate);
        return next;
    }
    return std::nullopt;
}

Position PositionEditRules::cleared(const Position& position)
{
    Position empty;
    empty.setSideToMove(position.sideToMove());
    empty.setMoveNumber(position.moveNumber());
    return empty;
}

bool PositionEditRules::apply(ShogiBoard& board, const QPoint& from, const QPoint& to, Turn pieceBoxSide)
{
    const std::optional<Position> next = moved(board.position(), from, to, pieceBoxSide);
    if (!next) return false;
    board.replacePosition(*next);
    return true;
}

bool PositionEditRules::cyclePiece(ShogiBoard& board, BoardSquare square)
{
    const std::optional<Position> next = cycled(board.position(), square);
    if (!next) return false;
    board.replacePosition(*next);
    return true;
}

void PositionEditRules::clear(ShogiBoard& board)
{
    board.setPosition(cleared(board.position()));
}
