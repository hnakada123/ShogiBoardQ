#ifndef USIENGINESESSION_H
#define USIENGINESESSION_H

/// @file usienginesession.h
/// @brief USI エンジン1台への接続（起動・初期化・探索・終了）。GUI と CLI が共用する

#include <QList>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <functional>
#include <optional>
#include <vector>

#include "usigotime.h"
#include "usiposition.h"

class EngineProcessManager;
class UsiProtocolHandler;

/// 探索1回の依頼
struct UsiSearchRequest {
    UsiPosition position;      ///< 探索する局面（空の依頼は受け付けない）
    UsiGo go;                  ///< go の種類と時間
    /// 始める直前に go を決める（あれば go の代わりに使う。対局では前の探索の停止を待つ間も時計が進むので、
    /// 送るときの残り時間で決める）
    std::function<UsiGo()> goAtStart;
    /// position の前に送るオプション（検討の候補手の数など。前の探索を止めた後に送る）
    QList<QPair<QString, QString>> options;
    int stopAfterMs = 0;       ///< この時間が過ぎたら stop を送る（0 は送らない）
    /// 結果を待つ時間。stopAfterMs があれば stop の後から、なければ go の後から数える（0 は待ち続ける）。
    /// 過ぎたら searchTimedOut を知らせる
    int replyTimeoutMs = 0;

    /// 制限時間を過ぎても答えない詰み探索に stop を送るまでの余裕
    static constexpr int kMateStopMarginMs = 5000;
    /// stop を送った後に結果を待つ上限
    static constexpr int kStopReplyMs = 3000;

    /**
     * @brief 詰み探索（go mate）の依頼
     *
     * 制限時間があれば、制限時間＋余裕で stop を送り、その後も答えなければ searchTimedOut を知らせる。
     * 制限時間が 0 以下（go mate infinite）なら打ち切らない（利用者が止める）
     */
    static UsiSearchRequest mate(const UsiPosition& position, int timeLimitMs)
    {
        UsiSearchRequest request;
        request.position = position;
        request.go = UsiGo::mate(timeLimitMs);
        if (timeLimitMs > 0) {
            request.stopAfterMs = timeLimitMs + kMateStopMarginMs;
            request.replyTimeoutMs = kStopReplyMs;
        }
        return request;
    }
};

/**
 * @brief USI エンジン1台への接続
 *
 * プロセス（EngineProcessManager）と状態機械（UsiProtocolHandler）を持ち、起動から readyok までと終了を行う。
 * 初期化の間に頼まれたオプション・コマンドは、初期化が終わってから順に送る。
 * search() は「go → 一定時間で stop → 結果を一定時間待つ」を1回で頼む入口で、結果は handler() の
 * bestMoveReceived・checkmateReceived、待ちきれなければ searchTimedOut で知らせる。
 * 探索は最後に頼まれたものだけを行う。探索中に次を頼まれたら stop を送り、前の結果を捨ててから始める
 * （エンジンに探索を重ねて頼まない）。
 */
class UsiEngineSession : public QObject
{
    Q_OBJECT

public:
    explicit UsiEngineSession(QObject* parent = nullptr);
    ~UsiEngineSession() override;

    /**
     * @brief エンジンを起動して初期化する。完了は ready、失敗は errorOccurred で知らせる
     *
     * 失敗（パスが空・実行できない・起動できない）は、同期では決して知らせない。必ずこの呼び出しから
     * 戻った後のイベントループで errorOccurred にする（使う側は起動の途中の再入に備えなくてよい）。
     * 知らせる前に quit()・次の起動をしたら、その失敗は知らせない。
     */
    void startAsync(const QString& enginePath, const QString& engineName);
    /// 起動して usi〜usiok だけ行う（エンジンの登録。名前・作者・オプションは handler() に残る）。失敗の知らせ方は startAsync と同じ
    void probeAsync(const QString& enginePath);
    /// 起動から readyok までの待ち時間（既定 5000ms。大きなハッシュを確保するエンジンなどで延ばす）
    void setInitializationTimeoutMs(int timeoutMs) { m_initializationTimeoutMs = qMax(1, timeoutMs); }
    /// プロセスを止める（quit を送り、応答がなければ強制終了する）
    void quit();
    bool isRunning() const;
    /// 起動待ちか初期化中か
    bool isInitializing() const;

    /**
     * @brief 探索を頼む（最後に頼んだものだけを行う）
     * @return 局面が空なら何もせず false
     *
     * 初期化中なら初期化の後で始める。探索中なら stop を送ってその結果を捨て、届いてから始める
     * （stop に kStopReplyMs 答えなければ errorOccurred）。始めた探索の結果は1回だけ届く
     * （次の search・cancelSearch・cancelCurrentOperation で取り消さなければ）。
     */
    bool search(const UsiSearchRequest& request);
    /// 最後に頼んだ探索をできるだけ早く終わらせる（結果は届く）。まだ始めていなければ、始めてすぐ stop を送る
    void stop();
    /// 最後に頼んだ探索を取りやめる（結果は知らせない。探索中なら stop を送り、結果を捨てる）
    void cancelSearch();
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
    /// 探索していない状態に戻った（待っている依頼があれば始める）
    void onSearchEnded();
    /// 捨てる探索が stop に答えない
    void onAbandonTimeout();
    void reportLaunchFailure();

private:
    void launch(const QString& enginePath);
    /// 起動の失敗を、呼び出しから戻った後に errorOccurred で知らせる
    void failLaunch(const QString& enginePath);
    void startQueuedSearch();
    /// 今の探索の結果を捨てる（stop を送り、答えを待つ期限を数える）
    void abandonRunningSearch();
    void clearSearchTimers();

    EngineProcessManager* m_process = nullptr; ///< QObject parent 所有
    UsiProtocolHandler* m_handler = nullptr;   ///< QObject parent 所有
    QString m_lastError;
    /// プロセスを起動している間（このとき届いたエラーは覚えるだけにし、戻った後に知らせる）
    bool m_launching = false;
    /// 起動の失敗をまだ知らせていない（quit・次の起動で取り消す）
    bool m_launchFailurePending = false;
    bool m_probeOnly = false;   ///< usiok で初期化を終える
    QTimer m_startTimer;
    int m_initializationTimeoutMs = 5000;
    std::vector<std::function<void()>> m_pending;  ///< 初期化の後に行うこと

    QTimer m_stopTimer;            ///< stopAfterMs で stop を送る
    QTimer m_replyTimer;           ///< 結果を待つ期限
    int m_replyTimeoutMs = 0;      ///< stop の後に結果を待つ時間（stopAfterMs があるとき）
    std::optional<UsiSearchRequest> m_queued;  ///< まだ始めていない最後の依頼（初期化・前の探索の停止を待つ）
    bool m_stopQueued = false;     ///< m_queued を始めたらすぐ stop を送る
    QTimer m_abandonTimer;         ///< 捨てる探索の結果を待つ期限
};

#endif // USIENGINESESSION_H
