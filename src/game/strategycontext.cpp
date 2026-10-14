/// @file strategycontext.cpp
/// @brief 対局モードに共通する一手の確定処理
#include "strategycontext.h"
#include "shogiclock.h"
#include "boardcoordinates.h"
#include "usi.h"

bool MatchCoordinator::StrategyContext::completeHumanMove(
    Turn mover, const QString& text, const QPoint& from, const QPoint& to)
{
    return completeMove(mover, text, from, to, false);
}

bool MatchCoordinator::StrategyContext::applyEngineMove(
    const UsiMove& move, Turn mover, qint64 elapsedMs, bool appendEvaluation)
{
    if (!c_.m_gc || c_.m_gameOver.isOver) return false;
    QString text;
    int nextPly = c_.m_currentMoveIndex + 1;
    if (!c_.m_gc->applyMove(move, text, nextPly, c_.m_sfenHistory, c_.gameMovesRef())) {
        c_.handleEngineIllegalMove(mover);
        return false;
    }
    c_.m_currentMoveIndex = nextPly;
    if (auto* clk = clock()) clk->setMeasuredConsiderationTime(mover, elapsedMs);
    return completeMove(mover, text, BoardCoordinates::fromSquare(move, mover),
                        BoardCoordinates::toSquare(move), appendEvaluation);
}

bool MatchCoordinator::StrategyContext::completeMove(
    Turn mover, const QString& text, const QPoint& from, const QPoint& to, bool appendEvaluation)
{
    if (c_.m_gameOver.isOver) return false;
    if (c_.m_sfenHistory)
        c_.m_currentMoveIndex = static_cast<int>(qMax<qsizetype>(0, c_.m_sfenHistory->size() - 1));

    RecordEntry entry;
    entry.mover = mover;
    entry.displayText = text;
    entry.time = RecordTime::fromMilliseconds(0, 0);
    if (auto* clk = clock()) {
        clk->commitMove(mover);
        entry.time = clk->recordTime(mover);
        clk->setConsiderationTimeMs(mover, 0);
    }
    const auto& moves = c_.gameMovesRef();
    if (!moves.isEmpty()) entry.move = moves.last();
    if (c_.m_sfenHistory && !c_.m_sfenHistory->isEmpty()) entry.sfen = c_.m_sfenHistory->last();
    if (c_.m_hooks.game.appendRecord) c_.m_hooks.game.appendRecord(entry);
    if (c_.m_hooks.ui.renderBoardFromGc) c_.m_hooks.ui.renderBoardFromGc();
    if (c_.m_hooks.ui.showMoveHighlights) c_.m_hooks.ui.showMoveHighlights(from, to);
    updateTurnDisplay(c_.m_gc ? c_.m_gc->currentPlayer() : (mover == Turn::Black ? Turn::White : Turn::Black));
    c_.pokeTimeUpdateNow();
    if (appendEvaluation && c_.m_hooks.game.appendEval) c_.m_hooks.game.appendEval(mover);
    if (checkAndHandleSennichite()) return false;
    if (c_.m_maxMoves > 0 && c_.m_currentMoveIndex >= c_.m_maxMoves) {
        c_.handleMaxMovesJishogi();
        return false;
    }
    return !c_.m_gameOver.isOver;
}

void MatchCoordinator::StrategyContext::setEnginePosition(int slot, const QString& position, const QString& ponder)
{
    (slot == 1 ? c_.m_positionStr1 : c_.m_positionStr2) = position;
    (slot == 1 ? c_.m_positionPonder1 : c_.m_positionPonder2) = ponder;
}

void MatchCoordinator::StrategyContext::syncOpponentPosition(int moverSlot, const QString& position)
{
    (moverSlot == 1 ? c_.m_positionStr2 : c_.m_positionStr1) = position;
}

void MatchCoordinator::StrategyContext::requestEngineMove(Usi* engine, int slot)
{
    QString& position = slot == 1 ? c_.m_positionStr1 : c_.m_positionStr2;
    if (!position.startsWith(QLatin1String("position "))) position = QStringLiteral("position startpos moves");
    engine->requestMatchMove(position, slot == 1 ? c_.m_positionPonder1 : c_.m_positionPonder2, {});
}

void MatchCoordinator::StrategyContext::requestHumanReply(Usi* engine, const UsiMove& humanMove)
{
    if (!c_.m_positionStr1.startsWith(QLatin1String("position ")))
        c_.m_positionStr1 = QStringLiteral("position startpos moves");
    engine->requestHumanReply(c_.m_positionStr1, c_.m_positionPonder1, humanMove, {}, c_.m_positionStrHistory);
}
