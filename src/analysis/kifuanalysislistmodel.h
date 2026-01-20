#ifndef KIFUANALYSISLISTMODEL_H
#define KIFUANALYSISLISTMODEL_H

#include <QVariant>
#include "abstractlistmodel.h"
#include "kifuanalysisresultsdisplay.h"

// 棋譜解析結果をGUI上で表示するためのクラス
class KifuAnalysisListModel : public AbstractListModel<KifuAnalysisResultsDisplay>
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuAnalysisListModel(QObject *parent = nullptr);

    // 列数を返すメソッド
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    // データを返すメソッド
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // ヘッダを返すメソッド
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 指定行のアイテムを取得
    KifuAnalysisResultsDisplay* item(int row) const
    {
        if (row < 0 || row >= list.size()) return nullptr;
        return list.at(row);
    }
};

#endif // KIFUANALYSISLISTMODEL_H

