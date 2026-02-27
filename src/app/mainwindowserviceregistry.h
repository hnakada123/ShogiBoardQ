#ifndef MAINWINDOWSERVICEREGISTRY_H
#define MAINWINDOWSERVICEREGISTRY_H

/// @file mainwindowserviceregistry.h
/// @brief ensure*/操作メソッドを一括管理するレジストリ
///
/// 旧5サブレジストリ（Analysis/Board/Game/Kifu/Ui）を統合し、
/// MainWindow の friend class を ServiceRegistry 1 つに集約する。

#include <QObject>
#include <QPoint>

#include "matchcoordinatorwiring.h"

class MainWindow;
class QString;

/**
 * @brief ensure* メソッドと主要操作を一括管理するサービスレジストリ
 *
 * 責務:
 * - UI / Game / Kifu / Analysis / Board の各カテゴリに属する
 *   遅延初期化メソッドと操作メソッドの実装を集約する
 * - MainWindow 側の呼び出しコードは `m_registry->ensureXxx()` のまま変更不要
 *
 * 実装は複数 .cpp ファイルに分割されている:
 * - mainwindowserviceregistry.cpp       (コンストラクタ)
 * - mainwindowanalysisregistry.cpp      (Analysis 系)
 * - mainwindowboardregistry.cpp         (Board/共通 系)
 * - mainwindowgameregistry.cpp          (Game 系)
 * - mainwindowkifuregistry.cpp          (Kifu 系)
 * - mainwindowuiregistry.cpp            (UI 系)
 */
class MainWindowServiceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowServiceRegistry(MainWindow& mw, QObject* parent = nullptr);

    // ===== UI系 =====
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

    // ===== Game系 =====
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

    // ===== Kifu系 =====
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

    // ===== Analysis系 =====
    void ensurePvClickController();
    void ensureConsiderationPositionService();
    void ensureAnalysisPresenter();
    void ensureConsiderationWiring();
    void ensureUsiCommandController();

    // ===== Board/共通系 =====
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

    // ===== DockBootstrapper系 =====
    void setupRecordPane();
    void setupEngineAnalysisTab();
    void createEvalChartDock();
    void createRecordPaneDock();
    void createAnalysisDocks();
    void createMenuWindowDockImpl();
    void createJosekiWindowDock();
    void createAnalysisResultsDock();
    void initializeBranchNavigationClasses();

    // ===== UiBootstrapper系 =====
    void buildGamePanels();
    void restoreWindowAndSync();
    void finalizeCoordinators();

    // ===== WiringAssembler系 =====
    void initializeDialogLaunchWiring();

private:
    /// MatchCoordinatorWiring::Deps を構築する
    MatchCoordinatorWiring::Deps buildMatchWiringDeps();

    /// KifuLoadCoordinator を生成・配線し m_kifuLoadCoordinator に設定するヘルパー
    void createAndWireKifuLoadCoordinator();

    /// エンジン解析タブの依存コンポーネントを設定する
    void configureAnalysisTabDependencies();

    MainWindow& m_mw;  ///< MainWindow への参照（生涯有効）
};

#endif // MAINWINDOWSERVICEREGISTRY_H
