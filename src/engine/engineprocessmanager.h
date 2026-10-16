#ifndef ENGINEPROCESSMANAGER_H
#define ENGINEPROCESSMANAGER_H

/// @file engineprocessmanager.h
/// @brief 将棋エンジンプロセス管理クラスの定義

#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

/**
 * @brief 将棋エンジンのプロセスの起動・終了と、1行ずつの送受信
 *
 * USI の解釈はしない（UsiProtocolHandler）。stopProcessAsync はプロセスを手放して quit を書き、
 * 応答がなければ terminate・kill まで進める。手放した後のプロセスの出力は受け取らない。
 */
class EngineProcessManager : public QObject
{
    Q_OBJECT

public:
    explicit EngineProcessManager(QObject* parent = nullptr);
    ~EngineProcessManager() override;

    /// 非同期に起動する。戻り値は要求の受付、完了は processStarted で知らせる
    [[nodiscard]] bool startProcessAsync(const QString& engineFile);
    /// quit を書いてプロセスを手放す（応答がなければ terminate・kill まで進める）
    void stopProcessAsync();
    bool isRunning() const;
    QString currentEnginePath() const { return m_currentEnginePath; }

    /// 1行送り、commandSent で知らせる
    void sendCommand(const QString& command);

    /// ログに出す識別（"E1"・"black"・エンジン名）
    void setLogIdentity(const QString& engineTag, const QString& sideTag, const QString& engineName = QString());
    /// ログの行頭（"[E1/black] エンジン名"）
    QString logPrefix() const;

signals:
    void processStarted();
    /// 手放す前にプロセスが終わった（異常終了）
    void processExited();
    /// 標準出力の1行
    void dataReceived(const QString& line);
    /// 標準エラーの1行
    void stderrReceived(const QString& line);
    void commandSent(const QString& command);
    void processError(QProcess::ProcessError error, const QString& errorMessage);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    /// 未読の行が残っていれば、イベントループを回してから続きを読む
    void scheduleMoreReading();

    std::unique_ptr<QProcess> m_process;
    QString m_currentEnginePath;
    QString m_logEngineTag;
    QString m_logSideTag;
    QString m_logEngineName;
    static constexpr int kMaxLinesPerChunk = 64; ///< 1回の読み取りで処理する最大行数
};

#endif // ENGINEPROCESSMANAGER_H
