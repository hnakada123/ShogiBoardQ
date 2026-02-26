/// @file analysistabwiring.cpp
/// @brief 解析タブ配線クラスの実装

#include "analysistabwiring.h"

#include "branchnavigationwiring.h"
#include "commentcoordinator.h"
#include "considerationwiring.h"
#include "engineanalysistab.h"
#include "pvclickcontroller.h"
#include "shogienginethinkingmodel.h"
#include "usicommandcontroller.h"

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
    // buildUi()は呼ばない（各ページは独立したドックとして作成）

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

void AnalysisTabWiring::wireExternalSignals(const ExternalSignalDeps& deps)
{
    if (!m_analysisTab) return;

    // 分岐ツリーのアクティベートを BranchNavigationWiring へ
    if (deps.branchNavigationWiring) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
            deps.branchNavigationWiring, &BranchNavigationWiring::onBranchNodeActivated,
            Qt::UniqueConnection);
    }

    // コメント更新を CommentCoordinator へ
    if (deps.commentCoordinator) {
        deps.commentCoordinator->setCommentEditor(m_analysisTab->commentEditor());
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::commentUpdated,
            deps.commentCoordinator, &CommentCoordinator::onCommentUpdated,
            Qt::UniqueConnection);
    }

    // 読み筋行クリックを PvClickController へ直接接続
    if (deps.pvClickController) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::pvRowClicked,
            deps.pvClickController, &PvClickController::onPvRowClicked,
            Qt::UniqueConnection);
    }

    // USIコマンド送信を UsiCommandController へ
    if (deps.usiCommandController) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::usiCommandRequested,
            deps.usiCommandController, &UsiCommandController::sendCommand,
            Qt::UniqueConnection);
    }

    // 検討開始・エンジン設定・エンジン変更シグナルを ConsiderationWiring に接続
    if (deps.considerationWiring) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::startConsiderationRequested,
            deps.considerationWiring, &ConsiderationWiring::displayConsiderationDialog,
            Qt::UniqueConnection);
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::engineSettingsRequested,
            deps.considerationWiring, &ConsiderationWiring::onEngineSettingsRequested,
            Qt::UniqueConnection);
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::considerationEngineChanged,
            deps.considerationWiring, &ConsiderationWiring::onEngineChanged,
            Qt::UniqueConnection);
    }
}
