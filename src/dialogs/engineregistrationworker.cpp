/// @file engineregistrationworker.cpp
/// @brief エンジン登録ワーカークラスの実装（ワーカースレッドでのQProcess通信）

#include "engineregistrationworker.h"
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

namespace {
constexpr int kWaitSliceMs = 100;

void drainStderr(QProcess& process)
{
    (void)process.readAllStandardError();
}

bool isCancellationRequested(const CancelFlag& cancelFlag)
{
    return (cancelFlag && cancelFlag->load()) || QThread::currentThread()->isInterruptionRequested();
}

bool waitForStartedInterruptibly(QProcess& process, int timeoutMs, const CancelFlag& cancelFlag)
{
    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        if (process.state() == QProcess::Running) {
            return true;
        }
        if (isCancellationRequested(cancelFlag)) {
            return false;
        }

        const qint64 remainingMs = deadline.remainingTime();
        const int sliceMs = (remainingMs > 0)
                                ? qMax(1, qMin(kWaitSliceMs, static_cast<int>(remainingMs)))
                                : 1;
        if (process.waitForStarted(sliceMs)) {
            return true;
        }
    }

    return process.state() == QProcess::Running;
}

void stopProcessQuickly(QProcess& process)
{
    process.kill();
    (void)process.waitForFinished(kWaitSliceMs);
    drainStderr(process);
}
} // namespace

EngineRegistrationWorker::EngineRegistrationWorker(QObject* parent)
    : QObject(parent)
{
}

void EngineRegistrationWorker::setCancelFlag(CancelFlag flag)
{
    m_cancelFlag = std::move(flag);
}

void EngineRegistrationWorker::registerEngine(const QString& enginePath)
{
    const QFileInfo engineInfo(enginePath);
    if (!engineInfo.exists() || !engineInfo.isFile()) {
        emit registrationFailed(tr("エンジンファイルが見つかりません: %1").arg(enginePath));
        return;
    }

    QProcess process;
    process.setWorkingDirectory(engineInfo.absolutePath());
    process.start(engineInfo.absoluteFilePath(), QStringList(), QIODevice::ReadWrite);
    drainStderr(process);

    emit progressUpdated(tr("エンジンを起動中..."));

    if (!waitForStartedInterruptibly(process, StartTimeoutMs, m_cancelFlag)) {
        if (isCancellationRequested(m_cancelFlag)) {
            stopProcessQuickly(process);
            emit registrationCanceled();
            return;
        }
        drainStderr(process);
        emit registrationFailed(tr("エンジンの起動に失敗しました: %1").arg(enginePath));
        return;
    }

    emit progressUpdated(tr("USI通信中..."));
    process.write("usi\n");

    QString engineName;
    QString engineAuthor;
    QStringList optionLines;

    QDeadlineTimer deadline(UsiOkTimeoutMs);

    while (!deadline.hasExpired()) {
        drainStderr(process);

        if (isCancellationRequested(m_cancelFlag)) {
            stopProcessQuickly(process);
            emit registrationCanceled();
            return;
        }

        const qint64 remainingMs = deadline.remainingTime();
        const int waitMs = (remainingMs > 0)
                               ? qMax(1, qMin(ReadIntervalMs, static_cast<int>(remainingMs)))
                               : 1;
        if (!process.waitForReadyRead(waitMs)) {
            drainStderr(process);
            continue;
        }

        while (process.canReadLine()) {
            QString line = QString::fromUtf8(process.readLine()).trimmed();

            if (line.startsWith("id name ")) {
                engineName = line.mid(8);
            } else if (line.startsWith("id author ")) {
                engineAuthor = line.mid(10);
            } else if (line.startsWith("option name")) {
                optionLines.append(line);
            } else if (line.startsWith("usiok")) {
                process.write("quit\n");
                (void)process.waitForFinished(QuitTimeoutMs);
                drainStderr(process);
                if (engineName.trimmed().isEmpty()) {
                    emit registrationFailed(tr("エンジン名(id name)を取得できませんでした"));
                    return;
                }
                emit engineRegistered(engineName, engineAuthor, optionLines);
                return;
            }
        }

        drainStderr(process);
    }

    // タイムアウト
    stopProcessQuickly(process);
    emit registrationFailed(tr("エンジンからの応答がタイムアウトしました"));
}
