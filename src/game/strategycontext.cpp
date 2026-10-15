/// @file strategycontext.cpp
/// @brief 対局モードに共通する一手の確定処理
#include "strategycontext.h"
#include "shogiclock.h"
#include "boardcoordinates.h"
#include "usi.h"
#include <QSignalBlocker>

bool MatchStrategyContext::completeHumanMove(
    Turn mover, const QString& text, const QPoint& from, const QPoint& to)
{
    return completeMove(mover, text, from, to, false);
}

bool MatchStrategyContext::applyEngineMove(
    const UsiMove& move, Turn mover, qint64 elapsedMs, bool appendEvaluation)
{
    if (!m_deps.gc || gameOverState().isOver) return false;
    if (m_deps.hooks.game.canAppendRecord && !m_deps.hooks.game.canAppendRecord()) return false;
    const auto update = m_deps.gc->positionUpdate();
    QString text;
    int nextPly = currentMoveIndex() + 1;
    if (!m_deps.gc->applyMove(move, text, nextPly, m_deps.sfens, *m_deps.moves)) {
        handleEngineIllegalMove(mover);
        return false;
    }
    if (auto* clk = clock()) clk->setMeasuredConsiderationTime(mover, elapsedMs);
    return completeMove(mover, text, BoardCoordinates::fromSquare(move, mover),
                        BoardCoordinates::toSquare(move), appendEvaluation);
}

bool MatchStrategyContext::completeMove(
    Turn mover, const QString& text, const QPoint& from, const QPoint& to, bool appendEvaluation)
{
    if (gameOverState().isOver) return false;

    const QSignalBlocker clockNotifications(clock());
    RecordEntry entry;
    entry.mover = mover;
    entry.displayText = text;
    entry.time = RecordTime::fromMilliseconds(0, 0);
    if (auto* clk = clock()) {
        clk->commitMove(mover);
        entry.time = clk->recordTime(mover);
        clk->setConsiderationTimeMs(mover, 0);
        clk->setCurrentPlayer(m_deps.gc->currentPlayer());
    }
    const auto& moves = *m_deps.moves;
    if (!moves.isEmpty()) entry.move = moves.last();
    if (m_deps.sfens && !m_deps.sfens->isEmpty()) entry.sfen = m_deps.sfens->last();
    if (m_deps.hooks.game.appendRecord) m_deps.hooks.game.appendRecord(entry);
    if (m_deps.hooks.ui.renderBoardFromGc) m_deps.hooks.ui.renderBoardFromGc();
    if (m_deps.hooks.ui.showMoveHighlights) m_deps.hooks.ui.showMoveHighlights(from, to);
    updateTurnDisplay(m_deps.gc ? m_deps.gc->currentPlayer() : (mover == Turn::Black ? Turn::White : Turn::Black));
    pokeTimeUpdateNow();
    if (appendEvaluation && m_deps.hooks.game.appendEval) m_deps.hooks.game.appendEval(mover);
    if (checkAndHandleSennichite()) return false;
    if (maxMoves() > 0 && currentMoveIndex() >= maxMoves()) {
        handleMaxMovesJishogi();
        return false;
    }
    return !gameOverState().isOver;
}

void MatchStrategyContext::requestEngineMove(Usi* engine, int slot)
{
    if (!engine) return;
    RecordLine line;
    line.sfens = *m_deps.sfens;
    line.moves = *m_deps.moves;
    engine->requestMatchMove(line.positionCommand(currentMoveIndex()), m_deps.session->ponder(slot), {});
}

void MatchStrategyContext::initializeAndStartEngineFor(Turn side, const QString& path, const QString& name)
{
    m_deps.engines->initializeAndStartEngineFor(side, path, name);
    if (usi1()) usi1()->setMatchClock(clock());
    if (usi2()) usi2()->setMatchClock(clock());
}
