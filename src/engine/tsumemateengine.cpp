#include "tsumemateengine.h"
#include <QFileInfo>
#include <algorithm>

TsumeMateEngine::TsumeMateEngine(QObject* parent) : QObject(parent)
{
    m_deadline.setSingleShot(true);
    m_deadline.setTimerType(Qt::PreciseTimer);
    connect(&m_deadline, &QTimer::timeout, this, &TsumeMateEngine::timedOut);
}

TsumeMateEngine::~TsumeMateEngine() { cancel(); }

void TsumeMateEngine::setExecutable(const QString& path)
{
    if (path == m_executable) return;
    cancel();
    m_executable = path;
}

void TsumeMateEngine::cancel()
{
    m_deadline.stop();
    m_phase = Phase::Off;
    if (!m_process) return;
    auto* process = m_process;
    m_process = nullptr;
    disconnect(process, nullptr, this, nullptr);
    process->kill();
    // kill 後の回収のみ。探索終了待ちはしない。
    process->waitForFinished(100);
    process->deleteLater();
}

void TsumeMateEngine::search(const QString& sfen, int milliseconds)
{
    m_sfen = sfen;
    m_milliseconds = std::clamp(milliseconds, 1, 600000);
    if (m_phase == Phase::Idle) { beginSearch(); return; }
    cancel();
    m_options.clear();
    m_engineName.clear();
    m_minLength = false;
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(QFileInfo(m_executable).absolutePath());
    connect(m_process, &QProcess::started, this, &TsumeMateEngine::started);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TsumeMateEngine::readLines);
    connect(m_process, &QProcess::readyReadStandardError, this, &TsumeMateEngine::readErrors);
    connect(m_process, &QProcess::errorOccurred, this, &TsumeMateEngine::processError);
    connect(m_process, &QProcess::finished, this, &TsumeMateEngine::processFinished);
    m_phase = Phase::Usi;
    m_deadline.start(15000);
    m_process->start(m_executable, {});
}

void TsumeMateEngine::send(const QString& command)
{
    if (m_process) m_process->write(command.toUtf8() + '\n');
}

void TsumeMateEngine::started() { send(QStringLiteral("usi")); }
void TsumeMateEngine::readErrors() { if (m_process) m_process->readAllStandardError(); }

void TsumeMateEngine::beginSearch()
{
    m_phase = Phase::Searching;
    send(QStringLiteral("usinewgame"));
    send(QStringLiteral("position sfen ") + m_sfen);
    // Komoring 1.1.0 は時間制限で最短化を打ち切っても既発見の PV を返す場合がある。
    // infinite + GUI 側の打切りで、自然終了した MinLength の結果だけを確定扱いする。
    send(QStringLiteral("go mate infinite"));
    m_deadline.start(m_milliseconds);
}

void TsumeMateEngine::readLines()
{
    while (m_process && m_process->canReadLine()) {
        const QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        if (m_phase == Phase::Usi) {
            if (line.startsWith(QStringLiteral("id name "))) m_engineName = line.mid(8);
            if (line.startsWith(QStringLiteral("option name "))) {
                const QString name = line.mid(12).section(QStringLiteral(" type "), 0, 0);
                m_options.insert(name);
                if (name == QStringLiteral("PostSearchLevel") && line.contains(QStringLiteral("var MinLength"))) m_minLength = true;
            }
            if (line == QStringLiteral("usiok")) {
                if (!m_engineName.contains(QStringLiteral("KomoringHeights"), Qt::CaseInsensitive) || !m_minLength
                    || !m_options.contains(QStringLiteral("GenerateAllLegalMoves"))
                    || !m_options.contains(QStringLiteral("RootIsAndNodeIfChecked"))) {
                    fail(tr("最短手数を確認できるKomoringHeightsを選択してください。"));
                    return;
                }
                const QList<QPair<QString, QString>> options{
                    {QStringLiteral("PostSearchLevel"), QStringLiteral("MinLength")},
                    {QStringLiteral("GenerateAllLegalMoves"), QStringLiteral("true")},
                    {QStringLiteral("RootIsAndNodeIfChecked"), QStringLiteral("false")},
                    {QStringLiteral("NodesLimit"), QStringLiteral("0")},
                    {QStringLiteral("MultiPV"), QStringLiteral("1")},
                    {QStringLiteral("Threads"), QStringLiteral("1")},
                    {QStringLiteral("USI_Hash"), QStringLiteral("256")}};
                for (const auto& option : options) {
                    if (m_options.contains(option.first)) send(QStringLiteral("setoption name %1 value %2").arg(option.first, option.second));
                }
                m_phase = Phase::Ready;
                send(QStringLiteral("isready"));
            }
        } else if (m_phase == Phase::Ready && line == QStringLiteral("readyok")) {
            beginSearch();
        } else if (m_phase == Phase::Searching && line.startsWith(QStringLiteral("checkmate "))) {
            m_deadline.stop();
            m_phase = Phase::Idle;
            TsumeEvaluation result;
            const QString response = line.mid(10).simplified();
            if (response == QStringLiteral("nomate")) result.status = TsumeEvaluation::Status::NoMate;
            else if (response == QStringLiteral("timeout") || response == QStringLiteral("notimplemented")) {
                result.detail = tr("エンジンが判定を完了できませんでした。時間を増やして再判定できます。");
            } else {
                result.status = TsumeEvaluation::Status::Mate;
                result.pv = response.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                result.plies = static_cast<int>(result.pv.size());
            }
            emit finished(result);
            return;
        }
    }
}

void TsumeMateEngine::fail(const QString& message)
{
    cancel();
    emit finished({TsumeEvaluation::Status::Unknown, 0, {}, message});
}

void TsumeMateEngine::processError(QProcess::ProcessError)
{
    fail(tr("判定エンジンを実行できません: %1").arg(m_process ? m_process->errorString() : m_executable));
}

void TsumeMateEngine::processFinished(int, QProcess::ExitStatus)
{
    if (m_phase != Phase::Off) fail(tr("判定エンジンが終了しました。実行ファイルを確認してください。"));
}

void TsumeMateEngine::timedOut()
{
    fail(tr("制限時間内に判定が完了しませんでした。不詰とは判定していません。"));
}
