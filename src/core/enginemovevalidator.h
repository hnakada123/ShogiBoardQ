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
#include "usimove.h"

class EngineMoveValidator
{
public:
    static constexpr int BOARD_SIZE = 9;
    static constexpr int BLACK_HAND_FILE = BOARD_SIZE;
    static constexpr int WHITE_HAND_FILE = BOARD_SIZE + 1;


    /// 合法手判定用の局面。取り消し履歴を持たず、1手判定でも軽量に生成できる。
    struct Context {
        Turn turn = Turn::Black;
        bool synced = false;
        fmv::EnginePosition pos{};
    };

    // ---- Context主経路 ----
    [[nodiscard]] bool syncContext(Context& ctx,
                     Turn turn,
                     const QList<Piece>& boardData,
                     const QMap<Piece, int>& pieceStand) const;

    LegalMoveStatus isLegalMove(Context& ctx, ShogiMove& move) const;
    int generateLegalMoves(Context& ctx) const;
    /// 合法手の一覧。成り・不成のどちらも指せる手は両方を入れる。局面が同期されていなければ空
    QList<UsiMove> legalMoves(Context& ctx) const;
    /// 駒の動きだけで決めた指し手の一覧（自玉の安全・二歩・打ち歩詰め・行き所のない駒は見ない）。
    /// 反則手を書いた棋譜を読み書きするときに、合法手で決まらない手の候補にする
    QList<UsiMove> pseudoLegalMoves(Context& ctx) const;
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
                     Turn turn,
                     const QList<Piece>& boardData,
                     const QMap<Piece, int>& pieceStand) const;
    [[nodiscard]] bool tryApplyMove(SimulationContext& simulation, ShogiMove& move) const;
    [[nodiscard]] bool undoLastMove(SimulationContext& simulation) const;
#endif

    // ---- 互換ラッパ ----
    LegalMoveStatus isLegalMove(Turn turn,
                                const QList<Piece>& boardData,
                                const QMap<Piece, int>& pieceStand,
                                ShogiMove& currentMove) const;

    int generateLegalMoves(Turn turn,
                           const QList<Piece>& boardData,
                           const QMap<Piece, int>& pieceStand) const;

    QList<UsiMove> legalMoves(Turn turn,
                              const QList<Piece>& boardData,
                              const QMap<Piece, int>& pieceStand) const;

    QList<UsiMove> pseudoLegalMoves(Turn turn,
                                    const QList<Piece>& boardData,
                                    const QMap<Piece, int>& pieceStand) const;

    int checkIfKingInCheck(Turn turn,
                           const QList<Piece>& boardData) const;
};

#endif // ENGINEMOVEVALIDATOR_H
