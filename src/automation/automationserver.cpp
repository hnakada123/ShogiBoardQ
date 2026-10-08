/// @file automationserver.cpp
/// @brief 自動化 API のローカルソケットサーバーの実装

#include "automationserver.h"
#include "logcategories.h"
#include "settingscommon.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QLatin1StringView>
#include <QStandardPaths>

namespace {
constexpr qsizetype kMaxLineBytes = 8 * 1024 * 1024; ///< 1 メッセージの上限（棋譜テキストを含む）
constexpr QLatin1StringView kEndpointFileName("automation-endpoint.json");
}

AutomationServer::AutomationServer(QObject* parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &AutomationServer::onNewConnection);
}

AutomationServer::~AutomationServer()
{
    close();
}

QString AutomationServer::defaultSocketPath()
{
    QString env = qEnvironmentVariable("SHOGIBOARDQ_AUTOMATION_SOCKET");
    if (!env.isEmpty()) return env;
#ifdef Q_OS_WIN
    const QString user = qEnvironmentVariable("USERNAME", QStringLiteral("user"));
    return QStringLiteral("shogiboardq-automation-") + user;
#else
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) runtime = QDir::tempPath();
    return runtime + QStringLiteral("/shogiboardq/automation.sock");
#endif
}

QString AutomationServer::endpointFilePath()
{
    return QFileInfo(SettingsCommon::settingsFilePath()).dir().filePath(kEndpointFileName);
}

bool AutomationServer::listen(const QString& socketPath, QString* error)
{
    close();
    const QString path = socketPath.isEmpty() ? defaultSocketPath() : socketPath;
#ifndef Q_OS_WIN
    // Unix ドメインソケットのパス長には上限（sun_path、Linux で 108 バイト）がある
    constexpr int kMaxSocketPathBytes = 100;
    if (path.toUtf8().size() > kMaxSocketPathBytes) {
        if (error) {
            *error = QStringLiteral("Socket path is too long (%1 bytes, max %2): %3. Pass a shorter --automation-socket")
                         .arg(path.toUtf8().size()).arg(kMaxSocketPathBytes).arg(path);
        }
        return false;
    }
    // ソケットの親ディレクトリを所有者専用で用意し、前回の残骸を消す
    const QDir dir = QFileInfo(path).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Cannot create directory %1").arg(dir.path());
        return false;
    }
    QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif
    QLocalServer::removeServer(path);
    if (!m_server->listen(path)) {
        if (error) *error = QStringLiteral("Cannot listen on %1: %2").arg(path, m_server->errorString());
        return false;
    }
    m_socketPath = path;
    writeEndpointFile();
    qCInfo(lcApp) << "automation API listening on" << path;
    return true;
}

void AutomationServer::close()
{
    if (!m_server->isListening()) return;
    m_server->close();
    removeEndpointFile();
    if (!m_socketPath.isEmpty()) QLocalServer::removeServer(m_socketPath);
}

bool AutomationServer::isListening() const
{
    return m_server->isListening();
}

void AutomationServer::writeEndpointFile()
{
    QJsonObject info;
    info[QStringLiteral("socket")] = m_socketPath;
    info[QStringLiteral("pid")] = static_cast<double>(QCoreApplication::applicationPid());
    info[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    QFile file(endpointFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcApp) << "automation: cannot write endpoint file" << file.fileName();
        return;
    }
    file.write(QJsonDocument(info).toJson(QJsonDocument::Compact));
    file.close();
    QFile::setPermissions(file.fileName(), QFile::ReadOwner | QFile::WriteOwner);
}

void AutomationServer::removeEndpointFile()
{
    QFile::remove(endpointFilePath());
}

void AutomationServer::onNewConnection()
{
    while (QLocalSocket* socket = m_server->nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QLocalSocket::readyRead, this, &AutomationServer::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &AutomationServer::onDisconnected);
        qCDebug(lcApp) << "automation: client connected";
    }
}

void AutomationServer::onReadyRead()
{
    // ハンドラがモーダルダイアログを開くと、入れ子のイベントループ中にクライアントが切断し、
    // onDisconnected() でバッファが消えてソケットも削除され得る。ソケットは QPointer で生存を確認し、
    // バッファは参照を保持せず毎回引き直す。
    const QPointer<QLocalSocket> socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;
    auto entry = m_buffers.find(socket.data());
    if (entry == m_buffers.end()) return;
    entry->append(socket->readAll());
    if (entry->size() > kMaxLineBytes) {
        const QJsonObject error = AutomationDispatcher::makeError(
            QJsonValue::Null, AutomationErrorCode::InvalidRequest,
            QStringLiteral("Message exceeds %1 bytes").arg(kMaxLineBytes));
        entry->clear();
        socket->write(QJsonDocument(error).toJson(QJsonDocument::Compact) + '\n');
        socket->disconnectFromServer();
        return;
    }
    // 入れ子のイベントループ中に切断されたとき、ここで deleteLater() すると入れ子のループで削除され、
    // この readyRead を通知している Qt 側のコードが削除済みのソケットに触れてしまう。
    // 処理中は削除を保留し、処理が戻ってから削除する。
    QLocalSocket* const rawSocket = socket.data();
    m_handlingSockets.insert(rawSocket);
    while (socket) {
        entry = m_buffers.find(socket.data());
        if (entry == m_buffers.end()) break;
        const qsizetype newline = entry->indexOf('\n');
        if (newline < 0) break;
        const QByteArray line = entry->left(newline);
        entry->remove(0, newline + 1);
        const QByteArray response = m_dispatcher.handleLine(line);
        if (response.isEmpty() || !socket || socket->state() != QLocalSocket::ConnectedState) continue;
        socket->write(response);
        socket->flush();
    }
    m_handlingSockets.remove(rawSocket);
    if (m_pendingDeletion.remove(rawSocket) && socket) {
        socket->deleteLater();
    }
}

void AutomationServer::onDisconnected()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;
    m_buffers.remove(socket);
    if (m_handlingSockets.contains(socket)) {
        m_pendingDeletion.insert(socket);
    } else {
        socket->deleteLater();
    }
    qCDebug(lcApp) << "automation: client disconnected";
}
