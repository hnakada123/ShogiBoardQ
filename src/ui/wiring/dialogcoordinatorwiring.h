#ifndef DIALOGCOORDINATORWIRING_H
#define DIALOGCOORDINATORWIRING_H

/// @file dialogcoordinatorwiring.h
/// @brief DialogCoordinator の生成・配線・コンテキスト設定を担当するクラスの定義

#include <QObject>
#include <QStringList>
#include <QList>


class QWidget;
struct AppState;
class DialogCoordinator;
class AnalysisSession;
class ShogiGameController;
class KifuRecordListModel;
class KifuBranchTree;
class KifuNavigationState;
class ShogiEngineThinkingModel;
class KifuLoadCoordinator;
class GameInfoPaneController;
class EvaluationChartWidget;
class AnalysisResultsPresenter;
class ConsiderationUiController;
class KifuNavigationController;
class ShogiView;
class UiStatePolicyManager;
class EngineAnalysisTab;
class UsiCommLogModel;
class KifuAnalysisListModel;

/**
 * @brief DialogCoordinator の生成・コンテキスト設定・シグナル配線を担当するクラス
 *
 * 責務:
 * - DialogCoordinator の遅延生成
 * - 3つのコンテキスト構造体（Consideration, TsumeSearch, KifuAnalysis）の構築・設定
 * - シグナル/スロット接続（解析の進捗・検討タブ・メニューの状態）
 */
class DialogCoordinatorWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // --- 生成に必要な依存 ---
        QWidget* parentWidget = nullptr;               ///< 親ウィジェット（DialogCoordinator コンストラクタ用）
        AnalysisSession* analysisSession = nullptr;    ///< 検討・詰み探索のセッション（非所有）
        ShogiGameController* gameController = nullptr; ///< ゲーム制御（非所有）

        // --- 検討・詰み探索・棋譜解析で共有（手順は分岐ツリーから取る） ---
        const AppState* state = nullptr;               ///< 手順が無いときに使う表示中・開始局面（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr; ///< 棋譜リストモデル（非所有）
        KifuNavigationState* navState = nullptr;       ///< 表示中の手順（分岐ツリー、非所有）
        ShogiEngineThinkingModel* considerationModel = nullptr;  ///< 検討タブの思考情報モデル（非所有）
        UsiCommLogModel* logModel = nullptr;                     ///< E1 の通信ログ（詰み探索・棋譜解析、非所有）
        ShogiEngineThinkingModel* thinkingModel = nullptr;       ///< E1 の思考情報（詰み探索・棋譜解析、非所有）
        KifuAnalysisListModel* analysisModel = nullptr;          ///< 棋譜解析の結果（非所有）

        // --- 棋譜解析 ---
        GameInfoPaneController* gameInfoController = nullptr; ///< 対局情報コントローラ（非所有）
        EvaluationChartWidget* evalChart = nullptr;    ///< 評価値グラフ（非所有）
        AnalysisResultsPresenter* presenter = nullptr; ///< 解析結果プレゼンタ（非所有）
        const ShogiView* shogiView = nullptr;          ///< 盤の反転状態を解析ダイアログに合わせる（非所有）
        KifuNavigationController* kifuNavController = nullptr; ///< 解析した手へ棋譜欄を移す（非所有）

        // --- シグナルの接続先 ---
        ConsiderationUiController* considerationUi = nullptr;  ///< 検討タブの表示（非所有）
        UiStatePolicyManager* uiStatePolicy = nullptr;       ///< UI 状態（非所有）

        EngineAnalysisTab* analysisTab = nullptr;           ///< エンジン解析タブ（非所有）
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
    KifuNavigationController* m_kifuNavController = nullptr;
};

#endif // DIALOGCOORDINATORWIRING_H
