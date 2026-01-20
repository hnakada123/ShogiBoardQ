#ifndef GAMESTATECONTROLLER_H
#define GAMESTATECONTROLLER_H

#include <QObject>
#include <functional>

#include "mainwindow.h"  // PlayMode enum
#include "matchcoordinator.h"  // GameOverState, GameEndInfo, Cause, Player
#include "shogigamecontroller.h"  // ShogiGameController::Player

class QAbstractItemView;
class ShogiView;
class RecordPane;
class ReplayController;
class TimeControlController;
class KifuLoadCoordinator;
class KifuRecordListModel;

/**
 * @brief GameStateController - ゲーム状態管理クラス
 *
 * MainWindowから終局処理・ゲーム状態管理を分離したクラス。
 * 以下の責務を担当:
 * - 投了/中断処理
 * - 終局状態の遷移管理
 * - 人間/エンジン判定
 */
class GameStateController : public QObject
{
    Q_OBJECT

public:
    explicit GameStateController(QObject* parent = nullptr);
    ~GameStateController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setMatchCoordinator(MatchCoordinator* match);
    void setShogiView(ShogiView* view);
    void setRecordPane(RecordPane* pane);
    void setReplayController(ReplayController* replay);
    void setTimeController(TimeControlController* tc);
    void setKifuLoadCoordinator(KifuLoadCoordinator* kc);
    void setKifuRecordModel(KifuRecordListModel* model);

    // --------------------------------------------------------
    // 状態設定（呼び出し元から更新）
    // --------------------------------------------------------
    void setPlayMode(PlayMode mode);
    PlayMode playMode() const { return m_playMode; }

    // --------------------------------------------------------
    // 投了・中断処理
    // --------------------------------------------------------

    /**
     * @brief 投了処理を実行
     */
    void handleResignation();

    /**
     * @brief 対局を中断
     */
    void handleBreakOffGame();

    // --------------------------------------------------------
    // 終局処理
    // --------------------------------------------------------

    /**
     * @brief 終局手を追加
     * @param cause 終局原因
     * @param loserIsPlayerOne 敗者がPlayer1かどうか
     */
    void setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne);

    // --------------------------------------------------------
    // 人間/エンジン判定
    // --------------------------------------------------------

    /**
     * @brief 人間対人間かどうかを判定
     */
    bool isHvH() const;

    /**
     * @brief 指定プレイヤーが人間側かどうかを判定
     */
    bool isHumanSide(ShogiGameController::Player p) const;

    /**
     * @brief 対局終了状態かどうかを判定
     */
    bool isGameOver() const;

    // --------------------------------------------------------
    // コールバック設定（MainWindowへの委譲用）
    // --------------------------------------------------------
    using EnableArrowButtonsCallback = std::function<void()>;
    using SetReplayModeCallback = std::function<void(bool)>;
    using RefreshBranchTreeCallback = std::function<void()>;
    using UpdatePlyStateCallback = std::function<void(int activePly, int selectedPly, int moveIndex)>;

    void setEnableArrowButtonsCallback(EnableArrowButtonsCallback cb);
    void setSetReplayModeCallback(SetReplayModeCallback cb);
    void setRefreshBranchTreeCallback(RefreshBranchTreeCallback cb);
    void setUpdatePlyStateCallback(UpdatePlyStateCallback cb);

public Q_SLOTS:
    /**
     * @brief 対局終了シグナルのハンドラ
     */
    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info);

    /**
     * @brief 終局状態変更シグナルのハンドラ
     */
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);

    /**
     * @brief 終局手追加要求のハンドラ
     */
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

Q_SIGNALS:
    /**
     * @brief 終局処理完了
     */
    void gameOverProcessed();

private:
    MatchCoordinator* m_match = nullptr;
    ShogiView* m_shogiView = nullptr;
    RecordPane* m_recordPane = nullptr;
    ReplayController* m_replayController = nullptr;
    TimeControlController* m_timeController = nullptr;
    KifuLoadCoordinator* m_kifuLoadCoordinator = nullptr;
    KifuRecordListModel* m_kifuRecordModel = nullptr;

    PlayMode m_playMode = PlayMode::NotStarted;

    // コールバック
    EnableArrowButtonsCallback m_enableArrowButtons;
    SetReplayModeCallback m_setReplayMode;
    RefreshBranchTreeCallback m_refreshBranchTree;
    UpdatePlyStateCallback m_updatePlyState;
};

#endif // GAMESTATECONTROLLER_H
