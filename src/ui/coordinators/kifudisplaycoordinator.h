#ifndef KIFUDISPLAYCOORDINATOR_H
#define KIFUDISPLAYCOORDINATOR_H

#include <QObject>
#include <QVector>

class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;
class KifuNavigationController;
class RecordPane;
class BranchTreeWidget;
class KifuRecordListModel;
class KifuBranchListModel;

/**
 * @brief UI更新を統括するコーディネーター
 *
 * KifuNavigationControllerからのシグナルを受けて、
 * 棋譜欄、分岐ツリー、分岐候補欄の更新を行う。
 */
class KifuDisplayCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit KifuDisplayCoordinator(
        KifuBranchTree* tree,
        KifuNavigationState* state,
        KifuNavigationController* navController,
        QObject* parent = nullptr);

    // === UI要素の設定 ===

    /**
     * @brief 棋譜ペインを設定
     */
    void setRecordPane(RecordPane* pane);

    /**
     * @brief 分岐ツリーウィジェットを設定
     */
    void setBranchTreeWidget(BranchTreeWidget* widget);

    /**
     * @brief 棋譜欄モデルを設定
     */
    void setRecordModel(KifuRecordListModel* model);

    /**
     * @brief 分岐候補モデルを設定
     */
    void setBranchModel(KifuBranchListModel* model);

    // === 初期化 ===

    /**
     * @brief シグナル接続を行う
     */
    void wireSignals();

public slots:
    // === KifuNavigationControllerからのシグナルを受けるスロット ===

    /**
     * @brief ナビゲーション完了時
     */
    void onNavigationCompleted(KifuBranchNode* node);

    /**
     * @brief 盤面更新要求
     */
    void onBoardUpdateRequired(const QString& sfen);

    /**
     * @brief 棋譜欄ハイライト要求
     */
    void onRecordHighlightRequired(int ply);

    /**
     * @brief 分岐ツリーハイライト要求
     */
    void onBranchTreeHighlightRequired(int lineIndex, int ply);

    /**
     * @brief 分岐候補更新要求
     */
    void onBranchCandidatesUpdateRequired(const QVector<KifuBranchNode*>& candidates);

    // === ツリー変更時 ===

    /**
     * @brief ツリー構造変更時
     */
    void onTreeChanged();

    /**
     * @brief 分岐ツリーノードクリック時
     */
    void onBranchTreeNodeClicked(int lineIndex, int ply);

    /**
     * @brief 旧ナビゲーションシステムからの位置変更通知
     * @param row 行番号（分岐インデックス）
     * @param ply 手数
     * @param sfen 現在局面のSFEN（ツリーから正しいノードを探すために使用）
     */
    void onLegacyPositionChanged(int row, int ply, const QString& sfen);

signals:
    /**
     * @brief 盤面更新が必要
     */
    void boardSfenChanged(const QString& sfen);

private:
    void updateRecordView();
    void updateBranchTreeView();
    void updateBranchCandidatesView();
    void highlightCurrentPosition();
    void populateRecordModel();
    void populateBranchMarks();

    KifuBranchTree* m_tree;
    KifuNavigationState* m_state;
    KifuNavigationController* m_navController;

    RecordPane* m_recordPane = nullptr;
    BranchTreeWidget* m_branchTreeWidget = nullptr;
    KifuRecordListModel* m_recordModel = nullptr;
    KifuBranchListModel* m_branchModel = nullptr;
};

#endif // KIFUDISPLAYCOORDINATOR_H
