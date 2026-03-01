#ifndef GAMESTATECONTROLLER_H
#define GAMESTATECONTROLLER_H

/// @file gamestatecontroller.h
/// @brief 終局処理・投了/中断・人間/エンジン判定を統括するゲーム状態コントローラ


#include <QObject>
#include <QPointer>
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
 * @brief 終局処理・投了/中断・人間エンジン判定を担うゲーム状態コントローラ
 *
 * MainWindowから終局処理・ゲーム状態管理を分離したクラス。
 * 投了/中断処理、終局状態の遷移管理、人間/エンジン判定を担当する。
 */
class GameStateController : public QObject
{
    Q_OBJECT

public:
    explicit GameStateController(QObject* parent = nullptr);
    ~GameStateController() override;

    // --- 依存オブジェクトの設定 ---
    void setMatchCoordinator(MatchCoordinator* match);
    void setShogiView(ShogiView* view);
    void setRecordPane(RecordPane* pane);
    void setReplayController(ReplayController* replay);
    void setTimeController(TimeControlController* tc);
    void setKifuLoadCoordinator(KifuLoadCoordinator* kc);
    void setKifuRecordModel(KifuRecordListModel* model);

    // --- 状態設定 ---
    void setPlayMode(PlayMode mode);
    PlayMode playMode() const { return m_playMode; }

    // --- 投了・中断処理 ---
    void handleResignation();
    void handleBreakOffGame();

    // --- 終局処理 ---

    /**
     * @brief 終局手を棋譜に追加し、UI後処理を行う
     * @param cause 終局原因
     * @param loserIsPlayerOne 敗者がPlayer1かどうか
     */
    void setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne);

    // --- 人間/エンジン判定 ---
    bool isHvH() const;
    bool isHumanSide(ShogiGameController::Player p) const;
    bool isGameOver() const;

    // --- コールバック設定（MainWindowへの委譲用） ---
    using EnableArrowButtonsCallback = std::function<void()>;
    using SetReplayModeCallback = std::function<void(bool)>;
    using RefreshBranchTreeCallback = std::function<void()>;
    using UpdatePlyStateCallback = std::function<void(int activePly, int selectedPly, int moveIndex)>;

    void setEnableArrowButtonsCallback(EnableArrowButtonsCallback cb);
    void setSetReplayModeCallback(SetReplayModeCallback cb);
    void setRefreshBranchTreeCallback(RefreshBranchTreeCallback cb);
    void setUpdatePlyStateCallback(UpdatePlyStateCallback cb);

public Q_SLOTS:
    /// 対局終了シグナルのハンドラ（→ MatchCoordinator::gameEnded から接続）
    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info);

    /// 終局状態変更シグナルのハンドラ（→ MatchCoordinator::gameOverStateChanged から接続）
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);

    /// 終局手追加要求のハンドラ（→ MatchCoordinator::requestAppendGameOverMove から接続）
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

private:
    QPointer<MatchCoordinator> m_match;                  ///< 非所有（再生成追跡）
    ShogiView* m_shogiView = nullptr;                   ///< 非所有
    RecordPane* m_recordPane = nullptr;                 ///< 非所有
    ReplayController* m_replayController = nullptr;     ///< 非所有
    TimeControlController* m_timeController = nullptr;  ///< 非所有
    KifuLoadCoordinator* m_kifuLoadCoordinator = nullptr; ///< 非所有
    KifuRecordListModel* m_kifuRecordModel = nullptr;   ///< 非所有

    PlayMode m_playMode = PlayMode::NotStarted; ///< 現在のプレイモード

    // --- コールバック ---
    EnableArrowButtonsCallback m_enableArrowButtons;   ///< 矢印ボタン有効化
    SetReplayModeCallback m_setReplayMode;             ///< リプレイモード切替
    RefreshBranchTreeCallback m_refreshBranchTree;     ///< 分岐ツリー再構築
    UpdatePlyStateCallback m_updatePlyState;           ///< 手数状態更新
};

#endif // GAMESTATECONTROLLER_H
