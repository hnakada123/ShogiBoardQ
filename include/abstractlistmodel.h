#ifndef ABSTRACTLISTMODEL_H
#define ABSTRACTLISTMODEL_H

#include <QAbstractTableModel>

// Tは具体的なリスト項目の型を指すテンプレートパラメータ
template<typename T>
class AbstractListModel : public QAbstractTableModel
{
public:
    // コンストラクタ
    explicit AbstractListModel(QObject *parent = nullptr)
        // 基底クラスのコンストラクタにparentを渡す。
        : QAbstractTableModel(parent)
    {
    }

    // デストラクタ
    ~AbstractListModel()
    {
        // リストの全項目を削除する（QtのqDeleteAll関数を使用する）。
        qDeleteAll(list);
    }

    // 行数を返すメソッド。基底クラスの純粋仮想関数をオーバーライドする。
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        // QModelIndex引数はこのメソッド内では使用しない。
        Q_UNUSED(parent)

        // リストの項目数を返す。
        return list.count();
    }

    // リストに項目を追加する。
    void appendItem(T *item)
    {
        // リストに項目を追加する前にモデルの状態を変更することを通知する。
        beginInsertRows(QModelIndex(), list.count(), list.count());

        // リストに項目を追加する。
        list.append(item);

        // モデルの状態の変更を完了することを通知する。
        endInsertRows();
    }

    void prependItem(T *item)
    {
        // リストに項目を追加する前にモデルの状態を変更することを通知する。
        // インデックスを0に変更する。
        beginInsertRows(QModelIndex(), 0, 0);

        // リストの先頭に項目を追加する。
        list.prepend(item);

        // モデルの状態の変更を完了することを通知する。
        endInsertRows();
    }

    // リストから特定の項目を削除する。
    void removeItem(T *item)
    {
        // 削除する項目のインデックスを取得する。
        const int idx = list.indexOf(item);

         // 項目が存在する場合
        if (idx != -1) {
            // リストから項目を削除する前にモデルの状態を変更することを通知する。
            beginRemoveRows(QModelIndex(), idx, idx);

            // リストから項目を削除する。
            list.removeAll(item);

            // モデルの状態の変更を完了することを通知する。
            endRemoveRows();
        }
    }

    // リストから最後の項目を削除する。
    void removeLastItem()
    {
        // リストが空でない場合
        if (!list.isEmpty()) {
            // 最後の項目のインデックスを取得します
            const int lastIdx = list.count() - 1;

            // リストから項目を削除する前にモデルの状態を変更することを通知する。
            beginRemoveRows(QModelIndex(), lastIdx, lastIdx);

            // リストから最後の項目を削除する。
            list.removeLast();

            // モデルの状態の変更を完了することを通知する。
            endRemoveRows();
        }
    }

    // リストから全ての項目を削除する。
    void clearAllItems()
    {
        // モデル全体のリセットを開始することを通知する。
        beginResetModel();

        // リスト内の全てのポインタを削除し、その後リストをクリアする。
        qDeleteAll(list);

        // リストから全ての項目を削除する。
        list.clear();

        // モデル全体のリセットを完了することを通知する。
        endResetModel();
    }

protected:
    // テンプレート型Tのポインタを項目とするリスト
    QList<T*> list;
};

#endif // ABSTRACTLISTMODEL_H
