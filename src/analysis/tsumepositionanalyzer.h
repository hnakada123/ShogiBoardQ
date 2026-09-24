#ifndef TSUMEPOSITIONANALYZER_H
#define TSUMEPOSITIONANALYZER_H

#include "tsumeevaluation.h"
#include "tsume.h"
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <memory>

class TsumeMateEngine;
class TsumeProgressStore;

class TsumePositionAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit TsumePositionAnalyzer(QObject* parent = nullptr);
    ~TsumePositionAnalyzer() override;
    void configure(const QString& enginePath, TsumeProgressStore* store);
    void evaluate(const QString& sfen, int milliseconds, bool requireLine = false);
    void cancel();
    QString engineKey() const { return m_engineKey; }
    QString enginePath() const { return m_enginePath; }
signals:
    void finished(const TsumeEvaluation& result);
private slots:
    void engineFinished(const TsumeEvaluation& result);
    void internalFinished();
    void deliverCached();
private:
    void complete(const TsumeEvaluation& result);
    TsumeMateEngine* m_engine = nullptr;
    TsumeProgressStore* m_store = nullptr;
    QString m_enginePath;
    QString m_engineKey;
    QString m_sfen;
    QString m_id;
    bool m_active = false;
    TsumeEvaluation m_cached;
    QTimer m_cacheTimer;
    std::unique_ptr<QFutureWatcher<TsumeEvaluation>> m_watcher;
    std::shared_ptr<std::atomic_bool> m_stop;
};

#endif
