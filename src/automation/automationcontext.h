#ifndef AUTOMATIONCONTEXT_H
#define AUTOMATIONCONTEXT_H

/// @file automationcontext.h
/// @brief 自動化 API のメソッド実装に注入する MainWindow 側依存（Deps 構造体）

#include <QString>
#include <QStringList>
#include <functional>

#include "playmode.h"

class QMainWindow;
class ShogiGameController;
class ShogiView;
class KifuBranchTree;
class KifuNavigationController;
class UiStatePolicyManager;
class KifuFileController;
class KifuExportController;
class GameRecordModel;

/**
 * @brief 自動化 API が参照する MainWindow の状態とコントローラ（すべて非所有）
 *
 * 遅延生成されるオブジェクトは `std::function` のプロバイダで受け取り、呼び出し時に `ensure*` を通す。
 * 生成は `MainWindowServiceRegistry::ensureAutomationServer()` が行う。
 */
struct AutomationContext {
    QMainWindow* mainWindow = nullptr;
    ShogiGameController* gameController = nullptr;
    ShogiView* shogiView = nullptr;
    KifuBranchTree* branchTree = nullptr;

    // MainWindow の状態（読み取り）
    PlayMode* playMode = nullptr;
    int* currentMoveIndex = nullptr;
    QString* startSfenStr = nullptr;
    QString* currentSfenStr = nullptr;
    QString* saveFileName = nullptr;
    QString* engineName1 = nullptr;
    QString* engineName2 = nullptr;

    // 差し替わる・遅延生成されるオブジェクトのプロバイダ
    std::function<const QStringList*()> sfenRecord;
    std::function<KifuNavigationController*()> navigationController;
    std::function<UiStatePolicyManager*()> uiStatePolicy;
    std::function<KifuFileController*()> kifuFileController;       ///< ensure 済みを返す
    std::function<KifuExportController*()> kifuExportController;   ///< ensure + 依存更新済みを返す
    std::function<GameRecordModel*()> gameRecordModel;             ///< ensure 済みを返す

    // 上位層の操作
    std::function<void()> quitApplication;
};

#endif // AUTOMATIONCONTEXT_H
