/// @file engineregistrationworker.cpp
/// @brief エンジン登録ワーカークラスの実装（ワーカースレッドでのQProcess通信）

#include "engineregistrationworker.h"
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QProcess>

namespace {
void drainStderr(QProcess& process)
{
    (void)process.readAllStandardError();
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

    if (!process.waitForStarted(StartTimeoutMs)) {
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

        if (m_cancelFlag && m_cancelFlag->load()) {
            process.kill();
            process.waitForFinished(QuitTimeoutMs);
            drainStderr(process);
            emit registrationCanceled();
            return;
        }

        if (!process.waitForReadyRead(ReadIntervalMs)) {
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
                process.waitForFinished(QuitTimeoutMs);
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
    process.kill();
    process.waitForFinished(QuitTimeoutMs);
    drainStderr(process);
    emit registrationFailed(tr("エンジンからの応答がタイムアウトしました"));
}
