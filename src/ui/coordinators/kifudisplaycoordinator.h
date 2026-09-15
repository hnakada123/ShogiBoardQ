#ifndef KIFUDISPLAYCOORDINATOR_H
#define KIFUDISPLAYCOORDINATOR_H

/// @file kifudisplaycoordinator.h
/// @brief 棋譜表示コーディネータクラスの定義


#include "kifudisplaypresenter.h"

#include <QObject>
#include <QList>
#include <QSet>
#include <functional>
#include <memory>

class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;
class KifuNavigationController;
class RecordPane;
class BranchTreeManager;
class KifuRecordListModel;
class KifuBranchListModel;
class KifuSelectionSync;
class LiveGameSession;
class QModelIndex;

/**
 * @brief UI更新を統括するコーディネーター
 *
 * KifuNavigationControllerからのシグナルを受けて、
 * 棋譜欄、分岐ツリー、分岐候補欄の更新を行う。
 *
 * 表示データ構築は KifuDisplayPresenter に、
 * 選択状態同期は KifuSelectionSync に委譲する。
 */
class KifuDisplayCoordinator : public QObject
{
    Q_OBJECT

public:
    using DisplaySnapshot = KifuDisplayPresenter::DisplaySnapshot;
    using BoardSfenProvider = std::function<QString()>;

    explicit KifuDisplayCoordinator(
        KifuBranchTree* tree,
        KifuNavigationState* state,
        KifuNavigationController* navController,
        QObject* parent = nullptr);
    ~KifuDisplayCoordinator() override;

    void setBoardSfenProvider(BoardSfenProvider provider);

    // === UI要素の設定 ===

    void setRecordPane(RecordPane* pane);
    void setRecordModel(KifuRecordListModel* model);
    void setBranchModel(KifuBranchListModel* model);
    void setBranchTreeManager(BranchTreeManager* manager);
    void setLiveGameSession(LiveGameSession* session);

    // === 初期化 ===

    void wireSignals();

    // === 一致性検証 ===

    DisplaySnapshot captureDisplaySnapshot() const;
    bool verifyDisplayConsistencyDetailed(QString* reason = nullptr) const;
    bool verifyDisplayConsistency() const;
    QString consistencyReport() const;

public slots:
    // === KifuNavigationControllerからのシグナルを受けるスロット ===

    void onNavigationCompleted(KifuBranchNode* node);
    void onBoardUpdateRequired(const QString& sfen);
    void onRecordHighlightRequired(int ply);
    void onBranchTreeHighlightRequired(int lineIndex, int ply);
    // NOLINTNEXTLINE(clazy-fully-qualified-moc-types) -- QList<Ptr> false positive in clazy 1.17
    void onBranchCandidatesUpdateRequired(const QList<KifuBranchNode *>& candidates);

    // === ツリー変更時 ===

    void onTreeChanged();
    void resetTracking();
    void onBranchCandidateActivated(const QModelIndex& index);
    void onPositionChanged(int lineIndex, int ply, const QString& sfen);

    // === ライブ対局セッションからのシグナル ===

    void onLiveGameMoveAdded(int ply, const QString& displayText);
    void onLiveGameSessionStarted(KifuBranchNode* branchPoint);
    void onLiveGameCommitted(KifuBranchNode* newLineEnd);
    void onLiveGameDiscarded();
    void onLiveGameRecordModelUpdateRequired();

signals:
    void boardSfenChanged(const QString& sfen);
    void boardWithHighlightsRequired(const QString& currentSfen, const QString& prevSfen);
    void commentUpdateRequired(int ply, const QString& comment, bool asHtml);

private:
    /// 状態の現在ラインと棋譜欄モデルのラインがずれていれば棋譜欄を再構築する
    /// @return 再構築した場合 true
    bool syncRecordViewToCurrentLine();
    /// ライブ対局で追加されたノードを分岐ツリーへ差分反映する
    /// @return 反映できた（または反映不要だった）場合 true。false なら全再構築が必要
    bool appendLiveNodeToBranchTree(KifuBranchNode* liveNode);
    void updateRecordView();
    void updateBranchTreeView();
    void updateBranchCandidatesView();
    void highlightCurrentPosition();
    void runPendingNavResultCheck();
    void syncPresenterRefs();
    void syncSelectionSyncRefs();
    KifuDisplayPresenter::TrackingState trackingState() const;

    KifuBranchTree* m_tree;
    KifuNavigationState* m_state;
    KifuNavigationController* m_navController;

    RecordPane* m_recordPane = nullptr;
    BranchTreeManager* m_branchTreeManager = nullptr;
    KifuRecordListModel* m_recordModel = nullptr;
    KifuBranchListModel* m_branchModel = nullptr;
    LiveGameSession* m_liveSession = nullptr;
    BoardSfenProvider m_boardSfenProvider;
    int m_branchTreeNodeCount = -1;   ///< 分岐ツリーに最後に反映したときのツリーのノード数

    std::unique_ptr<KifuDisplayPresenter> m_presenter;
    std::unique_ptr<KifuSelectionSync> m_selectionSync;

    int m_lastLineIndex = 0;
    bool m_pendingNavResultCheck = false;
    bool m_signalsWired = false;
};

#endif // KIFUDISPLAYCOORDINATOR_H
