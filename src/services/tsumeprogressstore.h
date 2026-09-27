#ifndef TSUMEPROGRESSSTORE_H
#define TSUMEPROGRESSSTORE_H

#include "tsumeevaluation.h"
#include <QSqlDatabase>
#include <QCache>
#include <QHash>
#include <QSet>
#include <optional>

namespace TsumeCollection { struct Result; }

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
    QHash<QString, Progress> progress(const QStringList& ids);
    bool recordAttempt(const QString& id);
    bool recordSolved(const QString& id);
    std::optional<TsumeEvaluation> cached(const QString& id, const QString& engine);
    /// 合法手順を一度確認したキャッシュを再利用。手順必須時は手数だけの記録を返さない。
    std::optional<TsumeEvaluation> validatedCached(const QString& id, const QString& engine, bool requireLine);
    /// 同梱監査と内容が一致する問題集だけを登録。外部の自己申告の検証記録は採用しない。
    void setVerifiedCollection(const QByteArray& contents, const TsumeCollection::Result& parsed);
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
    QCache<QString, TsumeEvaluation> m_validated{1000};
    QHash<QString, TsumeEvaluation> m_certified;
    QSet<QString> m_bypassCertified;
};

#endif
