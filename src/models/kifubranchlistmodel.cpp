#include "kifubranchlistmodel.h"

KifuBranchListModel::KifuBranchListModel(QObject *parent)
    : AbstractListModel<KifuBranchDisplay>(parent)
{}

int KifuBranchListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1; // 「分岐候補」1列
}

int KifuBranchListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_useInternalRows ? m_rows.size() : list.size(); // 後方互換
}

QVariant KifuBranchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) return {};

    if (m_useInternalRows) {
        // 新方式：KifDisplayItem を直接表示
        return m_rows.at(index.row()).prettyMove;
    } else {
        // 旧方式（後方互換）：KifuBranchDisplay（QObject派生）
        // 既存コードと同じ
        return list[index.row()]->currentMove();
    }
}

QVariant KifuBranchListModel::headerData(int section, Qt::Orientation orientation,
                                         int role) const
{
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        if (section == 0) return tr("分岐候補");
        return {};
    } else {
        return section + 1; // 行番号
    }
}

// ---------------- 追加API ----------------

void KifuBranchListModel::clearBranchCandidates()
{
    beginResetModel();
    m_rows.clear();
    m_useInternalRows = true;   // 空の内部行を使う
    endResetModel();
}

void KifuBranchListModel::setBranchCandidatesFromKif(const QList<KifDisplayItem>& rows)
{
    beginResetModel();
    m_rows = rows;
    m_useInternalRows = true;   // 以降は内部行を使う
    endResetModel();
}
