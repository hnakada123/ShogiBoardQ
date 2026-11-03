/// @file positionvalidator.cpp
/// @brief 局面の検査の実装

#include "positionvalidator.h"

#include "enginemovevalidator.h"
#include "piecerules.h"

#include <array>

namespace PositionValidator {

Problem boardProblem(const Position& position)
{
    std::array<int, kHandKindCount> totals{};
    std::array<int, 2> kings{};
    for (int file = 1; file <= 9; ++file) {
        for (int rank = 1; rank <= 9; ++rank) {
            const Piece piece = position.pieceAt(file, rank);
            if (piece == Piece::None) continue;
            const PieceKind kind = pieceKind(demote(piece));
            ++totals[static_cast<size_t>(kind)];
            if (kind == PieceKind::King) ++kings[pieceSide(piece) == Turn::Black ? 0 : 1];
        }
    }
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (int kind = 0; kind < kHandKindCount; ++kind) {
            const int count = position.handCount(makePiece(side, static_cast<PieceKind>(kind)));
            if (count <= 0) continue;
            if (static_cast<PieceKind>(kind) == PieceKind::King) return Problem::KingInHand;
            totals[static_cast<size_t>(kind)] += count;
        }
    }
    // 駒の種類ごとの枚数は1組の駒の枚数まで（成駒は成る前の駒として数える）
    for (size_t kind = 0; kind < totals.size(); ++kind)
        if (totals[kind] > PieceRules::kSetCounts[kind]) return Problem::PieceCountOver;
    if (kings[0] > 1 || kings[1] > 1) return Problem::DuplicatedKing;
    if (kings[0] == 0 && kings[1] == 0) return Problem::NoKing;
    return Problem::None;
}

Report check(const Position& position)
{
    Report report;
    report.blackKing = position.countOnBoard(makePiece(Turn::Black, PieceKind::King)) > 0;
    report.whiteKing = position.countOnBoard(makePiece(Turn::White, PieceKind::King)) > 0;
    report.problem = boardProblem(position);
    if (report.problem != Problem::None) return report;

    const Turn mover = position.sideToMove();
    const EngineMoveValidator::Context context = EngineMoveValidator::load(position, mover);
    report.inCheck = EngineMoveValidator::checkCount(context) > 0;
    report.opponentInCheck = EngineMoveValidator::isInCheck(position, opponent(mover));
    report.legalMoveCount = EngineMoveValidator::legalMoveCount(context);
    if (report.opponentInCheck) report.problem = Problem::OpponentInCheck;
    return report;
}

} // namespace PositionValidator
