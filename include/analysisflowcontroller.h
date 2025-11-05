#ifndef ANALYSISFLOWCONTROLLER_H
#define ANALYSISFLOWCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <functional>

class KifuAnalysisDialog;
class EngineAnalysisTab;
class KifuAnalysisListModel;
class AnalysisCoordinator;
class AnalysisResultsPresenter;
class Usi;
class UsiCommLogModel;
class KifuDisplay;

class AnalysisFlowController : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        QStringList*                 sfenRecord = nullptr;   // required
        QList<KifuDisplay *>*        moveRecords = nullptr;  // optional
        KifuAnalysisListModel*       analysisModel = nullptr;// required
        EngineAnalysisTab*           analysisTab = nullptr;  // optional
        Usi*                         usi = nullptr;          // required
        UsiCommLogModel*             logModel = nullptr;     // optional（info/bestmove 橋渡し）
        int                          activePly = 0;
        std::function<void(const QString&)> displayError;    // required
    };

    explicit AnalysisFlowController(QObject* parent = nullptr);
    void start(const Deps& d, KifuAnalysisDialog* dlg);

private slots:
    void onUsiCommLogChanged_();
    void onAnalysisProgress_(int ply, int depth, int seldepth,
                             int scoreCp, int mate,
                             const QString& pv, const QString& raw);

private:
    QPointer<AnalysisCoordinator>      m_coord;
    QPointer<AnalysisResultsPresenter> m_presenter;

    // cached deps
    QStringList*           m_sfenRecord = nullptr;
    QList<KifuDisplay *>*  m_moveRecords = nullptr;
    KifuAnalysisListModel* m_analysisModel = nullptr;
    EngineAnalysisTab*     m_analysisTab = nullptr;
    Usi*                   m_usi = nullptr;
    UsiCommLogModel*       m_logModel = nullptr;
    int                    m_activePly = 0;
    int                    m_prevEvalCp = 0;  // 前回評価値（差分算出用）
    std::function<void(const QString&)> m_err;

    void applyDialogOptions_(KifuAnalysisDialog* dlg);
};

#endif // ANALYSISFLOWCONTROLLER_H
