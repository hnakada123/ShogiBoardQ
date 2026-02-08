#ifndef KIFUDISPLAYCOORDINATOR_H
#define KIFUDISPLAYCOORDINATOR_H

/// @file kifudisplaycoordinator.h
/// @brief 棋譜表示コーディネータクラスの定義


#include <QObject>
#include <QVector>
#include <QSet>

class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;
class KifuNavigationController;
class RecordPane;
class BranchTreeWidget;
class EngineAnalysisTab;
class KifuRecordListModel;
class KifuBranchListModel;
class LiveGameSession;
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

    /**
     * @brief ライブ対局セッションを設定
     */
    void setLiveGameSession(LiveGameSession* session);

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

    // === 一致性検証 ===

    /**
     * @brief 表示状態の一致性を検証する
     * @return 一致している場合は true
     *
     * 以下の項目を検証する:
     * - m_lastLineIndex と m_state->currentLineIndex() の一致
     * - 棋譜欄の内容が現在のラインと一致しているか
     * - ツリーハイライトの期待値と実際の状態
     */
    bool verifyDisplayConsistency() const;

    /**
     * @brief 一致性レポートを生成する
     * @return 診断情報を含む文字列
     */
    QString getConsistencyReport() const;

    /**
     * @brief 分岐ツリーノードクリック時
     */
    void onBranchTreeNodeClicked(int lineIndex, int ply);

    /**
     * @brief 分岐候補クリック時
     */
    void onBranchCandidateActivated(const QModelIndex& index);

    /**
     * @brief MainWindowからの位置変更通知（棋譜欄ナビゲーション）
     * @param lineIndex 分岐ラインインデックス（0=本譜）
     * @param ply 手数
     * @param sfen 現在局面のSFEN（ツリーから正しいノードを探すために使用）
     */
    void onPositionChanged(int lineIndex, int ply, const QString& sfen);

    // === ライブ対局セッションからのシグナル ===

    /**
     * @brief ライブ対局で手が追加された
     */
    void onLiveGameMoveAdded(int ply, const QString& displayText);

    /**
     * @brief ライブ対局セッション開始時
     */
    void onLiveGameSessionStarted(KifuBranchNode* branchPoint);

    /**
     * @brief ライブ対局の分岐マークが更新された
     */
    void onLiveGameBranchMarksUpdated(const QSet<int>& branchPlys);

    /**
     * @brief ライブ対局が確定された
     */
    void onLiveGameCommitted(KifuBranchNode* newLineEnd);

    /**
     * @brief ライブ対局が破棄された
     */
    void onLiveGameDiscarded();

    /**
     * @brief ライブ対局の棋譜欄更新が必要
     */
    void onLiveGameRecordModelUpdateRequired();

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
    void populateRecordModelFromPath(const QVector<KifuBranchNode*>& path, int highlightPly);

    KifuBranchTree* m_tree;
    KifuNavigationState* m_state;
    KifuNavigationController* m_navController;

    RecordPane* m_recordPane = nullptr;
    BranchTreeWidget* m_branchTreeWidget = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    KifuRecordListModel* m_recordModel = nullptr;
    KifuBranchListModel* m_branchModel = nullptr;
    LiveGameSession* m_liveSession = nullptr;

    int m_lastLineIndex = 0;  // ライン変更検出用
    int m_lastModelLineIndex = -1;  // 棋譜モデルが実際に表示しているライン (-1 = 不明)

    // ツリーハイライトの期待値追跡（一致性検証用）
    int m_expectedTreeLineIndex = 0;
    int m_expectedTreePly = 0;
};

#endif // KIFUDISPLAYCOORDINATOR_H
