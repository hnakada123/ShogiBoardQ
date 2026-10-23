/// @file clicommands_engine.cpp
/// @brief shogiboardq-cli のエンジン実行コマンド（analyze / analyze-kifu / mate）

#include "clicommands.h"

#include "cliargs.h"
#include "clioutput.h"
#include "clistopwatcher.h"
#include "engineanalysisrunner.h"
#include "kifuconversionservice.h"
#include "kifuformat.h"
#include "matesearchrunner.h"
#include "usimove.h"

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

/// 1局面の解析結果（bestmove・ponder・考えた時間・MultiPV ごとの最新の読み筋）
QJsonObject analysisResult(const EngineAnalysisRunner& runner, const QString& bestmove, const QString& ponder)
{
    QJsonObject obj;
    obj[QStringLiteral("bestmove")] = bestmove;
    if (!ponder.isEmpty()) obj[QStringLiteral("ponder")] = ponder;
    obj[QStringLiteral("elapsed_ms")] = static_cast<double>(runner.elapsedMs());
    QJsonArray lines;
    const auto latest = runner.latestLines();
    for (auto it = latest.cbegin(); it != latest.cend(); ++it) lines.append(it.value().toJson());
    obj[QStringLiteral("lines")] = lines;
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
    void onPositionFinished(int /*index*/, const QString& bestmove, const QString& ponder)
    {
        m_reported = true;
        CliOutput::printEvent(QStringLiteral("result"), analysisResult(*m_runner, bestmove, ponder));
    }
    void onFinished(bool /*stopped*/)
    {
        // 初期化の前に止めたときも結果を1つ出す（bestmove は空）
        if (!m_reported) CliOutput::printEvent(QStringLiteral("result"), analysisResult(*m_runner, QString(), QString()));
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
    bool m_reported = false;
};

/// 棋譜全体の解析を、局面ごとの position イベントと最後の finished イベントとして書く中継役
class KifuAnalyzeReporter : public QObject
{
    Q_OBJECT
public:
    struct Plan {
        int fromPly = 0;
        QStringList sfens;               ///< 本譜の各局面（[0] が開始局面）
        QStringList moves;               ///< 本譜の USI の指し手
        QList<UsiPosition> positions;    ///< 解析する局面（fromPly から順に）
    };

    KifuAnalyzeReporter(EngineAnalysisRunner* runner, QEventLoop* loop, Plan plan)
        : m_runner(runner), m_loop(loop), m_plan(std::move(plan)) {}
    int exitCode() const { return m_exitCode; }

public slots:
    void onPositionStarted(int index)
    {
        QJsonObject obj;
        obj[QStringLiteral("current_ply")] = m_plan.fromPly + index;
        obj[QStringLiteral("completed")] = index;
        CliOutput::printEvent(QStringLiteral("progress"), obj);
    }
    void onPositionFinished(int index, const QString& bestmove, const QString& ponder)
    {
        const int ply = m_plan.fromPly + index;
        QJsonObject obj = analysisResult(*m_runner, bestmove, ponder);
        obj[QStringLiteral("ply")] = ply;
        obj[QStringLiteral("sfen")] = m_plan.sfens.value(ply);
        obj[QStringLiteral("played_move")] = ply > 0 ? QJsonValue(m_plan.moves.value(ply - 1)) : QJsonValue();
        obj[QStringLiteral("partial")] = m_runner->isStopRequested();
        // 最善の読み筋の評価値を先手から見た値にする（評価値は手番の側から見た値）
        const auto latest = m_runner->latestLines();
        if (const auto best = latest.constFind(1); best != latest.cend()) {
            const int sign = m_plan.positions.at(index).sideToMove() == Turn::Black ? 1 : -1;
            if (best->score.isCp()) obj[QStringLiteral("score_cp_black")] = sign * best->score.value;
            if (best->score.isMate()) obj[QStringLiteral("score_mate_black")] = sign * best->score.value;
        }
        m_completed = index + 1;
        CliOutput::printEvent(QStringLiteral("position"), obj);
    }
    void onFinished(bool stopped)
    {
        QJsonObject obj;
        obj[QStringLiteral("completed")] = m_completed;
        obj[QStringLiteral("stopped")] = stopped;
        CliOutput::printEvent(QStringLiteral("finished"), obj);
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
    Plan m_plan;
    int m_completed = 0;
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
    QObject::connect(&runner, &EngineAnalysisRunner::positionFinished, &reporter, &AnalyzeReporter::onPositionFinished);
    QObject::connect(&runner, &EngineAnalysisRunner::finished, &reporter, &AnalyzeReporter::onFinished);
    QObject::connect(&runner, &EngineAnalysisRunner::errorOccurred, &reporter, &AnalyzeReporter::onError);
    CliStopWatcher watcher(params->value(QStringLiteral("stdin_control")).toBool());
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &EngineAnalysisRunner::stop);

    EngineAnalysisRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.positions = {position->position};
    request.thinkMs = seconds * 1000;
    request.multiPv = multiPv;

    QJsonObject started = startedFields(*engine, *position);
    started[QStringLiteral("seconds")] = seconds;
    started[QStringLiteral("multipv")] = multiPv;
    CliOutput::printEvent(QStringLiteral("started"), started);

    runner.start(request);
    loop.exec();
    return reporter.exitCode();
}

int CliCommands::analyzeKifu(const QStringList& args)
{
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("analyze-kifu"), args, true, &exitCode);
    if (!params) return exitCode;

    QString error;
    const auto engine = CliArgs::resolveEngine(*params, &error);
    if (!engine) return CliOutput::failEvent(ErrorKind::EngineNotFound, error);

    // 本譜を棋譜の変換と同じく読む
    KifuConversionService::Request conversion;
    conversion.inputPath = params->value(QStringLiteral("input_path")).toString();
    conversion.text = params->value(QStringLiteral("text")).toString();
    const QString inputName = params->value(QStringLiteral("input_format")).toString();
    if (inputName != QLatin1String("auto")) conversion.inputFormat = KifuFormats::fromName(inputName);
    conversion.outputFormat = KifuFormat::Sfen;
    const KifuConversionService::Result record = KifuConversionService::convert(conversion);
    if (!record.ok) return CliOutput::failEvent(record.failure);

    const int moveCount = static_cast<int>(record.usiMoves.size());
    const int fromPly = params->value(QStringLiteral("from_ply")).toInt();
    const int toPly = params->contains(QStringLiteral("to_ply")) ? params->value(QStringLiteral("to_ply")).toInt() : moveCount;
    if (fromPly > toPly || toPly > moveCount) {
        return CliOutput::failEvent(ErrorKind::InvalidArgument,
                                    QStringLiteral("The ply range %1-%2 is outside the main line (0-%3)")
                                        .arg(fromPly).arg(toPly).arg(moveCount));
    }
    // 局面は開始局面からの手順で送る（千日手の判断材料をエンジンに渡す）
    KifuAnalyzeReporter::Plan plan;
    plan.fromPly = fromPly;
    plan.sfens = record.sfens;
    plan.moves = record.usiMoves;
    QList<UsiMove> played;
    for (const QString& text : record.usiMoves) {
        const std::optional<UsiMove> move = UsiMove::parse(text);
        if (!move) return CliOutput::failEvent(ErrorKind::InvalidContent, QStringLiteral("Unreadable move: %1").arg(text));
        played << *move;
    }
    for (int ply = fromPly; ply <= toPly; ++ply) {
        const std::optional<UsiPosition> position = UsiPosition::fromMoves(record.initialSfen, played.mid(0, ply));
        if (!position) {
            return CliOutput::failEvent(ErrorKind::InvalidContent,
                                        QStringLiteral("The position after %1 moves could not be built").arg(ply));
        }
        plan.positions << *position;
    }
    const int seconds = params->value(QStringLiteral("seconds_per_position")).toInt();
    const int multiPv = params->value(QStringLiteral("multipv")).toInt();

    EngineAnalysisRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.positions = plan.positions;
    request.thinkMs = seconds * 1000;
    request.multiPv = multiPv;

    EngineAnalysisRunner runner;
    QEventLoop loop;
    KifuAnalyzeReporter reporter(&runner, &loop, plan);
    QObject::connect(&runner, &EngineAnalysisRunner::positionStarted, &reporter, &KifuAnalyzeReporter::onPositionStarted);
    QObject::connect(&runner, &EngineAnalysisRunner::positionFinished, &reporter, &KifuAnalyzeReporter::onPositionFinished);
    QObject::connect(&runner, &EngineAnalysisRunner::finished, &reporter, &KifuAnalyzeReporter::onFinished);
    QObject::connect(&runner, &EngineAnalysisRunner::errorOccurred, &reporter, &KifuAnalyzeReporter::onError);
    CliStopWatcher watcher(params->value(QStringLiteral("stdin_control")).toBool());
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &EngineAnalysisRunner::stop);

    QJsonObject started;
    started[QStringLiteral("engine")] = engine->name;
    started[QStringLiteral("engine_path")] = engine->path;
    started[QStringLiteral("start_sfen")] = record.initialSfen;
    started[QStringLiteral("from_ply")] = fromPly;
    started[QStringLiteral("to_ply")] = toPly;
    started[QStringLiteral("total_positions")] = toPly - fromPly + 1;
    started[QStringLiteral("seconds_per_position")] = seconds;
    started[QStringLiteral("multipv")] = multiPv;
    started[QStringLiteral("has_branches")] = record.hasBranches;
    started[QStringLiteral("warnings")] = QJsonArray::fromStringList(record.warnings);
    CliOutput::printEvent(QStringLiteral("started"), started);

    runner.start(request);
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

    runner.start(request);
    loop.exec();
    return reporter.exitCode();
}

#include "clicommands_engine.moc"
