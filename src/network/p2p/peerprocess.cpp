/// @file peerprocess.cpp
/// @brief 通信プロセスの起動・停止と非同期の IPC の実装

#include "peerprocess.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QPointer>
#include <QProcessEnvironment>
#include <QTimer>

#include "logcategories.h"
#include "peerconfinement.h"
#include "peeripc.h"

namespace {

constexpr qsizetype kMaxStderrBytes = 64 * 1024;
constexpr int kStopStepMs = 2000;
/// hello の応答が来るまでの上限（権限の制限は起動直後に掛けるので短い）
constexpr int kHelloTimeoutMs = 10000;

/// 世代は起動ごとに増やす（前のプロセスの残りの応答を捨てる）
quint64 nextGeneration()
{
    static quint64 generation = 0;
    return ++generation;
}

/// 権限の制限の結果として必須のもの（protocol/ipc.md 3章）
bool sandboxIsApplied(const QJsonObject& sandbox)
{
    if (!sandbox.value(QStringLiteral("applied")).toBool()) return false;
    const QJsonArray mechanisms = sandbox.value(QStringLiteral("mechanisms")).toArray();
    QStringList names;
    for (const QJsonValue& m : mechanisms) names << m.toString();
    // OS ごとの必須の仕組み（Linux と macOS は通信プロセスが自分で掛け、Windows は起動のときに掛けたものを通信プロセスが確かめる）
    const QStringList required = PeerConfinement::requiredMechanisms();
    if (required.isEmpty()) return false;
    for (const QString& name : required) {
        if (!names.contains(name)) return false;
    }
    return true;
}

/// 権限の制限に失敗したときの診断（標準エラーの最初の行）。印字できる ASCII だけを、短く残す。
/// 知らせの文に添えるので、アプリの文と読み違えるような任意の文を通さない
QString diagnosticLine(const QByteArray& stderrText)
{
    QString line;
    for (const char c : stderrText) {
        if (c == '\n' || line.size() >= 200) break;
        if (c >= 0x20 && c <= 0x7e) line.append(QLatin1Char(c));
    }
    return line.trimmed();
}

} // namespace

PeerProcess::PeerProcess(QObject* parent)
    : QObject(parent)
    , m_stopTimer(new QTimer(this))
{
    m_stopTimer->setSingleShot(true);
    connect(m_stopTimer, &QTimer::timeout, this, &PeerProcess::onStopTimeout);
}

PeerProcess::~PeerProcess()
{
    if (m_process) {
        // 親が終わると通信プロセスも終わる（標準入力の終端・PR_SET_PDEATHSIG・kqueue の見張り・ジョブ）。知らせを受けずに止める
        m_process->disconnect(this);
        m_process->kill();
    }
}

