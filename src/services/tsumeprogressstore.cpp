#include "tsumeprogressstore.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

TsumeProgressStore::TsumeProgressStore(const QString& dataDirectory, const QString& cacheDirectory)
    : m_dataDirectory(dataDirectory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : dataDirectory)
    , m_cacheDirectory(cacheDirectory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) : cacheDirectory)
{
    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_progress = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("tsume-progress-") + suffix);
    m_cache = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("tsume-cache-") + suffix);
    m_progress.setDatabaseName(m_dataDirectory + QStringLiteral("/tsume_progress.sqlite"));
    m_cache.setDatabaseName(m_cacheDirectory + QStringLiteral("/tsume_cache.sqlite"));
    m_progress.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=250"));
    m_cache.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=250"));
}

TsumeProgressStore::~TsumeProgressStore()
{
    for (auto* db : {&m_progress, &m_cache}) {
        const QString name = db->connectionName();
        db->close();
        *db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}

bool TsumeProgressStore::open()
{
    m_error.clear();
    QDir().mkpath(m_dataDirectory);
    if (!m_progress.open()) { m_error = m_progress.lastError().text(); return false; }
    QSqlQuery query(m_progress);
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS progress (position TEXT PRIMARY KEY, attempts INTEGER NOT NULL DEFAULT 0, solves INTEGER NOT NULL DEFAULT 0, last_attempt TEXT, last_solved TEXT)"))) {
        m_error = query.lastError().text();
        m_progress.close();
        return false;
    }
    QDir().mkpath(m_cacheDirectory);
    if (m_cache.open()) {
        QSqlQuery cacheQuery(m_cache);
        cacheQuery.exec(QStringLiteral("PRAGMA page_size=4096"));
        cacheQuery.exec(QStringLiteral("PRAGMA auto_vacuum=INCREMENTAL"));
        cacheQuery.exec(QStringLiteral("PRAGMA max_page_count=16384")); // 64 MiB（履歴は制限しない）
        if (!cacheQuery.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS evaluations (position TEXT, engine TEXT, status INTEGER, plies INTEGER, pv TEXT, used INTEGER, PRIMARY KEY(position,engine))"))) {
            m_cache.close(); // キャッシュ障害は履歴保存・対局を妨げない。
        } else {
            cacheQuery.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS evaluation_age ON evaluations(used)"));
        }
    }
    return true;
}

TsumeProgressStore::Progress TsumeProgressStore::progress(const QString& id)
{
    Progress result;
    if (!m_progress.isOpen()) return result;
    QSqlQuery query(m_progress);
    query.prepare(QStringLiteral("SELECT attempts,solves,last_attempt,last_solved FROM progress WHERE position=?"));
    query.addBindValue(id);
    if (!query.exec()) m_error = query.lastError().text();
    else if (query.next()) {
        result = {query.value(0).toInt(), query.value(1).toInt(), query.value(2).toString(), query.value(3).toString()};
    }
    return result;
}

bool TsumeProgressStore::record(const QString& id, bool solved)
{
    if (!m_progress.isOpen() || id.isEmpty()) return false;
    QSqlQuery query(m_progress);
    query.prepare(solved
        ? QStringLiteral("INSERT INTO progress(position,attempts,solves,last_solved) VALUES(?,1,1,?) ON CONFLICT(position) DO UPDATE SET solves=solves+1,last_solved=excluded.last_solved")
        : QStringLiteral("INSERT INTO progress(position,attempts,last_attempt) VALUES(?,1,?) ON CONFLICT(position) DO UPDATE SET attempts=attempts+1,last_attempt=excluded.last_attempt"));
    query.addBindValue(id);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) { m_error = query.lastError().text(); return false; }
    m_error.clear();
    return true;
}

bool TsumeProgressStore::recordAttempt(const QString& id) { return record(id, false); }
bool TsumeProgressStore::recordSolved(const QString& id) { return record(id, true); }

std::optional<TsumeEvaluation> TsumeProgressStore::cached(const QString& id, const QString& engine)
{
    if (!m_cache.isOpen()) return std::nullopt;
    QSqlQuery query(m_cache);
    query.prepare(QStringLiteral("SELECT status,plies,pv FROM evaluations WHERE position=? AND engine=?"));
    query.addBindValue(id);
    query.addBindValue(engine);
    if (!query.exec() || !query.next()) return std::nullopt;
    TsumeEvaluation result;
    const int status = query.value(0).toInt();
    if (status != 1 && status != 2) return std::nullopt;
    result.status = status == 1 ? TsumeEvaluation::Status::Mate : TsumeEvaluation::Status::NoMate;
    result.plies = query.value(1).toInt();
    for (const auto& move : QJsonDocument::fromJson(query.value(2).toByteArray()).array()) result.pv.append(move.toString());
    if (result.status == TsumeEvaluation::Status::Mate && (result.plies < 1 || result.plies % 2 != 1)) return std::nullopt;
    query.finish();
    QSqlQuery touch(m_cache);
    touch.prepare(QStringLiteral("UPDATE evaluations SET used=? WHERE position=? AND engine=?"));
    touch.addBindValue(QDateTime::currentMSecsSinceEpoch());
    touch.addBindValue(id);
    touch.addBindValue(engine);
    touch.exec();
    return result;
}

void TsumeProgressStore::pruneCache()
{
    QSqlQuery query(m_cache);
    query.exec(QStringLiteral("DELETE FROM evaluations WHERE rowid IN (SELECT rowid FROM evaluations ORDER BY used DESC LIMIT -1 OFFSET 10000)"));
    if (query.exec(QStringLiteral("PRAGMA page_count")) && query.next() && query.value(0).toInt() > 14000) {
        query.finish();
        query.exec(QStringLiteral("DELETE FROM evaluations WHERE rowid IN (SELECT rowid FROM evaluations ORDER BY used LIMIT 2500)"));
        query.exec(QStringLiteral("PRAGMA incremental_vacuum"));
    }
}

void TsumeProgressStore::cache(const QString& id, const QString& engine, const TsumeEvaluation& result)
{
    if (!m_cache.isOpen() || result.status == TsumeEvaluation::Status::Unknown) return;
    pruneCache();
    QSqlQuery query(m_cache);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO evaluations VALUES(?,?,?,?,?,?)"));
    query.addBindValue(id);
    query.addBindValue(engine);
    query.addBindValue(result.status == TsumeEvaluation::Status::Mate ? 1 : 2);
    query.addBindValue(result.plies);
    query.addBindValue(QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(result.pv)).toJson(QJsonDocument::Compact)));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.exec(); // 満杯・読取専用などでも解析結果自体は利用できる。
    pruneCache();
}

void TsumeProgressStore::removeCached(const QString& id, const QString& engine)
{
    if (!m_cache.isOpen()) return;
    QSqlQuery query(m_cache);
    query.prepare(QStringLiteral("DELETE FROM evaluations WHERE position=? AND engine=?"));
    query.addBindValue(id);
    query.addBindValue(engine);
    query.exec();
}
