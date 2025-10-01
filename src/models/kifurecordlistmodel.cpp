#include "kifurecordlistmodel.h"
#include <QDebug> // 必要なら

KifuRecordListModel::KifuRecordListModel(QObject *parent)
    : AbstractListModel<KifuDisplay>(parent)
{
}

int KifuRecordListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    // 「指し手」「消費時間」の2列
    return 2;
}

QVariant KifuRecordListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) return QVariant();

    switch (index.column()) {
    case 0:  return list[index.row()]->currentMove(); // 指し手
    case 1:  return list[index.row()]->timeSpent();   // 消費時間
    default: return QVariant();
    }
}

QVariant KifuRecordListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("指し手");
        case 1: return tr("消費時間");
        default: return QVariant();
        }
    } else {
        return QVariant(section + 1);
    }
}

// ----------★ ここから追加実装 ----------

// 末尾の1手（=1行）を削除
bool KifuRecordListModel::removeLastItem()
{
    if (list.isEmpty()) return false;

    const int last = list.size() - 1;
    beginRemoveRows(QModelIndex(), last, last);

    // AbstractListModel<T> が保持している list の型に合わせて消す。
    // 多くの実装では QList<T*> か QList<QSharedPointer<T>> のどちらかです。
    // 1) 生ポインタ（T*）所有の場合：takeLast() して delete
    // 2) QSharedPointer の場合：takeLast() だけ（参照が切れれば自動で delete）
    // ここでは 1) を例示。QSharedPointer の場合は delete を外してください。
    auto *ptr = list.takeLast();
    endRemoveRows();

    // 所有権がモデル側にある前提なら delete
    delete ptr;

    return true;
}

// 末尾から n 手（=n行）を一括削除
bool KifuRecordListModel::removeLastItems(int n)
{
    if (n <= 0 || list.isEmpty()) return false;

    const int toRemove = qMin(n, list.size());
    const int first = list.size() - toRemove;
    const int last  = list.size() - 1;

    beginRemoveRows(QModelIndex(), first, last);

    // まとめて takeLast() して破棄
    for (int i = 0; i < toRemove; ++i) {
        auto *ptr = list.takeLast();
        // 所有権がモデル側にある前提なら delete
        delete ptr;
    }

    endRemoveRows();
    return true;
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
