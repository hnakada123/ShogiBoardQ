#ifndef ENGINEPROCESSMANAGER_H
#define ENGINEPROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <QStringList>

/**
 * @brief 将棋エンジンプロセスの管理クラス
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

    // === プロセス管理 ===
    
    /// エンジンプロセスを起動する
    bool startProcess(const QString& engineFile);
    
    /// エンジンプロセスを終了する
    void stopProcess();
    
    /// プロセスが実行中かどうか
    bool isRunning() const;
    
    /// プロセスの状態を取得
    QProcess::ProcessState state() const;

    // === コマンド送信 ===
    
    /// コマンドを送信する
    void sendCommand(const QString& command);
    
    /// 書き込みチャネルを閉じる
    void closeWriteChannel();

    // === 状態管理 ===
    
    /// シャットダウン状態を設定
    void setShutdownState(ShutdownState state);
    
    /// シャットダウン状態を取得
    ShutdownState shutdownState() const { return m_shutdownState; }
    
    /// quit後に許可するinfo stringの行数を設定
    void setPostQuitInfoStringLinesLeft(int count);
    
    /// quit後に許可するinfo stringの行数を取得
    int postQuitInfoStringLinesLeft() const { return m_postQuitInfoStringLinesLeft; }
    
    /// quit後の行数をデクリメント
    void decrementPostQuitLines();

    // === バッファ管理 ===
    
    /// 標準出力バッファを読み取って破棄
    void discardStdout();
    
    /// 標準エラーバッファを読み取って破棄
    void discardStderr();
    
    /// 1行読み取り可能かどうか
    bool canReadLine() const;
    
    /// 1行読み取る
    QByteArray readLine();
    
    /// 標準エラーを全て読み取る
    QByteArray readAllStderr();

    // === ログ識別 ===
    
    /// ログ用識別子を設定
    void setLogIdentity(const QString& engineTag, const QString& sideTag, 
                        const QString& engineName = QString());
    
    /// ログ用プレフィックスを取得
    QString logPrefix() const;

signals:
    /// データ受信シグナル（標準出力）
    void dataReceived(const QString& line);
    
    /// データ受信シグナル（標準エラー）
    void stderrReceived(const QString& line);
    
    /// コマンド送信シグナル（ログ用）
    void commandSent(const QString& command);
    
    /// プロセスエラーシグナル
    void processError(QProcess::ProcessError error, const QString& errorMessage);
    
    /// プロセス終了シグナル
    void processFinished(int exitCode, QProcess::ExitStatus status);
    
    /// エンジン名検出シグナル
    void engineNameDetected(const QString& name);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess* m_process = nullptr;
    ShutdownState m_shutdownState = ShutdownState::Running;
    int m_postQuitInfoStringLinesLeft = 0;
    
    /// ログ識別用
    QString m_logEngineTag;
    QString m_logSideTag;
    QString m_logEngineName;
    
    /// 1回の読み取りで処理する最大行数
    static constexpr int kMaxLinesPerChunk = 64;
    
    /// 未処理の行数カウンタ
    int m_processedLines = 0;
    
    /// データ受信継続用
    void scheduleMoreReading();
};

#endif // ENGINEPROCESSMANAGER_H
