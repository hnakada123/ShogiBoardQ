#ifndef TSUMEVERIFICATIONRUNNER_H
#define TSUMEVERIFICATIONRUNNER_H

/// @file tsumeverificationrunner.h
/// @brief 登録エンジンを使って 1 局面の余詰検査を行う実行器（自動化・CLI 用）

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "tsumeshogiverifier.h"

class MateOracle;
class MateQueryLoop;
class TsumeVerificationJob;

/**
 * @brief 1局面の唯一解の検査（TsumeVerificationJob）をエンジンの go mate で進める
 *
 * 手順（1回の問い合わせは1秒まで、総時間で打ち切る）は詰将棋生成の検査と同じ部品で決まる。
 */
class TsumeVerificationRunner : public QObject
{
    Q_OBJECT

public:
    struct Request {
        QString enginePath;
        QString engineName;
        QString sfen;
        int targetMoves = 3;
        int timeoutMs = 15000;
        bool allowFinalMoveAlternatives = true;
    };

    explicit TsumeVerificationRunner(QObject* parent = nullptr);
    ~TsumeVerificationRunner() override;

    [[nodiscard]] bool start(const Request& request, QString* error = nullptr);
    void abort();

    static QString statusName(TsumeshogiVerifier::Status status);

signals:
    void progress(int queries);
    void finished(TsumeshogiVerifier::Status status, const QStringList& pv, int queries, qint64 elapsedMs);
    void errorOccurred(const QString& message);

private slots:
    void onReady();
    void onQueryStarted();
    void onFinished();
    void onUnsupported();
    void onNoResponse();
    void onEngineError(const QString& message);

private:
    void fail(const QString& message);

    MateOracle* m_oracle = nullptr;    ///< QObject parent 所有
    MateQueryLoop* m_loop = nullptr;   ///< QObject parent 所有
    std::unique_ptr<TsumeVerificationJob> m_job;
    QElapsedTimer m_elapsed;
    bool m_done = false;
};

#endif // TSUMEVERIFICATIONRUNNER_H
