/// @file engineregistrationworker.cpp
/// @brief エンジン登録ワーカークラスの実装（ワーカースレッドでのQProcess通信）

#include "engineregistrationworker.h"
#include <QDeadlineTimer>
#include <QProcess>

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
    QProcess process;
    process.start(enginePath, QStringList(), QIODevice::ReadWrite);

    emit progressUpdated(tr("エンジンを起動中..."));

    if (!process.waitForStarted(StartTimeoutMs)) {
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
        if (m_cancelFlag && m_cancelFlag->load()) {
            process.kill();
            process.waitForFinished(QuitTimeoutMs);
            return;
        }

        if (!process.waitForReadyRead(ReadIntervalMs)) {
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
                emit engineRegistered(engineName, engineAuthor, optionLines);
                return;
            }
        }
    }

    // タイムアウト
    process.kill();
    process.waitForFinished(QuitTimeoutMs);
    emit registrationFailed(tr("エンジンからの応答がタイムアウトしました"));
}
