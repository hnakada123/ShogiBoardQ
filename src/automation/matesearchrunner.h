#ifndef MATESEARCHRUNNER_H
#define MATESEARCHRUNNER_H

/// @file matesearchrunner.h
/// @brief 登録エンジンの `go mate` で 1 局面の詰みを探索する実行器（自動化・CLI 用）

#include <QElapsedTimer>
#include <QObject>

#include "usiposition.h"
#include "usimessages.h"
#include <QString>
#include <QStringList>
#include <QTimer>

class UsiEngineSession;

/**
 * @brief `position` → `go mate <ms>` → `checkmate ...` までを 1 回実行する
 */
class MateSearchRunner : public QObject
{
    Q_OBJECT

public:
    enum class Status { Mate, NoMate, Unknown, NotImplemented };

    struct Request {
        QString enginePath;
        QString engineName;
        UsiPosition position;      ///< 探索する局面
        int timeMs = 5000;
    };

    explicit MateSearchRunner(QObject* parent = nullptr);

    [[nodiscard]] bool start(const Request& request, QString* error = nullptr);
    /// 早期停止（応答を待って finished を発行する）
    void stop();
    qint64 elapsedMs() const { return m_elapsed.isValid() ? m_elapsed.elapsed() : 0; }

    static QString statusName(Status status);

signals:
    void finished(MateSearchRunner::Status status, const QStringList& pv);
    void errorOccurred(const QString& message);

private slots:
    void onSessionReady();
    void onCheckmate(const UsiCheckmate& result);
    void onNoResponse();
    void onSessionError(const QString& message);

private:
    void finish(Status status, const QStringList& pv);

    UsiEngineSession* m_session = nullptr; ///< QObject parent 所有
    QElapsedTimer m_elapsed;
    bool m_done = false;
    bool m_searchStarted = false;
    Request m_request;
};

#endif // MATESEARCHRUNNER_H
