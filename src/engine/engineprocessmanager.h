#ifndef ENGINEPROCESSMANAGER_H
#define ENGINEPROCESSMANAGER_H

/// @file engineprocessmanager.h
/// @brief 将棋エンジンプロセス管理クラスの定義

#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <QStringList>

/**
 * @brief 将棋エンジンプロセスの管理クラス
 *
 * @todo remove コメントスタイルガイド適用済み
 *
 * 責務:
 * - QProcessの起動・終了
 * - プロセスへのコマンド送信
 * - プロセスからのデータ受信
 * - プロセスの状態管理
 *
 * 単一責任原則（SRP）に基づき、プロセス管理のみを担当する。
 * USIプロトコルの解釈やGUI更新は他のクラスに委譲する。
 */
class EngineProcessManager : public QObject
{
    Q_OBJECT

public:
    /// シャットダウン状態
    enum class ShutdownState {
        Running,                    ///< 通常動作中
        IgnoreAll,                  ///< 全ての受信を無視
        IgnoreAllExceptInfoString   ///< info string以外を無視
    };

    explicit EngineProcessManager(QObject* parent = nullptr);
    ~EngineProcessManager() override;

    // --- プロセス管理 ---
    
    /// エンジンプロセスを起動し、シグナル接続を行う
    bool startProcess(const QString& engineFile);

    void stopProcess();
    bool isRunning() const;
    QProcess::ProcessState state() const;
    QString currentEnginePath() const { return m_currentEnginePath; }

    // --- コマンド送信 ---
    
    /// USIコマンド文字列をプロセスへ送信し、commandSentシグナルを発行
    void sendCommand(const QString& command);

    void closeWriteChannel();

    // --- 状態管理 ---
    
    void setShutdownState(ShutdownState state);
    ShutdownState shutdownState() const { return m_shutdownState; }

    void setPostQuitInfoStringLinesLeft(int count);
    int postQuitInfoStringLinesLeft() const { return m_postQuitInfoStringLinesLeft; }

    /// quit後の残行数をデクリメントし、0になったらIgnoreAllへ遷移
    void decrementPostQuitLines();

    // --- バッファ管理 ---
    
    void discardStdout();
    void discardStderr();
    bool canReadLine() const;
    QByteArray readLine();
    QByteArray readAllStderr();

    // --- ログ識別 ---
    
    /// ログ出力用の識別情報（エンジンタグ・手番タグ・エンジン名）を設定
    void setLogIdentity(const QString& engineTag, const QString& sideTag,
                        const QString& engineName = QString());

    QString logPrefix() const;

signals:
    /// 標準出力から1行受信（→ USIProtocolHandler）
    void dataReceived(const QString& line);

    /// 標準エラーから1行受信（→ USIProtocolHandler）
    void stderrReceived(const QString& line);

    /// コマンド送信通知（→ UsiCommLogModel：ログ記録用）
    void commandSent(const QString& command);

    /// プロセスエラー発生（→ USIProtocolHandler）
    void processError(QProcess::ProcessError error, const QString& errorMessage);

    /// プロセス終了通知（→ USIProtocolHandler）
    void processFinished(int exitCode, QProcess::ExitStatus status);

    /// エンジン名を"id name"応答から検出（→ USIProtocolHandler）
    void engineNameDetected(const QString& name);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess* m_process = nullptr;              ///< エンジンプロセス（所有、this親）
    ShutdownState m_shutdownState = ShutdownState::Running; ///< 現在のシャットダウン状態
    int m_postQuitInfoStringLinesLeft = 0;      ///< quit後に許可するinfo string残行数

    QString m_logEngineTag;                     ///< ログ用エンジン識別タグ
    QString m_logSideTag;                       ///< ログ用手番識別タグ
    QString m_logEngineName;                    ///< ログ用エンジン名

    static constexpr int kMaxLinesPerChunk = 64; ///< 1回の読み取りで処理する最大行数
    int m_processedLines = 0;                   ///< 今回チャンクで処理済みの行数

    QString m_currentEnginePath;                ///< 現在のエンジンファイルパス

    /// 未読データが残っている場合にイベントループ経由で再読み取りを予約
    void scheduleMoreReading();
};

#endif // ENGINEPROCESSMANAGER_H
