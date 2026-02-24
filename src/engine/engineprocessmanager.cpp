/// @file engineprocessmanager.cpp
/// @brief 将棋エンジンプロセス管理クラスの実装

#include "engineprocessmanager.h"
#include "usi.h"
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

// ============================================================
// プロセス管理
// ============================================================

bool EngineProcessManager::startProcess(const QString& engineFile)
{
    // 処理フロー:
    // 1. ファイル存在チェック
    // 2. 既存プロセスの終了
    // 3. QProcess作成・作業ディレクトリ設定・シグナル接続
    // 4. プロセス起動・起動確認

    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        emit processError(QProcess::FailedToStart,
                          tr("Engine file does not exist: %1").arg(engineFile));
        return false;
    }

    if (m_process) {
        stopProcess();
    }

    m_process = new QProcess(this);

    // エンジンが相対パスで定跡ファイルや評価関数ファイルを読み込むため
    QFileInfo engineFileInfo(engineFile);
    m_process->setWorkingDirectory(engineFileInfo.absolutePath());

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &EngineProcessManager::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &EngineProcessManager::onReadyReadStderr);
    connect(m_process, &QProcess::errorOccurred,
            this, &EngineProcessManager::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EngineProcessManager::onProcessFinished);

    m_process->start(engineFile, QStringList(), QIODevice::ReadWrite);

    if (!m_process->waitForStarted()) {
        const QString errorMsg = tr("Failed to start engine: %1").arg(engineFile);
        emit processError(QProcess::FailedToStart, errorMsg);

        delete m_process;
        m_process = nullptr;
        return false;
    }

    m_shutdownState = ShutdownState::Running;
    m_currentEnginePath = engineFile;
    return true;
}

void EngineProcessManager::stopProcess()
{
    if (!m_process) return;

    // waitForFinished() 中のシグナル配信を防ぐため、先にシグナルを切断
    disconnect(m_process, nullptr, this, nullptr);

    if (m_process->state() == QProcess::Running) {
        // quitコマンドは上位クラス（USIProtocolHandler）から送信済みと想定
        if (!m_process->waitForFinished(3000)) {
            m_process->terminate();
            if (!m_process->waitForFinished(1000)) {
                m_process->kill();
                m_process->waitForFinished(-1);
            }
        }
    }

    delete m_process;
    m_process = nullptr;

    m_shutdownState = ShutdownState::Running;
    m_postQuitInfoStringLinesLeft = 0;
    m_currentEnginePath.clear();
}

bool EngineProcessManager::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

QProcess::ProcessState EngineProcessManager::state() const
{
    return m_process ? m_process->state() : QProcess::NotRunning;
}

// ============================================================
// コマンド送信
// ============================================================

void EngineProcessManager::sendCommand(const QString& command)
{
    if (!m_process || 
        (m_process->state() != QProcess::Running && 
         m_process->state() != QProcess::Starting)) {
        qCWarning(lcEngine) << logPrefix() << "プロセス未準備、コマンド無視:" << command;
        return;
    }

    if (m_process->write((command + "\n").toUtf8()) == -1) {
        qCWarning(lcEngine) << logPrefix() << "コマンド送信失敗:" << command;
        return;
    }

    emit commandSent(command);
    qCDebug(lcEngine).nospace() << logPrefix() << " 送信: " << command;
}

void EngineProcessManager::closeWriteChannel()
{
    if (m_process) {
        m_process->closeWriteChannel();
    }
}

// ============================================================
// 状態管理
// ============================================================

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

// ============================================================
// バッファ管理
// ============================================================

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

// ============================================================
// ログ識別
// ============================================================

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

// ============================================================
// プライベートスロット
// ============================================================

void EngineProcessManager::onReadyReadStdout()
{
    // 処理フロー:
    // 1. チャンク上限まで行単位で読み取り
    // 2. "id name"応答からエンジン名を検出
    // 3. シャットダウン状態に応じてフィルタリング
    // 4. 読み残しがあればイベントループ経由で再スケジュール

    if (!m_process) return;

    // シグナル受信側が所有者チェーンを破壊する場合に備えたガード。
    // 例: dataReceived → Usi::checkmateSolved → TsumeshogiGenerator::cleanup()
    //     → m_usi.reset() で EngineProcessManager が削除されるケース。
    QPointer<EngineProcessManager> guard(this);

    m_processedLines = 0;

    while (m_process && m_process->canReadLine() && m_processedLines < kMaxLinesPerChunk) {
        const QByteArray data = m_process->readLine();
        QString line = QString::fromUtf8(data).trimmed();

        if (line.isEmpty()) {
            ++m_processedLines;
            continue;
        }

        if (line.startsWith(QStringLiteral("id name "))) {
            const QString name = line.mid(8).trimmed();
            if (!name.isEmpty() && m_logEngineName.isEmpty()) {
                m_logEngineName = name;
                if (!guard) return;
            }
        }

        if (m_shutdownState == ShutdownState::IgnoreAll) {
            qCDebug(lcEngine).nospace() << logPrefix() << " [drop-ignore-all] " << line;
            ++m_processedLines;
            continue;
        }

        if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
            if (line.startsWith(QStringLiteral("info string")) &&
                m_postQuitInfoStringLinesLeft > 0) {
                emit dataReceived(line);
                if (!guard) return;
                decrementPostQuitLines();
            } else {
                qCDebug(lcEngine).nospace() << logPrefix() << " [drop-after-quit] " << line;
            }
            ++m_processedLines;
            continue;
        }

        emit dataReceived(line);
        if (!guard) return;
        qCDebug(lcEngine).nospace() << logPrefix() << " 受信: " << line;

        ++m_processedLines;
    }

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
        qCDebug(lcEngine).nospace() << logPrefix() << " stderr: " << line;
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
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    // プロセス終了時にバッファに残ったデータを最後に処理する
    if (m_process) {
        QPointer<EngineProcessManager> guard(this);
        const QByteArray outTail = m_process->readAllStandardOutput();
        m_process->readAllStandardError();

        if (!outTail.isEmpty()) {
            const QStringList lines = QString::fromUtf8(outTail).split('\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                const QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() &&
                    m_shutdownState != ShutdownState::IgnoreAll) {
                    emit dataReceived(trimmed);
                    if (!guard) return;
                }
            }
        }
    }

    m_shutdownState = ShutdownState::IgnoreAll;
}

void EngineProcessManager::scheduleMoreReading()
{
    if (m_process && m_process->canReadLine()) {
        QTimer::singleShot(0, this, &EngineProcessManager::onReadyReadStdout);
    }
}
