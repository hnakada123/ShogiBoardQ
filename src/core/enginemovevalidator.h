#ifndef ENGINEMOVEVALIDATOR_H
#define ENGINEMOVEVALIDATOR_H

/// @file enginemovevalidator.h
/// @brief 合法手の判定（ビットボードの合法手生成 fmv を使う）

#include <QList>
#include <QMap>
#include <array>

#include "fmvposition.h"
#include "legalmovestatus.h"
#include "shogitypes.h"
#include "usimove.h"

class ShogiBoard;

/**
 * @brief 合法手の判定
 *
 * 局面は Context に一度読み込んでから問い合わせる。同じ局面への問い合わせ
 * （行き先の候補・合法手の一覧・王手の数）は、読み込んだ Context を使い回す。
 * 1回だけ問い合わせるときは ShogiBoard を受け取る省略形を使う。
 */
class EngineMoveValidator
{
public:
    /// 読み込んだ局面と手番
    struct Context {
        Turn turn = Turn::Black;
        bool synced = false;
        fmv::EnginePosition pos{};
    };

    // ---- 局面の読み込み ----
    static Context load(const ShogiBoard& board, Turn turn);
    static Context load(const QList<Piece>& boardData, const QMap<Piece, int>& pieceStand, Turn turn);

    // ---- 読み込んだ局面への問い合わせ ----

    /// 指し手（成りの指定は見ない）の成り・不成それぞれが合法か
    static LegalMoveStatus legalStatus(const Context& ctx, const UsiMove& move);
    /// 指し手（成りの指定を含む）が合法か
    static bool isLegal(const Context& ctx, const UsiMove& move);
    static int legalMoveCount(const Context& ctx);
    /// 合法手の一覧。成り・不成のどちらも指せる手は両方を入れる。局面が読めていなければ空
    static QList<UsiMove> legalMoves(const Context& ctx);
    /// 駒の動きだけで決めた指し手の一覧（自玉の安全・二歩・打ち歩詰め・行き所のない駒は見ない）。
    /// 反則手を書いた棋譜を読み書きするときに、合法手で決まらない手の候補にする
    static QList<UsiMove> pseudoLegalMoves(const Context& ctx);
    /// 手番側の玉にかかっている王手の数
    static int checkCount(const Context& ctx);

    // ---- 1回だけ問い合わせるときの省略形 ----
    static LegalMoveStatus legalStatus(const ShogiBoard& board, Turn turn, const UsiMove& move);
    static QList<UsiMove> legalMoves(const ShogiBoard& board, Turn turn);
    static QList<UsiMove> pseudoLegalMoves(const ShogiBoard& board, Turn turn);
    static bool isInCheck(const ShogiBoard& board, Turn turn);

#ifdef SHOGIBOARDQ_TESTING
    /// 連続適用・取り消しの検証用。通常の判定では生成しない。
    struct SimulationContext {
        Context context;
        static constexpr int kUndoMax = 512;
        std::array<fmv::UndoState, kUndoMax> undoStack{};
        int undoSize = 0;
    };

    [[nodiscard]] static bool syncContext(SimulationContext& simulation,
                                          const QList<Piece>& boardData,
                                          const QMap<Piece, int>& pieceStand,
                                          Turn turn);
    [[nodiscard]] static bool tryApplyMove(SimulationContext& simulation, const UsiMove& move);
    [[nodiscard]] static bool undoLastMove(SimulationContext& simulation);
#endif
};

#endif // ENGINEMOVEVALIDATOR_H
