#ifndef MAINWINDOWSERVICEREGISTRY_H
#define MAINWINDOWSERVICEREGISTRY_H

/// @file mainwindowserviceregistry.h
/// @brief ensure*/操作メソッドを一括管理するレジストリ
///
/// ensure* メソッドは主に3区分に分かれる:
/// - MainWindowFoundationRegistry: ドメイン横断的な共通基盤（Tier 0/1）
/// - KifuSubRegistry: Kifu ドメイン専用サブレジストリ
/// - MainWindowServiceRegistry: UI/Analysis/Board/Game 系 + オーケストレーション
///
/// 設計根拠: docs/dev/done/ensure-graph-analysis.md

#include <QObject>
#include <QPoint>

#include "matchcoordinatorwiring.h"

class KifuSubRegistry;
class MainWindow;
class MainWindowFoundationRegistry;
class QString;

/**
 * @brief ドメイン固有の ensure* メソッドと主要操作を管理するサービスレジストリ
 *
 * 責務:
 * - UI / Analysis / Board の各カテゴリに属する
 *   遅延初期化メソッドと操作メソッドの実装を集約する
 * - Game 系は本クラスに直接集約し、Kifu 系は KifuSubRegistry に委譲する
 * - 共通基盤メソッドは MainWindowFoundationRegistry に委譲する
 *
 * 実装は複数 .cpp ファイルに分割されている:
 * - mainwindowserviceregistry.cpp       (コンストラクタ)
 * - mainwindowanalysisregistry.cpp      (Analysis 系)
 * - mainwindowboardregistry.cpp         (Board/共通 系)
 * - gamesubregistry.cpp                 (Game 状態/コントローラ管理)
 * - gamesubregistry_session.cpp         (Game セッション管理)
 * - gamesubregistry_wiring.cpp          (Game 配線管理)
 * - mainwindowuiregistry.cpp            (UI 系)
 * - mainwindowdockbootstrapper.cpp      (ドック生成)
 * - mainwindowuibootstrapper.cpp        (UI ブートストラップ)
 * - mainwindowwiringassembler.cpp       (配線アセンブラ)
 */
class MainWindowServiceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowServiceRegistry(MainWindow& mw, QObject* parent = nullptr);
    ~MainWindowServiceRegistry() override;

    /// 共通基盤レジストリへのアクセサ
    MainWindowFoundationRegistry* foundation() const { return m_foundation; }

    /// Kifu系サブレジストリへのアクセサ
    KifuSubRegistry* kifu() const { return m_kifu; }

    // ===== UI系 =====
    void ensureRecordPresenter();
    void ensureDialogCoordinator();
    void ensureDockLayoutManager();
    void ensureRecordNavigationHandler();
    void unlockGameOverStyle();
    void createMenuWindowDock();
    void clearEvalState();

    // ===== Analysis系 =====
    void ensurePvClickController();
    void ensureConsiderationPositionService();
    void ensureConsiderationWiring();
    void ensureUsiCommandController();

    // ===== Board/共通系 =====
    void ensureBoardSetupController();
    void ensurePositionEditCoordinator();
    void ensureBoardLoadService();
    void setupBoardInteractionController();
    void handleMoveRequested(const QPoint& from, const QPoint& to);
    void handleMoveCommitted(int mover, int ply);
    void handleBeginPositionEditing();
    void handleFinishPositionEditing();
    void resetModels(const QString& hirateStartSfen);
    void resetUiState(const QString& hirateStartSfen);
    void clearSessionDependentUi();

    // ===== delegation (MainWindow スロットからの転送先) =====
    void loadBoardFromSfen(const QString& sfen);
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen);
    void onRecordPaneMainRowChanged(int row);

    // ===== Game系 =====
    void ensureTimeController();
    void ensureReplayController();
    void ensureGameStateController();
    void ensureGameStartCoordinator();
    void ensurePreStartCleanupHandler();
    void ensureLiveGameSessionUpdater();
    void ensureGameSessionOrchestrator();
    void ensureCoreInitCoordinator();
    void ensureSessionLifecycleCoordinator();
    void ensureMatchCoordinatorWiring();
    void ensureCsaGameWiring();
    void ensureConsecutiveGamesController();
    void prepareUndoFlowService();
    void prepareTurnStateSyncService();
    void prepareTurnSyncBridge();
    void resetToInitialState();
    void resetGameState();
    void setReplayMode(bool on);
    void initMatchCoordinator();
    void startLiveGameSessionIfNeeded();

    // ===== Kifu convenience wrappers =====
    void prepareGameRecordLoadService();
    void updateJosekiWindow();

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
    /// エンジン解析タブの依存コンポーネントを設定する
    void configureAnalysisTabDependencies();
    void createLiveGameSessionUpdater();
    void createGameSessionOrchestrator();
    void createCoreInitCoordinator();
    void createSessionLifecycleCoordinator();
    void createMatchCoordinatorWiring();
    void createTurnStateSyncService();
    void createUndoFlowService();
    void refreshLiveGameSessionUpdaterDeps();
    void refreshGameSessionOrchestratorDeps();
    void refreshCoreInitDeps();
    void refreshSessionLifecycleDeps();
    void refreshMatchWiringDeps();
    void refreshTurnStateSyncDeps();
    void refreshUndoFlowDeps();
    void startGameSession();
    MatchCoordinatorWiring::Deps buildMatchWiringDeps();
    qint64 queryRemainingMsFor(MatchCoordinator::Player player);
    qint64 queryIncrementMsFor(MatchCoordinator::Player player);
    qint64 queryByoyomiMs();
    bool queryIsHumanSide(ShogiGameController::Player player);
    void updateTurnStatus(int currentPlayer);
    void clearGameStateFields();
    void resetEngineState();

    MainWindow& m_mw;  ///< MainWindow への参照（生涯有効）
    MainWindowFoundationRegistry* m_foundation;  ///< 共通基盤レジストリ（子オブジェクト）
    KifuSubRegistry* m_kifu;    ///< Kifu系サブレジストリ（子オブジェクト）
};

#endif // MAINWINDOWSERVICEREGISTRY_H
