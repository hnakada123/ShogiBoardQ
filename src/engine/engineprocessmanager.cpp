/**
 * @file engineprocessmanager.cpp
 * @brief 将棋エンジンプロセス管理クラスの実装
 */

#include "engineprocessmanager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

EngineProcessManager::EngineProcessManager(QObject* parent)
    : QObject(parent)
{
}

EngineProcessManager::~EngineProcessManager()
{
    stopProcess();
}

// === プロセス管理 ===

bool EngineProcessManager::startProcess(const QString& engineFile)
{
    // ファイル存在チェック
    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        emit processError(QProcess::FailedToStart,
                          tr("Engine file does not exist: %1").arg(engineFile));
        return false;
    }

    // 既存プロセスがあれば終了
    if (m_process) {
        stopProcess();
    }

    // 新規プロセス作成
    m_process = new QProcess(this);
    
    // エンジンファイルのディレクトリを作業ディレクトリに設定
    // （エンジンが相対パスで定跡ファイルや評価関数ファイルを読み込むため）
    QFileInfo engineFileInfo(engineFile);
    m_process->setWorkingDirectory(engineFileInfo.absolutePath());
    
    // シグナル接続
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &EngineProcessManager::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &EngineProcessManager::onReadyReadStderr);
    connect(m_process, &QProcess::errorOccurred,
            this, &EngineProcessManager::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EngineProcessManager::onProcessFinished);

    // プロセス起動
    m_process->start(engineFile, QStringList(), QIODevice::ReadWrite);
    
    if (!m_process->waitForStarted()) {
        const QString errorMsg = tr("Failed to start engine: %1").arg(engineFile);
        emit processError(QProcess::FailedToStart, errorMsg);
        
        delete m_process;
        m_process = nullptr;
        return false;
    }

    m_shutdownState = ShutdownState::Running;
    m_currentEnginePath = engineFile;  // エンジンパスを記録
    return true;
}

void EngineProcessManager::stopProcess()
{
    if (!m_process) return;

    if (m_process->state() == QProcess::Running) {
        // quitコマンドは上位クラスから送信済みと想定
        if (!m_process->waitForFinished(3000)) {
            m_process->terminate();
            if (!m_process->waitForFinished(1000)) {
                m_process->kill();
                m_process->waitForFinished(-1);
            }
        }
    }

    disconnect(m_process, nullptr, this, nullptr);
    delete m_process;
    m_process = nullptr;

    m_shutdownState = ShutdownState::Running;
    m_postQuitInfoStringLinesLeft = 0;
    m_currentEnginePath.clear();  // エンジンパスをクリア
}

bool EngineProcessManager::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

QProcess::ProcessState EngineProcessManager::state() const
{
    return m_process ? m_process->state() : QProcess::NotRunning;
}

// === コマンド送信 ===

void EngineProcessManager::sendCommand(const QString& command)
{
    if (!m_process || 
        (m_process->state() != QProcess::Running && 
         m_process->state() != QProcess::Starting)) {
        qWarning() << "[EngineProcessManager] Process not ready; command ignored:" << command;
        return;
    }

    m_process->write((command + "\n").toUtf8());
    
    emit commandSent(command);
    qDebug().nospace() << logPrefix() << " > " << command;
}

void EngineProcessManager::closeWriteChannel()
{
    if (m_process) {
        m_process->closeWriteChannel();
    }
}

// === 状態管理 ===

void EngineProcessManager::setShutdownState(ShutdownState state)
{
    m_shutdownState = state;
}

void EngineProcessManager::setPostQuitInfoStringLinesLeft(int count)
{
    m_postQuitInfoStringLinesLeft = count;
}

void EngineProcessManager::decrementPostQuitLines()
{
    if (m_postQuitInfoStringLinesLeft > 0) {
        --m_postQuitInfoStringLinesLeft;
        if (m_postQuitInfoStringLinesLeft <= 0) {
            m_shutdownState = ShutdownState::IgnoreAll;
        }
    }
}

// === バッファ管理 ===

void EngineProcessManager::discardStdout()
{
    if (m_process) {
        (void)m_process->readAllStandardOutput();
    }
}

void EngineProcessManager::discardStderr()
{
    if (m_process) {
        (void)m_process->readAllStandardError();
    }
}

bool EngineProcessManager::canReadLine() const
{
    return m_process && m_process->canReadLine();
}

QByteArray EngineProcessManager::readLine()
{
    return m_process ? m_process->readLine() : QByteArray();
}

QByteArray EngineProcessManager::readAllStderr()
{
    return m_process ? m_process->readAllStandardError() : QByteArray();
}

// === ログ識別 ===

