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

#ifndef Q_OS_WIN
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr QLatin1StringView kEndpointFileName("automation-endpoint.json");

#ifndef Q_OS_WIN
/// ソケットのフォルダを用意する。無ければ所有者専用で作り、あれば自分の持ち物でほかの利用者が書けないことを確かめる
/// （既にあるフォルダの権限は変えない。別の利用者が先に作ったフォルダでは、待ち受けを差し替えられる。セキュリティ監査 S10）
bool preparePrivateDirectory(const QString& dirPath, QString* error)
{
    const QByteArray native = QFile::encodeName(dirPath);
    struct stat info {};
    if (::lstat(native.constData(), &info) != 0) {
        if (!QDir().mkpath(dirPath)
            || !QFile::setPermissions(dirPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
            if (error) *error = QStringLiteral("Cannot create directory %1").arg(dirPath);
            return false;
        }
        return true;
    }
    if (!S_ISDIR(info.st_mode) || info.st_uid != ::geteuid() || (info.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        if (error) {
            *error = QStringLiteral("The socket directory %1 must be a directory owned by you that others cannot write to")
                         .arg(dirPath);
        }
        return false;
    }
    return true;
}

/// 前回の残骸のソケットを消す。ソケットでないもの・待ち受け中のソケットは消さない
bool removeStaleSocket(const QString& path, QString* error)
{
    struct stat info {};
    if (::lstat(QFile::encodeName(path).constData(), &info) != 0) return true;
    if (!S_ISSOCK(info.st_mode)) {
        if (error) *error = QStringLiteral("%1 exists and is not a socket").arg(path);
        return false;
    }
    QLocalSocket probe;
    probe.connectToServer(path);
    if (probe.waitForConnected(200)) {
        probe.disconnectFromServer();
        if (error) *error = QStringLiteral("Another ShogiBoardQ is already listening on %1").arg(path);
        return false;
    }
    return QLocalServer::removeServer(path);
}
#endif
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
    // RuntimeLocation（XDG_RUNTIME_DIR など）は所有者だけが使える。使えなければ設定のフォルダに置き、
    // 共有の一時ディレクトリの固定名は使わない（別の利用者に先に作られると待ち受けを奪われる。セキュリティ監査 S10）
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) return QFileInfo(endpointFilePath()).dir().filePath(QStringLiteral("automation.sock"));
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
    // 相対パスは Qt が一時ディレクトリの下として扱うので、絶対パスに限る
    if (!QDir::isAbsolutePath(path)) {
        if (error) *error = QStringLiteral("Socket path must be absolute: %1").arg(path);
        return false;
    }
    // Unix ドメインソケットのパス長には上限（sun_path。終端を含めて Linux は 108、macOS は 104 バイト）がある。
    // Qt は同じフォルダに一時フォルダ（"/.XXXXXX" の8バイト）を作ってその中で bind するので、その分も引く
#ifdef Q_OS_MACOS
    constexpr int kMaxSocketPathBytes = 95;
#else
    constexpr int kMaxSocketPathBytes = 99;
#endif
    if (path.toUtf8().size() > kMaxSocketPathBytes) {
        if (error) {
            *error = QStringLiteral("Socket path is too long (%1 bytes, max %2): %3. Pass a shorter --automation-socket")
                         .arg(path.toUtf8().size()).arg(kMaxSocketPathBytes).arg(path);
        }
        return false;
    }
    if (!preparePrivateDirectory(QFileInfo(path).absolutePath(), error) || !removeStaleSocket(path, error)) return false;
#endif
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
    // 後から起動したほかのアプリの情報は消さない
    QFile file(endpointFilePath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject info = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    if (info.value(QStringLiteral("pid")).toInteger() != QCoreApplication::applicationPid()) return;
    QFile::remove(file.fileName());
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
    if (entry->size() > kMaxMessageBytes) {
        const QJsonObject error = AutomationDispatcher::makeError(
            QJsonValue::Null, ErrorKind::InvalidRequest,
            QStringLiteral("Message exceeds %1 bytes").arg(kMaxMessageBytes));
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
