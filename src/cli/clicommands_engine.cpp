/// @file clicommands_engine.cpp
/// @brief shogiboardq-cli のエンジン実行コマンド（analyze / mate）

#include "clicommands.h"

#include "cliargs.h"
#include "clioutput.h"
#include "clistopwatcher.h"
#include "engineanalysisrunner.h"
#include "matesearchrunner.h"

#include <QCommandLineParser>
#include <QEventLoop>
#include <QJsonArray>

namespace {

const QString kInvalidArgument = QStringLiteral("invalid_argument");

/// analyze / mate に共通する開始イベントの内容
QJsonObject startedFields(const EngineListSettings::EngineEntry& engine, const CliPosition& position)
{
    QJsonObject obj;
    obj[QStringLiteral("engine")] = engine.name;
    obj[QStringLiteral("engine_path")] = engine.path;
    obj[QStringLiteral("position")] = position.positionCommand;
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
    void onLine(const UsiInfoLine& line) { CliOutput::printEvent(QStringLiteral("info"), line.toJson()); }
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
        m_exitCode = CliOutput::failEvent(QStringLiteral("engine_error"), message);
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
    void onFinished(MateSearchRunner::Status status, const QStringList& pv)
    {
        QJsonObject obj;
        obj[QStringLiteral("status")] = MateSearchRunner::statusName(status);
        obj[QStringLiteral("pv")] = QJsonArray::fromStringList(pv);
        obj[QStringLiteral("plies")] = static_cast<int>(pv.size());
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(m_runner->elapsedMs());
        CliOutput::printEvent(QStringLiteral("result"), obj);
        m_loop->quit();
    }
    void onError(const QString& message)
    {
        m_exitCode = CliOutput::failEvent(QStringLiteral("engine_error"), message);
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
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    CliArgs::addEngineOption(parser);
    CliArgs::addPositionOptions(parser);
    CliArgs::addStdinControlOption(parser);
    parser.addOption({QStringLiteral("seconds"), QStringLiteral("Thinking time in seconds (1-600)."), QStringLiteral("n"),
                      QStringLiteral("5")});
    parser.addOption({QStringLiteral("multipv"), QStringLiteral("Number of candidate lines (1-10)."), QStringLiteral("n"),
                      QStringLiteral("1")});
    if (!parser.parse(QStringList{QStringLiteral("shogiboardq-cli")} + args)) {
        return CliOutput::failEvent(kInvalidArgument, parser.errorText());
    }

    QString error;
    const auto engine = CliArgs::resolveEngine(parser, &error);
    if (!engine) return CliOutput::failEvent(QStringLiteral("engine_not_found"), error);
    const auto position = CliArgs::resolvePosition(parser, &error);
    if (!position) return CliOutput::failEvent(kInvalidArgument, error);
    const auto seconds = CliArgs::intOption(parser, QStringLiteral("seconds"), 5, 1, 600, &error);
    if (!seconds) return CliOutput::failEvent(kInvalidArgument, error);
    const auto multiPv = CliArgs::intOption(parser, QStringLiteral("multipv"), 1, 1, 10, &error);
    if (!multiPv) return CliOutput::failEvent(kInvalidArgument, error);

    EngineAnalysisRunner runner;
    QEventLoop loop;
    AnalyzeReporter reporter(&runner, &loop);
    QObject::connect(&runner, &EngineAnalysisRunner::lineUpdated, &reporter, &AnalyzeReporter::onLine);
    QObject::connect(&runner, &EngineAnalysisRunner::finished, &reporter, &AnalyzeReporter::onFinished);
    QObject::connect(&runner, &EngineAnalysisRunner::errorOccurred, &reporter, &AnalyzeReporter::onError);
    CliStopWatcher watcher(parser.isSet(QStringLiteral("stdin-control")));
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &EngineAnalysisRunner::stop);

    EngineAnalysisRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.positionCommand = position->positionCommand;
    request.thinkMs = *seconds * 1000;
    request.multiPv = *multiPv;

    QJsonObject started = startedFields(*engine, *position);
    started[QStringLiteral("seconds")] = *seconds;
    started[QStringLiteral("multipv")] = *multiPv;
    CliOutput::printEvent(QStringLiteral("started"), started);

    if (!runner.start(request, &error)) return CliOutput::failEvent(QStringLiteral("engine_error"), error);
    loop.exec();
    return reporter.exitCode();
}

int CliCommands::mate(const QStringList& args)
{
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    CliArgs::addEngineOption(parser);
    CliArgs::addPositionOptions(parser);
    CliArgs::addStdinControlOption(parser);
    parser.addOption({QStringLiteral("seconds"), QStringLiteral("Search time in seconds (1-600)."), QStringLiteral("n"),
                      QStringLiteral("10")});
    if (!parser.parse(QStringList{QStringLiteral("shogiboardq-cli")} + args)) {
        return CliOutput::failEvent(kInvalidArgument, parser.errorText());
    }

    QString error;
    const auto engine = CliArgs::resolveEngine(parser, &error);
    if (!engine) return CliOutput::failEvent(QStringLiteral("engine_not_found"), error);
    const auto position = CliArgs::resolvePosition(parser, &error);
    if (!position) return CliOutput::failEvent(kInvalidArgument, error);
    const auto seconds = CliArgs::intOption(parser, QStringLiteral("seconds"), 10, 1, 600, &error);
    if (!seconds) return CliOutput::failEvent(kInvalidArgument, error);

    MateSearchRunner runner;
    QEventLoop loop;
    MateReporter reporter(&runner, &loop);
    QObject::connect(&runner, &MateSearchRunner::finished, &reporter, &MateReporter::onFinished);
    QObject::connect(&runner, &MateSearchRunner::errorOccurred, &reporter, &MateReporter::onError);
    CliStopWatcher watcher(parser.isSet(QStringLiteral("stdin-control")));
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &MateSearchRunner::stop);

    MateSearchRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.positionCommand = position->positionCommand;
    request.timeMs = *seconds * 1000;

    QJsonObject started = startedFields(*engine, *position);
    started[QStringLiteral("seconds")] = *seconds;
    CliOutput::printEvent(QStringLiteral("started"), started);

    if (!runner.start(request, &error)) return CliOutput::failEvent(QStringLiteral("engine_error"), error);
    loop.exec();
    return reporter.exitCode();
}

#include "clicommands_engine.moc"
