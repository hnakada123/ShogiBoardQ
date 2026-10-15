/// @file enginemovevalidator.cpp
/// @brief 合法手の判定の実装

#include "enginemovevalidator.h"

#include "fmvattacks.h"
#include "fmvconverter.h"
#include "fmvlegalcore.h"
#include "fmvlegalcore_internal.h"
#include "shogiboard.h"

namespace {
fmv::LegalCore& legalCore()
{
    static fmv::LegalCore core;
    return core;
}
} // namespace

// ---- 局面の読み込み ----

EngineMoveValidator::Context EngineMoveValidator::load(const QList<Piece>& boardData,
                                                       const QMap<Piece, int>& pieceStand,
                                                       Turn turn)
{
    Context ctx;
    ctx.turn = turn;
    ctx.synced = fmv::Converter::toEnginePosition(ctx.pos, boardData, pieceStand);
    return ctx;
}

EngineMoveValidator::Context EngineMoveValidator::load(const ShogiBoard& board, Turn turn)
{
    return load(board.boardData(), board.pieceStand(), turn);
}

// ---- 読み込んだ局面への問い合わせ ----

LegalMoveStatus EngineMoveValidator::legalStatus(const Context& ctx, const UsiMove& move)
{
    if (!ctx.synced) {
        return LegalMoveStatus(false, false);
    }
    fmv::ConvertedMove cm;
    if (!fmv::Converter::toEngineMove(cm, ctx.turn, ctx.pos, move)) {
        return LegalMoveStatus(false, false);
    }
    fmv::EnginePosition work = ctx.pos;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    if (cm.hasPromoteVariant) {
        return legalCore().checkMoveVariants(work, side, cm.nonPromote);
    }
    return legalCore().checkMove(work, side, cm.nonPromote);
}

bool EngineMoveValidator::isLegal(const Context& ctx, const UsiMove& move)
{
    const LegalMoveStatus status = legalStatus(ctx, move);
    return move.promote ? status.promotingMoveExists : status.nonPromotingMoveExists;
}

int EngineMoveValidator::legalMoveCount(const Context& ctx)
{
    if (!ctx.synced) {
        return 0;
    }
    fmv::EnginePosition work = ctx.pos;
    return legalCore().countLegalMoves(work, fmv::Converter::toColor(ctx.turn));
}

QList<UsiMove> EngineMoveValidator::legalMoves(const Context& ctx)
{
    QList<UsiMove> out;
    if (!ctx.synced) {
        return out;
    }
    fmv::EnginePosition work = ctx.pos;
    fmv::MoveList list;
    legalCore().generateLegalMoves(work, fmv::Converter::toColor(ctx.turn), list);
    out.reserve(list.size);
    for (int i = 0; i < list.size; ++i) {
        out.append(fmv::Converter::toUsiMove(list.moves[static_cast<std::size_t>(i)]));
    }
    return out;
}

