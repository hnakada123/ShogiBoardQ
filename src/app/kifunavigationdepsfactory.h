#ifndef KIFUNAVIGATIONDEPSFACTORY_H
#define KIFUNAVIGATIONDEPSFACTORY_H

/// @file kifunavigationdepsfactory.h
/// @brief KifuNavigationCoordinator 用 Deps を生成するファクトリの定義

#include "kifunavigationcoordinator.h"
#include "mainwindowruntimerefs.h"

/**
 * @brief KifuNavigationCoordinator::Deps を RuntimeRefs + コールバックから生成する純粋ファクトリ
 *
 * 責務:
 * - MainWindowRuntimeRefs とコールバック群から KifuNavigationCoordinator::Deps への変換のみ
 * - 状態を持たない（全メソッド static）
 */
class KifuNavigationDepsFactory
{
public:
    /// KifuNavigationCoordinator 用コールバック群
    struct Callbacks {
        std::function<void()> setCurrentTurn;              ///< 手番表示更新
        std::function<void()> updateJosekiWindow;          ///< 定跡ウィンドウ更新
        std::function<void()> ensureBoardSyncPresenter;    ///< BoardSyncPresenter 遅延初期化
        std::function<bool()> isGameActivelyInProgress;    ///< 対局進行中判定
        std::function<QStringList*()> getSfenRecord;       ///< SFEN履歴取得
    };

    /// KifuNavigationCoordinator::Deps を生成する
    static KifuNavigationCoordinator::Deps createDeps(
        const MainWindowRuntimeRefs& refs,
        const Callbacks& callbacks);
};

#endif // KIFUNAVIGATIONDEPSFACTORY_H
