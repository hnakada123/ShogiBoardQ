/// @file enginevsenginestrategy.cpp
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "enginevsenginestrategy.h"

#include <QTimer>

#include "enginelifecyclemanager.h"
#include "logcategories.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "usi.h"

EngineVsEngineStrategy::EngineVsEngineStrategy(MatchStrategyHost& host, Engines engines, QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_engines(std::move(engines))
{
}

void EngineVsEngineStrategy::start()
{
    EngineLifecycleManager& seats = m_host.engines();
    seats.destroyEngines();
    Usi* black = seats.createEngine(1, Turn::Black, m_engines.blackName);
    Usi* white = seats.createEngine(2, Turn::White, m_engines.whiteName);
    for (Usi* engine : {black, white}) {
        connect(engine, &Usi::engineInitialized, this, &EngineVsEngineStrategy::kickNextTurn);
        connect(engine, &Usi::matchMoveReady, this, &EngineVsEngineStrategy::onEngineMoveReady);
    }
    seats.startEngine(black, m_engines.blackPath, m_engines.blackName);
    seats.startEngine(white, m_engines.whitePath, m_engines.whiteName);

    // 初手から時計を動かす（駒落ちでは後手番から始まる。手番は GC が SFEN に従う）
    if (ShogiClock* clock = m_host.clock()) {
        clock->startClock();
        qCDebug(lcGame) << "Clock started";
    }
    m_host.refreshBoardAndTurn();
    kickNextTurn();
}

void EngineVsEngineStrategy::kickNextTurn()
{
    if (m_waitingForMove || m_host.isGameOver()) return;
    Usi* black = m_host.engines().engineFor(Turn::Black);
    Usi* white = m_host.engines().engineFor(Turn::White);
    if (!black || !white) return;

    if (black->isInitializing() || white->isInitializing()) {
        m_waitingInitialization = true;
        if (ShogiClock* clock = m_host.clock()) clock->stopClock();
        return;
    }
    if (m_waitingInitialization) {
        m_waitingInitialization = false;
        if (ShogiClock* clock = m_host.clock()) clock->startClock();
    }
    m_waitingForMove = true;
    m_host.requestEngineMove(m_host.gameController()->currentPlayer());
}

void EngineVsEngineStrategy::onEngineMoveReady(const UsiMove& move, const QString& /*position*/, const QString& ponder)
{
    if (!m_waitingForMove || m_host.isGameOver()) return;
    const Turn mover = m_host.gameController()->currentPlayer();
    Usi* engine = m_host.engines().engineFor(mover);
    if (sender() != engine) return;
    m_waitingForMove = false;
    m_host.setEnginePonder(mover, ponder);
    // 相手のエンジンにも直前の手の行き先を知らせる（読み筋の「同」の判定に使う）
    if (Usi* receiver = m_host.engines().engineFor(mover == Turn::Black ? Turn::White : Turn::Black))
        receiver->setPreviousMove(move.to);
    if (!m_host.applyEngineMove(move, mover, engine->lastBestmoveElapsedMs())) return;
    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextTurn);
}
