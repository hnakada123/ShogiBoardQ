#ifndef KIFUBRANCHLISTMODEL_H
#define KIFUBRANCHLISTMODEL_H

#include <QVariant>
#include "abstractlistmodel.h"
#include "kifubranchdisplay.h"

// 棋譜の分岐欄を表示するクラス
class KifuBranchListModel : public AbstractListModel<KifuBranchDisplay>
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuBranchListModel(QObject *parent = nullptr);

    // 列数を返す。
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    // データを返す。
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // ヘッダを返す。
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
};

#endif // KIFUBRANCHLISTMODEL_H
