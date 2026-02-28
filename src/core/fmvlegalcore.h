#ifndef FMVLEGALCORE_H
#define FMVLEGALCORE_H

/// @file fmvlegalcore.h
/// @brief 合法手判定コア（Context方式）

#include "legalmovestatus.h"
#include "fmvposition.h"
#include "fmvtypes.h"

namespace fmv {

class LegalCore
{
public:
    LegalCore() = default;

    // Context主経路
    LegalMoveStatus checkMove(EnginePosition& pos, Color side, const Move& candidate) const;
    int countLegalMoves(EnginePosition& pos, Color side) const;
    int countChecksToKing(const EnginePosition& pos, Color side) const;

    [[nodiscard]] bool tryApplyLegalMove(EnginePosition& pos, Color side, const Move& m, UndoState& undo) const;
    void undoAppliedMove(EnginePosition& pos, Color side, const UndoState& undo) const;

    // Perft用: 全合法手生成
    void generateLegalMoves(EnginePosition& pos, Color side, MoveList& out) const;

private:
    bool isPseudoLegal(const EnginePosition& pos, Color side, const Move& m) const;
    bool ownKingInCheck(const EnginePosition& pos, Color side) const;
    bool isLegalAfterDoUndo(EnginePosition& pos, Color side, const Move& m) const;

    void generateEvasionMoves(const EnginePosition& pos, Color side, MoveList& out) const;
    void generateNonEvasionMoves(const EnginePosition& pos, Color side, MoveList& out) const;
    void generateDropMoves(const EnginePosition& pos, Color side, MoveList& out) const;

    bool isPawnDropMate(EnginePosition& pos, Color side, const Move& m) const;
    bool hasAnyLegalMove(EnginePosition& pos, Color side) const;
};

} // namespace fmv

#endif // FMVLEGALCORE_H
