#ifndef TSUMEPROGRESSSTORE_H
#define TSUMEPROGRESSSTORE_H

#include "tsumeevaluation.h"
#include <QSqlDatabase>
#include <optional>

/// 履歴は AppData、再生成可能な解析結果は Cache に分離する。
class TsumeProgressStore
{
public:
    struct Progress {
        int attempts = 0;
        int solves = 0;
        QString lastAttempt;
        QString lastSolved;
    };
    explicit TsumeProgressStore(const QString& dataDirectory = {}, const QString& cacheDirectory = {});
    ~TsumeProgressStore();
    TsumeProgressStore(const TsumeProgressStore&) = delete;
    TsumeProgressStore& operator=(const TsumeProgressStore&) = delete;
    bool open();
    QString error() const { return m_error; }
    QString progressPath() const { return m_progress.databaseName(); }
    Progress progress(const QString& id);
    bool recordAttempt(const QString& id);
    bool recordSolved(const QString& id);
    std::optional<TsumeEvaluation> cached(const QString& id, const QString& engine);
    void cache(const QString& id, const QString& engine, const TsumeEvaluation& result);
    void removeCached(const QString& id, const QString& engine);

private:
    bool record(const QString& id, bool solved);
    void pruneCache();
    QSqlDatabase m_progress;
    QSqlDatabase m_cache;
    QString m_dataDirectory;
    QString m_cacheDirectory;
    QString m_error;
};

#endif
