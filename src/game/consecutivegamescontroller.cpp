#include "consecutivegamescontroller.h"
#include "timecontrolcontroller.h"
#include "shogiclock.h"
#include "timecontrolutil.h"

#include <QDebug>
#include <utility>

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

void ConsecutiveGamesController::configure(int totalGames, bool switchTurn)
{
    qDebug().noquote() << "[CGC] configure: totalGames=" << totalGames
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
    qDebug().noquote() << "[CGC] onGameStarted: mode=" << static_cast<int>(opt.mode)
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

void ConsecutiveGamesController::prepareNextGameOptions()
{
    // 残り回数をデクリメント、対局番号をインクリメント
    m_remainingGames--;
    m_gameNumber++;

    // 1局ごとに手番を入れ替える場合
    if (m_switchTurnEachGame) {
        std::swap(m_lastStartOptions.engineName1, m_lastStartOptions.engineName2);
        std::swap(m_lastStartOptions.enginePath1, m_lastStartOptions.enginePath2);
        qDebug() << "[CGC] Switched engine sides for next game";
    }
}

void ConsecutiveGamesController::startNextGame()
{
    qDebug().noquote() << "[CGC] startNextGame: remaining=" << m_remainingGames
                       << " gameNumber=" << m_gameNumber;

    if (m_remainingGames <= 0) {
        qDebug() << "[CGC] No more consecutive games remaining.";
        return;
    }

    prepareNextGameOptions();

    // 前準備クリーンアップを要求
    emit requestPreStartCleanup();

    // 少し遅延を入れて次の対局を開始（UIの更新を待つため）
    // 実際の開始はrequestStartNextGameシグナルで行う
    QTimer::singleShot(100, this, [this]() {
        // 時計の適用
        if (m_timeController) {
            ShogiClock* clk = m_timeController->clock();
            TimeControlUtil::applyToClock(clk, m_lastTimeControl,
                                           m_lastStartOptions.sfenStart, QString());
        }

        emit requestStartNextGame(m_lastStartOptions, m_lastTimeControl);
    });
}
