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
class EngineAnalysisTab;
class KifuRecordListModel;
class KifuBranchListModel;
class QModelIndex;

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

    /**
     * @brief エンジン解析タブを設定（分岐ツリーハイライト用）
     */
    void setAnalysisTab(EngineAnalysisTab* tab);

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
     * @brief 分岐候補クリック時
     */
    void onBranchCandidateActivated(const QModelIndex& index);

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

    /**
     * @brief 盤面とハイライト更新が必要（分岐ナビゲーション用）
     * @param currentSfen 現在局面のSFEN
     * @param prevSfen 直前局面のSFEN（ハイライト計算用、開始局面の場合は空）
     */
    void boardWithHighlightsRequired(const QString& currentSfen, const QString& prevSfen);

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
    EngineAnalysisTab* m_analysisTab = nullptr;
    KifuRecordListModel* m_recordModel = nullptr;
    KifuBranchListModel* m_branchModel = nullptr;

    int m_lastLineIndex = 0;  // ライン変更検出用
};

#endif // KIFUDISPLAYCOORDINATOR_H
