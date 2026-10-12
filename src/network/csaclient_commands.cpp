/// @file csaclient_commands.cpp
/// @brief CSAプロトコルクライアント - 意思表示・生コマンド・接続の維持（キープアライブ、ソケット設定）

#include <QtGlobal>
#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "csaclient.h"
#include "logcategories.h"

// ============================================================
// 手番に縛られる送信（指し手・意思表示・生コマンド）
// ============================================================

bool CsaClient::ensureCanSendOnMyTurn(const QString& what)
{
    if (!m_isMyTurn) {
        emit commandRejected(tr("自分の手番ではないため送信しません: %1").arg(what));
        return false;
    }
    if (m_awaitingMoveConfirmation) {
        emit commandRejected(tr("直前の送信に対するサーバーの確認を待っているため送信しません: %1").arg(what));
        return false;
    }
    return true;
}

bool CsaClient::isTurnBoundCommand(const QString& command)
{
    if (command.startsWith(QLatin1Char('+')) || command.startsWith(QLatin1Char('-'))) {
        return true;
    }
    // %TORYO などの意思表示。shogi-server の拡張コマンド（%%）は手番に関係なく送れる
    return command.startsWith(QLatin1Char('%')) && !command.startsWith(QStringLiteral("%%"));
}

void CsaClient::sendRawCommand(const QString& command)
{
    if (command.isEmpty()) {
        // LF のみのキープアライブ（仕様 3.4）。対局中に限り、直前の送信から30秒以上空ける
        if (m_connectionState != ConnectionState::InGame) {
            emit commandRejected(tr("キープアライブ（空行）は対局中にのみ送信できます"));
            return;
        }
        if (m_lastSendTimer.isValid() && m_lastSendTimer.elapsed() < kKeepAliveMinIntervalMs) {
            emit commandRejected(tr("キープアライブは直前の送信から30秒以上空ける必要があります"));
            return;
        }
        writeLine(QString());
        return;
    }

    if (m_connectionState == ConnectionState::InGame && isTurnBoundCommand(command)) {
        if (!ensureCanSendOnMyTurn(command)) {
            return;
        }
        if (sendMessage(command)) {
            m_awaitingMoveConfirmation = true;
        }
        return;
    }

    sendMessage(command);
}

void CsaClient::resign()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }
    if (!ensureCanSendOnMyTurn(QStringLiteral("%TORYO"))) {
        return;
    }

    if (sendMessage(QStringLiteral("%TORYO"))) {
        m_awaitingMoveConfirmation = true;
    }
}

void CsaClient::declareWin()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }
    if (!ensureCanSendOnMyTurn(QStringLiteral("%KACHI"))) {
        return;
    }

    if (sendMessage(QStringLiteral("%KACHI"))) {
        m_awaitingMoveConfirmation = true;
    }
}

void CsaClient::requestChudan()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }
    if (!ensureCanSendOnMyTurn(QStringLiteral("%CHUDAN"))) {
        return;
    }

    if (sendMessage(QStringLiteral("%CHUDAN"))) {
        m_awaitingMoveConfirmation = true;
    }
}

// ============================================================
// 接続の維持
// ============================================================

void CsaClient::setKeepAliveEnabled(bool enabled)
{
    m_keepAliveEnabled = enabled;
    if (!enabled) {
        m_keepAliveTimer->stop();
    } else if (m_connectionState == ConnectionState::InGame) {
        m_keepAliveTimer->start();
    }
}

void CsaClient::setKeepAliveIntervalMs(int intervalMs)
{
    // 仕様 3.4: サーバーが受信してから30秒を経ずに送ってはならない
    m_keepAliveTimer->setInterval(qMax(kKeepAliveMinIntervalMs, intervalMs));
}

void CsaClient::applySocketOptions()
{
    // 小さな行をまとめて遅らせない（ShogiHome と同じく Nagle を無効化）
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    if (!m_tcpKeepAliveEnabled) {
        return;
    }
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    // Qt は SO_KEEPALIVE を立てるだけで間隔は OS の既定値（Linux 2時間など）のままなので、
    // NAT の無通信タイムアウトより短い間隔をソケットごとに設定する
    const qintptr descriptor = m_socket->socketDescriptor();
    if (descriptor < 0) {
        qCWarning(lcNetwork) << "TCP keep-alive: no socket descriptor";
        return;
    }
#if defined(Q_OS_WIN)
    tcp_keepalive values{};
    values.onoff = 1;
    values.keepalivetime = static_cast<ULONG>(kTcpKeepAliveIdleSec) * 1000UL;
    values.keepaliveinterval = static_cast<ULONG>(kTcpKeepAliveIntervalSec) * 1000UL;
    DWORD returned = 0;
    if (WSAIoctl(static_cast<SOCKET>(descriptor), SIO_KEEPALIVE_VALS, &values, sizeof(values),
                 nullptr, 0, &returned, nullptr, nullptr) != 0) {
        qCWarning(lcNetwork) << "TCP keep-alive: WSAIoctl failed:" << WSAGetLastError();
    }
#else
    const int fd = static_cast<int>(descriptor);
    const int idle = kTcpKeepAliveIdleSec;
    const int interval = kTcpKeepAliveIntervalSec;
    const int count = kTcpKeepAliveProbeCount;
#if defined(Q_OS_MACOS)
    const int idleOption = TCP_KEEPALIVE;
#else
    const int idleOption = TCP_KEEPIDLE;
#endif
    if (setsockopt(fd, IPPROTO_TCP, idleOption, &idle, sizeof(idle)) != 0
        || setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0
        || setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0) {
        qCWarning(lcNetwork) << "TCP keep-alive: setsockopt failed";
    }
#endif
    qCDebug(lcNetwork) << "TCP keep-alive enabled: idle" << kTcpKeepAliveIdleSec
                       << "s interval" << kTcpKeepAliveIntervalSec << "s";
}

void CsaClient::onKeepAliveTimeout()
{
    if (m_connectionState != ConnectionState::InGame || !isConnected()) {
        m_keepAliveTimer->stop();
        return;
    }
    qCDebug(lcNetwork) << "Sending keep-alive LF";
    writeLine(QString());
}
