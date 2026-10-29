#ifndef AUTOMATIONSERVER_H
#define AUTOMATIONSERVER_H

/// @file automationserver.h
/// @brief 自動化 API のローカルソケットサーバー（改行区切り JSON-RPC 2.0）

#include "automationdispatcher.h"
#include "automationendpoint.h"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

/**
 * @brief `QLocalServer` で接続を受け付け、1 行 1 メッセージの JSON-RPC を `AutomationDispatcher` へ渡す
 *
 * - ソケットは所有者だけがアクセスできる（`QLocalServer::UserAccessOption`）
 * - 待ち受け中は設定ディレクトリに `automation-endpoint.json`（ソケットパス・PID・バージョン）を書き、終了時に消す
 *   （場所と1メッセージの上限は headless の AutomationEndpoint で決める）
 * - すべてメインスレッドで処理する（`--automation` 指定時のみ生成される）
 */
class AutomationServer : public QObject
{
    Q_OBJECT

public:
    explicit AutomationServer(QObject* parent = nullptr);
    ~AutomationServer() override;

    AutomationDispatcher& dispatcher() { return m_dispatcher; }

    /**
     * @brief 待ち受けを開始する。socketPath が空なら既定のパス（AutomationEndpoint::defaultSocketPath）を使う
     *
     * Unix ではパスは絶対パスに限る。ソケットのフォルダは、無ければ所有者専用で作り、あれば自分の持ち物で
     * ほかの利用者が書けないことを確かめる（変えない）。前回の残骸は、ソケットで待ち受けがないときだけ消す。
     */
    [[nodiscard]] bool listen(const QString& socketPath, QString* error = nullptr);
    void close();
    bool isListening() const;
    QString socketPath() const { return m_socketPath; }

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
    /// 要求を処理中（readyRead の通知中）のソケット。処理中に切断されても削除を処理後まで遅らせる
    QSet<QLocalSocket*> m_handlingSockets;
    QSet<QLocalSocket*> m_pendingDeletion;
    QString m_socketPath;
};

#endif // AUTOMATIONSERVER_H
