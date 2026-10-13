/// @file matchcoordinator_engine.cpp
/// @brief MatchCoordinator エンジン管理・検討セッション転送メソッド

#include "matchcoordinator.h"
#include "enginelifecyclemanager.h"
#include "analysissessionhandler.h"
#include "usi.h"


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

void MatchCoordinator::setEngineModels(UsiCommLogModel* comm1, ShogiEngineThinkingModel* think1,
                                       UsiCommLogModel* comm2, ShogiEngineThinkingModel* think2)
{
    ensureEngineManager();
    m_engineManager->setModelPtrs(comm1, think1, comm2, think2);
}

void MatchCoordinator::resetPositionHistory()
{
    m_positionStr1.clear();
    m_positionPonder1.clear();
    m_positionStr2.clear();
    m_positionPonder2.clear();
    m_positionStrHistory.clear();
    m_allGameHistories.clear();
    m_cur = Turn::Black;
    m_currentMoveIndex = 0;
}

void MatchCoordinator::initializeAndStartEngineFor(Turn side,
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
{
    ensureEngineManager();
    m_engineManager->initializeAndStartEngineFor(side, enginePathIn, engineNameIn);
    if (primaryEngine()) primaryEngine()->setMatchClock(clock());
    if (secondaryEngine()) secondaryEngine()->setMatchClock(clock());
}

void MatchCoordinator::destroyEngines(bool clearModels)
{
    ensureEngineManager();
    m_engineManager->destroyEngines(clearModels);
}

void MatchCoordinator::setPlayMode(PlayMode m)
{
    m_playMode = m;
    emit playModeChanged();
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

void MatchCoordinator::sendStopToEngine(Usi* which)
{
    ensureEngineManager();
    m_engineManager->sendStopToEngine(which);
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
