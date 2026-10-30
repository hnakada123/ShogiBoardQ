/// @file humanturnpolicy.cpp
/// @brief 盤を人間が操作できるか・対局が進行中かの判定の実装

#include "humanturnpolicy.h"

#include "appactivitystate.h"
#include "branchtreeeditcontroller.h"
#include "csagamecoordinator.h"
#include "matchcoordinator.h"
#include "shogiboard.h"

HumanTurnPolicy::BoardInput HumanTurnPolicy::boardInput() const
{
    if (m_disabled || !m_deps.activity) return BoardInput::None;
    switch (m_deps.activity->activity()) {
    case AppActivityState::Activity::DuringPositionEdit:
        return BoardInput::Edit;
    case AppActivityState::Activity::Idle:
        // 対局していない待機中は、盤上の着手を表示中の手の次の手（変化）として記録する。
        // 終局後も同じ扱いで、指し継いだ手は分岐ツリーに変化として加わる。
        return m_deps.branchEdit && m_deps.branchEdit->canEdit() ? BoardInput::Variation : BoardInput::None;
    case AppActivityState::Activity::DuringGame: {
        const MatchCoordinator* match = m_deps.match;
        const bool humanTurn = match && m_deps.board && match->canAcceptMove()
            && match->isHumanSide(m_deps.board->currentPlayer());
        return humanTurn ? BoardInput::Game : BoardInput::None;
    }
    case AppActivityState::Activity::DuringCsaGame: {
        const CsaGameCoordinator* csa = m_deps.csaGame;
        const bool myTurn = csa && csa->gameState() == CsaGameCoordinator::GameState::InGame && csa->isHumanPlayer()
            && csa->isMyTurn();
        return myTurn ? BoardInput::CsaServer : BoardInput::None;
    }
    default:
        return BoardInput::None;
    }
}

bool HumanTurnPolicy::isHumanTurnNow() const
{
    const BoardInput input = boardInput();
    return input == BoardInput::Variation || input == BoardInput::Game || input == BoardInput::CsaServer;
}

bool HumanTurnPolicy::isGameActivelyInProgress() const
{
    if (!m_deps.activity) return false;
    switch (m_deps.activity->activity()) {
    case AppActivityState::Activity::DuringGame:
        return m_deps.match && !m_deps.match->isGameOver();
    case AppActivityState::Activity::DuringCsaGame: {
        const CsaGameCoordinator* csa = m_deps.csaGame;
        return csa && csa->gameState() == CsaGameCoordinator::GameState::InGame;
    }
    default:
        return false;
    }
}
