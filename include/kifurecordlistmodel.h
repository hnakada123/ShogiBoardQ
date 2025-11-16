#ifndef KIFURECORDLISTMODEL_H
#define KIFURECORDLISTMODEL_H

#include <QVariant>
#include "abstractlistmodel.h"
#include "kifudisplay.h"

// 棋譜欄を表示するクラス
class KifuRecordListModel : public AbstractListModel<KifuDisplay>
{
    Q_OBJECT

public:
    explicit KifuRecordListModel(QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool prependItem(KifuDisplay* item);

    // KifuRecordListModel クラス内（public セクション）にある宣言を以下に差し替え
    Q_INVOKABLE bool removeLastItem();
    Q_INVOKABLE bool removeLastItems(int n);
};

#endif // KIFURECORDLISTMODEL_H
