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

    // ★ 追加：末尾の1手（=1行）を削除。成功したら true
    bool removeLastItem();

    // ★ 追加（任意だが推奨）：末尾から n 手（=n行）を一括削除。成功したら true
    bool removeLastItems(int n);

    bool prependItem(KifuDisplay* item);
};

#endif // KIFURECORDLISTMODEL_H
