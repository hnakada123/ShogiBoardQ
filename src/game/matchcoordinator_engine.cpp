/// @file matchcoordinator_engine.cpp
/// @brief MatchCoordinator エンジン管理・検討セッション転送メソッド

#include "matchcoordinator.h"
#include "enginelifecyclemanager.h"
#include "analysissessionhandler.h"
#include "usi.h"

using P = MatchCoordinator::Player;

// --- エンジンマネージャアクセサ ---

EngineLifecycleManager* MatchCoordinator::engineManager()
{
    ensureEngineManager();
    return m_engineManager;
}

bool MatchCoordinator::isEngineShutdownInProgress() const
{
    return m_engineManager ? m_engineManager->isShutdownInProgress() : false;
}

// --- エンジン管理転送 ---

void MatchCoordinator::updateUsiPtrs(Usi* e1, Usi* e2) {
    ensureEngineManager();
    m_engineManager->updateUsiPtrs(e1, e2);
}

void MatchCoordinator::initializeAndStartEngineFor(Player side,
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
{
    ensureEngineManager();
    m_engineManager->initializeAndStartEngineFor(
        static_cast<EngineLifecycleManager::Player>(side), enginePathIn, engineNameIn);
}

void MatchCoordinator::destroyEngine(int idx, bool clearThinking)
{
    ensureEngineManager();
    m_engineManager->destroyEngine(idx, clearThinking);
}

void MatchCoordinator::destroyEngines(bool clearModels)
{
    ensureEngineManager();
    m_engineManager->destroyEngines(clearModels);
}

void MatchCoordinator::setPlayMode(PlayMode m)
{
    m_playMode = m;
}

void MatchCoordinator::initEnginesForEvE(const QString& engineName1,
                                         const QString& engineName2)
{
    ensureEngineManager();
    m_engineManager->initEnginesForEvE(engineName1, engineName2);
}

Usi* MatchCoordinator::primaryEngine() const
{
    return m_engineManager ? m_engineManager->usi1() : nullptr;
}

Usi* MatchCoordinator::secondaryEngine() const
{
    return m_engineManager ? m_engineManager->usi2() : nullptr;
}

void MatchCoordinator::sendGoToEngine(Usi* which, const GoTimes& t)
{
    ensureEngineManager();
    const EngineLifecycleManager::GoTimes et = { t.btime, t.wtime, t.byoyomi, t.binc, t.winc };
    m_engineManager->sendGoToEngine(which, et);
}

void MatchCoordinator::sendStopToEngine(Usi* which)
{
    ensureEngineManager();
    m_engineManager->sendStopToEngine(which);
}

void MatchCoordinator::sendRawToEngine(Usi* which, const QString& cmd)
{
    ensureEngineManager();
    m_engineManager->sendRawToEngine(which, cmd);
}

// --- 検討セッション転送 ---

void MatchCoordinator::startAnalysis(const AnalysisOptions& opt)
{
    ensureAnalysisSession();
    m_analysisSession->startFullAnalysis(opt);
}

void MatchCoordinator::stopAnalysisEngine()
{
    ensureAnalysisSession();
    m_analysisSession->stopFullAnalysis();
}

void MatchCoordinator::updateConsiderationMultiPV(int multiPV)
{
    ensureAnalysisSession();
    m_analysisSession->updateMultiPV(primaryEngine(), multiPV);
}

bool MatchCoordinator::updateConsiderationPosition(const QString& newPositionStr,
                                                   int previousFileTo, int previousRankTo,
                                                   const QString& lastUsiMove)
{
    ensureAnalysisSession();
    return m_analysisSession->updatePosition(primaryEngine(), newPositionStr,
                                             previousFileTo, previousRankTo, lastUsiMove);
}
