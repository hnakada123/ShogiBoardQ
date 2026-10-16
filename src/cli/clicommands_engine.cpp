/// @file clicommands_engine.cpp
/// @brief shogiboardq-cli のエンジン実行コマンド（analyze / mate）

#include "clicommands.h"

#include "cliargs.h"
#include "clioutput.h"
#include "clistopwatcher.h"
#include "engineanalysisrunner.h"
#include "matesearchrunner.h"

#include <QEventLoop>
#include <QJsonArray>

namespace {


/// analyze / mate に共通する開始イベントの内容
QJsonObject startedFields(const EngineListSettings::EngineEntry& engine, const CliPosition& position)
{
    QJsonObject obj;
    obj[QStringLiteral("engine")] = engine.name;
    obj[QStringLiteral("engine_path")] = engine.path;
    obj[QStringLiteral("position")] = position.position.command();
    obj[QStringLiteral("start_sfen")] = position.startSfen;
    obj[QStringLiteral("sfen")] = position.finalSfen;
    obj[QStringLiteral("moves")] = QJsonArray::fromStringList(position.moves);
    return obj;
}

/// 解析結果を JSON Lines の result イベントとして書く中継役
class AnalyzeReporter : public QObject
{
    Q_OBJECT
public:
    explicit AnalyzeReporter(EngineAnalysisRunner* runner, QEventLoop* loop)
        : m_runner(runner), m_loop(loop) {}
    int exitCode() const { return m_exitCode; }

public slots:
    void onLine(const UsiInfo& line) { CliOutput::printEvent(QStringLiteral("info"), line.toJson()); }
    void onFinished(const QString& bestmove, const QString& ponder)
    {
        QJsonObject obj;
        obj[QStringLiteral("bestmove")] = bestmove;
        if (!ponder.isEmpty()) obj[QStringLiteral("ponder")] = ponder;
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(m_runner->elapsedMs());
        QJsonArray lines;
        const auto latest = m_runner->latestLines();
        for (auto it = latest.cbegin(); it != latest.cend(); ++it) lines.append(it.value().toJson());
        obj[QStringLiteral("lines")] = lines;
        CliOutput::printEvent(QStringLiteral("result"), obj);
        m_loop->quit();
    }
    void onError(const QString& message)
    {
        m_exitCode = CliOutput::failEvent(ErrorKind::EngineError, message);
        m_loop->quit();
    }

private:
    EngineAnalysisRunner* m_runner;
    QEventLoop* m_loop;
    int m_exitCode = 0;
};

/// 詰み探索結果を JSON Lines の result イベントとして書く中継役
class MateReporter : public QObject
{
    Q_OBJECT
public:
    explicit MateReporter(MateSearchRunner* runner, QEventLoop* loop)
        : m_runner(runner), m_loop(loop) {}
    int exitCode() const { return m_exitCode; }

public slots:
    void onFinished(const MateAnswer& answer)
    {
        QJsonObject obj;
        obj[QStringLiteral("status")] = MateSearchRunner::kindName(answer.kind);
        obj[QStringLiteral("pv")] = QJsonArray::fromStringList(answer.pv);
        obj[QStringLiteral("plies")] = static_cast<int>(answer.pv.size());
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(m_runner->elapsedMs());
        CliOutput::printEvent(QStringLiteral("result"), obj);
        m_loop->quit();
    }
    void onError(const QString& message)
    {
        m_exitCode = CliOutput::failEvent(ErrorKind::EngineError, message);
        m_loop->quit();
    }

private:
    MateSearchRunner* m_runner;
    QEventLoop* m_loop;
    int m_exitCode = 0;
};

} // namespace

int CliCommands::analyze(const QStringList& args)
{
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("analyze"), args, true, &exitCode);
    if (!params) return exitCode;

    QString error;
    const auto engine = CliArgs::resolveEngine(*params, &error);
    if (!engine) return CliOutput::failEvent(ErrorKind::EngineNotFound, error);
    const auto position = CliArgs::resolvePosition(*params, &error);
    if (!position) return CliOutput::failEvent(ErrorKind::InvalidArgument, error);
    const int seconds = params->value(QStringLiteral("seconds")).toInt();
    const int multiPv = params->value(QStringLiteral("multipv")).toInt();

    EngineAnalysisRunner runner;
    QEventLoop loop;
    AnalyzeReporter reporter(&runner, &loop);
    QObject::connect(&runner, &EngineAnalysisRunner::lineUpdated, &reporter, &AnalyzeReporter::onLine);
    QObject::connect(&runner, &EngineAnalysisRunner::finished, &reporter, &AnalyzeReporter::onFinished);
    QObject::connect(&runner, &EngineAnalysisRunner::errorOccurred, &reporter, &AnalyzeReporter::onError);
    CliStopWatcher watcher(params->value(QStringLiteral("stdin_control")).toBool());
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &EngineAnalysisRunner::stop);

    EngineAnalysisRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.position = position->position;
    request.thinkMs = seconds * 1000;
    request.multiPv = multiPv;

    QJsonObject started = startedFields(*engine, *position);
    started[QStringLiteral("seconds")] = seconds;
    started[QStringLiteral("multipv")] = multiPv;
    CliOutput::printEvent(QStringLiteral("started"), started);

    if (!runner.start(request, &error)) return CliOutput::failEvent(ErrorKind::EngineError, error);
    loop.exec();
    return reporter.exitCode();
}

int CliCommands::mate(const QStringList& args)
{
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("mate"), args, true, &exitCode);
    if (!params) return exitCode;

    QString error;
    const auto engine = CliArgs::resolveEngine(*params, &error);
    if (!engine) return CliOutput::failEvent(ErrorKind::EngineNotFound, error);
    const auto position = CliArgs::resolvePosition(*params, &error);
    if (!position) return CliOutput::failEvent(ErrorKind::InvalidArgument, error);
    const int seconds = params->value(QStringLiteral("seconds")).toInt();

    MateSearchRunner runner;
    QEventLoop loop;
    MateReporter reporter(&runner, &loop);
    QObject::connect(&runner, &MateSearchRunner::finished, &reporter, &MateReporter::onFinished);
    QObject::connect(&runner, &MateSearchRunner::errorOccurred, &reporter, &MateReporter::onError);
    CliStopWatcher watcher(params->value(QStringLiteral("stdin_control")).toBool());
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &MateSearchRunner::stop);

    MateSearchRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.position = position->position;
    request.timeMs = seconds * 1000;

    QJsonObject started = startedFields(*engine, *position);
    started[QStringLiteral("seconds")] = seconds;
    CliOutput::printEvent(QStringLiteral("started"), started);

    if (!runner.start(request, &error)) return CliOutput::failEvent(ErrorKind::EngineError, error);
    loop.exec();
    return reporter.exitCode();
}

#include "clicommands_engine.moc"
