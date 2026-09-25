#ifndef TSUMEVERIFICATIONRUNNER_H
#define TSUMEVERIFICATIONRUNNER_H

/// @file tsumeverificationrunner.h
/// @brief 登録エンジンを使って 1 局面の余詰検査を行う実行器（自動化・CLI 用）

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "tsumeshogiverifier.h"

class UsiEngineSession;

/**
 * @brief `TsumeshogiVerifier` の問い合わせをエンジンの `go mate` で解決する
 *
 * `TsumeshogiGenerator` の検査フェーズと同じ手順（1 問い合わせ 1 秒以内、総時間で打ち切り）で動く。
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

    [[nodiscard]] bool start(const Request& request, QString* error = nullptr);
    void abort();

    static QString statusName(TsumeshogiVerifier::Status status);

signals:
    void progress(int queries);
    void finished(TsumeshogiVerifier::Status status, const QStringList& pv, int queries, qint64 elapsedMs);
    void errorOccurred(const QString& message);

private slots:
    void step();
    void onSolved(const QStringList& pv);
    void onNoMate();
    void onNotImplemented();
    void onUnknown();
    void onSafetyTimeout();
    void onSessionError(const QString& message);

private:
    void submit(TsumeshogiVerifier::Reply reply, const QStringList& pv = {});
    void finish();

    UsiEngineSession* m_session = nullptr; ///< QObject parent 所有
    TsumeshogiVerifier m_verifier;
    Request m_request;
    QTimer m_stepTimer;
    QTimer m_safetyTimer;
    QElapsedTimer m_elapsed;
    int m_queries = 0;
    bool m_awaiting = false;
    bool m_awaitingStop = false; ///< 無応答で stop を送り、その応答を待っている
    bool m_done = false;
};

#endif // TSUMEVERIFICATIONRUNNER_H
