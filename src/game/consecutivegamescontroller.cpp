/// @file consecutivegamescontroller.cpp
/// @brief 連続対局の進行管理と手番入れ替えの実装

#include "consecutivegamescontroller.h"
#include "timecontrolcontroller.h"
#include "shogiclock.h"
#include "timecontrolutil.h"

#include <QDebug>

#include "matchcoordinator.h"
#include <utility>

// ============================================================
// 生成・依存設定
// ============================================================

ConsecutiveGamesController::ConsecutiveGamesController(QObject* parent)
    : QObject(parent)
    , m_delayTimer(new QTimer(this))
{
    m_delayTimer->setSingleShot(true);
    connect(m_delayTimer, &QTimer::timeout, this, &ConsecutiveGamesController::startNextGame);
}

void ConsecutiveGamesController::setTimeController(TimeControlController* tc)
{
    m_timeController = tc;
}

void ConsecutiveGamesController::setGameStartCoordinator(GameStartCoordinator* gsc)
{
    m_gameStart = gsc;
}

// ============================================================
// 設定・状態管理
// ============================================================

void ConsecutiveGamesController::configure(int totalGames, bool switchTurn)
{
    qCDebug(lcGame).noquote() << "configure: totalGames=" << totalGames
                       << " switchTurn=" << switchTurn;

    m_totalGames = totalGames;
    m_remainingGames = totalGames - 1;  // 最初の1局目は既に開始されている
    m_gameNumber = 1;
    m_switchTurnEachGame = switchTurn;
}

void ConsecutiveGamesController::onGameStarted(
    const MatchCoordinator::StartOptions& opt,
    const GameStartCoordinator::TimeControl& tc)
{
    qCDebug(lcGame).noquote() << "onGameStarted: mode=" << static_cast<int>(opt.mode)
                       << " sfenStart=" << opt.sfenStart;

    m_lastStartOptions = opt;
    m_lastTimeControl = tc;
}

bool ConsecutiveGamesController::shouldStartNextGame() const
{
    return m_remainingGames > 0;
}

void ConsecutiveGamesController::reset()
{
    m_remainingGames = 0;
    m_totalGames = 1;
    m_gameNumber = 1;
    m_switchTurnEachGame = false;
    m_lastStartOptions = MatchCoordinator::StartOptions();
    m_lastTimeControl = GameStartCoordinator::TimeControl();

    if (m_delayTimer->isActive()) {
        m_delayTimer->stop();
    }
}

// ============================================================
// 次局の準備・開始
// ============================================================

void ConsecutiveGamesController::prepareNextGameOptions()
{
    m_remainingGames--;
    m_gameNumber++;

    // 1局ごとに手番を入れ替える場合
    if (m_switchTurnEachGame) {
        std::swap(m_lastStartOptions.engineName1, m_lastStartOptions.engineName2);
        std::swap(m_lastStartOptions.enginePath1, m_lastStartOptions.enginePath2);
        qCDebug(lcGame) << "Switched engine sides for next game";
    }
}

void ConsecutiveGamesController::startNextGame()
{
    qCDebug(lcGame).noquote() << "startNextGame: remaining=" << m_remainingGames
                       << " gameNumber=" << m_gameNumber;

    if (m_remainingGames <= 0) {
        qCDebug(lcGame) << "No more consecutive games remaining.";
        return;
    }

    prepareNextGameOptions();

    emit requestPreStartCleanup();

    // UIの更新を待つため少し遅延を入れて次の対局を開始
    QTimer::singleShot(100, this, [this]() {
        if (m_timeController) {
            ShogiClock* clk = m_timeController->clock();
            TimeControlUtil::applyToClock(clk, m_lastTimeControl,
                                           m_lastStartOptions.sfenStart, QString());
        }

        emit requestStartNextGame(m_lastStartOptions, m_lastTimeControl);
    });
}
