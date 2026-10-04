#ifndef USIENGINESESSION_H
#define USIENGINESESSION_H

/// @file usienginesession.h
/// @brief GUI モデルを持たない USI エンジン接続（自動化・CLI 用）

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

class EngineProcessManager;
class UsiProtocolHandler;

/**
 * @brief `EngineProcessManager` と `UsiProtocolHandler` を束ね、起動・初期化・終了を行う
 *
 * `Usi` ファサードは思考タブ用のプレゼンタと盤面データを必要とするため、
 * 自動化・CLI ではこのクラスでプロトコルハンドラを直接使う。
 * startAsync() は起動とusiok/readyokをシグナルで処理する。
 */
class UsiEngineSession : public QObject
{
    Q_OBJECT

public:
    explicit UsiEngineSession(QObject* parent = nullptr);
    ~UsiEngineSession() override;

    /// 起動要求の受付を返す。完了は ready、失敗は errorOccurred で通知する。
    [[nodiscard]] bool startAsync(const QString& enginePath, const QString& engineName, QString* error = nullptr);
    /// quit を送ってプロセスを止める（未起動なら何もしない）
    void quit();
    bool isRunning() const;

    UsiProtocolHandler* handler() const { return m_handler; }
    EngineProcessManager* process() const { return m_process; }

signals:
    void ready();
    void errorOccurred(const QString& message);

private slots:
    void onProcessStarted();
    void onInitialized(bool success);
    void onStartTimeout();
    void onProcessExited();
    void onProcessError(QProcess::ProcessError error, const QString& message);
    void onHandlerError(const QString& message);

private:
    EngineProcessManager* m_process = nullptr; ///< QObject parent 所有
    UsiProtocolHandler* m_handler = nullptr;   ///< QObject parent 所有
    QString m_lastError;
    bool m_starting = false;
    bool m_initializing = false;
    QTimer m_startTimer;
};

#endif // USIENGINESESSION_H
