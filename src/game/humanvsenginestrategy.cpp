/// @file humanvsenginestrategy.cpp
/// @brief 人間 vs エンジンモードの Strategy 実装

#include "humanvsenginestrategy.h"

#include "enginelifecyclemanager.h"
#include "logcategories.h"
#include "matchengineseat.h"

HumanVsEngineStrategy::HumanVsEngineStrategy(MatchStrategyHost& host, Turn engineSide, QString enginePath,
                                             QString engineName, QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_engineSide(engineSide)
    , m_enginePath(std::move(enginePath))
    , m_engineName(std::move(engineName))
    , m_seat(new MatchEngineSeat(&host.engines(), this))
{
    connect(m_seat, &MatchEngineSeat::initialized, this, &HumanVsEngineStrategy::onEngineInitialized);
    connect(m_seat, &MatchEngineSeat::moveReady, this, &HumanVsEngineStrategy::onEngineMoveReady);
}

void HumanVsEngineStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvE (engineSide=" << m_engineSide << ")";

    // 前の対局のエンジンは破棄する。人間対エンジンではエンジンは常に E1 の枠に置く
    m_host.engines().destroyEngines();
    MatchEngineSeat::Params params;
    params.slot = 1;
    params.side = m_engineSide;
    params.enginePath = m_enginePath;
    params.engineName = m_engineName;
    params.timeSource = m_host.timeSource();
    m_seat->start(params);
}

void HumanVsEngineStrategy::onHumanMove(const RecordEntry& played)
{
    if (!m_host.completeHumanMove(played)) return;
    if (m_host.sideToMove() == m_engineSide) requestEngineMoveWhenReady();
}

void HumanVsEngineStrategy::onEngineMoveReady(const UsiMatchMove& move)
{
    if (!m_waitingForMove || move.side != m_engineSide || m_host.isGameOver()) return;
    m_waitingForMove = false;
    m_host.applyEngineMove(move);
}

void HumanVsEngineStrategy::onEngineInitialized()
{
    if (!m_moveWanted || m_host.isGameOver()) return;
    m_moveWanted = false;
    m_host.setWaitingForEngines(false);
    if (m_host.sideToMove() == m_engineSide && !m_waitingForMove) requestEngineMove();
}

void HumanVsEngineStrategy::startInitialMoveIfNeeded()
{
    if (m_host.isGameOver() || m_waitingForMove) return;
    // 人間の初手は、相手エンジンの初期化中も計時を続ける。
    if (m_host.sideToMove() != m_engineSide) return;
    requestEngineMoveWhenReady();
}

void HumanVsEngineStrategy::requestEngineMoveWhenReady()
{
    if (m_seat->isInitializing()) {
        // エンジンの初期化を待つ間は、エンジンの持ち時間を減らさない（人間が先に指した後も同じ）
        m_moveWanted = true;
        m_host.setWaitingForEngines(true);
        return;
    }
    requestEngineMove();
}

void HumanVsEngineStrategy::requestEngineMove()
{
    if (!m_seat->engine()) return;
    m_waitingForMove = true;
    m_host.requestEngineMove(m_engineSide);
}
