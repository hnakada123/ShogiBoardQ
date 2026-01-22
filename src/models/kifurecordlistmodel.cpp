#include "kifurecordlistmodel.h"
#include <QDebug> // 必要なら
#include <QColor>
#include <QBrush>

KifuRecordListModel::KifuRecordListModel(QObject *parent)
    : AbstractListModel<KifuDisplay>(parent)
    , m_currentHighlightRow(0)  // ★ 起動時は開始局面（行0）をハイライト
{
}

int KifuRecordListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    // 「指し手」「消費時間」「コメント」の3列
    return 3;
}

QVariant KifuRecordListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    const int row = index.row();
    const int col = index.column();

    // 背景色：現在行は黄色、分岐ありの手はオレンジ系、その他はクリーム色
    if (role == Qt::BackgroundRole) {
        // 現在行（黄色ハイライト）を優先
        if (row == m_currentHighlightRow) {
            static const QBrush kYellowBg(QColor(255, 255, 0));
            return kYellowBg;
        }
        // 分岐ありの手はオレンジ系
        if (row > 0 && m_branchPlySet.contains(row)) {
            static const QBrush kOrangeBg(QColor(255, 224, 178));
            return kOrangeBg;
        }
        // デフォルト背景色（クリーム色）
        static const QBrush kCreamBg(QColor(0xfe, 0xfc, 0xf6));  // #fefcf6
        return kCreamBg;
    }

    // テキスト配置：消費時間列は中央揃え
    if (role == Qt::TextAlignmentRole) {
        if (col == 1) {
            return Qt::AlignCenter;
        }
        return QVariant();
    }

    if (role != Qt::DisplayRole) return QVariant();

    switch (col) {
    case 0: {
        // 指し手列：分岐ありなら末尾に '+' を付与（表示上のみ）
        QString s = list[row]->currentMove();
        if (row > 0 && m_branchPlySet.contains(row) && !s.endsWith(QLatin1Char('+'))) {
            s.append(QLatin1Char('+'));
        }
        return s;
    }
    case 1:
        // 消費時間列
        return list[row]->timeSpent();
    case 2:
        // コメント列
        return list[row]->comment();
    default:
        return QVariant();
    }
}

QVariant KifuRecordListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("指し手");
        case 1: return tr("消費時間");
        case 2: return tr("コメント");
        default: return QVariant();
        }
    } else {
        return QVariant(section + 1);
    }
}

// 先頭に1件追加
bool KifuRecordListModel::prependItem(KifuDisplay* item)
{
    if (!item) return false;

    beginInsertRows(QModelIndex(), 0, 0);
    list.insert(0, item);      // 先頭へ
    endInsertRows();
    return true;
}

// 末尾の1手（=1行）を削除
bool KifuRecordListModel::removeLastItem()
{
    if (list.isEmpty()) return false;

    const int last = static_cast<int>(list.size() - 1);
    beginRemoveRows(QModelIndex(), last, last);

    // 所有モデルで生ポインタ管理の場合
    auto *ptr = list.takeLast();
    endRemoveRows();

    delete ptr; // 所有権がモデル側にある前提

    return true;
}

// 末尾から n 手（=n行）を一括削除
bool KifuRecordListModel::removeLastItems(int n)
{
    if (n <= 0 || list.isEmpty()) return false;

    const int toRemove = static_cast<int>(qMin(qsizetype(n), list.size()));
    const int first = static_cast<int>(list.size()) - toRemove;
    const int last  = static_cast<int>(list.size() - 1);

    beginRemoveRows(QModelIndex(), first, last);

    for (int i = 0; i < toRemove; ++i) {
        auto *ptr = list.takeLast();
        delete ptr; // 所有権がモデル側にある前提
    }

    endRemoveRows();
    return true;
}

// 全項目をクリア（ハイライト行もリセット）
void KifuRecordListModel::clearAllItems()
{
    // ハイライト行を -1 にリセット（次に setCurrentHighlightRow(0) が呼ばれた時に
    // 正しく dataChanged が発火するようにするため）
    m_currentHighlightRow = -1;

    // 基底クラスの clearAllItems() を呼び出し
    AbstractListModel<KifuDisplay>::clearAllItems();
}

// 分岐あり手の集合をセットし、表示更新
void KifuRecordListModel::setBranchPlyMarks(const QSet<int>& ply1Set)
{
    m_branchPlySet = ply1Set;

    if (rowCount() > 0) {
        const QModelIndex tl = index(0, 0);
        const QModelIndex br = index(rowCount() - 1, columnCount() - 1);
        // 指し手の '+' 付与と背景色の両方を更新
        emit dataChanged(tl, br, { Qt::DisplayRole, Qt::BackgroundRole });
    }
}

// 現在の行（黄色ハイライト）を設定
void KifuRecordListModel::setCurrentHighlightRow(int row)
{
    if (m_currentHighlightRow == row) return;

    const int oldRow = m_currentHighlightRow;
    m_currentHighlightRow = row;

    // 旧行と新行の背景色を更新
    if (oldRow >= 0 && oldRow < rowCount()) {
        const QModelIndex tl = index(oldRow, 0);
        const QModelIndex br = index(oldRow, columnCount() - 1);
        emit dataChanged(tl, br, { Qt::BackgroundRole });
    }
    if (row >= 0 && row < rowCount()) {
        const QModelIndex tl = index(row, 0);
        const QModelIndex br = index(row, columnCount() - 1);
        emit dataChanged(tl, br, { Qt::BackgroundRole });
    }
}
