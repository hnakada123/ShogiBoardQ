/// @file humanvsenginestrategy.cpp
/// @brief 人間 vs エンジンモードの Strategy 実装

#include "humanvsenginestrategy.h"

#include "enginelifecyclemanager.h"
#include "logcategories.h"
#include "usi.h"

HumanVsEngineStrategy::HumanVsEngineStrategy(MatchStrategyHost& host, Turn engineSide, QString enginePath,
                                             QString engineName, QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_engineSide(engineSide)
    , m_enginePath(std::move(enginePath))
    , m_engineName(std::move(engineName))
{
}

void HumanVsEngineStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvE (engineSide=" << m_engineSide << ")";

    // 前の対局のエンジンは破棄する。人間対エンジンではエンジンは常に E1 の枠に置く
    m_host.engines().destroyEngines();
    Usi* engine = m_host.engines().createEngine(1, m_engineSide, m_engineName);
    connect(engine, &Usi::engineInitialized, this, &HumanVsEngineStrategy::onEngineInitialized);
    connect(engine, &Usi::matchMoveReady, this, &HumanVsEngineStrategy::onEngineMoveReady);
    m_host.engines().startEngine(engine, m_enginePath, m_engineName);
}

void HumanVsEngineStrategy::onHumanMove(const RecordEntry& played)
{
    if (!m_host.completeHumanMove(played)) return;
    if (m_host.sideToMove() == m_engineSide) requestEngineMove();
}

void HumanVsEngineStrategy::onEngineMoveReady(const UsiMatchMove& move)
{
    if (!m_waitingForMove || move.side != m_engineSide || m_host.isGameOver()) return;
    m_waitingForMove = false;
    m_host.applyEngineMove(move);
}

void HumanVsEngineStrategy::onEngineInitialized()
{
    if (!m_initialMoveRequested || m_host.isGameOver()) return;
    m_host.setWaitingForEngines(false);
    startInitialMoveIfNeeded();
}

void HumanVsEngineStrategy::startInitialMoveIfNeeded()
{
    if (m_host.isGameOver() || m_waitingForMove) return;
    // 人間の初手は、相手エンジンの初期化中も計時を続ける。
    if (m_host.sideToMove() != m_engineSide) return;

    m_initialMoveRequested = true;
    Usi* engine = m_host.engines().engineFor(m_engineSide);
    if (engine && engine->isInitializing()) {
        // エンジンの初期化を待つ間は、エンジンの持ち時間を減らさない
        m_host.setWaitingForEngines(true);
        return;
    }
    requestEngineMove();
}

void HumanVsEngineStrategy::requestEngineMove()
{
    if (!m_host.engines().engineFor(m_engineSide)) return;
    m_waitingForMove = true;
    m_host.requestEngineMove(m_engineSide);
}
