#ifndef ANALYSISJOB_H
#define ANALYSISJOB_H

#include <QObject>
#include <QPointer>
#include "analysiscoordinator.h"
#include "usimessages.h"
#include "recordline.h"
#include "thinkinginforow.h"

class Usi;

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
    struct Deps {
        Usi* engine = nullptr; ///< 使うエンジン（必須。借用。思考情報・通信ログの表示は持ち主が結ぶ）
    };
    AnalysisJob(AnalysisRequest request, const Deps& deps, QObject* parent = nullptr);
    ~AnalysisJob() override;
    bool start();
    void stop();
    QString error() const { return m_error; }
    const RecordLine& line() const { return m_request.line; }
    AnalysisCoordinator* coordinator() { return &m_coordinator; }
    Usi* engine() const;
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
    void onSearchRequested(const QString& positionCommand, int thinkMs);
    void onBestMove(const UsiBestMove& result);
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
