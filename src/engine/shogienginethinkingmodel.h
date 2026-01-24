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

    // ★ 追加: MultiPVモードで行を更新または挿入する
    // multipv値に基づいて既存の行を更新するか、新しい行を挿入する
    // maxMultiPV: 表示する最大行数（1〜10）
    void updateByMultipv(ShogiInfoRecord* record, int maxMultiPV);

    // ★ 追加: 全行を評価値でソートする（高い順）
    void sortByScore();
};

#endif // SHOGIENGINETHINKINGMODEL_H
