#ifndef KIFUANALYSISLISTMODEL_H
#define KIFUANALYSISLISTMODEL_H

/// @file kifuanalysislistmodel.h
/// @brief 棋譜解析の結果の表のモデル

#include <QAbstractTableModel>
#include <QList>
#include <QVariant>

#include "kifuanalysisrow.h"
#include "notationtextcache.h"

/**
 * @brief 棋譜解析の結果の表（1行が1局面）
 *
 * 行は KifuAnalysisRow の値で持つ。手・候補手・読み筋の表示は表記の設定に合わせて作り、作った文字列は
 * 表記の設定が変わるまで（KifuPresentation::revision）覚えておく（英語表記では局面をたどって作るので重い）。
 */
class KifuAnalysisListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// 列（「盤面」はその行の読み筋の局面を開くボタン）
    enum Column {
        MoveColumn,
        CandidateColumn,
        MatchColumn,
        EvaluationColumn,
        JudgementColumn,
        DifferenceColumn,
        BoardColumn,
        PvColumn,
        ColumnCount
    };

    explicit KifuAnalysisListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// 先手から見た評価値の表示（定跡・未取得は「-」、詰みは思考の表と同じ「3手詰」）
    static QVariant evaluationText(const UsiScore& score);
    /// 先手から見た評価値から判定した形勢（互角・先手：有利 など。評価値がなければ空）
    static QString judgementText(const UsiScore& score);

    /// 指定行の解析結果（範囲外なら nullptr。行を足す・消すまで有効）
    const KifuAnalysisRow* row(int row) const;
    /// 末尾に行を足す
    void appendRow(const KifuAnalysisRow& row);
    /// すべての行を消す
    void clear();

private:
    /// 表示用の表記の種類
    enum DisplayKind { MoveDisplay, CandidateDisplay, PvDisplay, DisplayKindCount };

    /// 1行と、表示用に作った表記
    struct Entry {
        KifuAnalysisRow row;
        NotationTextCache<DisplayKindCount> texts;
    };

    /// 表記の設定に合わせた手・候補手・読み筋
    QString display(const Entry& entry, DisplayKind kind, bool full = false) const;

    QList<Entry> m_rows;
};

#endif // KIFUANALYSISLISTMODEL_H
