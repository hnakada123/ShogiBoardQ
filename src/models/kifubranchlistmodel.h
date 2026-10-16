#ifndef KIFUBRANCHLISTMODEL_H
#define KIFUBRANCHLISTMODEL_H

/// @file kifubranchlistmodel.h
/// @brief 分岐候補欄のモデル（行は表示中の手の兄弟ノードと「本譜へ戻る」）

#include <QAbstractTableModel>
#include <QList>

class KifuBranchNode;
class KifuBranchTree;

/**
 * @brief 分岐候補欄のモデル
 *
 * 行は表示中の手と同じ親を持つ手（KifuNavigationState::branchCandidatesAtCurrent()）で、
 * 本譜以外を表示しているときは末尾に「本譜へ戻る」を付ける（KifuNavigationState::offersBackToMainLine()）。
 * 行の並びは KifuNavigationController::selectBranchCandidateRow() と同じ。
 */
class KifuBranchListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit KifuBranchListModel(const KifuBranchTree* tree, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// 候補・強調する行・「本譜へ戻る」を表示する
    void setCandidates(const QList<KifuBranchNode*>& candidates, int currentRow, bool backToMain);

    /// 「本譜へ戻る」を除いた候補の数
    int candidateCount() const { return static_cast<int>(m_nodeIds.size()); }
    bool hasBackToMainRow() const { return m_backToMain; }
    bool isBackToMainRow(int row) const { return m_backToMain && row == candidateCount(); }
    int currentHighlightRow() const { return m_currentRow; }

    /// 全行を描き直す（表記を変えた）
    void refreshAll();

private:
    QString moveText(const KifuBranchNode* node, bool fullOrigin) const;

    const KifuBranchTree* m_tree = nullptr;
    QList<int> m_nodeIds;
    bool m_backToMain = false;
    int m_currentRow = -1;
};

#endif // KIFUBRANCHLISTMODEL_H
