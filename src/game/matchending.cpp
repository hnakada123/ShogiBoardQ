/// @file matchending.cpp
/// @brief 対局の終局の原因からの対応の実装

#include "matchending.h"

namespace MatchEnding {

bool isDraw(MatchCause cause)
{
    switch (cause) {
    case MatchCause::BreakOff:
    case MatchCause::Jishogi:
    case MatchCause::MaxMoves:
    case MatchCause::Sennichite:
        return true;
    case MatchCause::Resignation:
    case MatchCause::Timeout:
    case MatchCause::NyugyokuWin:
    case MatchCause::IllegalMove:
    case MatchCause::OuteSennichite:
        return false;
    }
    return false;
}

TerminalType terminalOf(MatchCause cause)
{
    switch (cause) {
    case MatchCause::Resignation: return TerminalType::Resign;
    case MatchCause::Timeout: return TerminalType::Timeout;
    case MatchCause::BreakOff: return TerminalType::Interrupt;
    // 最大手数も持将棋と書く（どの棋譜の形式にも書け、ほかのアプリでも読める）
    case MatchCause::Jishogi:
    case MatchCause::MaxMoves: return TerminalType::Impasse;
    case MatchCause::NyugyokuWin: return TerminalType::DeclarationWin;
    case MatchCause::IllegalMove: return TerminalType::IllegalLoss;
    case MatchCause::Sennichite: return TerminalType::Repetition;
    // 連続王手の千日手は王手をかけていた側の反則負け
    case MatchCause::OuteSennichite: return TerminalType::IllegalLoss;
    }
    return TerminalType::Interrupt;
}

RecordEntry::Ending endingOf(const MatchGameEndInfo& info, Turn sideToMove)
{
    const TerminalType terminal = terminalOf(info.cause);
    return RecordEntry::forSideToMove(terminal, RecordEntry::endingSide(terminal, sideToMove, info.loser), sideToMove);
}

GameOverResult engineResult(const MatchGameEndInfo& info, Turn side)
{
    if (isDraw(info.cause)) return GameOverResult::Draw;
    return info.loser == side ? GameOverResult::Lose : GameOverResult::Win;
}

} // namespace MatchEnding
