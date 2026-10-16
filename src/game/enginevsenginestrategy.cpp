/// @file enginevsenginestrategy.cpp
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "enginevsenginestrategy.h"

#include <QTimer>

#include "enginelifecyclemanager.h"
#include "logcategories.h"
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
    kickNextTurn();
}

void EngineVsEngineStrategy::kickNextTurn()
{
    if (m_waitingForMove || m_host.isGameOver()) return;
    Usi* black = m_host.engines().engineFor(Turn::Black);
    Usi* white = m_host.engines().engineFor(Turn::White);
    if (!black || !white) return;

    // どちらかのエンジンの初期化を待つ間は、持ち時間を減らさない
    const bool initializing = black->isInitializing() || white->isInitializing();
    m_host.setWaitingForEngines(initializing);
    if (initializing) return;
    m_waitingForMove = true;
    m_host.requestEngineMove(m_host.sideToMove());
}

void EngineVsEngineStrategy::onEngineMoveReady(const UsiMatchMove& move)
{
    if (!m_waitingForMove || m_host.isGameOver() || move.side != m_host.sideToMove()) return;
    m_waitingForMove = false;
    // 相手のエンジンにも直前の手の行き先を知らせる（読み筋の「同」の判定に使う）
    if (Usi* receiver = m_host.engines().engineFor(opponent(move.side)))
        receiver->setPreviousMove(move.move.to);
    if (!m_host.applyEngineMove(move)) return;
    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextTurn);
}
