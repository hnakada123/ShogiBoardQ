#ifndef SESSIONLIFECYCLEDEPSFACTORY_H
#define SESSIONLIFECYCLEDEPSFACTORY_H

/// @file sessionlifecycledepsfactory.h
/// @brief SessionLifecycleCoordinator 用 Deps を生成するファクトリの定義

#include "sessionlifecyclecoordinator.h"
#include "mainwindowruntimerefs.h"

/**
 * @brief SessionLifecycleCoordinator::Deps を RuntimeRefs + コールバックから生成する純粋ファクトリ
 *
 * 責務:
 * - MainWindowRuntimeRefs とコールバック群から SessionLifecycleCoordinator::Deps への変換のみ
 * - 状態を持たない（全メソッド static）
 */
class SessionLifecycleDepsFactory
{
public:
    /// SessionLifecycleCoordinator 用コールバック群
    struct Callbacks {
        // --- リセット処理用 ---
        std::function<void()> clearGameStateFields;                ///< m_state/m_player/m_kifu のフィールドクリア
        std::function<void()> resetEngineState;                    ///< エンジン・通信の停止
        std::function<void()> onPreStartCleanupRequested;          ///< 対局開始前クリーンアップ
        std::function<void(const QString&)> resetModels;           ///< データモデルのクリア
        std::function<void(const QString&)> resetUiState;          ///< UI要素の状態リセット

        // --- startNewGame 用 ---
        std::function<void()> clearEvalState;                      ///< 評価グラフ・スコア・ライブ表示のクリア
        std::function<void()> unlockGameOverStyle;                 ///< 対局終了スタイルロック解除
        std::function<void()> startGame;                           ///< 対局開始（MatchCoordinator初期化含む）

        // --- handleGameEnded 用 ---
        std::function<void(const QDateTime&)> updateEndTime;       ///< 終了時刻をゲーム情報に反映
        std::function<void()> startNextConsecutiveGame;            ///< 次の連続対局を開始

        // --- applyTimeControl 用 ---
        GameStartCoordinator::TimeControl* lastTimeControl = nullptr; ///< 前回の時間設定（外部所有）
        std::function<void(bool, qint64, qint64, qint64)> updateGameInfoWithTimeControl; ///< 対局情報ドックへの持ち時間反映
    };

    /// SessionLifecycleCoordinator::Deps を生成する
    static SessionLifecycleCoordinator::Deps createDeps(
        const MainWindowRuntimeRefs& refs,
        const Callbacks& callbacks);
};

#endif // SESSIONLIFECYCLEDEPSFACTORY_H
