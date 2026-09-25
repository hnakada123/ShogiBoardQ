#ifndef AUTOMATIONSERVER_H
#define AUTOMATIONSERVER_H

/// @file automationserver.h
/// @brief 自動化 API のローカルソケットサーバー（改行区切り JSON-RPC 2.0）

#include "automationdispatcher.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

/**
 * @brief `QLocalServer` で接続を受け付け、1 行 1 メッセージの JSON-RPC を `AutomationDispatcher` へ渡す
 *
 * - ソケットは所有者だけがアクセスできる（`QLocalServer::UserAccessOption`）
 * - 待ち受け中は設定ディレクトリに `automation-endpoint.json`（ソケットパス・PID・バージョン）を書き、終了時に消す
 * - すべてメインスレッドで処理する（`--automation` 指定時のみ生成される）
 */
class AutomationServer : public QObject
{
    Q_OBJECT

public:
    explicit AutomationServer(QObject* parent = nullptr);
    ~AutomationServer() override;

    AutomationDispatcher& dispatcher() { return m_dispatcher; }

    /// 待ち受けを開始する。socketPath が空なら既定のパスを使う
    [[nodiscard]] bool listen(const QString& socketPath, QString* error = nullptr);
    void close();
    bool isListening() const;
    QString socketPath() const { return m_socketPath; }

    /// 既定のソケットパス（環境変数 SHOGIBOARDQ_AUTOMATION_SOCKET → RuntimeLocation/shogiboardq/automation.sock、Windows は名前付きパイプ名）
    static QString defaultSocketPath();
    /// エンドポイント情報ファイル（設定ディレクトリの automation-endpoint.json）
    static QString endpointFilePath();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void writeEndpointFile();
    void removeEndpointFile();

    QLocalServer* m_server = nullptr;      ///< QObject parent 所有
    AutomationDispatcher m_dispatcher;
    QHash<QLocalSocket*, QByteArray> m_buffers;
    QString m_socketPath;
};

#endif // AUTOMATIONSERVER_H
