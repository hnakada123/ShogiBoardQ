#ifndef SESSIONLIFECYCLECOORDINATOR_H
#define SESSIONLIFECYCLECOORDINATOR_H

/// @file sessionlifecyclecoordinator.h
/// @brief セッションライフサイクル（リセット・終局処理）を管理するコーディネータの定義

#include <QObject>
#include <QString>
#include <functional>

#include "gamestartcoordinator.h"
#include "matchcoordinator.h"
#include "playmode.h"

class ConsecutiveGamesController;
class MatchCoordinator;
class PlayerInfoWiring;
class ReplayController;
class ShogiGameController;
class ShogiView;
class GameStateController;
class TimeControlController;

/**
 * @brief セッションのライフサイクル（リセット・終局処理）を管理するコーディネータ
 *
 * MainWindow から resetToInitialState / resetGameState / onMatchGameEnded の
 * オーケストレーションを分離し、セッション状態遷移の責務を集約する。
 *
 * - resetToInitialState: 「新規」メニュー操作時の完全リセットシーケンス
 * - resetGameState: ゲーム状態変数のクリアとコントローラリセット
 * - handleGameEnded: 終局時刻反映・連続対局判定
 */
class SessionLifecycleCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // --- コントローラ（resetGameState で使用） ---
        ReplayController* replayController = nullptr;       ///< リプレイコントローラ（非所有）
        ShogiGameController* gameController = nullptr;      ///< ゲームコントローラ（非所有）
        GameStateController* gameStateController = nullptr; ///< ゲーム状態コントローラ（非所有）

        // --- 状態ポインタ（resetToInitialState で直接操作） ---
        PlayMode* playMode = nullptr;           ///< プレイモード（外部所有）
        QString* startSfenStr = nullptr;        ///< 開始局面SFEN（外部所有）
        QString* currentSfenStr = nullptr;      ///< 現在局面SFEN（外部所有）
        int* currentSelectedPly = nullptr;      ///< 現在選択手数（外部所有）

        // --- MainWindow に残る処理のコールバック ---
        std::function<void()> clearGameStateFields;       ///< m_state/m_player/m_kifu のフィールドクリア
        std::function<void()> resetEngineState;           ///< エンジン・通信の停止
        std::function<void()> onPreStartCleanupRequested; ///< 対局開始前クリーンアップ
        std::function<void(const QString&)> resetModels;  ///< データモデルのクリア
        std::function<void(const QString&)> resetUiState; ///< UI要素の状態リセット

        // --- startNewGame 用コールバック ---
        std::function<void()> clearEvalState;             ///< 評価グラフ・スコア・ライブ表示のクリア
        std::function<void()> unlockGameOverStyle;        ///< 対局終了スタイルロック解除
        std::function<void()> startGame;                  ///< 対局開始（MatchCoordinator初期化含む）

        // --- handleGameEnded 用 ---
        TimeControlController* timeController = nullptr;              ///< 時間制御コントローラ（非所有）
        ConsecutiveGamesController* consecutiveGamesController = nullptr; ///< 連続対局コントローラ（非所有）
        std::function<void(const QDateTime&)> updateEndTime;          ///< 終了時刻をゲーム情報に反映
        std::function<void()> startNextConsecutiveGame;               ///< 次の連続対局を開始

        // --- applyTimeControl 用 ---
        MatchCoordinator* match = nullptr;                            ///< 対局進行の司令塔（非所有）
        ShogiView* shogiView = nullptr;                               ///< 盤面ビュー（非所有）
        GameStartCoordinator::TimeControl* lastTimeControl = nullptr; ///< 前回の時間設定（外部所有）
        std::function<void(bool, qint64, qint64, qint64)> updateGameInfoWithTimeControl; ///< 対局情報ドックへの持ち時間反映
    };

    explicit SessionLifecycleCoordinator(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /// セッションを初期状態にリセットする（メニュー「新規」アクション用）
    void resetToInitialState();

    /// ゲーム状態をリセットする（状態変数クリア＋コントローラリセット）
    void resetGameState();

    /// 新規対局を開始する（再開判定・評価グラフ初期化・開始呼び出し）
    void startNewGame();

    /// 時間設定を適用する（保存・時計設定・UI反映）
    void applyTimeControl(const GameStartCoordinator::TimeControl& tc);

    /// 終局処理（終局時刻反映・連続対局判定）
    void handleGameEnded(const MatchCoordinator::GameEndInfo& info);

private:
    Deps m_deps;
};

#endif // SESSIONLIFECYCLECOORDINATOR_H
