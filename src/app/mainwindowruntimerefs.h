#ifndef MAINWINDOWRUNTIMEREFS_H
#define MAINWINDOWRUNTIMEREFS_H

/// @file mainwindowruntimerefs.h
/// @brief MainWindow が保持する注入対象参照をまとめて運ぶ値オブジェクトの定義

#include <QList>
#include <QStringList>
#include <QList>

// --- 前方宣言 ---
class QWidget;
class MainWindow;
class QStatusBar;
struct ShogiMove;
class KifuDisplay;
enum class PlayMode;

class MatchCoordinator;
class ShogiGameController;
class ShogiView;
class KifuLoadCoordinator;
class CsaGameCoordinator;

class KifuRecordListModel;
class ShogiEngineThinkingModel;

class KifuBranchTree;
class KifuNavigationState;
class KifuDisplayCoordinator;

class GameInfoPaneController;
class EvaluationChartWidget;
class EvaluationGraphController;
class AnalysisResultsPresenter;
class EngineAnalysisTab;

class RecordPane;
class GameRecordPresenter;
class ReplayController;
class TimeControlController;
class BoardInteractionController;
class PositionEditController;
class DialogCoordinator;
class PlayerInfoWiring;
class UsiCommLogModel;
class GameRecordModel;
class BoardSyncPresenter;
class UiStatePolicyManager;
class GameStateController;
class ConsecutiveGamesController;
namespace Ui { class MainWindow; }

// ==========================================================================
// 領域別サブ構造体
// ==========================================================================

/// @brief UI ウィジェット・フォーム参照
/// 全メンバは非所有。MainWindow およびそのレイアウトが所有する UI 要素への参照。
struct RuntimeUiRefs {
    QWidget* parentWidget = nullptr;           ///< 親ウィジェット
    MainWindow* mainWindow = nullptr;          ///< MainWindow（シグナル接続先）
    QStatusBar* statusBar = nullptr;           ///< ステータスバー
    Ui::MainWindow* uiForm = nullptr;          ///< UIフォーム（非所有）
    RecordPane* recordPane = nullptr;          ///< 棋譜欄ウィジェット（非所有）
    EvaluationChartWidget* evalChart = nullptr; ///< 評価値グラフ（非所有）
    EngineAnalysisTab* analysisTab = nullptr;   ///< エンジン解析タブ（非所有）
};

/// @brief データモデル参照
/// 全メンバは非所有。MainWindow の DataModels 構造体が所有するモデルへの参照。
struct RuntimeModelRefs {
    KifuRecordListModel* kifuRecordModel = nullptr;            ///< 棋譜リストモデル（非所有）
    ShogiEngineThinkingModel** considerationModel = nullptr;   ///< 検討モデル（ダブルポインタ、外部所有）
    ShogiEngineThinkingModel* thinking1 = nullptr;             ///< 思考モデル1（非所有）
    ShogiEngineThinkingModel* thinking2 = nullptr;             ///< 思考モデル2（非所有）
    ShogiEngineThinkingModel* consideration = nullptr;         ///< 検討モデル（非所有、シングルポインタ）
    UsiCommLogModel* commLog1 = nullptr;                       ///< 通信ログモデル1（非所有）
    UsiCommLogModel* commLog2 = nullptr;                       ///< 通信ログモデル2（非所有）
    GameRecordModel* gameRecordModel = nullptr;                ///< 棋譜記録モデル（非所有）
};

/// @brief 棋譜データ参照
/// 全メンバは非所有（ダブルポインタ経由）。MainWindow の KifuState 構造体が所有するデータへの参照。
struct RuntimeKifuRefs {
    QStringList* sfenRecord = nullptr;                     ///< SFEN記録（外部所有）
    QList<ShogiMove>* gameMoves = nullptr;               ///< 指し手リスト（外部所有）
    QStringList* gameUsiMoves = nullptr;                   ///< USI形式指し手リスト（外部所有）
    QList<KifuDisplay*>* moveRecords = nullptr;            ///< 移動記録（外部所有）
    QStringList* positionStrList = nullptr;                ///< 局面文字列リスト（外部所有）
    int* activePly = nullptr;                              ///< アクティブ手数（外部所有）
    int* currentSelectedPly = nullptr;                     ///< 選択中手数（外部所有）
    QString* saveFileName = nullptr;                       ///< 保存先ファイルパス（外部所有）
    QList<QString>* commentsByRow = nullptr;             ///< 行別コメント（外部所有）
    bool* onMainRowGuard = nullptr;                        ///< 本譜行ガード（外部所有）
};

/// @brief 可変状態ポインタ参照
/// 全メンバは非所有（ダブルポインタ経由）。MainWindow の GameState 構造体が所有する状態への参照。
struct RuntimeStateRefs {
    PlayMode* playMode = nullptr;                          ///< 対局モード（外部所有）
    int* currentMoveIndex = nullptr;                       ///< 現在の手インデックス（外部所有）
    QString* currentSfenStr = nullptr;                     ///< 現在局面SFEN文字列（外部所有）
    QString* startSfenStr = nullptr;                       ///< 開始局面SFEN文字列（外部所有）
    bool* skipBoardSyncForBranchNav = nullptr;             ///< 分岐ナビ中の盤面同期スキップフラグ（外部所有）
    QString* resumeSfenStr = nullptr;                      ///< 再開局面SFEN文字列（外部所有）
};

