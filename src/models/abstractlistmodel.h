#ifndef ABSTRACTLISTMODEL_H
#define ABSTRACTLISTMODEL_H

/// @file abstractlistmodel.h
/// @brief ポインタ所有リストモデルの基底テンプレートクラスの定義

#include <QAbstractTableModel>

/**
 * @brief QAbstractTableModel を継承した、ポインタ所有リストモデルの基底テンプレート
 *
 * T* のリストを保持し、追加・削除・全クリアの共通操作を提供する。
 * 破棄時にqDeleteAllで全要素を解放する（所有権あり）。
 *
 * @tparam T リスト項目の型
 *
 * @todo remove コメントスタイルガイド適用済み
 */
template<typename T>
class AbstractListModel : public QAbstractTableModel
{
public:
    explicit AbstractListModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }

    ~AbstractListModel() override
    {
        qDeleteAll(list);
    }

    // --- QAbstractTableModel オーバーライド ---

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return static_cast<int>(list.count());
    }

    // --- 項目操作 API ---

    /// リスト末尾に項目を追加する
    void appendItem(T *item)
    {
        const int count = static_cast<int>(list.count());
        beginInsertRows(QModelIndex(), count, count);
        list.append(item);
        endInsertRows();
    }

    /// リスト末尾に複数の項目を一括追加する（beginInsertRows/endInsertRows を1回で済ませる）
    void appendItems(const QList<T*>& items)
    {
        if (items.isEmpty()) return;

        const int first = static_cast<int>(list.count());
        const int last  = first + static_cast<int>(items.count()) - 1;

        beginInsertRows(QModelIndex(), first, last);
        list.append(items);
        endInsertRows();
    }

    /// リスト先頭に項目を追加する
    void prependItem(T *item)
    {
        beginInsertRows(QModelIndex(), 0, 0);
        list.prepend(item);
        endInsertRows();
    }

    /// リストから特定の項目を削除する
    void removeItem(T *item)
    {
        const int idx = list.indexOf(item);

        if (idx != -1) {
            beginRemoveRows(QModelIndex(), idx, idx);
            list.removeAll(item);
            endRemoveRows();
        }
    }

    /// リストから最後の項目を削除する
    void removeLastItem()
    {
        if (!list.isEmpty()) {
            const int lastIdx = list.count() - 1;
            beginRemoveRows(QModelIndex(), lastIdx, lastIdx);
            list.removeLast();
            endRemoveRows();
        }
    }

    /// リストの全項目を削除する（qDeleteAllで解放後にクリア）
    void clearAllItems()
    {
        beginResetModel();
        qDeleteAll(list);
        list.clear();
        endResetModel();
    }

protected:
    QList<T*> list; ///< 項目ポインタのリスト（所有権あり、破棄時にqDeleteAll）
};

#endif // ABSTRACTLISTMODEL_H
