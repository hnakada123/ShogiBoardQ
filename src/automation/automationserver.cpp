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
#include <QStandardPaths>

namespace {
constexpr qsizetype kMaxLineBytes = 8 * 1024 * 1024; ///< 1 メッセージの上限（棋譜テキストを含む）
const QString kEndpointFileName = QStringLiteral("automation-endpoint.json");
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
    const QString env = qEnvironmentVariable("SHOGIBOARDQ_AUTOMATION_SOCKET");
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
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;
    QByteArray& buffer = m_buffers[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > kMaxLineBytes) {
        const QJsonObject error = AutomationDispatcher::makeError(
            QJsonValue::Null, AutomationErrorCode::InvalidRequest,
            QStringLiteral("Message exceeds %1 bytes").arg(kMaxLineBytes));
        socket->write(QJsonDocument(error).toJson(QJsonDocument::Compact) + '\n');
        socket->disconnectFromServer();
        buffer.clear();
        return;
    }
    qsizetype newline = -1;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        const QByteArray response = m_dispatcher.handleLine(line);
        if (!response.isEmpty()) {
            socket->write(response);
            socket->flush();
        }
    }
}

void AutomationServer::onDisconnected()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;
    m_buffers.remove(socket);
    socket->deleteLater();
    qCDebug(lcApp) << "automation: client disconnected";
}
