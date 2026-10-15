#ifndef ANALYSISFLOWCONTROLLER_H
#define ANALYSISFLOWCONTROLLER_H

/// @file analysisflowcontroller.h
/// @brief 棋譜解析フローコントローラクラスの定義


#include <QObject>
#include <QPointer>
#include <QList>
#include <functional>
#include <memory>

#include "analysisjob.h"
#include "thinkinginforow.h"

class AnalysisResultHandler;
class KifuAnalysisDialog;
class KifuAnalysisListModel;
class AnalysisResultsPresenter;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class QWidget;

/**
 * @brief 棋譜解析ダイアログと解析実行を仲介するフローコントローラ
 *
 * ダイアログから AnalysisRequest を作り、AnalysisJob の進捗・結果を表示する。
 * エンジンの寿命や探索の進行は AnalysisJob が管理する。
 *
 */
class AnalysisFlowController : public QObject
{
    Q_OBJECT
public:
    /// 依存オブジェクト
    struct Deps {
        RecordLine                   line;                  ///< 解析対象の手順（値）
        KifuAnalysisListModel*       analysisModel = nullptr; ///< 解析結果モデル（必須、非所有）
        Usi*                         usi = nullptr;           ///< USI通信窓口（任意、指定時は借用）
        UsiCommLogModel*             logModel = nullptr;      ///< 通信ログモデル（任意、非所有）
        ShogiEngineThinkingModel*    thinkingModel = nullptr; ///< 思考情報モデル（任意、非所有）
        AnalysisResultsPresenter*    presenter = nullptr;     ///< 結果表示Presenter（任意、非所有）
        int                          lineIndex = 0;           ///< 解析対象の分岐ライン
        int                          activePly = 0;           ///< 現在の手数
        QString                      blackPlayerName;         ///< 先手名（任意）
        QString                      whitePlayerName;         ///< 後手名（任意）
        bool                          boardFlipped = false;    ///< GUI本体の盤面反転状態
        std::function<void(const QString&)> displayError;     ///< エラー表示コールバック（必須）
    };

    explicit AnalysisFlowController(QObject* parent = nullptr);

    ~AnalysisFlowController() override;

    /**
     * @brief 解析処理を開始する
     * @param d 依存オブジェクト
     * @param dlg 解析条件ダイアログ
     *
     */
    void start(const Deps& d, KifuAnalysisDialog* dlg);
    bool start(const Deps& d, AnalysisRequest request);

    /// ダイアログ生成から解析開始までを一括実行する
    /// @return 解析が開始された場合は true、キャンセルやエラーで中止された場合は false
    bool runWithDialog(const Deps& d, QWidget* parent);

    /// 実行中の解析を停止する
    void stop();

    /// 解析中かどうか
    bool isRunning() const { return m_running; }
    int analysisLineIndex() const { return m_analysisLineIndex; }

signals:
    /// 解析停止を通知する（中止/完了共通、→ DialogCoordinator経由でMainWindowへ）
    void analysisStopped();
    
    /// 解析進捗を通知する（→ DialogCoordinator::analysisProgressReported → MainWindow）
    void analysisProgressReported(int ply, int scoreCp, const QString& mate = {});
    void analysisPositionPrepared(int ply);
    void analysisEngineNameChanged(const QString& name);
    void analysisStarted(int lineIndex);
    
    /// 解析結果の行選択を通知する（→ DialogCoordinator::analysisResultRowSelected → MainWindow、棋譜欄・盤面・分岐ツリー連動）
    void analysisResultRowSelected(int row);

private slots:
    /// `bestmove`受信時に保留中結果を確定する
    void onBestMoveReceived();

    /// 思考情報更新を受けて漢字PV等を保持する
    void onThinkingInfoUpdated(const ThinkingInfoRow& row);

    /// 局面準備通知を受けて盤面データを同期する
    void onPositionPrepared(int ply, const QString& sfen);

    /// AnalysisCoordinatorの解析進捗通知を受けて一時結果を更新する
    void onAnalysisProgress(int ply, const UsiInfo& info);

    /// 解析完了通知時に最終状態を反映する
    void onAnalysisFinished(AnalysisCoordinator::Mode mode);

    /// 結果行ダブルクリック時に読み筋盤面を表示する
    void onResultRowDoubleClicked(int row);

    /// 結果一覧の行番号を解析範囲内の手数へ変換する
    void onResultRowSelected(int row);

    /// エンジンエラー受信時に解析を停止する
    void onEngineError(const QString& msg);

private:
    void emitAnalysisStoppedOnce();
    void publishCommittedEvaluation();

    std::unique_ptr<AnalysisJob> m_job;
    QPointer<AnalysisResultsPresenter> m_presenter;
    KifuAnalysisListModel* m_analysisModel = nullptr;
    int m_analysisLineIndex = 0;
    int m_analysisStartPly = 0;
    std::function<void(const QString&)> m_err;

    std::unique_ptr<AnalysisResultHandler> m_resultHandler; ///< 解析結果ハンドラ（所有）

    bool m_running = false;
    bool m_stoppedByUser = false;
    bool m_lastStartSucceeded = false;
    bool m_analysisStoppedEmitted = false;
};

#endif // ANALYSISFLOWCONTROLLER_H
