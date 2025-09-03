#ifndef KIFUBRANCHLISTMODEL_H
#define KIFUBRANCHLISTMODEL_H

#include <QVariant>
#include "abstractlistmodel.h"      // 既存
#include "kifubranchdisplay.h"      // 既存（QObject継承）
#include "kifdisplayitem.h"         // ← 先ほど作った表示用1行データ

// 分岐候補（右側リスト）モデル
class KifuBranchListModel : public AbstractListModel<KifuBranchDisplay>
{
    Q_OBJECT
public:
    explicit KifuBranchListModel(QObject *parent = nullptr);

    // === QAbstractItemModel ===
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // === 追加API（こちらを使ってください） ===
    void clearBranchCandidates();                                // 空にする
    void setBranchCandidatesFromKif(const QList<KifDisplayItem>& rows); // 一括差し替え

private:
    // ここに詰めて表示する（QObjectコピー問題を回避）
    QList<KifDisplayItem> m_rows;
    bool m_useInternalRows = false; // true: m_rows を使う / false: 旧 list を使う（後方互換）
};

#endif // KIFUBRANCHLISTMODEL_H
