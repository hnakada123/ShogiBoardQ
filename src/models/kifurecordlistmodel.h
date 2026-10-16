#ifndef KIFURECORDLISTMODEL_H
#define KIFURECORDLISTMODEL_H

/// @file kifurecordlistmodel.h
/// @brief 棋譜欄のモデル（行は分岐ツリーのノード。表示はノードから作る）

#include <QAbstractTableModel>
#include <QList>

class KifuBranchNode;
class KifuBranchTree;

/**
 * @brief 棋譜欄のモデル
 *
 * 行は開始局面からの手順のノード（行番号＝手数）。指し手・消費時間・しおり・コメントの文字列は持たず、
 * 表示するたびにノードから作る（棋譜の正本は分岐ツリーだけ）。
 * どの手順を出すかは KifuDisplayPresenter が setPath() で決める。
 */
class KifuRecordListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { MoveColumn, TimeColumn, BookmarkColumn, CommentColumn, ColumnCount };

    explicit KifuRecordListModel(const KifuBranchTree* tree, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief 行を手順 path（開始局面から）に合わせる
     *
     * 今の行と先頭が同じ部分は残し、違う部分だけ削って足す（1手指すたびに全行を並べ直さない）。
     */
    void setPath(const QList<KifuBranchNode*>& path);
    /// row 行目のノード（範囲外や消えたノードは nullptr）
    KifuBranchNode* nodeAt(int row) const;

    /// 強調する現在の行（-1 は強調しない）
    void setCurrentHighlightRow(int row);
    int currentHighlightRow() const { return m_currentRow; }

    /// ノードのコメント・しおりなどが変わった（その行を描き直す）
    void refreshNode(int nodeId);
    /// 全行を描き直す（表記を変えた・分岐が増えて印が変わった）
    void refreshAll();

private:
    QString moveText(const KifuBranchNode* node, bool fullOrigin) const;
    bool isBranchRow(const KifuBranchNode* node) const;

    const KifuBranchTree* m_tree = nullptr;
    QList<int> m_nodeIds;   ///< 行のノード ID（行0は開始局面）
    int m_currentRow = -1;
};

#endif // KIFURECORDLISTMODEL_H
