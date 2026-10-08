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

    // 対局していない待機中は、盤上の着手を表示中の手の次の手（変化）として記録する。
    // 終局後も同じ扱いで、指し継いだ手は分岐ツリーに変化として加わる。
    if (!isGameActivelyInProgress() && m_deps.canRecordVariationMove && m_deps.canRecordVariationMove()) {
        return true;
    }

    switch (*m_deps.playMode) {
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
