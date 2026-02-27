#ifndef MAINWINDOWSIGNALROUTER_H
#define MAINWINDOWSIGNALROUTER_H

/// @file mainwindowsignalrouter.h
/// @brief MainWindow のシグナル配線を集約するルーター

#include <QObject>
#include <functional>

class MainWindow;
class DialogCoordinatorWiring;
class DialogLaunchWiring;
class KifuFileController;
class GameSessionOrchestrator;
class MainWindowAppearanceController;
class ShogiView;
class EvaluationChartWidget;
class UiActionsWiring;
class ShogiGameController;
class UiNotificationService;
class BoardSetupController;
class BoardInteractionController;
class KifuExportController;
namespace Ui { class MainWindow; }

/**
 * @brief MainWindow のシグナル配線を集約するルーター
 *
 * 責務:
 * - コンストラクタ時の全シグナル/スロット接続を一括管理
 * - ダイアログ起動配線・アクション配線・コアシグナル配線を connectAll() に集約
 * - MatchCoordinator 初期化時の盤面クリック・着手要求配線
 *
 * Deps 構造体経由でアクセスするため friend 宣言は不要。
 */
class MainWindowSignalRouter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // --- 安定ポインタ (step 7 時点で生成済み) ---
        MainWindow* mainWindow = nullptr;
        Ui::MainWindow* ui = nullptr;
        MainWindowAppearanceController* appearanceController = nullptr;
        ShogiView* shogiView = nullptr;
        EvaluationChartWidget* evalChart = nullptr;
        ShogiGameController* gameController = nullptr;
        BoardInteractionController* boardController = nullptr;

        // --- 遅延生成オブジェクト (ダブルポインタで最新値を参照) ---
        DialogCoordinatorWiring** dialogCoordinatorWiringPtr = nullptr;
        DialogLaunchWiring** dialogLaunchWiringPtr = nullptr;
        KifuFileController** kifuFileControllerPtr = nullptr;
        GameSessionOrchestrator** gameSessionOrchestratorPtr = nullptr;
        UiNotificationService** notificationServicePtr = nullptr;
        BoardSetupController** boardSetupControllerPtr = nullptr;
        UiActionsWiring** actionsWiringPtr = nullptr;

        // --- コールバック ---
        std::function<void()> initializeDialogLaunchWiring;
        std::function<void()> ensureKifuFileController;
        std::function<void()> ensureGameSessionOrchestrator;
        std::function<void()> ensureUiNotificationService;
        std::function<void()> ensureBoardSetupController;
        std::function<KifuExportController*()> getKifuExportController;
    };

    explicit MainWindowSignalRouter(QObject* parent = nullptr);

    /// 依存を更新する
    void updateDeps(const Deps& deps);

    /// 全配線を実行する（コンストラクタから1回だけ呼ぶ）
    void connectAll();

    /// MatchCoordinator 初期化時の盤面クリック配線
    void connectBoardClicks();

    /// MatchCoordinator 初期化時の着手要求配線
    void connectMoveRequested();

private:
    /// 全メニューアクションのシグナル/スロットを接続する
    void connectAllActions();

    /// コアシグナル群を接続する
    void connectCoreSignals();

    Deps m_deps;
};

#endif // MAINWINDOWSIGNALROUTER_H
