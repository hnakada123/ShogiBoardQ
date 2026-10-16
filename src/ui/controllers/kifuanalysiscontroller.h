#ifndef KIFUANALYSISCONTROLLER_H
#define KIFUANALYSISCONTROLLER_H

/// @file kifuanalysiscontroller.h
/// @brief 棋譜解析（メニューの「棋譜解析」「解析中止」と、解析の進み具合の表示）

#include <QObject>
#include <QPointer>
#include <memory>

#include "analysisjob.h"

class AnalysisResultHandler;
class AnalysisResultsPanel;
class AppActivityState;
class EngineLifecycleManager;
class EvaluationChartWidget;
class KifuDocument;
class KifuAnalysisDialog;
class KifuAnalysisListModel;
class KifuNavigationController;
class KifuNavigationState;
class KifuRecordListModel;
class QWidget;
class ShogiView;
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
        AnalysisResultsPanel* resultsPanel = nullptr;   ///< 解析結果のドック（無ければ自分で作る）
        EngineLifecycleManager* engines = nullptr;          ///< 解析のエンジンを置く枠 E1（表示は枠を結ぶ側が行う）
        // --- 解析する手順と、結果の反映先（無くてもよい） ---
        const KifuNavigationState* navState = nullptr;      ///< 解析する手順（表示中の手順）
        const KifuRecordListModel* recordModel = nullptr;   ///< 結果の表に出す手の表記（手数・分岐の印付き）
        const KifuDocument* document = nullptr;   ///< 対局者名（対局情報）
        const ShogiView* shogiView = nullptr;               ///< 読み筋の盤の向きを本体の盤に合わせる
        KifuNavigationController* navController = nullptr;  ///< 解析中・結果の行を選んだ手へ棋譜を移す
        EvaluationChartWidget* evalChart = nullptr;         ///< 評価値グラフ
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
    void onPositionPrepared(int ply, const QString& sfen);
    void onProgress(int ply, const UsiInfo& info);
    void onResultReady();
    void onThinkingInfoUpdated(const ThinkingInfoRow& row);
    void onFinished(AnalysisCoordinator::Mode mode);
    void onEngineError(const QString& message);
    void onResultRowSelected(int row);
    void onResultRowDoubleClicked(int row);
    /// 評価値グラフの解析の点をクリックした
    void onAnalysisPlyClicked(int lineIndex, int ply);

private:
    /// 解析する手順（表示中の手順。結果の表の表記は棋譜欄と同じにする）
    RecordLine displayedLine() const;
    void showError(const QString& message) const;
    /// 直前の局面の確定した評価値をグラフに足す
    void publishCommittedEvaluation();
    void finishRun();

    Deps m_deps;
    QPointer<AnalysisResultsPanel> m_resultsPanel;
    std::unique_ptr<AnalysisResultHandler> m_resultHandler;
    std::unique_ptr<AnalysisJob> m_job;
    int m_lineIndex = 0;
    int m_startPly = 0;
    bool m_running = false;
    bool m_stoppedByUser = false;
};

#endif // KIFUANALYSISCONTROLLER_H
