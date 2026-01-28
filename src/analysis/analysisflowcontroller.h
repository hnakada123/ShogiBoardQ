#ifndef ANALYSISFLOWCONTROLLER_H
#define ANALYSISFLOWCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QVector>
#include <functional>

#include "playmode.h"
#include "analysiscoordinator.h"

class KifuAnalysisDialog;
class EngineAnalysisTab;
class KifuAnalysisListModel;
class KifuRecordListModel;
class AnalysisResultsPresenter;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiGameController;
class KifuDisplay;
class QWidget;

class AnalysisFlowController : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        QStringList*                 sfenRecord = nullptr;   // required
        QList<KifuDisplay *>*        moveRecords = nullptr;  // optional (deprecated)
        KifuRecordListModel*         recordModel = nullptr;  // optional（指し手ラベル取得用）
        KifuAnalysisListModel*       analysisModel = nullptr;// required
        EngineAnalysisTab*           analysisTab = nullptr;  // optional
        Usi*                         usi = nullptr;          // optional（無ければ内部生成）
        UsiCommLogModel*             logModel = nullptr;     // optional（info/bestmove 橋渡し）
        ShogiEngineThinkingModel*    thinkingModel = nullptr;// optional（思考情報表示用）
        ShogiGameController*         gameController = nullptr; // optional（Usi内部生成時に必要）
        AnalysisResultsPresenter*    presenter = nullptr;    // optional（外部から渡す場合）
        int                          activePly = 0;
        QString                      blackPlayerName;        // optional（先手対局者名）
        QString                      whitePlayerName;        // optional（後手対局者名）
        QStringList*                 usiMoves = nullptr;     // optional（USI形式の指し手リスト）
        std::function<void(const QString&)> displayError;    // required
    };

    explicit AnalysisFlowController(QObject* parent = nullptr);
    ~AnalysisFlowController() override;
    void start(const Deps& d, KifuAnalysisDialog* dlg);

    // ダイアログの生成・exec も含めて丸ごと実行する入口
    void runWithDialog(const Deps& d, QWidget* parent);

    // 解析中止
    void stop();

    // 解析中かどうか
    bool isRunning() const { return m_running; }

Q_SIGNALS:
    // 解析が停止した（中止または完了）
    void analysisStopped();
    
    // 解析進捗を通知（ply: 手数, scoreCp: 評価値）
    void analysisProgressReported(int ply, int scoreCp);
    
    // 解析結果の行が選択されたときに通知（棋譜欄・将棋盤・分岐ツリー連動用）
    void analysisResultRowSelected(int row);

private slots:
    void onUsiCommLogChanged();
    void onBestMoveReceived();
    void onInfoLineReceived(const QString& line);
    void onThinkingInfoUpdated(const QString& time, const QString& depth,
                                const QString& nodes, const QString& score,
                                const QString& pvKanjiStr, const QString& usiPv,
                                const QString& baseSfen, int multipv, int scoreCp);
    void onPositionPrepared(int ply, const QString& sfen);
    void onAnalysisProgress(int ply, int depth, int seldepth,
                             int scoreCp, int mate,
                             const QString& pv, const QString& raw);
    void onAnalysisFinished(AnalysisCoordinator::Mode mode);
    void onResultRowDoubleClicked(int row);

private:
    QPointer<AnalysisCoordinator>      m_coord;
    QPointer<AnalysisResultsPresenter> m_presenter;

    // cached deps
    QStringList*           m_sfenRecord = nullptr;
    QList<KifuDisplay *>*  m_moveRecords = nullptr;
    KifuRecordListModel*   m_recordModel = nullptr;
    KifuAnalysisListModel* m_analysisModel = nullptr;
    EngineAnalysisTab*     m_analysisTab = nullptr;
    Usi*                   m_usi = nullptr;
    UsiCommLogModel*       m_logModel = nullptr;
    int                    m_activePly = 0;
    int                    m_prevEvalCp = 0;  // 前回評価値（差分算出用）
    QString                m_blackPlayerName;  // 先手対局者名
    QString                m_whitePlayerName;  // 後手対局者名
    QStringList*           m_usiMoves = nullptr; // USI形式の指し手リスト
    std::function<void(const QString&)> m_err;

    // 現在解析中の局面の一時結果（bestmove時に確定）
    int m_pendingPly = -1;
    int m_pendingScoreCp = 0;
    int m_pendingMate = 0;
    QString m_pendingPv;
    QString m_pendingPvKanji;  // 漢字変換されたPV

    // 最後に確定した結果（GUI更新用）
    int m_lastCommittedPly = -1;
    int m_lastCommittedScoreCp = 0;

    void applyDialogOptions(KifuAnalysisDialog* dlg);
    void commitPendingResult();  // bestmove受信時に結果を確定
    QString extractUsiMoveFromKanji(const QString& kanjiMove) const;  // 漢字表記からUSI形式に変換

    // 自動生成したUsi用のPlayMode（インスタンス保持）
    PlayMode m_playModeForAnalysis = PlayMode::AnalysisMode;

    // 内部で生成したUsi関連リソース（所有権を持つ）
    bool m_ownsUsi = false;
    bool m_running = false;  // 解析中フラグ
    bool m_stoppedByUser = false;  // ユーザーによる中止フラグ
    UsiCommLogModel* m_ownedLogModel = nullptr;
    ShogiEngineThinkingModel* m_ownedThinkingModel = nullptr;
    ShogiGameController* m_gameController = nullptr;
};

#endif // ANALYSISFLOWCONTROLLER_H
