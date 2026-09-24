#ifndef TSUMEMATEENGINE_H
#define TSUMEMATEENGINE_H

#include "tsumeevaluation.h"
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>

/// 攻方手番の go mate のみを実行する。中断時の不完全な最短探索結果は採用しない。
class TsumeMateEngine : public QObject
{
    Q_OBJECT
public:
    explicit TsumeMateEngine(QObject* parent = nullptr);
    ~TsumeMateEngine() override;
    void setExecutable(const QString& path);
    void search(const QString& sfen, int milliseconds);
    void cancel();
signals:
    void finished(const TsumeEvaluation& result);
private slots:
    void started();
    void readLines();
    void readErrors();
    void processError(QProcess::ProcessError error);
    void processFinished(int code, QProcess::ExitStatus status);
    void timedOut();
private:
    enum class Phase { Off, Usi, Ready, Idle, Searching };
    void send(const QString& command);
    void beginSearch();
    void fail(const QString& message);
    QString m_executable;
    QString m_sfen;
    QString m_engineName;
    QSet<QString> m_options;
    QProcess* m_process = nullptr;
    QTimer m_deadline;
    int m_milliseconds = 5000;
    Phase m_phase = Phase::Off;
    bool m_minLength = false;
};

#endif