void EngineProcessManager::setLogIdentity(const QString& engineTag, 
                                          const QString& sideTag,
                                          const QString& engineName)
{
    m_logEngineTag = engineTag;
    m_logSideTag = sideTag;
    m_logEngineName = engineName;
}

QString EngineProcessManager::logPrefix() const
{
    QString within = m_logEngineTag.isEmpty() ? "[E?]" : m_logEngineTag;
    if (!m_logSideTag.isEmpty()) {
        within.insert(within.size() - 1, "/" + m_logSideTag);
    }

    QString head = within;
    if (!m_logEngineName.isEmpty()) {
        head += " " + m_logEngineName;
    }

    return head;
}

// === プライベートスロット ===

void EngineProcessManager::onReadyReadStdout()
{
    if (!m_process) return;

    m_processedLines = 0;

    while (m_process && m_process->canReadLine() && m_processedLines < kMaxLinesPerChunk) {
        const QByteArray data = m_process->readLine();
        QString line = QString::fromUtf8(data).trimmed();
        
        if (line.isEmpty()) {
            ++m_processedLines;
            continue;
        }

        // エンジン名の検出
        if (line.startsWith(QStringLiteral("id name "))) {
            const QString name = line.mid(8).trimmed();
            if (!name.isEmpty() && m_logEngineName.isEmpty()) {
                m_logEngineName = name;
                emit engineNameDetected(name);
            }
        }

        // シャットダウン状態による遮断
        if (m_shutdownState == ShutdownState::IgnoreAll) {
            qDebug().nospace() << logPrefix() << " [drop-ignore-all] " << line;
            ++m_processedLines;
            continue;
        }

        if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
            if (line.startsWith(QStringLiteral("info string")) &&
                m_postQuitInfoStringLinesLeft > 0) {
                emit dataReceived(line);
                decrementPostQuitLines();
            } else {
                qDebug().nospace() << logPrefix() << " [drop-after-quit] " << line;
            }
            ++m_processedLines;
            continue;
        }

        // 通常時：データ受信シグナルを発行
        emit dataReceived(line);
        qDebug().nospace() << logPrefix() << " < " << line;
        
        ++m_processedLines;
    }

    // まだ読み残しがある場合は継続
    scheduleMoreReading();
}

void EngineProcessManager::onReadyReadStderr()
{
    if (!m_process) return;

    if (m_shutdownState == ShutdownState::IgnoreAll) {
        discardStderr();
        return;
    }

    const QByteArray data = m_process->readAllStandardError();
    if (data.isEmpty()) return;

    const QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

    for (const QString& l : lines) {
        const QString line = l.trimmed();
        if (line.isEmpty()) continue;

        if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
            if (m_postQuitInfoStringLinesLeft > 0 &&
                line.startsWith(QStringLiteral("info string"))) {
                emit stderrReceived(line);
                decrementPostQuitLines();
            }
            continue;
        }

        emit stderrReceived(line);
        qDebug().nospace() << logPrefix() << " <stderr> " << line;
    }
}

void EngineProcessManager::onProcessError(QProcess::ProcessError error)
{
    QString errorMessage;

    switch (error) {
    case QProcess::FailedToStart:
        errorMessage = tr("The process failed to start.");
        break;
    case QProcess::Crashed:
        errorMessage = tr("The process crashed.");
        break;
    case QProcess::Timedout:
        errorMessage = tr("The process timed out.");
        break;
    case QProcess::WriteError:
        errorMessage = tr("An error occurred while writing data.");
        break;
    case QProcess::ReadError:
        errorMessage = tr("An error occurred while reading data.");
        break;
    case QProcess::UnknownError:
    default:
        errorMessage = tr("An unknown error occurred.");
        break;
    }

    emit processError(error, errorMessage);
}

void EngineProcessManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    // 残りのデータを読み取り
    if (m_process) {
        const QByteArray outTail = m_process->readAllStandardOutput();
        const QByteArray errTail = m_process->readAllStandardError();

        // 残りのデータをシグナルで通知（必要に応じて）
        if (!outTail.isEmpty()) {
            const QStringList lines = QString::fromUtf8(outTail).split('\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                const QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() && 
                    m_shutdownState != ShutdownState::IgnoreAll) {
                    emit dataReceived(trimmed);
                }
            }
        }
    }

    m_shutdownState = ShutdownState::IgnoreAll;
    emit processFinished(exitCode, status);
}

void EngineProcessManager::scheduleMoreReading()
{
    if (m_process && m_process->canReadLine()) {
        QTimer::singleShot(0, this, &EngineProcessManager::onReadyReadStdout);
    }
}
