#ifndef PEERPROCESS_H
#define PEERPROCESS_H

/// @file peerprocess.h
/// @brief 通信プロセス shogiboardq-peer の起動・停止と非同期の IPC（実装仕様 10.4節・10.5節、protocol/ipc.md）

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>

class QTimer;

/**
 * @brief 通信プロセス1つ
 *
 * 配布物の中の決まった場所から絶対パスで、空の環境で起動し、hello の応答で権限の制限を確かめてから使う。
 * 受けたメッセージは、世代・要求番号・形をすべて確かめ、合わなければ通信プロセスを終わらせて失敗として扱う。
 * 通信プロセスの「成功」の応答を、署名の成立や相手の身元の根拠にしない（それは対局の司令塔が確かめる）。
 * GUI スレッドで作り、イベントループを止めない（waitFor* を使わない）。
 */
class PeerProcess : public QObject
{
    Q_OBJECT
public:
    enum class State { NotRunning, Starting, Ready, Stopping };
    Q_ENUM(State)

    /// 使えなくなった理由
    enum class Failure {
        None,
        NotFound,           ///< 実行ファイルがない
        UnsupportedOs,      ///< この OS では権限の制限を掛けられない（P2P対局を使えない）
        SandboxFailed,      ///< 権限の制限を掛けられなかった（終了コード 3）
        HelloMismatch,      ///< hello の版・制限の結果が合わない
        ProtocolViolation,  ///< 上限超過・形の合わないメッセージ
        Crashed,            ///< 異常終了した
    };
    Q_ENUM(Failure)

    explicit PeerProcess(QObject* parent = nullptr);
    ~PeerProcess() override;

    /// 実行ファイルの場所（アプリと同じフォルダ。デバッグビルドだけ SHOGIBOARDQ_PEER_EXECUTABLE で差し替えられる）
    static QString executablePath();

    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }
    /// 今のプロセスの世代
    quint64 generation() const { return m_generation; }
    /// hello の応答の sandbox（権限の制限の結果）
    QJsonObject sandbox() const { return m_sandbox; }

    /// 起動して hello を送る。使えるようになれば ready、使えなければ failed
    void start();
    /// shutdown を頼み、2秒で終わらなければ terminate、さらに2秒で kill する（すべて非同期）
    void stop();

    /// 要求を送る（Ready のときだけ。0 は送れなかった）。結果は replied・requestFailed
    quint64 request(const QString& method, const QJsonObject& params = QJsonObject());
    /// 要求の応答を受け取らない（取り消した要求の応答は捨てる）
    void cancel(quint64 requestId);

signals:
    void ready();
    void replied(quint64 requestId, const QString& method, const QJsonObject& result);
    void requestFailed(quint64 requestId, const QString& method, const QString& code);
    void notified(const QString& name, const QJsonObject& params);
    /// 使えなくなった（起動の失敗・異常終了・IPC の違反）。detail は診断用（利用者に見せるなら PlainText）
    void failed(PeerProcess::Failure failure, const QString& detail);
    /// 止まった（stop の後、またはどの理由でも、プロセスが終わった後に1回）
    void stopped();

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);
    void onStopTimeout();

private:
    quint64 send(const QString& method, const QJsonObject& params);
    void handleMessage(const QByteArray& json);
    void fail(Failure failure, const QString& detail);
    void resetProcess();

    QProcess* m_process = nullptr;
    QTimer* m_stopTimer = nullptr;
    State m_state = State::NotRunning;
    quint64 m_generation = 0;
    quint64 m_nextRequestId = 1;
    quint64 m_helloRequestId = 0;
    QHash<quint64, QString> m_pending;   ///< 応答を待つ要求の番号とメソッド
    QByteArray m_buffer;
    QByteArray m_stderr;                 ///< 標準エラーの先頭（64 KiB まで。保存しない）
    QJsonObject m_sandbox;
    bool m_failed = false;
    bool m_terminateSent = false;
};

#endif // PEERPROCESS_H
