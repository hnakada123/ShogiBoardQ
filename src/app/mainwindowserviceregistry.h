#ifndef MAINWINDOWSERVICEREGISTRY_H
#define MAINWINDOWSERVICEREGISTRY_H

/// @file mainwindowserviceregistry.h
/// @brief 全サブレジストリを束ねるファサード

#include <QObject>
#include <QPoint>

class MainWindow;
class MainWindowAnalysisRegistry;
class MainWindowBoardRegistry;
class MainWindowGameRegistry;
class MainWindowKifuRegistry;
class MainWindowUiRegistry;
class QString;

/**
 * @brief 全サブレジストリを束ねるファサード
 *
 * 責務:
 * - 5 つのサブレジストリ（UI / Game / Kifu / Analysis / Board）を所有する
 * - 各 ensure* 呼び出しを対応するサブレジストリへ 1 行で転送する
 * - MainWindow 側の呼び出しコードは `m_serviceRegistry->ensureXxx()` のまま変更不要
 */
class MainWindowServiceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowServiceRegistry(MainWindow& mw, QObject* parent = nullptr);

    // ===== UI系（MainWindowUiRegistry へ委譲） =====
    void ensureEvaluationGraphController();
    void ensureRecordPresenter();
    void ensurePlayerInfoController();
    void ensurePlayerInfoWiring();
    void ensureDialogCoordinator();
    void ensureMenuWiring();
    void ensureDockLayoutManager();
    void ensureDockCreationService();
    void ensureUiStatePolicyManager();
    void ensureUiNotificationService();
    void ensureRecordNavigationHandler();
    void ensureLanguageController();
    void unlockGameOverStyle();
    void createMenuWindowDock();
    void clearEvalState();

    // ===== Game系（MainWindowGameRegistry へ委譲） =====
    void ensureTimeController();
    void ensureReplayController();
    void ensureMatchCoordinatorWiring();
    void ensureGameStateController();
    void ensureGameStartCoordinator();
    void ensureCsaGameWiring();
    void ensurePreStartCleanupHandler();
    void ensureTurnSyncBridge();
    void ensureTurnStateSyncService();
    void ensureLiveGameSessionStarted();
    void ensureLiveGameSessionUpdater();
    void ensureUndoFlowService();
    void ensureJishogiController();
    void ensureNyugyokuHandler();
    void ensureConsecutiveGamesController();
    void ensureGameSessionOrchestrator();
    void ensureSessionLifecycleCoordinator();
    void ensureCoreInitCoordinator();
    void clearGameStateFields();
    void resetEngineState();
    void updateTurnStatus(int currentPlayer);
    void initMatchCoordinator();

    // ===== Kifu系（MainWindowKifuRegistry へ委譲） =====
    void ensureBranchNavigationWiring();
    void ensureKifuFileController();
    void ensureKifuExportController();
    void ensureGameRecordUpdateService();
    void ensureGameRecordLoadService();
    void ensureKifuLoadCoordinatorForLive();
    void ensureGameRecordModel();
    void ensureCommentCoordinator();
    void ensureKifuNavigationCoordinator();
    void ensureJosekiWiring();
    void clearUiBeforeKifuLoad();
    void updateJosekiWindow();

    // ===== Analysis系（MainWindowAnalysisRegistry へ委譲） =====
    void ensurePvClickController();
    void ensureConsiderationPositionService();
    void ensureAnalysisPresenter();
    void ensureConsiderationWiring();
    void ensureUsiCommandController();

    // ===== Board/共通系（MainWindowBoardRegistry へ委譲） =====
    void ensureBoardSetupController();
    void ensurePositionEditCoordinator();
    void ensurePositionEditController();
    void ensureBoardSyncPresenter();
    void ensureBoardLoadService();
    void setupBoardInteractionController();
    void handleMoveRequested(const QPoint& from, const QPoint& to);
    void handleMoveCommitted(int mover, int ply);
    void handleBeginPositionEditing();
    void handleFinishPositionEditing();
    void resetModels(const QString& hirateStartSfen);
    void resetUiState(const QString& hirateStartSfen);
    void clearSessionDependentUi();

private:
    MainWindow& m_mw;  ///< MainWindow への参照（生涯有効）
    MainWindowAnalysisRegistry* m_analysisRegistry = nullptr;  ///< Analysis系サブレジストリ
    MainWindowBoardRegistry* m_boardRegistry = nullptr;        ///< Board/共通系サブレジストリ
    MainWindowGameRegistry* m_gameRegistry = nullptr;          ///< Game系サブレジストリ
    MainWindowKifuRegistry* m_kifuRegistry = nullptr;          ///< Kifu系サブレジストリ
    MainWindowUiRegistry* m_uiRegistry = nullptr;              ///< UI系サブレジストリ
};

#endif // MAINWINDOWSERVICEREGISTRY_H
