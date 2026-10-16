#ifndef SHOGIENGINETHINKINGMODEL_H
#define SHOGIENGINETHINKINGMODEL_H

/// @file shogienginethinkingmodel.h
/// @brief エンジン思考結果の表示用リストモデルの定義

#include <QVariant>
#include <optional>
#include "abstractlistmodel.h"
#include "shogiinforecord.h"

/**
 * @brief エンジンの思考結果（読み筋・評価値等）をテーブル表示するためのモデル
 *
 * AbstractListModel<ShogiInfoRecord> を継承し、
 * 時間・深さ・ノード数・評価値・盤面・読み筋の6列を提供する。
 * MultiPVモードではmultipv値に基づく行の更新・挿入をサポートする。
 *
 */
class ShogiEngineThinkingModel : public AbstractListModel<ShogiInfoRecord>
{
    Q_OBJECT

public:
    /// 列（「盤面」はその行の読み筋の局面を開くボタン）
    enum Column { TimeColumn, DepthColumn, NodesColumn, ScoreColumn, BoardColumn, PvColumn, ColumnCount };

    explicit ShogiEngineThinkingModel(QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// 評価値の表示文字列（cp は数、詰みは「3手詰」、境界つきは「120++」）。cp の数は QVariant の数で返す
    static QVariant scoreDisplay(const UsiScore& score);
    /// 詰みの評価値の表示（列幅の見積もりにも使う）
    static QString mateScoreText(int plies);
    /// 「盤面」列のボタンの文字（列幅の見積もりにも使う）
    static QString boardButtonText();

    /// 指定行のShogiInfoRecordを取得する（読み取り専用）
    const ShogiInfoRecord* recordAt(int row) const;

    /// 指定multipv値を持つ行のインデックスを返す（見つからなければstd::nullopt）
    std::optional<int> findRowByMultipv(int multipv) const;

    /// 思考の表: 先頭に行を足す（先頭と同じ内容なら足さない）。maxRows を超えた古い行は消す
    void prependRow(const ThinkingInfoRow& row, int maxRows);

    /**
     * @brief 検討の表: multipv の行を更新するか、multipv 順の位置に足す
     * @param maxMultiPV 表示する最大行数（1〜10）。超えた行は末尾から消す
     *
     * 同じ内容なら何もしない。
     */
    void updateByMultipv(const ThinkingInfoRow& row, int maxMultiPV);

    /// 指定行数を超えた古い行（末尾）を削除する
    void trimToMaxRows(int maxRows);
};

#endif // SHOGIENGINETHINKINGMODEL_H
