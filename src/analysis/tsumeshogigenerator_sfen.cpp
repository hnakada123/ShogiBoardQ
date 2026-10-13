/// @file tsumeshogigenerator_sfen.cpp
/// @brief TsumeshogiGenerator の駒を1枚除いた局面の作成（トリミング）の実装

#include "tsumeshogigenerator.h"
#include "shogiboard.h"

namespace {

/// 局面の SFEN（手番は攻方=先手、手数は1に固定。詰将棋は常に攻方の手番から始まる）
QString tsumeSfen(ShogiBoard& board)
{
    return board.convertBoardToSfen() + QStringLiteral(" b ") + board.convertStandToSfen() + QStringLiteral(" 1");
}

} // namespace

QList<TsumeshogiGenerator::TrimCandidate>
TsumeshogiGenerator::enumerateRemovablePieces(const QString& sfen)
{
    QList<TrimCandidate> candidates;
    ShogiBoard board;
    if (!board.trySetSfen(sfen)) {
        return candidates;
    }

    // 盤上の駒を列挙（玉以外）。上の段から、段の中は9筋から順に並べる。
    // 守方持駒は除去対象外だが、盤上の守方駒は除去可
    for (int rank = 1; rank <= 9; ++rank) {
        for (int file = 9; file >= 1; --file) {
            const BoardSquare square{file, rank};
            const Piece piece = board.pieceAt(square);
            // 玉はスキップ（攻方玉は詰将棋では配置されないが念のため）
            if (piece == Piece::None || toBlack(piece) == Piece::BlackKing) {
                continue;
            }
            candidates.append(TrimCandidate{TrimCandidate::Board, square, piece});
        }
    }

    // 攻方（先手）持駒を列挙（歩・香・桂・銀・金・角・飛の順）
    static constexpr Piece kHandOrder[] = {
        Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
        Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook};
    for (const Piece piece : kHandOrder) {
        if (board.pieceStandCount(piece) > 0) {
            candidates.append(TrimCandidate{TrimCandidate::Hand, BoardSquare{}, piece});
        }
    }

    // 守方持駒は除去対象外（守方の防御資源を減らすのは不正）
    return candidates;
}

QString TsumeshogiGenerator::removePieceFromSfen(
    const QString& sfen, const TrimCandidate& candidate)
{
    ShogiBoard board;
    if (!board.trySetSfen(sfen)) {
        return sfen;
    }

    // 駒の総数を維持: 除去した駒は後手持駒に移動（生駒に戻す）
    const Piece toDefenderHand = toWhite(toBlack(demote(candidate.piece)));
    if (candidate.location == TrimCandidate::Board) {
        if (board.pieceAt(candidate.square) == candidate.piece) {
            board.setPieceAt(candidate.square, Piece::None);
            board.addStandPiece(toDefenderHand);
        }
    } else if (board.consumeStandPiece(candidate.piece)) {
        // 攻方持駒から1枚除去 → 後手持駒に移動
        board.addStandPiece(toDefenderHand);
    }

    return tsumeSfen(board);
}

QStringList TsumeshogiGenerator::onePieceRemovedPositions(const QString& sfen)
{
    QStringList positions;
    const auto candidates = enumerateRemovablePieces(sfen);
    for (const auto& candidate : candidates) positions.append(removePieceFromSfen(sfen, candidate));
    return positions;
}
