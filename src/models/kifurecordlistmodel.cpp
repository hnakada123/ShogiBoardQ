/// @file kifurecordlistmodel.cpp
/// @brief 棋譜欄のモデルの実装

#include "kifurecordlistmodel.h"
#include "movefont.h"
#include "applicationfonts.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifupresentation.h"
#include "tablestyles.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <utility>

KifuRecordListModel::KifuRecordListModel(const KifuBranchTree* tree, QObject* parent)
    : QAbstractTableModel(parent)
    , m_tree(tree)
{
}

int KifuRecordListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_nodeIds.size());
}

int KifuRecordListModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

KifuBranchNode* KifuRecordListModel::nodeAt(int row) const
{
    return row >= 0 && row < m_nodeIds.size() ? m_tree->nodeAt(m_nodeIds.at(row)) : nullptr;
}

bool KifuRecordListModel::isBranchRow(const KifuBranchNode* node) const
{
    return node->parent() != nullptr && node->parent()->hasBranch();
}

QString KifuRecordListModel::moveText(const KifuBranchNode* node, bool fullOrigin) const
{
    if (!node->parent()) return tr("=== 開始局面 ===");
    const QString usi = node->isTerminal() ? QString() : KifuPresentation::usiMove(node->move());
    return QString::number(node->ply()).rightJustified(4, QLatin1Char(' ')) + QLatin1Char(' ')
        + KifuPresentation::moveLabel(node->displayText(), node->parent()->sfen(), usi, fullOrigin)
        + (isBranchRow(node) ? KifuPresentation::branchMark() : QString());
}

QVariant KifuRecordListModel::data(const QModelIndex& index, int role) const
{
    const KifuBranchNode* node = index.isValid() ? nodeAt(index.row()) : nullptr;
    if (!node) return QVariant();
    const int col = index.column();

    switch (role) {
    case Qt::DisplayRole:
        switch (col) {
        case MoveColumn: return moveText(node, false);
        case TimeColumn: return node->parent() ? node->timeText() : tr("（１手 / 合計）");
        case BookmarkColumn: return node->bookmark();
        case CommentColumn: return node->comment().simplified();
        default: return QVariant();
        }
    case Qt::ToolTipRole:
        if (col == MoveColumn) return moveText(node, true);
        if (col == CommentColumn && !node->comment().isEmpty()) {
            QString html = node->comment().toHtmlEscaped();
            html.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
            return QStringLiteral("<qt>%1</qt>").arg(html);
        }
        return QVariant();
    case Qt::FontRole:
        return col == MoveColumn && node->isActualMove() ? MoveFont::role() : QVariant();
    case Qt::BackgroundRole: {
        // 現在行は淡い青、分岐がある手はオレンジ系、その他は白
        static const QBrush kCurrentBg(TableStyles::selectionBackground());
        static const QBrush kBranchBg(QColor(255, 224, 178));
        static const QBrush kWhiteBg(QColor(0xff, 0xff, 0xff));
        if (index.row() == m_currentRow) return kCurrentBg;
        return isBranchRow(node) ? kBranchBg : kWhiteBg;
    }
    case Qt::TextAlignmentRole:
        return col == TimeColumn ? QVariant(Qt::AlignCenter) : QVariant();
    default:
        return QVariant();
    }
}

QVariant KifuRecordListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Vertical) return section + 1;
    switch (section) {
    case MoveColumn: return tr("指し手");
    case TimeColumn: return tr("消費時間");
    case BookmarkColumn: return tr("しおり");
    case CommentColumn: return tr("コメント");
    default: return QVariant();
    }
}

void KifuRecordListModel::setPath(const QList<KifuBranchNode*>& path)
{
    QList<int> ids;
    ids.reserve(path.size());
    for (const KifuBranchNode* node : path) ids.append(node->nodeId());

    // 先頭から同じ行は残す。何も同じでなければ全行を足し直す（棋譜欄が全行の幅を測り直す）
    qsizetype same = 0;
    while (same < ids.size() && same < m_nodeIds.size() && ids.at(same) == m_nodeIds.at(same)) ++same;
    if (same < m_nodeIds.size()) {
        beginRemoveRows(QModelIndex(), static_cast<int>(same), static_cast<int>(m_nodeIds.size() - 1));
        m_nodeIds.resize(same);
        if (m_currentRow >= same) m_currentRow = -1;
        endRemoveRows();
    }
    if (same < ids.size()) {
        beginInsertRows(QModelIndex(), static_cast<int>(same), static_cast<int>(ids.size() - 1));
        m_nodeIds = ids;
        endInsertRows();
    }
}

void KifuRecordListModel::setCurrentHighlightRow(int row)
{
    if (m_currentRow == row) return;
    const int oldRow = std::exchange(m_currentRow, row);
    for (const int r : {oldRow, row}) {
        if (r >= 0 && r < rowCount()) emit dataChanged(index(r, 0), index(r, ColumnCount - 1), {Qt::BackgroundRole});
    }
}

void KifuRecordListModel::refreshNode(int nodeId)
{
    const int row = static_cast<int>(m_nodeIds.indexOf(nodeId));
    if (row >= 0) emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

void KifuRecordListModel::refreshAll()
{
    if (rowCount() > 0) emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
}
