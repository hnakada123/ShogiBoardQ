#ifndef ANALYSISFLOWCONTROLLER_H
#define ANALYSISFLOWCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QVector>
#include <functional>

#include "playmode.h"

class KifuAnalysisDialog;
class EngineAnalysisTab;
class KifuAnalysisListModel;
class KifuRecordListModel;
class AnalysisCoordinator;
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
        int                          activePly = 0;
        std::function<void(const QString&)> displayError;    // required
    };

    explicit AnalysisFlowController(QObject* parent = nullptr);
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

private slots:
    void onUsiCommLogChanged_();
    void onBestMoveReceived_();
    void onInfoLineReceived_(const QString& line);
    void onThinkingInfoUpdated_(const QString& time, const QString& depth,
                                const QString& nodes, const QString& score,
                                const QString& pvKanjiStr, const QString& usiPv,
                                const QString& baseSfen);
    void onPositionPrepared_(int ply, const QString& sfen);
    void onAnalysisProgress_(int ply, int depth, int seldepth,
                             int scoreCp, int mate,
                             const QString& pv, const QString& raw);

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
    std::function<void(const QString&)> m_err;

    // 現在解析中の局面の一時結果（bestmove時に確定）
    int m_pendingPly = -1;
    int m_pendingScoreCp = 0;
    int m_pendingMate = 0;
    QString m_pendingPv;
    QString m_pendingPvKanji;  // 漢字変換されたPV

    void applyDialogOptions_(KifuAnalysisDialog* dlg);
    void commitPendingResult_();  // bestmove受信時に結果を確定

    // 自動生成したUsi用のPlayMode（インスタンス保持）
    PlayMode m_playModeForAnalysis = AnalysisMode;

    // 内部で生成したUsi関連リソース（所有権を持つ）
    bool m_ownsUsi = false;
    bool m_running = false;  // 解析中フラグ
    UsiCommLogModel* m_ownedLogModel = nullptr;
    ShogiEngineThinkingModel* m_ownedThinkingModel = nullptr;
    ShogiGameController* m_gameController = nullptr;
};

#endif // ANALYSISFLOWCONTROLLER_H
