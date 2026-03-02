/// @file piecemoverules.cpp
/// @brief 駒の移動ルール検証（禁じ手チェック・強制成り判定）の実装

#include "piecemoverules.h"
#include "shogiboard.h"
#include "boardconstants.h"

// ============================================================
// 総合チェック
// ============================================================

bool PieceMoveRules::checkMovePiece(ShogiBoard* board,
                                    Piece source, Piece dest,
                                    int fileFrom, int fileTo)
{
    if (!checkTwoPawn(board, source, fileFrom, fileTo)) return false;
    if (!checkWhetherAllyPiece(source, dest, fileFrom, fileTo)) return false;
    if (!checkNumberStandPiece(board, source, fileFrom)) return false;
    if (!checkFromPieceStandToPieceStand(source, dest, fileFrom, fileTo)) return false;
    if (!checkGetKingOpponentPiece(source, dest)) return false;

    return true;
}

// ============================================================
// 二歩チェック
// ============================================================

bool PieceMoveRules::checkTwoPawn(ShogiBoard* board,
                                  Piece source, int fileFrom, int fileTo)
{
    // 同じ筋内の移動は二歩にならない
    if (fileFrom == fileTo) return true;

    if (source == Piece::BlackPawn) {
        if (fileTo < BoardConstants::kBlackStandFile) {
            for (int rank = 1; rank <= 9; rank++) {
                if (board->getPieceCharacter(fileTo, rank) == Piece::BlackPawn) return false;
            }
        }
    }

    if (source == Piece::WhitePawn) {
        if (fileTo < BoardConstants::kBlackStandFile) {
            for (int rank = 1; rank <= 9; rank++) {
                if (board->getPieceCharacter(fileTo, rank) == Piece::WhitePawn) return false;
            }
        }
    }

    return true;
}

// ============================================================
// 味方駒チェック
// ============================================================

bool PieceMoveRules::checkWhetherAllyPiece(Piece source, Piece dest,
                                           int fileFrom, int fileTo)
{
    if (fileTo < BoardConstants::kBlackStandFile) {
        // 同じ先手/後手同士の場合は味方の駒
        if (isBlackPiece(source) && isBlackPiece(dest)) {
            if ((fileFrom < BoardConstants::kBlackStandFile) && (fileTo >= BoardConstants::kBlackStandFile)) {
                return true;
            } else {
                return false;
            }
        }
        if (isWhitePiece(source) && isWhitePiece(dest)) {
            if ((fileFrom < BoardConstants::kBlackStandFile) && (fileTo >= BoardConstants::kBlackStandFile)) {
                return true;
            } else {
                return false;
            }
        }
    }

    return true;
}

// ============================================================
// 駒台在庫チェック
// ============================================================

bool PieceMoveRules::checkNumberStandPiece(ShogiBoard* board,
                                           Piece source, int fileFrom)
{
    return board->isPieceAvailableOnStand(source, fileFrom);
}

// ============================================================
// 駒台間移動チェック
// ============================================================

bool PieceMoveRules::checkFromPieceStandToPieceStand(Piece source, Piece dest,
                                                     int fileFrom, int fileTo)
{
    // 先手駒台→後手駒台: 同種の駒のみ移動可能
    if ((fileFrom == BoardConstants::kBlackStandFile) && (fileTo == BoardConstants::kWhiteStandFile)) {
        Piece destAsBlack = toBlack(dest);
        if (source == destAsBlack) {
            return true;
        } else {
            return false;
        }
    }
    // 後手駒台→先手駒台: 同種の駒のみ移動可能
    if ((fileFrom == BoardConstants::kWhiteStandFile) && (fileTo == BoardConstants::kBlackStandFile)) {
        Piece destAsWhite = toWhite(dest);
        if (source == destAsWhite) {
            return true;
        } else {
            return false;
        }
    }

    return true;
}

// ============================================================
// 玉取りチェック
// ============================================================

bool PieceMoveRules::checkGetKingOpponentPiece(Piece source, Piece dest)
{
    // 後手の駒で先手玉は取れない
    if ((dest == Piece::BlackKing) && isWhitePiece(source)) return false;

    // 先手の駒で後手玉は取れない
    if ((dest == Piece::WhiteKing) && isBlackPiece(source)) return false;

    return true;
}

// ============================================================
// 強制成り判定
// ============================================================

bool PieceMoveRules::shouldAutoPromote(int fileTo, int rankTo, Piece source)
{
    // 駒台への移動は成りなし
    if (fileTo >= BoardConstants::kBlackStandFile) {
        return false;
    }

    // 先手: 歩・香は1段目、桂は1-2段目で必ず成る
    if ((source == Piece::BlackPawn && rankTo == 1) ||
        (source == Piece::BlackLance && rankTo == 1) ||
        (source == Piece::BlackKnight && rankTo <= 2)) {
        return true;
    }

    // 後手: 歩・香は9段目、桂は8-9段目で必ず成る
    if ((source == Piece::WhitePawn && rankTo == 9) ||
        (source == Piece::WhiteLance && rankTo == 9) ||
        (source == Piece::WhiteKnight && rankTo >= 8)) {
        return true;
    }

    return false;
}