/// @brief 分岐ナビゲーション参照
/// 全メンバは非所有。MainWindow の BranchNavigation 構造体が所有するオブジェクトへの参照。
struct RuntimeBranchNavRefs {
    KifuBranchTree* branchTree = nullptr;                  ///< 分岐ツリー（非所有）
    KifuNavigationState* navState = nullptr;               ///< ナビゲーション状態（非所有）
    KifuDisplayCoordinator* displayCoordinator = nullptr;  ///< 棋譜表示コーディネータ（非所有）
};

/// @brief 対局者名参照
/// 全メンバは非所有（ダブルポインタ経由）。MainWindow の PlayerState 構造体が所有する文字列への参照。
struct RuntimePlayerRefs {
    QString* humanName1 = nullptr;                             ///< 先手対局者名（外部所有）
    QString* humanName2 = nullptr;                             ///< 後手対局者名（外部所有）
    QString* engineName1 = nullptr;                            ///< 先手エンジン名（外部所有）
    QString* engineName2 = nullptr;                            ///< 後手エンジン名（外部所有）
};

/// @brief ゲームサービス参照
/// 全メンバは非所有。対局の中核となるサービス/ビューへの参照。
struct RuntimeGameServiceRefs {
    MatchCoordinator* match = nullptr;                     ///< 対局調整（非所有）
    ShogiGameController* gameController = nullptr;         ///< ゲーム制御（非所有）
    CsaGameCoordinator* csaGameCoordinator = nullptr;      ///< CSA対局コーディネータ（非所有）
    ShogiView* shogiView = nullptr;                        ///< 将棋盤ビュー（非所有）
};

/// @brief 棋譜サービス参照
/// 全メンバは非所有。棋譜の読み込み・表示・再生に関わるサービスへの参照。
struct RuntimeKifuServiceRefs {
    KifuLoadCoordinator* kifuLoadCoordinator = nullptr;    ///< 棋譜読み込みコーディネータ（非所有）
    GameRecordPresenter* recordPresenter = nullptr;            ///< 棋譜表示プレゼンタ（非所有）
    ReplayController* replayController = nullptr;              ///< リプレイコントローラ（非所有）
};

/// @brief UIコントローラ参照
/// 全メンバは非所有。UI操作・状態管理に関わるコントローラ/Wiring への参照。
struct RuntimeUiControllerRefs {
    TimeControlController* timeController = nullptr;           ///< 時間制御コントローラ（非所有）
    BoardInteractionController* boardController = nullptr;     ///< 盤面操作コントローラ（非所有）
    PositionEditController* positionEditController = nullptr;  ///< 局面編集コントローラ（非所有）
    DialogCoordinator* dialogCoordinator = nullptr;            ///< ダイアログコーディネータ（非所有）
    UiStatePolicyManager* uiStatePolicy = nullptr;             ///< UI状態ポリシーマネージャ（非所有）
    BoardSyncPresenter* boardSync = nullptr;                   ///< 盤面同期プレゼンタ（非所有）
    PlayerInfoWiring* playerInfoWiring = nullptr;              ///< 対局者情報配線（非所有）
};

/// @brief 解析参照
/// 全メンバは非所有。解析機能に関わるコントローラ/プレゼンタへの参照。
struct RuntimeAnalysisRefs {
    GameInfoPaneController* gameInfoController = nullptr;      ///< 対局情報コントローラ（非所有）
    EvaluationGraphController* evalGraphController = nullptr;  ///< 評価値グラフ制御（非所有）
    AnalysisResultsPresenter* analysisPresenter = nullptr;     ///< 解析結果プレゼンタ（非所有）
};

/// @brief ゲームコントローラ参照
/// 全メンバは非所有。ゲーム状態・連続対局の制御コントローラへの参照。
struct RuntimeGameControllerRefs {
    GameStateController* gameStateController = nullptr;        ///< ゲーム状態コントローラ（非所有）
    ConsecutiveGamesController* consecutiveGamesController = nullptr; ///< 連続対局コントローラ（非所有）
};

// ==========================================================================
// メイン構造体
// ==========================================================================

/**
 * @brief MainWindow が保持する注入対象参照をまとめて運ぶ値オブジェクト
 *
 * MainWindowDepsFactory が各 Wiring/Controller 用の Deps を組み立てる際に使用する。
 * 領域別サブ構造体で整理し、依存密度を低減している。
 */
struct MainWindowRuntimeRefs {
    // --- 領域別サブ構造体 ---
    RuntimeUiRefs ui;                        ///< UI参照
    RuntimeModelRefs models;                 ///< モデル参照
    RuntimeKifuRefs kifu;                    ///< 棋譜データ参照
    RuntimeStateRefs state;                  ///< 可変状態参照
    RuntimeBranchNavRefs branchNav;          ///< 分岐ナビゲーション参照
    RuntimePlayerRefs player;                ///< 対局者名参照
    RuntimeGameServiceRefs gameService;      ///< ゲームサービス参照
    RuntimeKifuServiceRefs kifuService;      ///< 棋譜サービス参照
    RuntimeUiControllerRefs uiController;    ///< UIコントローラ参照
    RuntimeAnalysisRefs analysis;            ///< 解析参照
    RuntimeGameControllerRefs gameCtrl;      ///< ゲームコントローラ参照
};

#endif // MAINWINDOWRUNTIMEREFS_H
