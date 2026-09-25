#ifndef CLISTOPWATCHER_H
#define CLISTOPWATCHER_H

/// @file clistopwatcher.h
/// @brief 長時間コマンドの停止要求（stdin の "stop"、SIGTERM/SIGINT）を監視する

#include <QByteArray>
#include <QObject>

class QSocketNotifier;

/**
 * @brief 停止要求をシグナルに変換する
 *
 * - stdin 監視（`watchStdin=true`）: 1 行 "stop" または EOF で `stopRequested()`
 * - Unix: SIGTERM / SIGINT を socketpair 経由でイベントループに届ける
 * - Windows: stdin 監視は読み取りスレッドで行う（未検証）
 */
class CliStopWatcher : public QObject
{
    Q_OBJECT

public:
    explicit CliStopWatcher(bool watchStdin, QObject* parent = nullptr);
    ~CliStopWatcher() override;

signals:
    void stopRequested();

private slots:
    void onStdinReadable();
    void onSignalReadable();
    void onStdinLine(const QString& line);
    void onStdinClosed();

private:
    void requestStop();

    QSocketNotifier* m_stdinNotifier = nullptr;
    QSocketNotifier* m_signalNotifier = nullptr;
    QByteArray m_stdinBuffer;
    bool m_emitted = false;
};

#endif // CLISTOPWATCHER_H
