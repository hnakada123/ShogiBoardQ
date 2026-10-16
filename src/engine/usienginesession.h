#ifndef USIENGINESESSION_H
#define USIENGINESESSION_H

/// @file usienginesession.h
/// @brief USI エンジン1台への接続（起動・初期化・探索・終了）。GUI と CLI が共用する

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <functional>
#include <vector>

#include "usigotime.h"

class EngineProcessManager;
class UsiProtocolHandler;

/// 探索1回の依頼
struct UsiSearchRequest {
    QString position;          ///< position コマンド（空なら送らない）
    UsiGo go;                  ///< go の種類と時間
    int stopAfterMs = 0;       ///< この時間が過ぎたら stop を送る（0 は送らない）
    /// 結果を待つ時間。stopAfterMs があれば stop の後から、なければ go の後から数える（0 は待ち続ける）。
    /// 過ぎたら searchTimedOut を知らせる
    int replyTimeoutMs = 0;
};

/**
 * @brief USI エンジン1台への接続
 *
 * プロセス（EngineProcessManager）と状態機械（UsiProtocolHandler）を持ち、起動から readyok までと終了を行う。
 * 初期化の間に頼まれた探索・オプション・コマンドは、初期化が終わってから順に送る。
 * search() は「go → 一定時間で stop → 結果を一定時間待つ」を1回で頼む入口で、結果は handler() の
 * bestMoveReceived・checkmateReceived、待ちきれなければ searchTimedOut で知らせる。
 */
class UsiEngineSession : public QObject
{
    Q_OBJECT

public:
    explicit UsiEngineSession(QObject* parent = nullptr);
    ~UsiEngineSession() override;

    /// 起動要求の受付を返す。完了は ready、失敗は errorOccurred で通知する
    [[nodiscard]] bool startAsync(const QString& enginePath, const QString& engineName, QString* error = nullptr);
    /// 起動して usi〜usiok だけ行う（エンジンの登録。名前・作者・オプションは handler() に残る）
    [[nodiscard]] bool probeAsync(const QString& enginePath, QString* error = nullptr);
    /// 起動から readyok までの待ち時間（既定 5000ms。大きなハッシュを確保するエンジンなどで延ばす）
    void setInitializationTimeoutMs(int timeoutMs) { m_initializationTimeoutMs = qMax(1, timeoutMs); }
    /// プロセスを止める（quit を送り、応答がなければ強制終了する）
    void quit();
    bool isRunning() const;
    /// 起動待ちか初期化中か
    bool isInitializing() const;

    /// 探索を頼む（初期化中なら初期化の後で始める）
    void search(const UsiSearchRequest& request);
    /// 探索を止める（初期化中なら、まだ始めていない探索を取りやめる）
    void stop();
    /// 初期化の後に行う（初期化中でなければすぐ行う）
    void whenReady(std::function<void()> action);
    /// 探索・初期化の結果を待つのをやめる（遅れて届いた結果は知らせない）
    void cancelCurrentOperation();

    UsiProtocolHandler* handler() const { return m_handler; }
    EngineProcessManager* process() const { return m_process; }

signals:
    void ready();
    void errorOccurred(const QString& message);
    /// 探索の結果が replyTimeoutMs までに届かなかった
    void searchTimedOut();

private slots:
    void onProcessStarted();
    void onInitialized(bool success);
    void onStartTimeout();
    void onProcessExited();
    void onProcessError(QProcess::ProcessError error, const QString& message);
    void onHandlerError(const QString& message);
    void onStopTimer();
    void onSearchFinished();

private:
    bool launch(const QString& enginePath, QString* error);
    void startRequestedSearch(const UsiSearchRequest& request);
    void clearSearchTimers();

    EngineProcessManager* m_process = nullptr; ///< QObject parent 所有
    UsiProtocolHandler* m_handler = nullptr;   ///< QObject parent 所有
    QString m_lastError;
    /// startAsync の中で同期的に届いたエラーは、errorOccurred ではなく戻り値で返す
    bool m_insideStart = false;
    bool m_probeOnly = false;   ///< usiok で初期化を終える
    QTimer m_startTimer;
    int m_initializationTimeoutMs = 5000;
    std::vector<std::function<void()>> m_pending;  ///< 初期化の後に行うこと

    QTimer m_stopTimer;            ///< stopAfterMs で stop を送る
    QTimer m_replyTimer;           ///< 結果を待つ期限
    int m_replyTimeoutMs = 0;      ///< stop の後に結果を待つ時間（stopAfterMs があるとき）
};

#endif // USIENGINESESSION_H
