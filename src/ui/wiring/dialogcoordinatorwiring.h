#ifndef DIALOGCOORDINATORWIRING_H
#define DIALOGCOORDINATORWIRING_H

/// @file dialogcoordinatorwiring.h
/// @brief DialogCoordinator の生成・配線・コンテキスト設定を担当するクラスの定義

#include <QObject>
#include <QStringList>
#include <QList>

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
class KifuNavigationController;
class ShogiView;
class UiStatePolicyManager;
class EngineAnalysisTab;
struct ShogiMove;

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

        // --- 検討・詰み探索・棋譜解析で共有（手順は分岐ツリーから取る） ---
        QString* startSfenStr = nullptr;               ///< 開始局面SFEN文字列（AppState）
        KifuRecordListModel* kifuRecordModel = nullptr; ///< 棋譜リストモデル（非所有）
        QString* currentSfenStr = nullptr;             ///< 手順が無いときに使う現在局面SFEN（AppState）
        KifuNavigationState* navState = nullptr;       ///< 表示中の手順（分岐ツリー、非所有）
        ShogiEngineThinkingModel* considerationModel = nullptr;  ///< 検討タブの思考情報モデル（非所有）

        // --- 棋譜解析 ---
        GameInfoPaneController* gameInfoController = nullptr; ///< 対局情報コントローラ（非所有）
        EvaluationChartWidget* evalChart = nullptr;    ///< 評価値グラフ（非所有）
        AnalysisResultsPresenter* presenter = nullptr; ///< 解析結果プレゼンタ（非所有）
        const ShogiView* shogiView = nullptr;          ///< 盤の反転状態を解析ダイアログに合わせる（非所有）
        KifuNavigationController* kifuNavController = nullptr; ///< 解析した手へ棋譜欄を移す（非所有）

        // --- シグナルの接続先 ---
        ConsiderationWiring* considerationWiring = nullptr;  ///< 検討モードの配線（非所有）
        UiStatePolicyManager* uiStatePolicy = nullptr;       ///< UI 状態（非所有）

        EngineAnalysisTab* analysisTab = nullptr;           ///< エンジン解析タブ（非所有）
        const PlayMode* playMode = nullptr;                       ///< プレイモード（AppState）
    };

    explicit DialogCoordinatorWiring(QObject* parent = nullptr);

    /// DialogCoordinator を必要に応じて生成し、依存関係を更新する
    /// DialogCoordinator を作り、シグナルとコンテキストを設定する（1回だけ呼ぶ）
    void wire(const Deps& deps);

    /// DialogCoordinator を返す（未生成時は nullptr）
    DialogCoordinator* coordinator() const { return m_coordinator; }

public slots:
    /// 棋譜解析の中止
    void cancelKifuAnalysis();
    /// 棋譜解析の進捗を受け取る
    void onKifuAnalysisPositionPrepared(int ply);
    void onKifuAnalysisProgress(int ply, int scoreCp, const QString& mate);
    /// 棋譜解析結果リストの行選択時に該当局面へ遷移する
    void onKifuAnalysisResultRowSelected(int row);
    void onKifuAnalysisStarted(int lineIndex);
    void onAnalysisPositionSelected(int lineIndex, int ply);

private:
    void wireSignals(const Deps& deps);
    void bindContexts(const Deps& deps);

    DialogCoordinator* m_coordinator = nullptr;

    // 解析イベントハンドラ用
    EvaluationChartWidget* m_evalChartWidget = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    const PlayMode* m_playMode = nullptr;
    KifuNavigationController* m_kifuNavController = nullptr;
};

#endif // DIALOGCOORDINATORWIRING_H
