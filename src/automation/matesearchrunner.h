#ifndef MATESEARCHRUNNER_H
#define MATESEARCHRUNNER_H

/// @file matesearchrunner.h
/// @brief 登録エンジンの `go mate` で 1 局面の詰みを探索する実行器（自動化・CLI 用）

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>

#include "mateoracle.h"
#include "usiposition.h"

/**
 * @brief `position` → `go mate <ms>` → `checkmate ...` までを 1 回実行する（MateOracle の1回の問い合わせ）
 */
class MateSearchRunner : public QObject
{
    Q_OBJECT

public:
    struct Request {
        QString enginePath;
        QString engineName;
        UsiPosition position;      ///< 探索する局面
        int timeMs = 5000;
    };

    explicit MateSearchRunner(QObject* parent = nullptr);

    void start(const Request& request);
    /// 早期停止（答えを待って finished を発行する。打ち切った探索の答えは unknown）
    void stop();
    qint64 elapsedMs() const { return m_elapsed.isValid() ? m_elapsed.elapsed() : 0; }

    /// 答えの種類の名前（mate・nomate・unknown・notimplemented）
    static QString kindName(MateAnswer::Kind kind);

signals:
    void finished(const MateAnswer& answer);
    void errorOccurred(const QString& message);

private slots:
    void onReady();
    void onAnswered(const MateAnswer& answer);
    void onNoResponse();
    void onEngineError(const QString& message);

private:
    void finish(const MateAnswer& answer);

    MateOracle* m_oracle = nullptr; ///< QObject parent 所有
    QElapsedTimer m_elapsed;
    bool m_done = false;
};

#endif // MATESEARCHRUNNER_H
