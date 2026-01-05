#ifndef KIFURECORDLISTMODEL_H
#define KIFURECORDLISTMODEL_H

#include <QVariant>
#include <QSet>                    // ★ 追加
#include "abstractlistmodel.h"
#include "kifudisplay.h"

class KifuRecordListModel : public AbstractListModel<KifuDisplay>
{
    Q_OBJECT
public:
    explicit KifuRecordListModel(QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool prependItem(KifuDisplay* item);
    Q_INVOKABLE bool removeLastItem();
    Q_INVOKABLE bool removeLastItems(int n);

    // ★ 追加：分岐のある手（ply1=1..N）集合をセット
    void setBranchPlyMarks(const QSet<int>& ply1Set);
    QSet<int> branchPlyMarks() const { return m_branchPlySet; }

    // ★ 追加：現在の行（黄色ハイライト）を設定
    void setCurrentHighlightRow(int row);
    int currentHighlightRow() const { return m_currentHighlightRow; }

    // ★ 追加：指定インデックスの項目を取得
    KifuDisplay* item(int index) const {
        if (index < 0 || index >= list.size()) return nullptr;
        return list.at(index);
    }

private:
    // ★ 追加：分岐あり手の集合（モデルの行番号＝ply1と一致。0は「開始局面」で除外）
    QSet<int> m_branchPlySet;

    // ★ 追加：現在ハイライトする行（-1 = ハイライトなし）
    int m_currentHighlightRow = 0;  // 起動時は開始局面（行0）をハイライト
};

#endif // KIFURECORDLISTMODEL_H
