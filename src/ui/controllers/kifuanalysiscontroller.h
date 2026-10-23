#ifndef KIFUANALYSISCONTROLLER_H
#define KIFUANALYSISCONTROLLER_H

/// @file kifuanalysiscontroller.h
/// @brief 棋譜解析（メニューの「棋譜解析」「解析中止」と、解析の進み具合の表示）

#include <QObject>
#include <QPointer>
#include <memory>
#include <optional>

#include "analysisjob.h"
#include "appactivitystate.h"
#include "enginelifecyclemanager.h"

class AnalysisResultsPanel;
class KifuAnalysisRowPresenter;
class QDockWidget;
class EvaluationChartWidget;
class EvaluationGraphPresenter;
class KifuDocument;
class KifuAnalysisDialog;
class KifuAnalysisListModel;
class KifuNavigationController;
class KifuNavigationState;
class PvClickController;
class QWidget;
class Usi;
struct ThinkingInfoRow;

/**
 * @brief 表示中の手順を棋譜解析し、結果を表・評価値グラフ・棋譜欄に出す
 *
 * 解析は AnalysisJob が画面と独立に進める。このクラスは利用者の操作（条件ダイアログ・中止・
 * 結果の行の選択）を受け、解析の進み具合を評価値グラフと棋譜欄（解析中の手へ移る）に反映する。
 * 解析中は動作状態を「棋譜解析中」にする（メニューの有効・無効は動作状態から決まる）。
 * エラーは ErrorBus で知らせる。
 */
class KifuAnalysisController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;                    ///< 条件ダイアログの親
        AppActivityState* activity = nullptr;               ///< 解析中の動作状態（必須）
        KifuAnalysisListModel* analysisModel = nullptr;     ///< 解析結果の表（必須）
        AnalysisResultsPanel* resultsPanel = nullptr;   ///< 解析結果の表（必須。モデルは作るときに結ぶ）
        QDockWidget* resultsDock = nullptr;             ///< 解析を始めたときに出す、結果の表のドック
        EngineLifecycleManager* engines = nullptr;          ///< 解析のエンジンを置く枠 E1（表示は枠を結ぶ側が行う）
        // --- 解析する手順と、結果の反映先（無くてもよい） ---
        const KifuNavigationState* navState = nullptr;      ///< 解析する手順（表示中の手順）
        PvClickController* pvBoards = nullptr;              ///< 結果の行のダブルクリックで読み筋盤を開く
        KifuNavigationController* navController = nullptr;  ///< 解析中・結果の行を選んだ手へ棋譜を移す
        EvaluationChartWidget* evalChart = nullptr;         ///< 評価値グラフ（点のクリックで解析結果の行へ移る）
        EvaluationGraphPresenter* evalGraph = nullptr;      ///< 評価値グラフの点（解析した局面の評価値）
        Usi* engine = nullptr;                              ///< 使うエンジン（テスト用。無ければ解析ごとに枠 E1 に作る）
    };

    explicit KifuAnalysisController(const Deps& deps, QObject* parent = nullptr);
    ~KifuAnalysisController() override;

    /// 解析を始める（前の解析は止める）。始められなければ ErrorBus で知らせて false
    bool start(AnalysisRequest request);
    /// 表示中の手順と条件ダイアログの値から解析の依頼を作る（エンジンが選ばれていなければ nullopt）
    std::optional<AnalysisRequest> requestFromDialog(const KifuAnalysisDialog& dialog) const;

    bool isRunning() const { return m_running; }

public slots:
    /// メニューの「棋譜解析」: 条件ダイアログを出し、選んだ条件で表示中の手順を解析する
    void showDialogAndStart();
    /// メニューの「解析中止」・結果の表の「中止」
    void stop();

private slots:
    void onPlyStarted(int ply);
    void onPlyAnalyzed(const PlyAnalysis& result);
    void onFinished(bool completed);
    void onEngineError(const QString& message);
    void onEngineRevoked(QObject* owner);
    /// ほかの部品が別の動作（検討・対局など）を始めた
    void onActivityRevoked(QObject* owner);
    void onResultRowSelected(int row);
    void onResultRowDoubleClicked(int row);
    /// 棋譜欄・グラフなどから移った局面に結果の選択を合わせる
    void syncResultSelection();
    /// 評価値グラフの解析の点をクリックした
    void onAnalysisPlyClicked(int lineEndNodeId, int ply);

private:
    /// 解析する手順（表示中の手順。分岐を選んでいればその分岐。終局の行は含まない）
    RecordLine displayedLine() const;
    /// 表示中の手順のうち、分岐がある手の手数（結果の表で棋譜欄と同じ印を付ける）
    QSet<int> displayedBranchPlies() const;
    void showError(const QString& message) const;
    /// 直前の局面の確定した評価値をグラフに足す
    void publishCommittedEvaluation();
    /// 解析を終える（completed なら最後まで解析した）。動作状態を待機に戻し、結果の表に知らせる
    void finishRun(bool completed = false);

    Deps m_deps;
    QPointer<AnalysisResultsPanel> m_resultsPanel;
    std::unique_ptr<KifuAnalysisRowPresenter> m_rows;   ///< 解析1回分の結果を表の行にする
    EngineLease m_lease;                       ///< 枠 E1 のエンジンを使う権利（テスト用のエンジンを渡されたときは使わない）
    ActivityLease m_activityLease;             ///< 解析中の動作状態
    std::unique_ptr<AnalysisJob> m_job;
    /// 解析を終えた局面の評価値（先手から見た値）。次の局面に移るときにグラフに足す
    struct PendingPlot {
        int ply = 0;
        UsiScore score;
    };
    std::optional<PendingPlot> m_pendingPlot;
    int m_lineEndNodeId = -1;   ///< 解析した手順の末尾の手（手順の番号は分岐を足すと変わるので手で覚える）
    int m_startPly = 0;
    bool m_running = false;
    bool m_stoppedByUser = false;
};

#endif // KIFUANALYSISCONTROLLER_H
