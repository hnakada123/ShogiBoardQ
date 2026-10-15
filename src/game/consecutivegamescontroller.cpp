/// @file consecutivegamescontroller.cpp
/// @brief エンジン同士の連続対局の実装

#include "consecutivegamescontroller.h"

#include "logcategories.h"

namespace {
constexpr int kNextGameDelayMs = 100;
} // namespace

ConsecutiveGamesController::ConsecutiveGamesController(QObject* parent)
    : QObject(parent)
{
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &ConsecutiveGamesController::launchPreparedNextGame);
}

void ConsecutiveGamesController::configure(int totalGames, bool switchTurn)
{
    qCDebug(lcGame).noquote() << "configure: totalGames=" << totalGames << " switchTurn=" << switchTurn;
    m_delayTimer.stop();
    m_totalGames = qMax(1, totalGames);
    m_remainingGames = m_totalGames - 1;  // 最初の1局目は既に開始されている
    m_gameNumber = 1;
    m_switchTurnEachGame = switchTurn;
}

void ConsecutiveGamesController::reset()
{
    m_remainingGames = 0;
    m_totalGames = 1;
    m_gameNumber = 1;
    m_switchTurnEachGame = false;
    m_lastStartOptions = MatchStartOptions();
    m_lastTimeControl = GameStartCoordinator::TimeSettings();
    m_delayTimer.stop();
}

void ConsecutiveGamesController::startNextGame()
{
    qCDebug(lcGame).noquote() << "startNextGame: remaining=" << m_remainingGames << " gameNumber=" << m_gameNumber;
    if (m_remainingGames <= 0 || m_delayTimer.isActive()) return;
    // 現在の終局処理・エンジン応答のスタックを抜けてから初期化する。
    m_delayTimer.start(kNextGameDelayMs);
}

void ConsecutiveGamesController::launchPreparedNextGame()
{
    --m_remainingGames;
    ++m_gameNumber;
    // 1局ごとに手番を入れ替える場合。持ち時間もエンジンと一緒に入れ替え、
    // エンジンごとに時間差を付けた対局でも各エンジンが同じ持ち時間で指し続けるようにする。
    if (m_switchTurnEachGame) {
        MatchPlayers& players = m_lastStartOptions.players;
        std::swap(players.blackEngineName, players.whiteEngineName);
        std::swap(m_lastStartOptions.enginePath1, m_lastStartOptions.enginePath2);
        std::swap(m_lastTimeControl.rules.black, m_lastTimeControl.rules.white);
    }
    // 次局は独立した棋譜として同じ開始局面から始める。
    m_lastStartOptions.recordLine = {};

    GameStartCoordinator::StartParams params;
    params.opt = m_lastStartOptions;
    params.tc = m_lastTimeControl;
    params.autoStartEngineMove = true;
    params.announcePlayerNames = true;
    emit nextGameReady(params);
}
