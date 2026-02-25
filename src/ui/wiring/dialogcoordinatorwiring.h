#ifndef DIALOGCOORDINATORWIRING_H
#define DIALOGCOORDINATORWIRING_H

/// @file dialogcoordinatorwiring.h
/// @brief DialogCoordinator の生成・配線・コンテキスト設定を担当するクラスの定義

#include <QObject>
#include <QStringList>
#include <QVector>
#include <functional>

#include "playmode.h"

class QWidget;
class DialogCoordinator;
class MatchCoordinator;
class ShogiGameController;
class KifuRecordListModel;
class KifuBranchTree;
class KifuNavigationState;
class ShogiEngineThinkingModel;
class KifuLoadCoordinator;
class GameInfoPaneController;
class EvaluationChartWidget;
class AnalysisResultsPresenter;
class ConsiderationWiring;
class UiStatePolicyManager;
class EngineAnalysisTab;
class RecordPane;
struct ShogiMove;
class KifuDisplay;

/**
 * @brief DialogCoordinator の生成・コンテキスト設定・シグナル配線を担当するクラス
 *
 * 責務:
 * - DialogCoordinator の遅延生成
 * - 3つのコンテキスト構造体（Consideration, TsumeSearch, KifuAnalysis）の構築・設定
 * - シグナル/スロット接続（2 direct connect + DialogOrchestrator 経由 7 connect）
 */
class DialogCoordinatorWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // --- 生成に必要な依存 ---
        QWidget* parentWidget = nullptr;               ///< 親ウィジェット（DialogCoordinator コンストラクタ用）
        MatchCoordinator* match = nullptr;             ///< 対局調整（非所有）
        ShogiGameController* gameController = nullptr; ///< ゲーム制御（非所有）

        // --- 複数コンテキストで共有 ---
        QStringList* sfenRecord = nullptr;             ///< SFEN記録（外部所有）
        int* currentMoveIndex = nullptr;               ///< 現在の手インデックス（外部所有）
        QStringList* gameUsiMoves = nullptr;           ///< USI形式指し手リスト（外部所有）
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr; ///< 棋譜読み込みコーディネータ（非所有）
        QString* startSfenStr = nullptr;               ///< 開始局面SFEN文字列（外部所有）

        // --- ConsiderationContext 固有 ---
        QVector<ShogiMove>* gameMoves = nullptr;       ///< 指し手リスト（外部所有）
        KifuRecordListModel* kifuRecordModel = nullptr; ///< 棋譜リストモデル（非所有）
        QString* currentSfenStr = nullptr;             ///< 現在局面SFEN文字列（外部所有）
        KifuBranchTree* branchTree = nullptr;          ///< 分岐ツリー（非所有）
        KifuNavigationState* navState = nullptr;       ///< ナビゲーション状態（非所有）
        ShogiEngineThinkingModel** considerationModel = nullptr; ///< 検討モデル（ダブルポインタ、外部所有）

        // --- TsumeSearchContext 固有 ---
        QStringList* positionStrList = nullptr;        ///< 局面文字列リスト（外部所有）

        // --- KifuAnalysisContext 固有 ---
        QList<KifuDisplay*>* moveRecords = nullptr;    ///< 移動記録（外部所有）
        int* activePly = nullptr;                      ///< アクティブ手数（外部所有）
        GameInfoPaneController* gameInfoController = nullptr; ///< 対局情報コントローラ（非所有）
        EvaluationChartWidget* evalChart = nullptr;    ///< 評価値グラフ（非所有）
        AnalysisResultsPresenter* presenter = nullptr; ///< 解析結果プレゼンタ（非所有）
        std::function<bool()> getBoardFlipped;         ///< 盤面反転状態取得コールバック

        // --- 遅延初期化コールバック ---
        std::function<ConsiderationWiring*()> getConsiderationWiring; ///< ConsiderationWiring取得
        std::function<UiStatePolicyManager*()> getUiStatePolicyManager; ///< UiStatePolicyManager取得

        // --- 解析イベントハンドラ用 ---
        EvaluationChartWidget* evalChartWidget = nullptr;   ///< 評価値グラフ（非所有）
        EngineAnalysisTab* analysisTab = nullptr;           ///< エンジン解析タブ（非所有）
        PlayMode* playMode = nullptr;                       ///< プレイモード（外部所有）
        std::function<void(int)> navigateKifuViewToRow;     ///< 棋譜ビューの行遷移コールバック
    };

    explicit DialogCoordinatorWiring(QObject* parent = nullptr);

    /// DialogCoordinator を必要に応じて生成し、依存関係を更新する
    void ensure(const Deps& deps);

    /// DialogCoordinator を返す（未生成時は nullptr）
    DialogCoordinator* coordinator() const { return m_coordinator; }

public slots:
    /// 棋譜解析の中止
    void cancelKifuAnalysis();
    /// 棋譜解析の進捗を受け取る
    void onKifuAnalysisProgress(int ply, int scoreCp);
    /// 棋譜解析結果リストの行選択時に該当局面へ遷移する
    void onKifuAnalysisResultRowSelected(int row);

private:
    void wireSignals(const Deps& deps);
    void bindContexts(const Deps& deps);

    DialogCoordinator* m_coordinator = nullptr;

    // 解析イベントハンドラ用
    EvaluationChartWidget* m_evalChartWidget = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    PlayMode* m_playMode = nullptr;
    std::function<void(int)> m_navigateKifuViewToRow;
};

#endif // DIALOGCOORDINATORWIRING_H
