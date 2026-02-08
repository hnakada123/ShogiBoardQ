#ifndef SHOGIENGINETHINKINGMODEL_H
#define SHOGIENGINETHINKINGMODEL_H

/// @file shogienginethinkingmodel.h
/// @brief エンジン思考結果の表示用リストモデルの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QVariant>
#include "abstractlistmodel.h"
#include "shogiinforecord.h"

/**
 * @brief エンジンの思考結果（読み筋・評価値等）をテーブル表示するためのモデル
 *
 * AbstractListModel<ShogiInfoRecord> を継承し、
 * 時間・深さ・ノード数・評価値・盤面・読み筋の6列を提供する。
 * MultiPVモードではmultipv値に基づく行の更新・挿入をサポートする。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class ShogiEngineThinkingModel : public AbstractListModel<ShogiInfoRecord>
{
    Q_OBJECT

public:
    explicit ShogiEngineThinkingModel(QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// 指定行のUSI形式の読み筋を取得する
    QString usiPvAt(int row) const;

    /// 指定行のShogiInfoRecordを取得する（読み取り専用）
    const ShogiInfoRecord* recordAt(int row) const;

    /**
     * @brief MultiPVモードで行を更新または挿入する
     * @param record 挿入するレコード（所有権を移譲）
     * @param maxMultiPV 表示する最大行数（1〜10）
     *
     * multipv値に基づいて既存の行を更新するか、新しい行を挿入する。
     * maxMultiPVを超える行は末尾から削除される。
     */
    void updateByMultipv(ShogiInfoRecord* record, int maxMultiPV);

    /// 全行を評価値の高い順にソートする
    void sortByScore();
};

#endif // SHOGIENGINETHINKINGMODEL_H
