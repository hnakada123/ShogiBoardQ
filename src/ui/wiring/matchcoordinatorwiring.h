#ifndef MATCHCOORDINATORWIRING_H
#define MATCHCOORDINATORWIRING_H

/// @file matchcoordinatorwiring.h
/// @brief MatchCoordinator配線クラスの定義


#include <QObject>
#include <QMetaObject>
#include <functional>

#include "matchcoordinator.h"

class ShogiGameController;
class ShogiView;
class ShogiClock;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class GameStartCoordinator;
class EvaluationGraphController;
class PlayerInfoWiring;
class TimeDisplayPresenter;
class TimeControlController;
class UiStatePolicyManager;
class BoardInteractionController;
class KifuRecordListModel;

/**
 * @brief MatchCoordinator関連のUI配線を担当するクラス
 *
 * 責務:
 * - GameStartCoordinatorの生成と管理
 * - MatchCoordinatorの生成とDeps構築
 * - MatchCoordinator/GameStartCoordinatorのシグナル配線
 * - UndoBindingsの設定
 * - 盤面操作・時計・評価値グラフの配線
 */
class MatchCoordinatorWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // --- MatchCoordinator::Deps 構築用 ---
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;
        Usi* usi1 = nullptr;
        Usi* usi2 = nullptr;
        UsiCommLogModel* comm1 = nullptr;
        ShogiEngineThinkingModel* think1 = nullptr;
        UsiCommLogModel* comm2 = nullptr;
        ShogiEngineThinkingModel* think2 = nullptr;
        QStringList* sfenRecord = nullptr;
        PlayMode* playMode = nullptr;

        // --- GameStartCoordinator 配線用 ---
        TimeDisplayPresenter* timePresenter = nullptr;

        // --- Post-creation 設定用 ---
        BoardInteractionController* boardController = nullptr;

        // --- UndoRefs 構築用 ---
        KifuRecordListModel* kifuRecordModel = nullptr;
        QVector<ShogiMove>* gameMoves = nullptr;
        QStringList* positionStrList = nullptr;
        int* currentMoveIndex = nullptr;

        // --- Pre-built hooks（MainWindow が構築） ---
        MatchCoordinator::Hooks hooks;
        MatchCoordinator::UndoHooks undoHooks;

        // --- 遅延初期化コールバック ---
        std::function<void()> ensureTimeController;
        std::function<void()> ensureEvaluationGraphController;
        std::function<void()> ensurePlayerInfoWiring;
        std::function<void()> ensureUsiCommandController;
        std::function<void()> ensureUiStatePolicyManager;
        std::function<void()> connectBoardClicks;
        std::function<void()> connectMoveRequested;

        // --- 遅延初期化オブジェクトのゲッター ---
        std::function<ShogiClock*()> getClock;
        std::function<TimeControlController*()> getTimeController;
        std::function<EvaluationGraphController*()> getEvalGraphController;
        std::function<UiStatePolicyManager*()> getUiStatePolicy;
    };

    explicit MatchCoordinatorWiring(QObject* parent = nullptr);
    ~MatchCoordinatorWiring() override = default;

    void updateDeps(const Deps& deps);

    /**
     * @brief MatchCoordinator/GameStartCoordinatorの生成と全配線を行う
     *
     * GameStartCoordinatorは初回のみ生成し、MatchCoordinatorは毎回再生成する。
     */
    void wireConnections();

    /// 生成された MatchCoordinator を返す（非所有）
    MatchCoordinator* match() const { return m_match; }

    /// GameStartCoordinator を返す（非所有）
    GameStartCoordinator* gameStartCoordinator() const { return m_gameStartCoordinator; }

signals:
    // --- GameStartCoordinator シグナルの転送 ---

    /// 終局手の棋譜追記要求
    void requestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    /// 盤面反転通知
    void boardFlipped(bool nowFlipped);

    /// 終局状態変更通知
    void gameOverStateChanged(const MatchCoordinator::GameOverState& st);

    /// 対局終了通知
    void matchGameEnded(const MatchCoordinator::GameEndInfo& info);

    // --- ShogiClock シグナルの転送 ---

    /// 時間切れ投了通知
    void resignationTriggered();

private:
    void ensureGameStartCoordinator();

    // --- Deps から取り出したメンバ ---
    ShogiGameController* m_gc = nullptr;
    ShogiView* m_view = nullptr;
    Usi* m_usi1 = nullptr;
    Usi* m_usi2 = nullptr;
    UsiCommLogModel* m_comm1 = nullptr;
    ShogiEngineThinkingModel* m_think1 = nullptr;
    UsiCommLogModel* m_comm2 = nullptr;
    ShogiEngineThinkingModel* m_think2 = nullptr;
    QStringList* m_sfenRecord = nullptr;
    PlayMode* m_playMode = nullptr;

    TimeDisplayPresenter* m_timePresenter = nullptr;
    BoardInteractionController* m_boardController = nullptr;

    KifuRecordListModel* m_kifuRecordModel = nullptr;
    QVector<ShogiMove>* m_gameMoves = nullptr;
    QStringList* m_positionStrList = nullptr;
    int* m_currentMoveIndex = nullptr;

    MatchCoordinator::Hooks m_hooks;
    MatchCoordinator::UndoHooks m_undoHooks;

    std::function<void()> m_ensureTimeController;
    std::function<void()> m_ensureEvaluationGraphController;
    std::function<void()> m_ensurePlayerInfoWiring;
    std::function<void()> m_ensureUsiCommandController;
    std::function<void()> m_ensureUiStatePolicyManager;
    std::function<void()> m_connectBoardClicks;
    std::function<void()> m_connectMoveRequested;

    std::function<ShogiClock*()> m_getClock;
    std::function<TimeControlController*()> m_getTimeController;
    std::function<EvaluationGraphController*()> m_getEvalGraphController;
    std::function<UiStatePolicyManager*()> m_getUiStatePolicy;

    // --- 内部管理オブジェクト ---
    GameStartCoordinator* m_gameStartCoordinator = nullptr;
    MatchCoordinator* m_match = nullptr;
    QMetaObject::Connection m_timeConn;
    QMetaObject::Connection m_clockConn;
};

#endif // MATCHCOORDINATORWIRING_H
