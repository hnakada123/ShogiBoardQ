/// @file enginedisplaypresenter.cpp
/// @brief 対局・検討のエンジンの思考情報と通信ログの表示の実装

#include "enginedisplaypresenter.h"

#include "analysissession.h"
#include "enginelifecyclemanager.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "usimodelbinding.h"

EngineDisplayPresenter::EngineDisplayPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.engines);
    connect(m_deps.engines, &EngineLifecycleManager::enginesChanged, this, &EngineDisplayPresenter::bindEngines);
    connect(m_deps.engines, &EngineLifecycleManager::displayClearRequested,
            this, &EngineDisplayPresenter::clearDisplay);
    if (AnalysisSession* analysis = m_deps.analysis) {
        connect(analysis, &AnalysisSession::considerationSearchStarted,
                this, &EngineDisplayPresenter::onConsiderationSearchStarted);
        connect(analysis, &AnalysisSession::considerationModeEnded,
                this, &EngineDisplayPresenter::onConsiderationModeEnded);
        connect(analysis, &AnalysisSession::considerationResultsCleared,
                this, &EngineDisplayPresenter::clearConsideration);
    }
}

void EngineDisplayPresenter::bindEngines()
{
    for (size_t i = 0; i < m_engines.size(); ++i) {
        Usi* engine = m_deps.engines->engineInSlot(static_cast<int>(i) + 1);
        if (engine == m_engines[i]) continue;
        m_engines[i] = engine;
        // 前のエンジンの結びはエンジンと一緒に破棄される
        m_bindings[i] = engine ? new UsiModelBinding(engine, {i == 0 ? m_deps.commLog1 : m_deps.commLog2,
                                                              i == 0 ? m_deps.thinking1 : m_deps.thinking2})
                               : nullptr;
    }
}

void EngineDisplayPresenter::clearDisplay()
{
    for (UsiCommLogModel* log : {m_deps.commLog1, m_deps.commLog2}) {
        if (log) log->clear();
    }
    for (ShogiEngineThinkingModel* thinking : {m_deps.thinking1, m_deps.thinking2}) {
        if (thinking) thinking->clearAllItems();
    }
}

void EngineDisplayPresenter::onConsiderationSearchStarted()
{
    // 検討は E1 のエンジンで行う。候補手の数は検討のやり直しのたびに合わせる
    UsiModelBinding* binding = m_bindings[0];
    if (!binding || !m_deps.consideration || !m_deps.analysis) return;
    binding->setConsideration(m_deps.consideration, m_deps.analysis->considerationMultiPV());
}

void EngineDisplayPresenter::onConsiderationModeEnded()
{
    if (UsiModelBinding* binding = m_bindings[0]) binding->setConsideration(nullptr, 1);
}

void EngineDisplayPresenter::clearConsideration()
{
    if (m_deps.consideration) m_deps.consideration->clearAllItems();
}
