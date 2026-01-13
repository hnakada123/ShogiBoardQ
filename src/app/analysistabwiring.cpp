#include "analysistabwiring.h"

#include "engineanalysistab.h"
#include "shogienginethinkingmodel.h"

#include <QTabWidget>
#include <QDebug>

AnalysisTabWiring::AnalysisTabWiring(const Deps& d, QObject* parent)
    : QObject(parent), m_d(d)
{
}

EngineAnalysisTab* AnalysisTabWiring::buildUiAndWire()
{
    // すでに構築済みならそのまま返す
    if (m_analysisTab) return m_analysisTab;

    // 1) タブ本体の生成と UI 構築
    m_analysisTab = new EngineAnalysisTab(m_d.centralParent);
    m_analysisTab->buildUi();

    // 2) 思考モデル（先後）を生成（親=this で寿命管理）
    m_think1 = new ShogiEngineThinkingModel(this);
    m_think2 = new ShogiEngineThinkingModel(this);

    // 3) モデルをタブにセット（EngineAnalysisTab の API 仕様に合わせる）
    m_analysisTab->setModels(m_think1, m_think2, m_d.log1, m_d.log2);

    // 4) 既定は単機表示（EvE で必要になれば MainWindow 側で切替）
    m_analysisTab->setDualEngineVisible(false);

    // 5) QTabWidget を拾っておく
    m_tab = m_analysisTab->tab();

    // 6) signal → signal の中継（ラムダ不使用）
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
        this,          &AnalysisTabWiring::branchNodeActivated,
        Qt::UniqueConnection);

    return m_analysisTab;
}
