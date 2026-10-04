/// @file playmodepolicyservice.cpp
/// @brief プレイモード依存の判定ロジックを集約するサービスの実装

#include "playmodepolicyservice.h"
#include "matchcoordinator.h"
#include "csagamecoordinator.h"
#include "playmode.h"

void PlayModePolicyService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

bool PlayModePolicyService::isHumanTurnNow() const
{
    if (!m_deps.playMode)
        return false;

    switch (*m_deps.playMode) {
    // 終局後もプレイモードは残るが、盤上の着手は棋譜に記録されないため受け付けない。
    // 終局後の局面から指し継ぐときは「現在の局面」から対局を始める（分岐として記録される）。
    case PlayMode::HumanVsHuman:
        return isGameActivelyInProgress();

    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        return isGameActivelyInProgress() && m_deps.gameController
               && m_deps.gameController->currentPlayer() == ShogiGameController::Player1;

    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        return isGameActivelyInProgress() && m_deps.gameController
               && m_deps.gameController->currentPlayer() == ShogiGameController::Player2;

    case PlayMode::CsaNetworkMode:
        if (m_deps.csaGameCoordinator) {
            return m_deps.csaGameCoordinator->gameState() == CsaGameCoordinator::GameState::InGame
                && m_deps.csaGameCoordinator->isHumanPlayer()
                && m_deps.csaGameCoordinator->isMyTurn();
        }
        return false;

    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
    default:
        return false;
    }
}

bool PlayModePolicyService::isHumanSide(ShogiGameController::Player p) const
{
    if (!m_deps.playMode)
        return false;

    switch (*m_deps.playMode) {
    case PlayMode::HumanVsHuman:
        return true;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        return (p == ShogiGameController::Player1);
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        return (p == ShogiGameController::Player2);
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
    default:
        return false;
    }
}

bool PlayModePolicyService::isHvH() const
{
    if (!m_deps.playMode)
        return false;

    return (*m_deps.playMode == PlayMode::HumanVsHuman);
}

bool PlayModePolicyService::isGameActivelyInProgress() const
{
    if (!m_deps.playMode)
        return false;

    switch (*m_deps.playMode) {
    case PlayMode::HumanVsHuman:
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapHumanVsEngine:
    case PlayMode::HandicapEngineVsHuman:
    case PlayMode::HandicapEngineVsEngine:
        return m_deps.match && !m_deps.match->gameOverState().isOver;
    default:
        return false;
    }
}
