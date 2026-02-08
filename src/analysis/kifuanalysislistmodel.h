#ifndef KIFUANALYSISLISTMODEL_H
#define KIFUANALYSISLISTMODEL_H

/// @file kifuanalysislistmodel.h
/// @brief 棋譜解析結果リストモデルクラスの定義


#include <QVariant>
#include "abstractlistmodel.h"
#include "kifuanalysisresultsdisplay.h"

/**
 * @brief 棋譜解析結果を表示するテーブルモデル
 *
 * 解析結果1行を `KifuAnalysisResultsDisplay` として保持し、
 * 解析ダイアログで表示する各列データを提供する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class KifuAnalysisListModel : public AbstractListModel<KifuAnalysisResultsDisplay>
{
    Q_OBJECT

public:
    explicit KifuAnalysisListModel(QObject *parent = nullptr);

    /// 列数を返す
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /// 指定セルの表示データを返す
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /// ヘッダ表示データを返す
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// 指定行の解析結果を返す
    KifuAnalysisResultsDisplay* item(int row) const
    {
        if (row < 0 || row >= list.size()) return nullptr;
        return list.at(row);
    }
};

#endif // KIFUANALYSISLISTMODEL_H
