#ifndef ENGINEMOVEVALIDATOR_H
#define ENGINEMOVEVALIDATOR_H

/// @file enginemovevalidator.h
/// @brief Context方式の高速合法手判定クラス

#include <QMap>
#include <QList>

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

    /// 合法手判定用の局面。取り消し履歴を持たず、1手判定でも軽量に生成できる。
    struct Context {
        Turn turn = BLACK;
        bool synced = false;
        fmv::EnginePosition pos{};
    };

    // ---- Context主経路 ----
    [[nodiscard]] bool syncContext(Context& ctx,
                     const Turn& turn,
                     const QList<Piece>& boardData,
                     const QMap<Piece, int>& pieceStand) const;

    LegalMoveStatus isLegalMove(Context& ctx, ShogiMove& move) const;
    int generateLegalMoves(Context& ctx) const;
    int checkIfKingInCheck(Context& ctx) const;

#ifdef SHOGIBOARDQ_TESTING
    /// 連続適用・取り消しの検証用。通常の判定では生成しない。
    struct SimulationContext {
        Context context;
        static constexpr int kUndoMax = 512;
        std::array<fmv::UndoState, kUndoMax> undoStack{};
        int undoSize = 0;
    };

    [[nodiscard]] bool syncContext(SimulationContext& simulation,
                     const Turn& turn,
                     const QList<Piece>& boardData,
                     const QMap<Piece, int>& pieceStand) const;
    [[nodiscard]] bool tryApplyMove(SimulationContext& simulation, ShogiMove& move) const;
    [[nodiscard]] bool undoLastMove(SimulationContext& simulation) const;
#endif

    // ---- 互換ラッパ ----
    LegalMoveStatus isLegalMove(const Turn& turn,
                                const QList<Piece>& boardData,
                                const QMap<Piece, int>& pieceStand,
                                ShogiMove& currentMove) const;

    int generateLegalMoves(const Turn& turn,
                           const QList<Piece>& boardData,
                           const QMap<Piece, int>& pieceStand) const;

    int checkIfKingInCheck(const Turn& turn,
                           const QList<Piece>& boardData) const;
};

#endif // ENGINEMOVEVALIDATOR_H
