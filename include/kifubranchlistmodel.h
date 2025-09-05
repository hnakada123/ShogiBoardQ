#ifndef KIFUBRANCHLISTMODEL_H
#define KIFUBRANCHLISTMODEL_H

#include <QVariant>
#include <QVector>
#include "abstractlistmodel.h"
#include "kifubranchdisplay.h"
#include "kifdisplayitem.h"

// 分岐候補欄のモデル
class KifuBranchListModel : public AbstractListModel<KifuBranchDisplay>
{
    Q_OBJECT
public:
    explicit KifuBranchListModel(QObject *parent = nullptr);

    // 1列（指し手）
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    // 候補行 + （必要なら）最後に「本譜へ戻る」
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // ---- 追加API ----
    // 候補クリア
    void clearBranchCandidates();

    // KIF表示用（prettyMove/time）から候補を一括設定
    void setBranchCandidatesFromKif(const QList<KifDisplayItem>& rows);

    // 最下段に「本譜へ戻る」を表示する/しない
    void setHasBackToMainRow(bool enabled);
    bool hasBackToMainRow() const;

    // 指定行が「本譜へ戻る」か？
    bool isBackToMainRow(int row) const;
    // 「本譜へ戻る」行のindex（無効時は -1）
    int backToMainRowIndex() const;

private:
    bool m_hasBackToMainRow = false;
};

#endif // KIFUBRANCHLISTMODEL_H
