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

    // ★ 追加: 指定行のUSI形式の読み筋を取得
    QString usiPvAt(int row) const;

    // ★ 追加: 指定行のShogiInfoRecordを取得（読み取り専用）
    const ShogiInfoRecord* recordAt(int row) const;
};

#endif // SHOGIENGINETHINKINGMODEL_H
