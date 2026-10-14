#ifndef MATCHCOORDINATORWIRING_H
#define MATCHCOORDINATORWIRING_H

/// @file matchcoordinatorwiring.h
/// @brief MatchCoordinator配線クラスの定義


#include <QObject>
#include <QMetaObject>
#include <memory>

#include "matchcoordinator.h"
#include "gamestartcoordinator.h"

class GameSessionOrchestrator;
class SessionLifecycleCoordinator;
class GameStateController;
class ConsecutiveGamesController;
class ShogiGameController;
class ShogiView;
class ShogiClock;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class EvaluationGraphController;
class PlayerInfoWiring;
class TimeDisplayPresenter;
class TimeControlController;
class UiStatePolicyManager;
class BoardInteractionController;
class KifuRecordListModel;
class MainWindowAppearanceController;
class KifuNavigationCoordinator;
class BranchNavigationWiring;
class UsiCommandController;

/**
 * @brief MatchCoordinator関連のUI配線を担当するクラス
 *
 * 責務:
 * - GameStartCoordinatorの生成と管理
 * - MatchCoordinatorの生成とDeps構築
 * - MatchCoordinator/GameStartCoordinatorのシグナルを、作るときに受け手（SignalTargets）へ直接つなぐ
 * - UndoBindingsの設定
 * - 盤面操作・時計・評価値グラフの配線
 */
class MatchCoordinatorWiring : public QObject
{
    Q_OBJECT

public:
    /// 司令塔と GameStartCoordinator のシグナルの受け手（呼び出し元で ensure 済み。nullptr の受け手はつながない）
    struct SignalTargets {
        GameSessionOrchestrator* gameSession = nullptr;
        SessionLifecycleCoordinator* sessionLifecycle = nullptr;
        GameStateController* gameStateController = nullptr;
        ConsecutiveGamesController* consecutiveGamesController = nullptr;
        MainWindowAppearanceController* appearance = nullptr;
        PlayerInfoWiring* playerInfo = nullptr;
        KifuNavigationCoordinator* kifuNav = nullptr;
        BranchNavigationWiring* branchNav = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;
    };

    struct Deps {
        // --- MatchCoordinator::Deps 構築用 ---
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;
        UsiCommLogModel* comm1 = nullptr;
        ShogiEngineThinkingModel* think1 = nullptr;
        UsiCommLogModel* comm2 = nullptr;
        ShogiEngineThinkingModel* think2 = nullptr;
        QStringList* sfenRecord = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;
        PlayMode* playMode = nullptr;

        // --- GameStartCoordinator 配線用 ---
        TimeDisplayPresenter* timePresenter = nullptr;

        // --- Post-creation 設定用 ---
        BoardInteractionController* boardController = nullptr;

        // --- UndoRefs 構築用 ---
        KifuRecordListModel* kifuRecordModel = nullptr;

        // --- Pre-built hooks（MainWindow が構築） ---
        MatchCoordinator::Hooks hooks;             ///< MC 用コールバック群（MainWindowComposer::buildMatchHooks）
        MatchCoordinator::UndoHooks undoHooks;      ///< UNDO 用コールバック群

        // --- 司令塔とつなぐ部品（起動時に作られている。非所有） ---
        ShogiClock* clock = nullptr;
        TimeControlController* timeController = nullptr;
        EvaluationGraphController* evalGraphController = nullptr;
        UsiCommandController* usiCommandController = nullptr;
    };

    explicit MatchCoordinatorWiring(QObject* parent = nullptr);
    ~MatchCoordinatorWiring() override;

    void updateDeps(const Deps& deps);

    /**
     * @brief シグナルの受け手を登録する（司令塔を作る initializeSession より前に1回だけ呼ぶ）
     *
     * 受け手は、司令塔（createMatchCoordinator）と GameStartCoordinator（createMenuGameStartCoordinator）を
     * 作るときに、それぞれのシグナルへ直接つなぐ。どちらも一度だけ作るので、つなぐのも一度だけである。
     */
    void setSignalTargets(const SignalTargets& targets);

    /// メニュー操作用 GameStartCoordinator を作り、シグナルを受け手へつなぐ（司令塔を作った後に1回だけ呼ぶ）
    void createMenuGameStartCoordinator();

    /**
     * @brief MatchCoordinator を作り、実行時の依存（モデル・時計・盤操作など）を結ぶ
     *
     * MainWindowComposer が起動時に1回だけ呼ぶ。2回目以降は何もしない（司令塔は作り直さない）。
     * @return 依存（盤・ゲームコントローラ）が揃っていれば true
     */
    bool initializeSession();

    /// 生成された MatchCoordinator を返す（非所有）
    MatchCoordinator* match() const { return m_match.get(); }

    /// メニュー操作用 GameStartCoordinator を返す（非所有）
    GameStartCoordinator* menuGameStartCoordinator() const { return m_menuGameStart; }

private:
    /// 司令塔を作り、司令塔のシグナルを受け手へつなぐ（一度だけ）
    void createMatchCoordinator();
    /// 司令塔のシグナルを受け手へつなぐ。同じシグナルの受け手は、ここでつないだ順に呼ばれる
    void connectMatchSignals();
    /// GameStartCoordinator のシグナルを受け手へつなぐ。同じシグナルの受け手は、ここでつないだ順に呼ばれる
    void connectGameStartSignals();
    /// 司令塔と、起動の途中で作られる部品（時計・評価値グラフ・盤操作など）との接続を結び直す（何度呼んでもよい）
    void bindRuntimeDependencies();

    // --- Deps から取り出したメンバ ---
    ShogiGameController* m_gc = nullptr;
    ShogiView* m_view = nullptr;
    UsiCommLogModel* m_comm1 = nullptr;
    ShogiEngineThinkingModel* m_think1 = nullptr;
    UsiCommLogModel* m_comm2 = nullptr;
    ShogiEngineThinkingModel* m_think2 = nullptr;
    QStringList* m_sfenRecord = nullptr;
    PlayMode* m_playMode = nullptr;

    TimeDisplayPresenter* m_timePresenter = nullptr;
    BoardInteractionController* m_boardController = nullptr;

    KifuRecordListModel* m_kifuRecordModel = nullptr;
    QList<ShogiMove>* m_gameMoves = nullptr;

    MatchCoordinator::Hooks m_hooks;
    MatchCoordinator::UndoHooks m_undoHooks;

    ShogiClock* m_clock = nullptr;
    TimeControlController* m_timeController = nullptr;
    EvaluationGraphController* m_evalGraphController = nullptr;
    UsiCommandController* m_usiCommandController = nullptr;

    SignalTargets m_targets;

    // --- 内部管理オブジェクト ---
    GameStartCoordinator* m_menuGameStart = nullptr;
    std::unique_ptr<MatchCoordinator> m_match;
};

#endif // MATCHCOORDINATORWIRING_H
