#ifndef MAINWINDOWDEPSFACTORY_H
#define MAINWINDOWDEPSFACTORY_H

/// @file mainwindowdepsfactory.h
/// @brief 各 Wiring/Controller 用の Deps 構造体を生成する純粋ファクトリの定義

#include "mainwindowruntimerefs.h"
#include "ui/wiring/dialogcoordinatorwiring.h"
#include "kifu/kifufilecontroller.h"
#include "ui/wiring/recordnavigationwiring.h"

/**
 * @brief MainWindowRuntimeRefs を受け取り、各 Wiring/Controller 用の Deps を生成する純粋ファクトリ
 *
 * 責務:
 * - MainWindowRuntimeRefs から各 Deps 構造体への変換のみ
 * - コールバック（std::function）はファクトリ自体が保持せず、メソッド引数で受け取る
 * - 状態を持たない（全メソッド static）
 */
class MainWindowDepsFactory
{
public:
    /// DialogCoordinatorWiring 用コールバック群
    struct DialogCoordinatorCallbacks {
        std::function<bool()> getBoardFlipped;                              ///< 盤面反転状態取得
        std::function<ConsiderationWiring*()> getConsiderationWiring;       ///< ConsiderationWiring取得
        std::function<UiStatePolicyManager*()> getUiStatePolicyManager;    ///< UiStatePolicyManager取得
        std::function<void(int)> navigateKifuViewToRow;                    ///< 棋譜ビューの行遷移
    };

    /// KifuFileController 用コールバック群
    struct KifuFileCallbacks {
        std::function<void()> clearUiBeforeKifuLoad;                       ///< UI状態クリア
        std::function<void(bool)> setReplayMode;                           ///< リプレイモード設定
        std::function<void()> ensurePlayerInfoAndGameInfo;                 ///< PlayerInfoWiring+GameInfoController 確保
        std::function<void()> ensureGameRecordModel;                       ///< GameRecordModel 確保
        std::function<void()> ensureKifuExportController;                  ///< KifuExportController 確保
        std::function<void()> updateKifuExportDependencies;                ///< KifuExportController 依存更新
        std::function<void()> createAndWireKifuLoadCoordinator;            ///< KifuLoadCoordinator 生成・配線
        std::function<void()> ensureKifuLoadCoordinatorForLive;            ///< ライブ用 KifuLoadCoordinator 確保
        std::function<KifuExportController*()> getKifuExportController;    ///< KifuExportController 取得
        std::function<KifuLoadCoordinator*()> getKifuLoadCoordinator;      ///< KifuLoadCoordinator 取得
    };

    /// DialogCoordinatorWiring::Deps を生成する
    static DialogCoordinatorWiring::Deps createDialogCoordinatorDeps(
        const MainWindowRuntimeRefs& refs,
        const DialogCoordinatorCallbacks& callbacks);

    /// KifuFileController::Deps を生成する
    static KifuFileController::Deps createKifuFileControllerDeps(
        const MainWindowRuntimeRefs& refs,
        const KifuFileCallbacks& callbacks);

    /// RecordNavigationWiring::Deps を生成する
    static RecordNavigationWiring::Deps createRecordNavigationDeps(
        const MainWindowRuntimeRefs& refs);
};

#endif // MAINWINDOWDEPSFACTORY_H