QList<UsiMove> EngineMoveValidator::pseudoLegalMoves(const Context& ctx)
{
    QList<UsiMove> out;
    if (!ctx.synced) {
        return out;
    }
    const fmv::EnginePosition& pos = ctx.pos;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    const auto ci = static_cast<std::size_t>(side);
    for (int from = 0; from < fmv::kSquareNb; ++from) {
        const auto fromSq = static_cast<fmv::Square>(from);
        if (!pos.colorOcc[ci].test(fromSq)) {
            continue;
        }
        const fmv::PieceType pt = fmv::detail::charToPieceType(pos.board[fromSq]);
        if (pt == fmv::PieceType::PieceTypeNb) {
            continue;
        }
        for (int to = 0; to < fmv::kSquareNb; ++to) {
            const auto toSq = static_cast<fmv::Square>(to);
            if (pos.colorOcc[ci].test(toSq) || !fmv::pieceAttacksSquare(pos, side, pt, fromSq, toSq)) {
                continue;
            }
            fmv::Move m;
            m.from = fromSq;
            m.to = toSq;
            m.piece = pt;
            out.append(fmv::Converter::toUsiMove(m));
            if (fmv::detail::isPromotable(pt)
                && (fmv::detail::isPromotionZone(side, fmv::squareRank(fromSq))
                    || fmv::detail::isPromotionZone(side, fmv::squareRank(toSq)))) {
                m.promote = true;
                out.append(fmv::Converter::toUsiMove(m));
            }
        }
    }
    for (int hand = 0; hand < static_cast<int>(fmv::HandType::HandTypeNb); ++hand) {
        if (pos.hand[ci][static_cast<std::size_t>(hand)] <= 0) {
            continue;
        }
        fmv::Move m;
        m.kind = fmv::MoveKind::Drop;
        // HandType と PieceType は歩〜飛の並びが同じ
        m.piece = static_cast<fmv::PieceType>(hand);
        for (int to = 0; to < fmv::kSquareNb; ++to) {
            m.to = static_cast<fmv::Square>(to);
            if (!pos.occupied.test(m.to)) {
                out.append(fmv::Converter::toUsiMove(m));
            }
        }
    }
    return out;
}

int EngineMoveValidator::checkCount(const Context& ctx)
{
    if (!ctx.synced) {
        return 0;
    }
    return legalCore().countChecksToKing(ctx.pos, fmv::Converter::toColor(ctx.turn));
}

// ---- 1回だけ問い合わせるときの省略形 ----

LegalMoveStatus EngineMoveValidator::legalStatus(const ShogiBoard& board, Turn turn, const UsiMove& move)
{
    return legalStatus(load(board, turn), move);
}

QList<UsiMove> EngineMoveValidator::legalMoves(const ShogiBoard& board, Turn turn)
{
    return legalMoves(load(board, turn));
}

QList<UsiMove> EngineMoveValidator::pseudoLegalMoves(const ShogiBoard& board, Turn turn)
{
    return pseudoLegalMoves(load(board, turn));
}

bool EngineMoveValidator::isInCheck(const ShogiBoard& board, Turn turn)
{
    return checkCount(load(board, turn)) > 0;
}

#ifdef SHOGIBOARDQ_TESTING
bool EngineMoveValidator::syncContext(SimulationContext& simulation,
                                      const QList<Piece>& boardData,
                                      const QMap<Piece, int>& pieceStand,
                                      Turn turn)
{
    simulation.undoSize = 0;
    simulation.context = load(boardData, pieceStand, turn);
    return simulation.context.synced;
}

bool EngineMoveValidator::tryApplyMove(SimulationContext& simulation, const UsiMove& move)
{
    Context& ctx = simulation.context;
    if (!ctx.synced || simulation.undoSize >= SimulationContext::kUndoMax) {
        return false;
    }

    fmv::ConvertedMove cm;
    if (!fmv::Converter::toEngineMove(cm, ctx.turn, ctx.pos, move)) {
        return false;
    }

    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    fmv::UndoState undo;
    const fmv::Move selected = move.promote && cm.hasPromoteVariant ? cm.promote : cm.nonPromote;
    if (!legalCore().tryApplyLegalMove(ctx.pos, side, selected, undo)) {
        return false;
    }

    simulation.undoStack[static_cast<std::size_t>(simulation.undoSize++)] = undo;
    ctx.turn = (ctx.turn == Turn::Black) ? Turn::White : Turn::Black;
    return true;
}

bool EngineMoveValidator::undoLastMove(SimulationContext& simulation)
{
    Context& ctx = simulation.context;
    if (!ctx.synced || simulation.undoSize <= 0) {
        return false;
    }

    ctx.turn = (ctx.turn == Turn::Black) ? Turn::White : Turn::Black;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    const fmv::UndoState undo = simulation.undoStack[static_cast<std::size_t>(--simulation.undoSize)];
    legalCore().undoAppliedMove(ctx.pos, side, undo);
    return true;
}
#endif
