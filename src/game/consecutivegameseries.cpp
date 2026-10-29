/// @file consecutivegameseries.cpp
/// @brief エンジン同士の連続対局の実装

#include "consecutivegameseries.h"
#include "logcategories.h"

namespace {
constexpr int kNextGameDelayMs = 100;
} // namespace

ConsecutiveGameSeries::ConsecutiveGameSeries(QObject* parent)
    : QObject(parent)
{
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &ConsecutiveGameSeries::launchPreparedNextGame);
}

void ConsecutiveGameSeries::beginSeries(const GameStartPlan& firstGame)
{
    qCDebug(lcGame).noquote() << "beginSeries: totalGames=" << firstGame.consecutiveGames
                              << " switchTurn=" << firstGame.switchTurnEachGame;
    m_delayTimer.stop();
    m_totalGames = qMax(1, firstGame.consecutiveGames);
    m_remainingGames = m_totalGames - 1;  // 1局目は既に始めている
    m_gameNumber = 1;
    m_nextGame = firstGame;
    // 次局は独立した棋譜として、1局目を始めた局面から始める
    const QString startSfen = firstGame.options.recordLine.lastSfen();
    m_nextGame.options.recordLine = RecordLine{};
    m_nextGame.options.recordLine.startSfen = startSfen;
    m_nextGame.preparation = {GameStartPreparation::Mode::NewGame, startSfen, -1};
    m_nextGame.flipBoard = false;
}

void ConsecutiveGameSeries::reset()
{
    m_remainingGames = 0;
    m_totalGames = 1;
    m_gameNumber = 1;
    m_nextGame = GameStartPlan();
    m_delayTimer.stop();
}

void ConsecutiveGameSeries::startNextGame()
{
    qCDebug(lcGame).noquote() << "startNextGame: remaining=" << m_remainingGames << " gameNumber=" << m_gameNumber;
    if (m_remainingGames <= 0 || m_delayTimer.isActive()) return;
    // 現在の終局処理・エンジン応答のスタックを抜けてから初期化する。
    m_delayTimer.start(kNextGameDelayMs);
}

void ConsecutiveGameSeries::launchPreparedNextGame()
{
    --m_remainingGames;
    ++m_gameNumber;
    // 1局ごとに手番を入れ替える場合（持ち時間もエンジンと一緒に入れ替える）
    if (m_nextGame.switchTurnEachGame) m_nextGame.options.swapSides();
    emit nextGameReady(m_nextGame);
}
