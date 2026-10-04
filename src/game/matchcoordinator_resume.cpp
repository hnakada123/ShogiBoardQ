/// @file matchcoordinator_resume.cpp
/// @brief 中断したローカル対局の保存・再開
#include "matchcoordinator.h"
#include "matchturnhandler.h"
#include "shogiboard.h"
#include "shogiclock.h"

void MatchCoordinator::captureInterruptedGame()
{
    switch (m_playMode) {
    case PlayMode::HumanVsHuman:
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapHumanVsEngine:
    case PlayMode::HandicapEngineVsHuman:
    case PlayMode::HandicapEngineVsEngine:
        break;
    default:
        return;
    }
    if (!clock() || !m_gc || !m_sfenHistory || m_sfenHistory->isEmpty()) return;
    auto state = std::make_unique<InterruptedGameState>();
    state->clock = clock()->pauseAndSnapshot();
    if (m_gameOver.isOver) return;
    state->options = m_lastStartOptions;
    state->options.mode = m_playMode;
    state->options.sfenStart = m_sfenHistory->last();
    state->options.resumeFromBreakOff = true;
    state->timeControl = timeControl();
    state->sfens = *m_sfenHistory;
    state->moves = gameMovesRef();
    state->position = m_positionStr1;
    state->positionHistory = m_positionStrHistory;
    state->previousFile = m_gc->previousMoveDestination().x();
    state->previousRank = m_gc->previousMoveDestination().y();
    m_interruptedGame = std::move(state);
}

void MatchCoordinator::discardInterruptedGame()
{
    m_interruptedGame.reset();
}

bool MatchCoordinator::resumeInterruptedGame()
{
    if (!m_interruptedGame || !clock() || !m_gc || !m_gc->board()) return false;
    auto state = std::move(m_interruptedGame);
    clearGameOverState();
    m_playMode = state->options.mode;
    m_gc->board()->setSfen(state->options.sfenStart);
    m_gc->setCurrentPlayer(state->clock.currentPlayer == 1
                              ? ShogiGameController::Player1 : ShogiGameController::Player2);
    m_gc->resetResult();
    m_gc->setPromote(false);
    m_gc->setPreviousMoveDestination(QPoint(state->previousFile, state->previousRank));
    *m_sfenHistory = state->sfens;
    gameMovesRef() = state->moves;
    m_currentMoveIndex = static_cast<int>(state->sfens.size() - 1);
    m_positionStr1 = m_positionStr2 = state->position;
    // 先読みは停止済み。確定局面から新しい探索を開始する。
    m_positionPonder1 = m_positionPonder2 = state->position;
    m_positionStrHistory = state->positionHistory;
    const auto& tc = state->timeControl;
    setTimeControlConfig(tc.useByoyomi, tc.byoyomiMs1, tc.byoyomiMs2,
                         tc.incMs1, tc.incMs2, tc.loseOnTimeout);
    clock()->restoreSnapshot(state->clock);
    ensureMatchTurnHandler();
    m_turnHandler->createAndStartModeStrategy(state->options);
    armTurnTimerIfNeeded();
    // 対局開始通知でUIを整えてから、呼び出し側が初手探索を起動する。
    return true;
}
