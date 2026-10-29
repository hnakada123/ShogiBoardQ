/// @file enginedisplaypresenter.cpp
/// @brief エンジンの表示の持ち主の実装

#include "enginedisplaypresenter.h"

#include "analysissession.h"
#include "considerationpanel.h"
#include "csalogpanel.h"
#include "engineinfowidget.h"
#include "engineslots.h"
#include "evaluationgraphpresenter.h"
#include "kifuanalysislistmodel.h"
#include "kifudocument.h"
#include "shogienginethinkingmodel.h"
#include "thinkingpanel.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "usilogpanel.h"
#include "usimodelbinding.h"

EngineDisplayPresenter::EngineDisplayPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.engines);
    connect(m_deps.engines, &EngineSlots::enginesChanged, this, &EngineDisplayPresenter::bindEngines);
    connect(m_deps.engines, &EngineSlots::displayClearRequested,
            this, &EngineDisplayPresenter::clearEngineOutput);
    if (AnalysisSession* analysis = m_deps.analysis) {
        connect(analysis, &AnalysisSession::considerationSearchStarted,
                this, &EngineDisplayPresenter::onConsiderationSearchStarted);
        connect(analysis, &AnalysisSession::considerationWaitingStarted,
                this, &EngineDisplayPresenter::onConsiderationWaitingStarted);
        connect(analysis, &AnalysisSession::considerationModeEnded,
                this, &EngineDisplayPresenter::onConsiderationModeEnded);
        connect(analysis, &AnalysisSession::considerationResultsCleared,
                this, &EngineDisplayPresenter::clearConsideration);
        connect(analysis, &AnalysisSession::engineNamesChanged,
                this, &EngineDisplayPresenter::onAnalysisEngineNamesChanged);
    }
    if (m_deps.document) {
        connect(m_deps.document, &KifuDocument::playersChanged, this, &EngineDisplayPresenter::onPlayersChanged);
    }
    setSecondEngineVisible(false);
}

// ============================================================
// エンジンとモデルを結ぶ
// ============================================================

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

void EngineDisplayPresenter::clearEngineOutput()
{
    for (UsiCommLogModel* log : {m_deps.commLog1, m_deps.commLog2}) {
        if (log) log->clear();
    }
    for (ShogiEngineThinkingModel* thinking : {m_deps.thinking1, m_deps.thinking2}) {
        if (thinking) thinking->clear();
    }
}

void EngineDisplayPresenter::clearAll()
{
    clearEngineOutput();
    clearConsideration();
    if (m_deps.analysisModel) m_deps.analysisModel->clear();
    if (UsiLogPanel* log = m_deps.usiLog) log->clear();
    if (CsaLogPanel* log = m_deps.csaLog) log->clear();
    if (ThinkingPanel* panel = m_deps.thinkingPanel) {
        panel->info1()->setDisplayNameFallback(QString());
        panel->info2()->setDisplayNameFallback(QString());
    }
    if (ConsiderationPanel* tab = m_deps.considerationTab) {
        if (tab->considerationInfo()) tab->considerationInfo()->setModel(nullptr);
        // 検討の実行の状態（ボタン・経過時間）は AnalysisSession の知らせだけから決める
        tab->setConsiderationEngineName(QString());
    }
}

// ============================================================
// 検討
// ============================================================

void EngineDisplayPresenter::onConsiderationSearchStarted()
{
    // 検討は E1 のエンジンで行う。候補手の数は検討のやり直しのたびに合わせる
    if (UsiModelBinding* binding = m_bindings[0]; binding && m_deps.consideration && m_deps.analysis)
        binding->setConsideration(m_deps.consideration, m_deps.analysis->considerationMultiPV());
    if (ConsiderationPanel* tab = m_deps.considerationTab) {
        // 検討タブと思考タブのエンジン欄に E1 の情報欄をつなぐ
        if (tab->considerationInfo()) tab->considerationInfo()->setModel(m_deps.commLog1);
        tab->setRunState(ConsiderationPanel::RunState::Running);
    }
    if (m_deps.thinkingPanel) m_deps.thinkingPanel->info1()->setModel(m_deps.commLog1);
}

void EngineDisplayPresenter::onConsiderationWaitingStarted()
{
    if (ConsiderationPanel* tab = m_deps.considerationTab)
        tab->setRunState(ConsiderationPanel::RunState::Waiting);
}

void EngineDisplayPresenter::onConsiderationModeEnded()
{
    if (UsiModelBinding* binding = m_bindings[0]) binding->setConsideration(nullptr, 1);
    if (ConsiderationPanel* tab = m_deps.considerationTab)
        tab->setRunState(ConsiderationPanel::RunState::Idle);
}

void EngineDisplayPresenter::clearConsideration()
{
    if (m_deps.consideration) m_deps.consideration->clear();
}

// ============================================================
// エンジン名・2つ目のエンジン欄
// ============================================================

void EngineDisplayPresenter::onAnalysisEngineNamesChanged(const QString& engine1, const QString& /*engine2*/)
{
    // 検討・詰み探索のエンジンは1つ（2つ目のエンジン欄を隠し、評価値グラフは1つ目の系列に出す）
    setSecondEngineVisible(false);
    if (EvaluationGraphPresenter* graph = m_deps.evalGraph) {
        graph->setEngine1Name(engine1);
        graph->setEngine2Name(QString());
    }
    if (m_deps.considerationTab) m_deps.considerationTab->setConsiderationEngineName(engine1);
    if (m_deps.thinkingPanel && !engine1.isEmpty()) m_deps.thinkingPanel->info1()->setDisplayNameFallback(engine1);
}

void EngineDisplayPresenter::onPlayersChanged()
{
    const MatchPlayers& players = m_deps.document->players();
    // エンジン同士の対局だけ2つ目のエンジン情報を表示する
    setSecondEngineVisible(players.engineVsEngine());
    // 評価値グラフは先手のエンジンを1つ目、後手のエンジンを2つ目の系列として描く
    if (EvaluationGraphPresenter* graph = m_deps.evalGraph) {
        graph->setEngine1Name(players.engineName(Turn::Black));
        graph->setEngine2Name(players.engineName(Turn::White));
    }
}

void EngineDisplayPresenter::setSecondEngineVisible(bool visible)
{
    if (m_deps.thinkingPanel) m_deps.thinkingPanel->setSecondEngineVisible(visible);
}
