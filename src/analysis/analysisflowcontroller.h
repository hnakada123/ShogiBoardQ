#ifndef ANALYSISFLOWCONTROLLER_H
#define ANALYSISFLOWCONTROLLER_H

/// @file analysisflowcontroller.h
/// @brief 棋譜解析フローコントローラクラスの定義


#include <QObject>
#include <QPointer>
#include <QVector>
#include <functional>
#include <memory>

#include "playmode.h"
#include "analysiscoordinator.h"

class AnalysisResultHandler;
class KifuAnalysisDialog;
class KifuAnalysisListModel;
class KifuRecordListModel;
class AnalysisResultsPresenter;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiGameController;
class KifuDisplay;
class QWidget;

/**
 * @brief 棋譜解析ダイアログと解析実行を仲介するフローコントローラ
 *
 * ダイアログ入力を解析オプションへ反映し、AnalysisCoordinator/Usi/表示Presenter
 * 間の接続を構成して解析開始から結果確定までを管理する。
 *
 */
class AnalysisFlowController : public QObject
{
    Q_OBJECT
public:
    /// 依存オブジェクト
    struct Deps {
        QStringList*                 sfenRecord = nullptr;    ///< 各手数の局面コマンド列（必須、非所有）
        QList<KifuDisplay *>*        moveRecords = nullptr;   ///< 旧棋譜データ（任意、非所有）
        KifuRecordListModel*         recordModel = nullptr;   ///< 棋譜表示モデル（任意、非所有）
        KifuAnalysisListModel*       analysisModel = nullptr; ///< 解析結果モデル（必須、非所有）
        Usi*                         usi = nullptr;           ///< USI通信窓口（必須、非所有）
        UsiCommLogModel*             logModel = nullptr;      ///< 通信ログモデル（任意、非所有）
        ShogiEngineThinkingModel*    thinkingModel = nullptr; ///< 思考情報モデル（任意、非所有）
        ShogiGameController*         gameController = nullptr; ///< 盤面同期用GC（任意、非所有）
        AnalysisResultsPresenter*    presenter = nullptr;     ///< 結果表示Presenter（任意、非所有）
        int                          activePly = 0;           ///< 現在の手数
        QString                      blackPlayerName;         ///< 先手名（任意）
        QString                      whitePlayerName;         ///< 後手名（任意）
        QStringList*                 usiMoves = nullptr;      ///< USI形式の指し手列（任意、非所有）
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

    /// ダイアログ生成から解析開始までを一括実行する
    /// @return 解析が開始された場合は true、キャンセルやエラーで中止された場合は false
    bool runWithDialog(const Deps& d, QWidget* parent);

    /// 実行中の解析を停止する
    void stop();

    /// 解析中かどうか
    bool isRunning() const { return m_running; }

Q_SIGNALS:
    /// 解析停止を通知する（中止/完了共通、→ DialogCoordinator経由でMainWindowへ）
    void analysisStopped();
    
    /// 解析進捗を通知する（→ DialogCoordinator::analysisProgressReported → MainWindow）
    void analysisProgressReported(int ply, int scoreCp);
    
    /// 解析結果の行選択を通知する（→ DialogCoordinator::analysisResultRowSelected → MainWindow、棋譜欄・盤面・分岐ツリー連動）
    void analysisResultRowSelected(int row);

private slots:
    /// USI通信ログの更新時に`bestmove`検出を行う
    void onUsiCommLogChanged();

    /// `bestmove`受信時に保留中結果を確定する
    void onBestMoveReceived();

    /// `info`行受信時に最新の評価値/PVを更新する
    void onInfoLineReceived(const QString& line);

    /// 思考情報更新を受けて漢字PV等を保持する
    void onThinkingInfoUpdated(const QString& time, const QString& depth,
                                const QString& nodes, const QString& score,
                                const QString& pvKanjiStr, const QString& usiPv,
                                const QString& baseSfen, int multipv, int scoreCp);

    /// 局面準備通知を受けて盤面データを同期する
    void onPositionPrepared(int ply, const QString& sfen);

    /// AnalysisCoordinatorの解析進捗通知を受けて一時結果を更新する
    void onAnalysisProgress(int ply, int depth, int seldepth,
                             int scoreCp, int mate,
                             const QString& pv, const QString& raw);

    /// 解析完了通知時に最終状態を反映する
    void onAnalysisFinished(AnalysisCoordinator::Mode mode);

    /// 結果行ダブルクリック時に読み筋盤面を表示する
    void onResultRowDoubleClicked(int row);

    /// エンジンエラー受信時に解析を停止する
    void onEngineError(const QString& msg);

private:
    QPointer<AnalysisCoordinator>      m_coord;      ///< 解析司令塔（非所有）
    QPointer<AnalysisResultsPresenter> m_presenter;  ///< 結果表示Presenter（非所有）

    QStringList*           m_sfenHistory = nullptr;  ///< 局面コマンド列（非所有）
    QList<KifuDisplay *>*  m_moveRecords = nullptr; ///< 旧棋譜データ（非所有）
    KifuRecordListModel*   m_recordModel = nullptr; ///< 棋譜モデル（非所有）
    KifuAnalysisListModel* m_analysisModel = nullptr; ///< 解析結果モデル（非所有）
    Usi*                   m_usi = nullptr;         ///< USI通信窓口（非所有）
    UsiCommLogModel*       m_logModel = nullptr;    ///< USIログモデル（非所有）
    int                    m_activePly = 0;         ///< 開始時の手数
    QString                m_blackPlayerName;       ///< 先手名
    QString                m_whitePlayerName;       ///< 後手名
    QStringList*           m_usiMoves = nullptr;    ///< USI形式指し手列（非所有）
    bool                   m_boardFlipped = false;  ///< GUI本体の盤面反転状態
    std::function<void(const QString&)> m_err;      ///< エラー表示コールバック

    /// ダイアログの解析条件をAnalysisCoordinatorへ反映する
    void applyDialogOptions(KifuAnalysisDialog* dlg);

    std::unique_ptr<AnalysisResultHandler> m_resultHandler; ///< 解析結果ハンドラ（所有）

    PlayMode m_playModeForAnalysis = PlayMode::AnalysisMode;  ///< 内部生成Usiに渡すPlayMode

    bool m_ownsUsi = false;                               ///< Usiを内部生成したか
    bool m_running = false;                               ///< 解析実行中フラグ
    bool m_stoppedByUser = false;                        ///< ユーザー中止フラグ
    UsiCommLogModel* m_ownedLogModel = nullptr;          ///< 内部生成ログモデル（所有）
    ShogiEngineThinkingModel* m_ownedThinkingModel = nullptr; ///< 内部生成思考モデル（所有）
    ShogiGameController* m_gameController = nullptr;     ///< 盤面同期用GC（非所有、Deps経由）
};

#endif // ANALYSISFLOWCONTROLLER_H
