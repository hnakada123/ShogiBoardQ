#ifndef ANALYSISJOB_H
#define ANALYSISJOB_H

#include <QObject>
#include <QPointer>
#include "analysiscoordinator.h"
#include "recordline.h"
#include "thinkinginforow.h"

class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;

struct AnalysisRequest {
    RecordLine line;
    AnalysisCoordinator::Options options;
    QString enginePath;
    QString engineName;
    int lineIndex = 0;
};

/// 1回の解析の入力・接続・エンジンの利用期間を管理する。画面の応答を待たずに進行する。
class AnalysisJob : public QObject
{
    Q_OBJECT
public:
    struct Dependencies {
        Usi* engine = nullptr; ///< 指定時は借用。未指定ならこの実行が生成・所有する。
        UsiCommLogModel* log = nullptr;
        ShogiEngineThinkingModel* thinking = nullptr;
    };
    AnalysisJob(AnalysisRequest request, const Dependencies& deps, QObject* parent = nullptr);
    ~AnalysisJob() override;
    bool start();
    void stop();
    QString error() const { return m_error; }
    const RecordLine& line() const { return m_request.line; }
    AnalysisCoordinator* coordinator() { return &m_coordinator; }
    bool isRunning() const { return m_state == State::Initializing || m_state == State::Analyzing; }

signals:
    void positionPrepared(int ply, const QString& sfen);
    void progress(int ply, const UsiInfo& info);
    void thinkingInfoUpdated(const ThinkingInfoRow& row);
    void resultReady();
    void finished(AnalysisCoordinator::Mode mode);
    void errorOccurred(const QString& message);

private slots:
    void onInitialized();
    void onPositionPrepared(int ply, const QString& sfen);
    void onBestMove();
    void onInfo(const UsiInfo& info);
    void onThinkingInfo(const ThinkingInfoRow& row);
    void onFinished(AnalysisCoordinator::Mode mode);
    void onError(const QString& message);

private:
    enum class State { Ready, Initializing, Analyzing, Finished };
    AnalysisRequest m_request;
    AnalysisCoordinator m_coordinator;
    QPointer<Usi> m_engine;
    State m_state = State::Ready;
    QString m_error;
};

#endif
