/// @file recordpane_branch.cpp
/// @brief 棋譜欄の分岐候補の開閉と選択操作

#include "recordpane.h"
#include "gamesettings.h"
#include "kifubranchlistmodel.h"
#include "logcategories.h"

#include <QHeaderView>
#include <QTableView>
#include <QToolButton>
#include <QTimer>

void RecordPane::onBranchToggled(bool expanded)
{
    GameSettings::setKifuBranchExpanded(expanded);
    updateBranchAppearance();
    updateKifuTableWidth();
}

void RecordPane::updateBranchAppearance()
{
    if (!m_branchToggle) return;
    const bool expanded = m_branchToggle->isChecked();
    m_branchToggle->setFont(m_branch->font());
    m_branchToggle->setText(expanded ? tr("分岐候補") : QString());
    m_branchToggle->setAccessibleName(tr("分岐候補"));
    m_branchToggle->setToolTip(expanded ? tr("分岐候補を折りたたむ") : tr("分岐候補を展開する"));
    m_branchToggle->setArrowType(expanded ? Qt::LeftArrow : Qt::RightArrow);
    m_branchToggle->setFixedHeight(m_kifu->horizontalHeader()->sizeHint().height());
    m_branch->setVisible(expanded);
    const int expandedWidth = qMax(120, m_branchToggle->fontMetrics().horizontalAdvance(tr("分岐候補")) + 32);
    m_branchContainer->setFixedWidth(expanded ? expandedWidth : 28);
}

void RecordPane::onBranchCurrentRowChanged(const QModelIndex& current, const QModelIndex&)
{
    auto* brModel = qobject_cast<KifuBranchListModel*>(m_branch->model());
    if (brModel && current.isValid()) {
        brModel->setCurrentHighlightRow(current.row());
    }
}

void RecordPane::onBranchClicked(const QModelIndex& index)
{
    m_lastClickedBranchIndex = index;
    m_branchClickGuard = true;
    QTimer::singleShot(0, this, &RecordPane::clearBranchClickGuard);
    emit branchActivated(index);
}

void RecordPane::onBranchActivated(const QModelIndex& index)
{
    if (m_branchClickGuard && index.isValid() && index == m_lastClickedBranchIndex) {
        qCDebug(lcUi).noquote() << "[RecordPane] onBranchActivated: skipped (already handled by clicked) row=" << index.row();
        return;
    }
    emit branchActivated(index);
}

void RecordPane::clearBranchClickGuard()
{
    m_branchClickGuard = false;
    m_lastClickedBranchIndex = QPersistentModelIndex();
}

void RecordPane::onBranchContextMenuRequested(const QPoint& pos)
{
    const QModelIndex index = m_branch->indexAt(pos);
    emit branchContextMenuRequested(index.isValid() ? index.row() : -1, m_branch->viewport()->mapToGlobal(pos));
}
