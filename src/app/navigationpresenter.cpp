#include "navigationpresenter.h"
#include "kifuloadcoordinator.h"
#include "engineanalysistab.h"
#include <QDebug>

NavigationPresenter::NavigationPresenter(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_coordinator(d.coordinator)
    , m_analysisTab(d.analysisTab)
{
}

void NavigationPresenter::setDeps(const Deps& d)
{
    m_coordinator = d.coordinator;
    m_analysisTab = d.analysisTab;
}

void NavigationPresenter::highlightBranchTree(int row, int ply)
{
    if (!m_analysisTab) return;
    // ツリー側に行/手のハイライトとセンタリングを依頼
    m_analysisTab->highlightBranchTreeAt(row, ply, /*centerOn=*/true);
}

void NavigationPresenter::refreshAll(int row, int ply)
{
    if (!m_coordinator) {
        qDebug() << "[NAV-PRES] refreshAll skipped (coordinator==null)";
        return;
    }

    const int safeRow = qMax(0, row);
    const int safePly = qMax(0, ply);

    // 1) 候補再構築は Coordinator に依頼
    m_coordinator->showBranchCandidates(safeRow, safePly);

    // 2) 再構築が終わった前提でツリーハイライト＆通知
    highlightBranchTree(safeRow, safePly);
    emit branchUiUpdated(safeRow, safePly);
}

void NavigationPresenter::refreshBranchCandidates(int row, int ply)
{
    // 実質 refreshAll と同義（必要なら分離可能）
    refreshAll(row, ply);
}

void NavigationPresenter::updateAfterBranchListChanged(int row, int ply)
{
    // ★ Coordinator 側からのみ呼ばれる“事後通知API”
    //    （ここでは Coordinator には戻らず、ハイライトと通知だけ行う）
    const int safeRow = qMax(0, row);
    const int safePly = qMax(0, ply);

    highlightBranchTree(safeRow, safePly);
    emit branchUiUpdated(safeRow, safePly);
}
