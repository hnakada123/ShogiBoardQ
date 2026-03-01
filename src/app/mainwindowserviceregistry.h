#ifndef MAINWINDOWSERVICEREGISTRY_H
#define MAINWINDOWSERVICEREGISTRY_H

/// @file mainwindowserviceregistry.h
/// @brief ensure*/操作メソッドを一括管理するレジストリ
///
/// ensure* メソッドは3層に分かれる:
/// - MainWindowFoundationRegistry: ドメイン横断的な共通基盤（Tier 0/1）
/// - GameSubRegistry / KifuSubRegistry: ドメイン固有サブレジストリ
/// - MainWindowServiceRegistry: UI/Analysis/Board 系 + オーケストレーション
///
/// 設計根拠: docs/dev/ensure-graph-analysis.md

#include <QObject>
#include <QPoint>

class GameSubRegistry;
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
 * - Game 系は GameSubRegistry、Kifu 系は KifuSubRegistry に委譲する
 * - 共通基盤メソッドは MainWindowFoundationRegistry に委譲する
 *
 * 実装は複数 .cpp ファイルに分割されている:
 * - mainwindowserviceregistry.cpp       (コンストラクタ)
 * - mainwindowanalysisregistry.cpp      (Analysis 系)
 * - mainwindowboardregistry.cpp         (Board/共通 系)
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

    /// 共通基盤レジストリへのアクセサ
    MainWindowFoundationRegistry* foundation() const { return m_foundation; }

    /// Game系サブレジストリへのアクセサ
    GameSubRegistry* game() const { return m_game; }

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

    MainWindow& m_mw;  ///< MainWindow への参照（生涯有効）
    MainWindowFoundationRegistry* m_foundation;  ///< 共通基盤レジストリ（子オブジェクト）
    GameSubRegistry* m_game;    ///< Game系サブレジストリ（子オブジェクト）
    KifuSubRegistry* m_kifu;    ///< Kifu系サブレジストリ（子オブジェクト）
};

#endif // MAINWINDOWSERVICEREGISTRY_H
