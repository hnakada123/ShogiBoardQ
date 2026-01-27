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

    // 1) EngineAnalysisTabを生成（UIはMainWindowのcreateAnalysisDocks()で構築する）
    m_analysisTab = new EngineAnalysisTab(m_d.centralParent);
    // ★ 変更: buildUi()は呼ばない（各ページは独立したドックとして作成）

    // 2) 思考モデル（先後）を生成（親=this で寿命管理）
    m_think1 = new ShogiEngineThinkingModel(this);
    m_think2 = new ShogiEngineThinkingModel(this);

    // 3) 既定は単機表示（EvE で必要になれば MainWindow 側で切替）
    m_analysisTab->setDualEngineVisible(false);

    // 4) QTabWidget は使用しない（ドックモード）
    m_tab = nullptr;

    // 5) signal → signal の中継（ラムダ不使用）
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
        this,          &AnalysisTabWiring::branchNodeActivated,
        Qt::UniqueConnection);

    return m_analysisTab;
}
