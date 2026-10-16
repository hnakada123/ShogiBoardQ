/// @file kifubranchlistmodel.cpp
/// @brief 分岐候補欄のモデルの実装

#include "kifubranchlistmodel.h"
#include "movefont.h"
#include "applicationfonts.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifupresentation.h"
#include "tablestyles.h"

#include <QBrush>
#include <QFont>
#include <utility>

KifuBranchListModel::KifuBranchListModel(const KifuBranchTree* tree, QObject* parent)
    : QAbstractTableModel(parent)
    , m_tree(tree)
{
}

int KifuBranchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : candidateCount() + (m_backToMain ? 1 : 0);
}

int KifuBranchListModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1;
}

QString KifuBranchListModel::moveText(const KifuBranchNode* node, bool fullOrigin) const
{
    const QString usi = node->isTerminal() ? QString() : KifuPresentation::usiMove(node->move());
    const QString before = node->parent() ? node->parent()->sfen() : QString();
    return KifuPresentation::moveLabel(node->displayText(), before, usi, fullOrigin);
}

QVariant KifuBranchListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return QVariant();
    const bool backRow = isBackToMainRow(index.row());
    const KifuBranchNode* node = backRow ? nullptr : m_tree->nodeAt(m_nodeIds.at(index.row()));
    if (!backRow && !node) return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        return backRow ? tr("本譜へ戻る") : moveText(node, false);
    case Qt::ToolTipRole:
        return backRow ? QVariant() : QVariant(moveText(node, true));
    case Qt::FontRole: {
        if (backRow) {
            QFont f;
            f.setBold(true);
            return f;
        }
        return node->isActualMove() ? MoveFont::role() : QVariant();
    }
    case Qt::BackgroundRole: {
        static const QBrush kCurrentBg(TableStyles::selectionBackground());
        return index.row() == m_currentRow ? QVariant(kCurrentBg) : QVariant();
    }
    case Qt::TextAlignmentRole:
        return Qt::AlignCenter;
    default:
        return QVariant();
    }
}

QVariant KifuBranchListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Vertical) return section + 1;
    return section == 0 ? QVariant(tr("分岐候補")) : QVariant();
}

void KifuBranchListModel::setCandidates(const QList<KifuBranchNode*>& candidates, int currentRow, bool backToMain)
{
    QList<int> ids;
    ids.reserve(candidates.size());
    for (const KifuBranchNode* node : candidates) ids.append(node->nodeId());
    if (ids != m_nodeIds || backToMain != m_backToMain) {
        // 分岐の無い局面では手を指すたびに呼ばれるので、行が変わったときだけ作り直す
        beginResetModel();
        m_nodeIds = ids;
        m_backToMain = backToMain;
        m_currentRow = currentRow;
        endResetModel();
        return;
    }
    if (currentRow == m_currentRow) return;
    const int oldRow = std::exchange(m_currentRow, currentRow);
    for (const int r : {oldRow, currentRow}) {
        if (r >= 0 && r < rowCount()) emit dataChanged(index(r, 0), index(r, 0), {Qt::BackgroundRole});
    }
}

void KifuBranchListModel::refreshAll()
{
    if (rowCount() > 0) emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}
