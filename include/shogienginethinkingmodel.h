#ifndef SHOGIENGINETHINKINGMODEL_H
#define SHOGIENGINETHINKINGMODEL_H

#include <QVariant>
#include "abstractlistmodel.h"
#include "shogiinforecord.h"

// 将棋エンジンの思考結果をGUI上で表示するためのクラス
class ShogiEngineThinkingModel : public AbstractListModel<ShogiInfoRecord>
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit ShogiEngineThinkingModel(QObject *parent = nullptr);

    // 列数を返す。
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    // データを返す。
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // ヘッダーを返す。
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
};

#endif // SHOGIENGINETHINKINGMODEL_H
