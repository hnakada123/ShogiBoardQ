#ifndef USIENGINESESSION_H
#define USIENGINESESSION_H

/// @file usienginesession.h
/// @brief GUI モデルを持たない USI エンジン接続（自動化・CLI 用）

#include <QObject>
#include <QProcess>
#include <QString>

class EngineProcessManager;
class UsiProtocolHandler;

/**
 * @brief `EngineProcessManager` と `UsiProtocolHandler` を束ね、起動・初期化・終了を行う
 *
 * `Usi` ファサードは思考タブ用のプレゼンタと盤面データを必要とするため、
 * 自動化・CLI ではこのクラスでプロトコルハンドラを直接使う。
 * すべてメインスレッドで動作し、`start()` は usiok/readyok をイベントループで待つ。
 */
class UsiEngineSession : public QObject
{
    Q_OBJECT

public:
    explicit UsiEngineSession(QObject* parent = nullptr);
    ~UsiEngineSession() override;

    /// エンジンを起動して usi/isready の初期化を行う。失敗時は error に理由を入れる
    [[nodiscard]] bool start(const QString& enginePath, const QString& engineName, QString* error = nullptr);
    /// quit を送ってプロセスを止める（未起動なら何もしない）
    void quit();
    bool isRunning() const;

    UsiProtocolHandler* handler() const { return m_handler; }
    EngineProcessManager* process() const { return m_process; }

signals:
    void errorOccurred(const QString& message);

private slots:
    void onProcessError(QProcess::ProcessError error, const QString& message);
    void onHandlerError(const QString& message);

private:
    EngineProcessManager* m_process = nullptr; ///< QObject parent 所有
    UsiProtocolHandler* m_handler = nullptr;   ///< QObject parent 所有
    QString m_lastError;
    bool m_starting = false;
};

#endif // USIENGINESESSION_H
