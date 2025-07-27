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
    // コンストラクタ
    explicit KifuRecordListModel(QObject *parent = nullptr);

    // 列数を返す。
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    // データを返す。
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // ヘッダを返す。
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
};

#endif // KIFURECORDLISTMODEL_H
