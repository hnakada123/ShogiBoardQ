/// @file enginevsenginestrategy.cpp
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "enginevsenginestrategy.h"

#include <QTimer>

#include "enginelifecyclemanager.h"
#include "logcategories.h"
#include "matchengineseat.h"

EngineVsEngineStrategy::EngineVsEngineStrategy(MatchStrategyHost& host, Engines engines, QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_engines(std::move(engines))
    , m_black(new MatchEngineSeat(&host.engines(), this))
    , m_white(new MatchEngineSeat(&host.engines(), this))
{
    for (MatchEngineSeat* seat : {m_black, m_white}) {
        connect(seat, &MatchEngineSeat::initialized, this, &EngineVsEngineStrategy::kickNextTurn);
        connect(seat, &MatchEngineSeat::moveReady, this, &EngineVsEngineStrategy::onEngineMoveReady);
    }
}

void EngineVsEngineStrategy::start()
{
    m_host.engines().destroyEngines();
    MatchEngineSeat::Params black;
    black.slot = 1;
    black.side = Turn::Black;
    black.enginePath = m_engines.blackPath;
    black.engineName = m_engines.blackName;
    black.timeSource = m_host.timeSource();
    MatchEngineSeat::Params white = black;
    white.slot = 2;
    white.side = Turn::White;
    white.enginePath = m_engines.whitePath;
    white.engineName = m_engines.whiteName;
    m_black->start(black);
    m_white->start(white);
    kickNextTurn();
}

MatchEngineSeat* EngineVsEngineStrategy::engineSeat(Turn side) const
{
    return side == Turn::Black ? m_black : m_white;
}

void EngineVsEngineStrategy::kickNextTurn()
{
    if (m_waitingForMove || m_host.isGameOver()) return;
    if (!m_black->engine() || !m_white->engine()) return;

    // どちらかのエンジンの初期化を待つ間は、持ち時間を減らさない
    const bool initializing = m_black->isInitializing() || m_white->isInitializing();
    m_host.setWaitingForEngines(initializing);
    if (initializing) return;
    m_waitingForMove = true;
    m_host.requestEngineMove(m_host.sideToMove());
}

void EngineVsEngineStrategy::onEngineMoveReady(const UsiMatchMove& move)
{
    if (!m_waitingForMove || m_host.isGameOver() || move.side != m_host.sideToMove()) return;
    m_waitingForMove = false;
    if (!m_host.applyEngineMove(move)) return;
    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextTurn);
}
