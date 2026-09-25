/// @file clistopwatcher.cpp
/// @brief 長時間コマンドの停止要求監視の実装

#include "clistopwatcher.h"

#include <QSocketNotifier>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <QThread>
#include <iostream>
#include <string>
#endif

namespace {

#ifdef Q_OS_UNIX
int g_signalFds[2] = {-1, -1};

void handleUnixSignal(int)
{
    const char byte = 1;
    const ssize_t written = ::write(g_signalFds[0], &byte, sizeof(byte));
    Q_UNUSED(written)
}
#else
/// Windows では stdin の非同期監視ができないため、読み取り専用スレッドで行単位に読む
class StdinReaderThread : public QThread
{
    Q_OBJECT
public:
    using QThread::QThread;
signals:
    void lineRead(const QString& line);
    void closed();
protected:
    void run() override
    {
        std::string line;
        while (std::getline(std::cin, line)) {
            emit lineRead(QString::fromStdString(line));
        }
        emit closed();
    }
};
#endif

} // namespace

CliStopWatcher::CliStopWatcher(bool watchStdin, QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_UNIX
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalFds) == 0) {
        m_signalNotifier = new QSocketNotifier(g_signalFds[1], QSocketNotifier::Read, this);
        connect(m_signalNotifier, &QSocketNotifier::activated, this, &CliStopWatcher::onSignalReadable);
        struct sigaction action {};
        action.sa_handler = handleUnixSignal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        ::sigaction(SIGTERM, &action, nullptr);
        ::sigaction(SIGINT, &action, nullptr);
    }
    if (watchStdin) {
        m_stdinNotifier = new QSocketNotifier(0, QSocketNotifier::Read, this);
        connect(m_stdinNotifier, &QSocketNotifier::activated, this, &CliStopWatcher::onStdinReadable);
    }
#else
    if (watchStdin) {
        // スレッドは終了時に join しない（getline でブロックしたまま）。プロセス終了で消える
        auto* reader = new StdinReaderThread();
        connect(reader, &StdinReaderThread::lineRead, this, &CliStopWatcher::onStdinLine, Qt::QueuedConnection);
        connect(reader, &StdinReaderThread::closed, this, &CliStopWatcher::onStdinClosed, Qt::QueuedConnection);
        reader->start();
    }
#endif
}

CliStopWatcher::~CliStopWatcher() = default;

void CliStopWatcher::onStdinReadable()
{
#ifdef Q_OS_UNIX
    char buffer[256];
    const ssize_t n = ::read(0, buffer, sizeof(buffer));
    if (n <= 0) {
        if (m_stdinNotifier) m_stdinNotifier->setEnabled(false);
        onStdinClosed();
        return;
    }
    m_stdinBuffer.append(buffer, static_cast<qsizetype>(n));
    qsizetype newline = -1;
    while ((newline = m_stdinBuffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_stdinBuffer.left(newline)).trimmed();
        m_stdinBuffer.remove(0, newline + 1);
        onStdinLine(line);
    }
#endif
}

void CliStopWatcher::onSignalReadable()
{
#ifdef Q_OS_UNIX
    char byte = 0;
    const ssize_t n = ::read(g_signalFds[1], &byte, sizeof(byte));
    Q_UNUSED(n)
#endif
    requestStop();
}

void CliStopWatcher::onStdinLine(const QString& line)
{
    if (line.compare(QStringLiteral("stop"), Qt::CaseInsensitive) == 0) requestStop();
}

void CliStopWatcher::onStdinClosed()
{
    // 親プロセスが終了して stdin が閉じた場合もエンジンを残さず止める
    requestStop();
}

void CliStopWatcher::requestStop()
{
    if (m_emitted) return;
    m_emitted = true;
    emit stopRequested();
}

#ifndef Q_OS_UNIX
#include "clistopwatcher.moc"
#endif
