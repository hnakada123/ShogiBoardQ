#ifndef MAINWINDOWSERVICEREGISTRY_H
#define MAINWINDOWSERVICEREGISTRY_H

/// @file mainwindowserviceregistry.h
/// @brief ensure* 群の生成責務を集約する ServiceRegistry の定義

#include <QObject>
#include <QPoint>

class MainWindow;
class QString;

/**
 * @brief MainWindow から ensure* の生成ロジックを受け取る ServiceRegistry
 *
 * 責務:
 * - 全 ensure* メソッドの実装を一箇所に集約する
 * - MainWindow の private メンバに friend 経由でアクセスし遅延初期化を行う
 * - 所有権は MainWindow 側に残す（生成結果は MainWindow のメンバに代入）
 *
 * MainWindow は 1 行転送プロキシのみ保持し、実ロジックは本クラスに委譲する。
 */
class MainWindowServiceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowServiceRegistry(MainWindow& mw, QObject* parent = nullptr);

    // --- 評価値グラフ / 時間制御 ---
    void ensureEvaluationGraphController();
    void ensureTimeController();
    void ensureReplayController();

    // --- 分岐ナビゲーション ---
    void ensureBranchNavigationWiring();

    // --- MatchCoordinator 配線 ---
    void ensureMatchCoordinatorWiring();

    // --- ダイアログ / 棋譜ファイル ---
    void ensureDialogCoordinator();
    void ensureKifuFileController();
    void ensureKifuExportController();

    // --- ゲーム状態 ---
    void ensureGameStateController();
    void ensurePlayerInfoController();
    void ensureBoardSetupController();
    void ensurePvClickController();
    void ensurePositionEditCoordinator();

    // --- 通信対局 / ウィンドウ配線 ---
    void ensureCsaGameWiring();
    void ensureJosekiWiring();
    void ensureMenuWiring();
    void ensurePlayerInfoWiring();
    void ensurePreStartCleanupHandler();

    // --- 同期 / 盤面 ---
    void ensureTurnSyncBridge();
    void ensurePositionEditController();
    void ensureBoardSyncPresenter();
    void ensureBoardLoadService();
    void ensureConsiderationPositionService();
    void ensureAnalysisPresenter();

    // --- 対局開始 ---
    void ensureGameStartCoordinator();

    // --- 棋譜表示 ---
    void ensureRecordPresenter();
    void ensureLiveGameSessionStarted();
    void ensureLiveGameSessionUpdater();
    void ensureGameRecordUpdateService();
    void ensureUndoFlowService();
    void ensureGameRecordLoadService();
    void ensureTurnStateSyncService();
    void ensureKifuLoadCoordinatorForLive();
    void ensureGameRecordModel();

    // --- 補助コントローラ ---
    void ensureJishogiController();
    void ensureNyugyokuHandler();
    void ensureConsecutiveGamesController();
    void ensureLanguageController();
    void ensureConsiderationWiring();

    // --- ドック / UI ---
    void ensureDockLayoutManager();
    void ensureDockCreationService();
    void ensureCommentCoordinator();
    void ensureUsiCommandController();
    void ensureRecordNavigationHandler();
    void ensureUiStatePolicyManager();

    // --- ナビゲーション / セッション ---
    void ensureKifuNavigationCoordinator();
    void ensureSessionLifecycleCoordinator();

    // --- 通知サービス ---
    void ensureUiNotificationService();

    // --- 対局ライフサイクル ---
    void ensureGameSessionOrchestrator();

    // --- コア初期化（MainWindow から移譲） ---
    void ensureCoreInitCoordinator();

    // --- セッションリセット / UI クリア（MainWindow から移譲） ---
    void clearGameStateFields();
    void resetEngineState();
    void clearEvalState();
    void unlockGameOverStyle();
    void updateTurnStatus(int currentPlayer);
    void resetModels(const QString& hirateStartSfen);
    void resetUiState(const QString& hirateStartSfen);
    void clearSessionDependentUi();
    void clearUiBeforeKifuLoad();

    // --- 盤面操作 / 対局初期化（MainWindow から移譲） ---
    void setupBoardInteractionController();
    void initMatchCoordinator();
    void createMenuWindowDock();

    // --- スロット転送先（MainWindow から移譲） ---
    void handleMoveRequested(const QPoint& from, const QPoint& to);
    void handleMoveCommitted(int mover, int ply);
    void handleBeginPositionEditing();
    void handleFinishPositionEditing();
    void updateJosekiWindow();

private:
    MainWindow& m_mw;  ///< MainWindow への参照（生涯有効）
};

#endif // MAINWINDOWSERVICEREGISTRY_H
