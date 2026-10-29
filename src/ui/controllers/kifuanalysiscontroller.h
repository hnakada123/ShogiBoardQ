#ifndef KIFUANALYSISCONTROLLER_H
#define KIFUANALYSISCONTROLLER_H

/// @file kifuanalysiscontroller.h
/// @brief 棋譜解析（メニューの「棋譜解析」「解析中止」と、解析の進み具合の表示）

#include <QObject>
#include <QPointer>
#include <memory>
#include <optional>

#include "kifuanalysissession.h"

class AnalysisResultsPanel;
class KifuAnalysisRowPresenter;
class QDockWidget;
class EvaluationGraphPresenter;
class KifuAnalysisDialog;
class KifuAnalysisListModel;
class KifuNavigationController;
class KifuNavigationState;
class PvClickController;
class QWidget;

/**
 * @brief 表示中の手順を棋譜解析し、結果を表・評価値グラフ・棋譜欄に出す
 *
 * 解析の実行（エンジン・動作状態「棋譜解析中」）は KifuAnalysisSession が画面と独立に進める（検討の
 * ConsiderationUiController と AnalysisSession と同じ形）。このクラスは利用者の操作（条件ダイアログ・中止・
 * 結果の行の選択）を受け、解析の進み具合を結果の表・評価値グラフ・棋譜欄（解析中の手へ移る）に反映する。
 * エラーは ErrorBus で知らせる。評価値グラフの解析の点のクリックは、合成ルートが棋譜の移動につなぐ。
 */
class KifuAnalysisController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;                    ///< 条件ダイアログの親
        KifuAnalysisSession* session = nullptr;             ///< 解析の実行（必須）
        KifuAnalysisListModel* analysisModel = nullptr;     ///< 解析結果の表（必須）
        AnalysisResultsPanel* resultsPanel = nullptr;   ///< 解析結果の表（必須。モデルは作るときに結ぶ）
        QDockWidget* resultsDock = nullptr;             ///< 解析を始めたときに出す、結果の表のドック
        // --- 解析する手順と、結果の反映先（無くてもよい） ---
        const KifuNavigationState* navState = nullptr;      ///< 解析する手順（表示中の手順）
        PvClickController* pvBoards = nullptr;              ///< 結果の行のダブルクリックで読み筋盤を開く
        KifuNavigationController* navController = nullptr;  ///< 解析中・結果の行を選んだ手へ棋譜を移す
        EvaluationGraphPresenter* evalGraph = nullptr;      ///< 評価値グラフの点（解析した局面の評価値）
    };

    explicit KifuAnalysisController(const Deps& deps, QObject* parent = nullptr);
    ~KifuAnalysisController() override;

    /// 解析を始める（前の解析は止める）。始められなければ ErrorBus で知らせて false
    bool start(AnalysisRequest request);
    /// 表示中の手順と条件ダイアログの値から解析の依頼を作る（エンジンが選ばれていなければ nullopt）
    std::optional<AnalysisRequest> requestFromDialog(const KifuAnalysisDialog& dialog) const;

    bool isRunning() const { return m_deps.session->isRunning(); }

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
    void onResultRowSelected(int row);
    void onResultRowDoubleClicked(int row);
    /// 棋譜欄・グラフなどから移った局面に結果の選択を合わせる
    void syncResultSelection();

private:
    /// 解析する手順（表示中の手順。分岐を選んでいればその分岐。終局の行は含まない）
    RecordLine displayedLine() const;
    /// 表示中の手順のうち、分岐がある手の手数（結果の表で棋譜欄と同じ印を付ける）
    QSet<int> displayedBranchPlies() const;
    void showError(const QString& message) const;
    /// 直前の局面の確定した評価値をグラフに足す
    void publishCommittedEvaluation();
    /// 解析を終えた（completed なら最後まで解析した）ことを結果の表に知らせる
    void finishRun(bool completed);

    Deps m_deps;
    QPointer<AnalysisResultsPanel> m_resultsPanel;
    std::unique_ptr<KifuAnalysisRowPresenter> m_rows;   ///< 解析1回分の結果を表の行にする
    /// 解析を終えた局面の評価値（先手から見た値）。次の局面に移るときにグラフに足す
    struct PendingPlot {
        int ply = 0;
        UsiScore score;
    };
    std::optional<PendingPlot> m_pendingPlot;
    int m_lineEndNodeId = -1;   ///< 解析した手順の末尾の手（手順の番号は分岐を足すと変わるので手で覚える）
    int m_startPly = 0;
    bool m_displaying = false;  ///< 解析の進み具合を結果の表に出している（start から finishRun まで）
};

#endif // KIFUANALYSISCONTROLLER_H
