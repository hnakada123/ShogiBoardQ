#ifndef AUTOMATIONCONTEXT_H
#define AUTOMATIONCONTEXT_H

/// @file automationcontext.h
/// @brief 自動化 API のメソッド実装に渡すアプリ側の状態と部品

#include <QString>
#include <QStringList>
#include <functional>

#include "appactivitystate.h"

class QMainWindow;
class ShogiGameController;
class ShogiView;
class KifuBranchTree;
class KifuNavigationState;
class KifuNavigationController;
class UiStatePolicyManager;
class KifuFileController;
class KifuDocument;
class CsaGameCoordinator;
class AnalysisSession;

/**
 * @brief 自動化 API が参照するアプリの状態と部品（すべて非所有）
 *
 * 部品は起動時に全部作られているので、そのまま指す。設定されていないもの（テストなど）は nullptr。
 */
struct AutomationContext {
    QMainWindow* mainWindow = nullptr;
    ShogiGameController* gameController = nullptr;
    ShogiView* shogiView = nullptr;
    KifuBranchTree* branchTree = nullptr;

    const AppActivityState* activity = nullptr;               ///< 動作状態
    const KifuDocument* document = nullptr;                   ///< 開いている棋譜（対局者・上書き保存先・書き出し）
    const AnalysisSession* analysis = nullptr;                ///< 検討・詰み探索（エンジン名）
    const KifuNavigationState* navState = nullptr;            ///< 表示中の手数（分岐ツリーのカーソル）
    KifuNavigationController* navigationController = nullptr;
    UiStatePolicyManager* uiStatePolicy = nullptr;
    KifuFileController* kifuFileController = nullptr;
    const CsaGameCoordinator* csaGame = nullptr;              ///< 通信対局の司令塔

    // メニューウィンドウ（widgets 層）の操作。automation 層からは直接触れないので関数で受け取る
    std::function<QStringList()> menuFavorites;
    std::function<QStringList()> availableMenuActions;
    std::function<void(const QStringList&)> setMenuFavorites;
    std::function<void()> quitApplication;

    /// 表示中の手数（navState が無ければ 0）
    int currentPly() const;
};

#endif // AUTOMATIONCONTEXT_H