QString PeerProcess::executablePath()
{
#ifndef QT_NO_DEBUG
    // 開発用の差し替えはデバッグビルドだけ（実装仕様 10.4節）
    const QString overridePath = qEnvironmentVariable("SHOGIBOARDQ_PEER_EXECUTABLE");
    if (!overridePath.isEmpty()) return QFileInfo(overridePath).absoluteFilePath();
#endif
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("shogiboardq-peer.exe");
#else
    const QString name = QStringLiteral("shogiboardq-peer");
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

void PeerProcess::start(PeerConfinement::Reach reach)
{
    if (m_state != State::NotRunning) return;
    if (!PeerConfinement::isSupported()) {
        // 権限の制限を確かめた OS だけで使う
        m_failed = false;
        fail(Failure::UnsupportedOs, QString());
        emit stopped();
        return;
    }
    const QString path = executablePath();
    if (!QFileInfo(path).isExecutable()) {
        m_failed = false;
        fail(Failure::NotFound, path);
        emit stopped();
        return;
    }
    m_failed = false;
    m_terminateSent = false;
    m_buffer.clear();
    m_stderr.clear();
    m_pending.clear();
    m_sandbox = QJsonObject();
    m_generation = nextGeneration();
    m_nextRequestId = 1;

    m_process = new QProcess(this);
    m_process->setProgram(path);
    m_process->setArguments({});
    // 環境変数と標準入出力以外のハンドル・記述子を引き継がず、OS ごとの権限の制限を掛けて起動する
    QString confinementError;
    m_confinement = PeerConfinement::apply(m_process, path, reach, &confinementError);
    if (!m_confinement) {
        // 制限なしでは起動しない
        resetProcess();
        fail(Failure::SandboxFailed, confinementError);
        emit stopped();
        return;
    }
    connect(m_process, &QProcess::readyReadStandardOutput, this, &PeerProcess::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &PeerProcess::onReadyReadStderr);
    connect(m_process, &QProcess::finished, this, &PeerProcess::onFinished);
    connect(m_process, &QProcess::errorOccurred, this, &PeerProcess::onErrorOccurred);
    m_state = State::Starting;
    m_process->start(QIODevice::ReadWrite);

    QJsonObject hello;
    hello[QStringLiteral("uiVersion")] = QStringLiteral(APP_VERSION);
    m_helloRequestId = send(QStringLiteral("hello"), hello);
    m_stopTimer->start(kHelloTimeoutMs);
}

void PeerProcess::stop()
{
    if (!m_process || m_state == State::Stopping) return;
    if (m_state == State::Ready) send(QStringLiteral("shutdown"), QJsonObject());
    m_state = State::Stopping;
    m_process->closeWriteChannel();
    m_stopTimer->start(kStopStepMs);
}

void PeerProcess::waitForExit(int msecs)
{
    if (!m_process || m_process->state() == QProcess::NotRunning) return;
    QProcess* process = m_process;
    QElapsedTimer timer;
    timer.start();
    while (process->bytesToWrite() > 0 && timer.elapsed() < msecs) {
        if (!process->waitForBytesWritten(static_cast<int>(msecs - timer.elapsed()))) break;
    }
    // 終わると onFinished で m_process を外す（QProcess は deleteLater なので、ここで使い終えるまで残る）
    const qint64 left = msecs - timer.elapsed();
    if (left > 0 && process->state() != QProcess::NotRunning) process->waitForFinished(static_cast<int>(left));
}

void PeerProcess::onStopTimeout()
{
    if (!m_process) return;
    if (m_state == State::Starting) {
        fail(Failure::HelloMismatch, QStringLiteral("no hello response"));
        return;
    }
    if (!m_terminateSent) {
        m_terminateSent = true;
        m_process->terminate();
        m_stopTimer->start(kStopStepMs);
    } else {
        m_process->kill();
    }
}

quint64 PeerProcess::request(const QString& method, const QJsonObject& params)
{
    if (m_state != State::Ready) return 0;
    return send(method, params);
}

quint64 PeerProcess::send(const QString& method, const QJsonObject& params)
{
    if (!m_process) return 0;
    const quint64 id = m_nextRequestId++;
    QJsonObject message;
    message[QStringLiteral("protocolVersion")] = PeerIpc::kProtocolVersion;
    message[QStringLiteral("processGeneration")] = QString::number(m_generation);
    message[QStringLiteral("requestId")] = QString::number(id);
    message[QStringLiteral("method")] = method;
    message[QStringLiteral("params")] = params;
    const QByteArray bytes = PeerIpc::frame(message);
    if (bytes.size() - 4 > PeerIpc::kMaxMessageBytes) return 0;
    m_pending.insert(id, method);
    m_process->write(bytes);
    return id;
}

void PeerProcess::onReadyReadStdout()
{
    if (!m_process || m_failed) return;
    m_buffer.append(m_process->readAllStandardOutput());
    // 知らせの受け手がイベントループを回すと、その間にこのオブジェクトが消えることがある（受け手は待たない知らせを
    // 使うが、念のため）。消えた後と、違反で失敗した後は、残りを読まない
    const QPointer<PeerProcess> self(this);
    while (m_buffer.size() >= 4 && m_process && !m_failed) {
        const quint32 length = (static_cast<quint32>(static_cast<quint8>(m_buffer.at(0))) << 24)
            | (static_cast<quint32>(static_cast<quint8>(m_buffer.at(1))) << 16)
            | (static_cast<quint32>(static_cast<quint8>(m_buffer.at(2))) << 8)
            | static_cast<quint32>(static_cast<quint8>(m_buffer.at(3)));
        // 長さを確かめてから読む
        if (length < 2 || length > static_cast<quint32>(PeerIpc::kMaxMessageBytes)) {
            fail(Failure::ProtocolViolation, QStringLiteral("message length out of range"));
            return;
        }
        if (m_buffer.size() < static_cast<qsizetype>(length) + 4) break;
        const QByteArray json = m_buffer.mid(4, static_cast<qsizetype>(length));
        m_buffer.remove(0, static_cast<qsizetype>(length) + 4);
        handleMessage(json);
        if (!self) return;
    }
}

void PeerProcess::handleMessage(const QByteArray& json)
{
    if (m_failed) return;
    QString error;
    const std::optional<PeerIpc::Incoming> in = PeerIpc::parse(json, &error);
    if (!in) {
        fail(Failure::ProtocolViolation, error);
        return;
    }
    // 古いプロセスの残りは捨てる
    if (in->generation != m_generation) return;

    if (in->kind == PeerIpc::Incoming::Kind::Notification) {
        if (m_state != State::Ready && m_state != State::Stopping) {
            fail(Failure::ProtocolViolation, QStringLiteral("notification before hello"));
            return;
        }
        if (!PeerIpc::checkNotification(in->name, in->body)) {
            fail(Failure::ProtocolViolation, QStringLiteral("malformed notification %1").arg(in->name));
            return;
        }
        emit notified(in->name, in->body);
        return;
    }

    // 送っていない・応答済みの要求への応答は違反。取り消した要求の応答は黙って捨てる
    if (in->requestId == 0 || in->requestId >= m_nextRequestId) {
        fail(Failure::ProtocolViolation, QStringLiteral("response to unknown request"));
        return;
    }
    const auto it = m_pending.find(in->requestId);
    if (it == m_pending.end()) return;
    const QString method = it.value();
    m_pending.erase(it);

    if (in->kind == PeerIpc::Incoming::Kind::Error) {
        if (in->requestId == m_helloRequestId) {
            fail(Failure::HelloMismatch, in->body.value(QStringLiteral("code")).toString());
            return;
        }
        emit requestFailed(in->requestId, method, in->body.value(QStringLiteral("code")).toString());
        return;
    }
    if (!PeerIpc::checkResult(method, in->body)) {
        fail(Failure::ProtocolViolation, QStringLiteral("malformed result of %1").arg(method));
        return;
    }
    if (in->requestId == m_helloRequestId) {
        m_stopTimer->stop();
        const QJsonObject sandbox = in->body.value(QStringLiteral("sandbox")).toObject();
        if (!sandboxIsApplied(sandbox)) {
            fail(Failure::SandboxFailed, QStringLiteral("sandbox not applied"));
            return;
        }
        m_sandbox = sandbox;
        m_state = State::Ready;
        qCInfo(lcGame) << "P2P peer process ready, generation" << m_generation;
        emit ready();
        return;
    }
    emit replied(in->requestId, method, in->body);
}

void PeerProcess::onReadyReadStderr()
{
    if (!m_process) return;
    const QByteArray data = m_process->readAllStandardError();
    const qsizetype room = kMaxStderrBytes - m_stderr.size();
    if (room > 0) m_stderr.append(data.left(room));
}

void PeerProcess::onFinished(int exitCode, QProcess::ExitStatus status)
{
    m_stopTimer->stop();
    const bool expected = m_state == State::Stopping;
    if (!expected && !m_failed) {
        // 権限の制限の失敗（終了コード 3）とみなすのは、hello に答える前だけ（制限は外部の入力を読む前に掛ける）
        if (status == QProcess::NormalExit && exitCode == 3 && m_state == State::Starting) {
            fail(Failure::SandboxFailed, diagnosticLine(m_stderr));
        } else {
            fail(Failure::Crashed, QStringLiteral("exit code %1").arg(exitCode));
        }
    }
    resetProcess();
    m_state = State::NotRunning;
    emit stopped();
}

void PeerProcess::onErrorOccurred(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
#ifdef Q_OS_WIN
        // 実行ファイルがあるのに起動できないのは、権限の制限（AppContainer・ジョブ）を付けてプロセスを作れなかったとき
        if (QFileInfo(executablePath()).isExecutable()) {
            fail(Failure::SandboxFailed, QStringLiteral("the confined process could not be created"));
        } else {
            fail(Failure::NotFound, executablePath());
        }
#else
        fail(Failure::NotFound, executablePath());
#endif
        resetProcess();
        m_state = State::NotRunning;
        emit stopped();
    }
}

void PeerProcess::fail(Failure failure, const QString& detail)
{
    if (m_failed) return;
    m_failed = true;
    qCWarning(lcGame) << "P2P peer process failed:" << failure;
    m_pending.clear();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // 違反・失敗したプロセスは使わない。終わるのを待たない
        m_state = State::Stopping;
        m_terminateSent = true;
        m_process->kill();
    }
    emit failed(failure, detail);
}

void PeerProcess::resetProcess()
{
    if (!m_process) return;
    m_process->disconnect(this);
    m_process->deleteLater();
    m_process = nullptr;
    m_buffer.clear();
    // プロセスが終わった後に手放す（Windows では、ジョブを閉じると動いている通信プロセスが終わる）
    m_confinement.reset();
}
