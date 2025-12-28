#ifndef RECORDNAVIGATIONCONTROLLER_H
#define RECORDNAVIGATIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

#include "kifutypes.h"  // ResolvedRow

class ShogiView;
class BoardSyncPresenter;
class KifuRecordListModel;
class KifuLoadCoordinator;
class ReplayController;
class EngineAnalysisTab;
class RecordPane;
class GameRecordPresenter;
class QTableView;

/**
 * @brief RecordNavigationController - 棋譜ナビゲーション管理クラス
 *
 * MainWindowから棋譜選択・盤面同期の処理を分離したクラス。
 * 以下の責務を担当:
 * - 棋譜行選択時の盤面同期
 * - 未保存コメントの確認
 * - 分岐候補の表示
 * - ナビゲーション状態の更新
 */
class RecordNavigationController : public QObject
{
    Q_OBJECT

public:
    explicit RecordNavigationController(QObject* parent = nullptr);
    ~RecordNavigationController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setShogiView(ShogiView* view);
    void setBoardSyncPresenter(BoardSyncPresenter* boardSync);
    void setKifuRecordModel(KifuRecordListModel* model);
    void setKifuLoadCoordinator(KifuLoadCoordinator* coordinator);
    void setReplayController(ReplayController* replayController);
    void setAnalysisTab(EngineAnalysisTab* analysisTab);
    void setRecordPane(RecordPane* recordPane);
    void setRecordPresenter(GameRecordPresenter* presenter);

    // --------------------------------------------------------
    // 状態参照の設定（ポインタ経由）
    // --------------------------------------------------------
    void setResolvedRows(const QVector<ResolvedRow>* resolvedRows);
    void setActiveResolvedRow(const int* activeResolvedRow);

    // --------------------------------------------------------
    // コールバック設定
    // --------------------------------------------------------
    using EnsureBoardSyncCallback = std::function<void()>;
    using EnableArrowButtonsCallback = std::function<void()>;
    using SetCurrentTurnCallback = std::function<void()>;
    using BroadcastCommentCallback = std::function<void(const QString&, bool)>;
    using ApplyResolvedRowAndSelectCallback = std::function<void(int, int)>;
    using UpdatePlyStateCallback = std::function<void(int, int, int)>;

    void setEnsureBoardSyncCallback(EnsureBoardSyncCallback cb);
    void setEnableArrowButtonsCallback(EnableArrowButtonsCallback cb);
    void setSetCurrentTurnCallback(SetCurrentTurnCallback cb);
    void setBroadcastCommentCallback(BroadcastCommentCallback cb);
    void setApplyResolvedRowAndSelectCallback(ApplyResolvedRowAndSelectCallback cb);
    void setUpdatePlyStateCallback(UpdatePlyStateCallback cb);

    // --------------------------------------------------------
    // ナビゲーション処理
    // --------------------------------------------------------

    /**
     * @brief 盤面とハイライトを指定手数に同期
     */
    void syncBoardAndHighlightsAtRow(int ply);

    /**
     * @brief 棋譜行を選択して盤面を同期
     */
    void applySelect(int row, int ply);

public Q_SLOTS:
    /**
     * @brief Presenterからの行変更通知を処理
     */
    void onRecordRowChangedByPresenter(int row, const QString& comment);

private:
    /**
     * @brief 未保存コメントの確認ダイアログを表示
     * @return true: 続行, false: キャンセル
     */
    bool checkUnsavedComment(int targetRow);

    /**
     * @brief 棋譜欄を指定行に移動
     */
    void navigateKifuViewToRow(int row);

private:
    ShogiView* m_shogiView = nullptr;
    BoardSyncPresenter* m_boardSync = nullptr;
    KifuRecordListModel* m_kifuRecordModel = nullptr;
    KifuLoadCoordinator* m_kifuLoadCoordinator = nullptr;
    ReplayController* m_replayController = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    RecordPane* m_recordPane = nullptr;
    GameRecordPresenter* m_recordPresenter = nullptr;

    const QVector<ResolvedRow>* m_resolvedRows = nullptr;
    const int* m_activeResolvedRow = nullptr;

    // コールバック
    EnsureBoardSyncCallback m_ensureBoardSync;
    EnableArrowButtonsCallback m_enableArrowButtons;
    SetCurrentTurnCallback m_setCurrentTurn;
    BroadcastCommentCallback m_broadcastComment;
    ApplyResolvedRowAndSelectCallback m_applyResolvedRowAndSelect;
    UpdatePlyStateCallback m_updatePlyState;
};

#endif // RECORDNAVIGATIONCONTROLLER_H
