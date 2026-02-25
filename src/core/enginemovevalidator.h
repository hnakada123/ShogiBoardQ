#ifndef ENGINEMOVEVALIDATOR_H
#define ENGINEMOVEVALIDATOR_H

/// @file enginemovevalidator.h
/// @brief Context方式の高速合法手判定クラス

#include <QMap>
#include <QVector>

#include "legalmovestatus.h"
#include "shogimove.h"
#include "shogitypes.h"
#include "fmvposition.h"

class EngineMoveValidator
{
public:
    static constexpr int BOARD_SIZE = 9;
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;
    static constexpr int BLACK_HAND_FILE = BOARD_SIZE;
    static constexpr int WHITE_HAND_FILE = BOARD_SIZE + 1;

    enum Turn {
        BLACK,
        WHITE,
        TURN_SIZE
    };

    struct Context {
        Turn turn = BLACK;
        bool synced = false;
        fmv::EnginePosition pos{};
        static constexpr int kUndoMax = 512;
        std::array<fmv::UndoState, kUndoMax> undoStack{};
        int undoSize = 0;
    };

    // ---- Context主経路 ----
    bool syncContext(Context& ctx,
                     const Turn& turn,
                     const QVector<Piece>& boardData,
                     const QMap<Piece, int>& pieceStand) const;

    LegalMoveStatus isLegalMove(Context& ctx, ShogiMove& move) const;
    int generateLegalMoves(Context& ctx) const;
    int checkIfKingInCheck(Context& ctx) const;

    bool tryApplyMove(Context& ctx, ShogiMove& move) const;
    bool undoLastMove(Context& ctx) const;

    // ---- 互換ラッパ ----
    LegalMoveStatus isLegalMove(const Turn& turn,
                                const QVector<Piece>& boardData,
                                const QMap<Piece, int>& pieceStand,
                                ShogiMove& currentMove) const;

    int generateLegalMoves(const Turn& turn,
                           const QVector<Piece>& boardData,
                           const QMap<Piece, int>& pieceStand) const;

    int checkIfKingInCheck(const Turn& turn,
                           const QVector<Piece>& boardData) const;
};

#endif // ENGINEMOVEVALIDATOR_H
