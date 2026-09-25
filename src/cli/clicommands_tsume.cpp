/// @file clicommands_tsume.cpp
/// @brief shogiboardq-cli の詰将棋コマンド（generate-tsume / verify-tsume）

#include "clicommands.h"

#include "cliargs.h"
#include "clioutput.h"
#include "clistopwatcher.h"
#include "sfenvalidationservice.h"
#include "tsumeshogigenerator.h"
#include "tsumeverificationrunner.h"

#include <QCommandLineParser>
#include <QLatin1StringView>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>

namespace {

constexpr QLatin1StringView kInvalidArgument("invalid_argument");
constexpr qint64 kProgressIntervalMs = 1000;

/// 生成の進捗・発見局面を JSON Lines で書く中継役
class GenerateReporter : public QObject
{
    Q_OBJECT
public:
    GenerateReporter(TsumeshogiGenerator* generator, QEventLoop* loop)
        : m_generator(generator), m_loop(loop) { m_timer.start(); }
    int exitCode() const { return m_exitCode; }

public slots:
    void onPositionFound(const QString& sfen, const QStringList& pv)
    {
        ++m_found;
        QJsonObject obj;
        obj[QStringLiteral("index")] = m_found;
        obj[QStringLiteral("sfen")] = sfen;
        obj[QStringLiteral("pv")] = QJsonArray::fromStringList(pv);
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(m_timer.elapsed());
        CliOutput::printEvent(QStringLiteral("position"), obj);
    }
    void onProgress(int generated, int found, qint64 elapsedMs)
    {
        m_generated = generated;
        m_found = found;
        if (elapsedMs - m_lastProgressMs < kProgressIntervalMs) return;
        m_lastProgressMs = elapsedMs;
        CliOutput::printEvent(QStringLiteral("progress"), progressFields(elapsedMs));
    }
    void onVerificationStats(int rejected, int inconclusive)
    {
        m_rejected = rejected;
        m_inconclusive = inconclusive;
    }
    void onStopRequested()
    {
        m_stopped = true;
        m_generator->stop();
    }
    void onFinished()
    {
        QJsonObject obj = progressFields(m_timer.elapsed());
        obj[QStringLiteral("stopped")] = m_stopped;
        CliOutput::printEvent(QStringLiteral("finished"), obj);
        m_loop->quit();
    }
    void onError(const QString& message)
    {
        m_exitCode = CliOutput::failEvent(QStringLiteral("engine_error"), message);
    }

private:
    QJsonObject progressFields(qint64 elapsedMs) const
    {
        QJsonObject obj;
        obj[QStringLiteral("generated")] = m_generated;
        obj[QStringLiteral("found")] = m_found;
        obj[QStringLiteral("rejected")] = m_rejected;
        obj[QStringLiteral("inconclusive")] = m_inconclusive;
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(elapsedMs);
        return obj;
    }

    TsumeshogiGenerator* m_generator;
    QEventLoop* m_loop;
    QElapsedTimer m_timer;
    int m_generated = 0;
    int m_found = 0;
    int m_rejected = 0;
    int m_inconclusive = 0;
    qint64 m_lastProgressMs = 0;
    bool m_stopped = false;
    int m_exitCode = 0;
};

/// 余詰検査の結果を JSON で書く中継役
class VerifyReporter : public QObject
{
    Q_OBJECT
public:
    VerifyReporter(QEventLoop* loop, QString sfen, int targetMoves)
        : m_loop(loop), m_sfen(std::move(sfen)), m_targetMoves(targetMoves) {}
    int exitCode() const { return m_exitCode; }

public slots:
    void onFinished(TsumeshogiVerifier::Status status, const QStringList& pv, int queries, qint64 elapsedMs)
    {
        QJsonObject obj;
        obj[QStringLiteral("ok")] = true;
        obj[QStringLiteral("sfen")] = m_sfen;
        obj[QStringLiteral("target_moves")] = m_targetMoves;
        obj[QStringLiteral("status")] = TsumeVerificationRunner::statusName(status);
        obj[QStringLiteral("pv")] = QJsonArray::fromStringList(pv);
        obj[QStringLiteral("queries")] = queries;
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(elapsedMs);
        CliOutput::printJson(obj);
        m_loop->quit();
    }
    void onError(const QString& message)
    {
        m_exitCode = CliOutput::fail(QStringLiteral("engine_error"), message);
        m_loop->quit();
    }

private:
    QEventLoop* m_loop;
    QString m_sfen;
    int m_targetMoves;
    int m_exitCode = 0;
};

} // namespace

int CliCommands::generateTsume(const QStringList& args)
{
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    CliArgs::addEngineOption(parser);
    CliArgs::addStdinControlOption(parser);
    parser.addOption({QStringLiteral("target-moves"), QStringLiteral("Mate length in plies (odd, 1-19)."), QStringLiteral("n"), QStringLiteral("3")});
    parser.addOption({QStringLiteral("max-positions"), QStringLiteral("Stop after this many positions (1-100)."), QStringLiteral("n"), QStringLiteral("10")});
    parser.addOption({QStringLiteral("timeout-ms"), QStringLiteral("Engine time per candidate/trim/verification (500-60000)."), QStringLiteral("ms"), QStringLiteral("5000")});
    parser.addOption({QStringLiteral("max-attack"), QStringLiteral("Maximum attacking pieces on board and in hand (1-10)."), QStringLiteral("n"), QStringLiteral("4")});
    parser.addOption({QStringLiteral("max-defend"), QStringLiteral("Maximum defending pieces other than the king (0-10)."), QStringLiteral("n"), QStringLiteral("1")});
    parser.addOption({QStringLiteral("attack-range"), QStringLiteral("Placement range around the king (1-8)."), QStringLiteral("n"), QStringLiteral("3")});
    parser.addOption({QStringLiteral("no-remaining-to-hand"), QStringLiteral("Do not give unused pieces to the defender's hand.")});
    parser.addOption({QStringLiteral("no-final-alternatives"), QStringLiteral("Reject positions whose final mating move has alternatives.")});
    if (!parser.parse(QStringList{QStringLiteral("shogiboardq-cli")} + args)) {
        return CliOutput::failEvent(kInvalidArgument, parser.errorText());
    }

    QString error;
    const auto engine = CliArgs::resolveEngine(parser, &error);
    if (!engine) return CliOutput::failEvent(QStringLiteral("engine_not_found"), error);
    const auto targetMoves = CliArgs::intOption(parser, QStringLiteral("target-moves"), 3, 1, 19, &error);
    if (!targetMoves) return CliOutput::failEvent(kInvalidArgument, error);
    if (*targetMoves % 2 == 0) return CliOutput::failEvent(kInvalidArgument, QStringLiteral("--target-moves must be odd"));
    const auto maxPositions = CliArgs::intOption(parser, QStringLiteral("max-positions"), 10, 1, 100, &error);
    if (!maxPositions) return CliOutput::failEvent(kInvalidArgument, error);
    const auto timeoutMs = CliArgs::intOption(parser, QStringLiteral("timeout-ms"), 5000, 500, 60000, &error);
    if (!timeoutMs) return CliOutput::failEvent(kInvalidArgument, error);
    const auto maxAttack = CliArgs::intOption(parser, QStringLiteral("max-attack"), 4, 1, 10, &error);
    if (!maxAttack) return CliOutput::failEvent(kInvalidArgument, error);
    const auto maxDefend = CliArgs::intOption(parser, QStringLiteral("max-defend"), 1, 0, 10, &error);
    if (!maxDefend) return CliOutput::failEvent(kInvalidArgument, error);
    const auto attackRange = CliArgs::intOption(parser, QStringLiteral("attack-range"), 3, 1, 8, &error);
    if (!attackRange) return CliOutput::failEvent(kInvalidArgument, error);

    TsumeshogiGenerator::Settings settings;
    settings.enginePath = engine->path;
    settings.engineName = engine->name;
    settings.targetMoves = *targetMoves;
    settings.timeoutMs = *timeoutMs;
    settings.maxPositionsToFind = *maxPositions;
    settings.allowFinalMoveAlternatives = !parser.isSet(QStringLiteral("no-final-alternatives"));
    settings.posGenSettings.maxAttackPieces = *maxAttack;
    settings.posGenSettings.maxDefendPieces = *maxDefend;
    settings.posGenSettings.attackRange = *attackRange;
    settings.posGenSettings.addRemainingToDefenderHand = !parser.isSet(QStringLiteral("no-remaining-to-hand"));

    TsumeshogiGenerator generator;
    QEventLoop loop;
    GenerateReporter reporter(&generator, &loop);
    QObject::connect(&generator, &TsumeshogiGenerator::positionFound, &reporter, &GenerateReporter::onPositionFound);
    QObject::connect(&generator, &TsumeshogiGenerator::progressUpdated, &reporter, &GenerateReporter::onProgress);
    QObject::connect(&generator, &TsumeshogiGenerator::verificationStatsUpdated, &reporter, &GenerateReporter::onVerificationStats);
    QObject::connect(&generator, &TsumeshogiGenerator::finished, &reporter, &GenerateReporter::onFinished);
    QObject::connect(&generator, &TsumeshogiGenerator::errorOccurred, &reporter, &GenerateReporter::onError);
    CliStopWatcher watcher(parser.isSet(QStringLiteral("stdin-control")));
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &reporter, &GenerateReporter::onStopRequested);

    QJsonObject started;
    started[QStringLiteral("engine")] = engine->name;
    started[QStringLiteral("target_moves")] = *targetMoves;
    started[QStringLiteral("max_positions")] = *maxPositions;
    started[QStringLiteral("timeout_ms")] = *timeoutMs;
    started[QStringLiteral("max_attack_pieces")] = *maxAttack;
    started[QStringLiteral("max_defend_pieces")] = *maxDefend;
    started[QStringLiteral("attack_range")] = *attackRange;
    started[QStringLiteral("add_remaining_to_defender_hand")] = settings.posGenSettings.addRemainingToDefenderHand;
    started[QStringLiteral("allow_final_move_alternatives")] = settings.allowFinalMoveAlternatives;
    CliOutput::printEvent(QStringLiteral("started"), started);

    generator.start(settings);
    if (generator.isRunning()) loop.exec();
    return reporter.exitCode();
}

int CliCommands::verifyTsume(const QStringList& args)
{
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    CliArgs::addEngineOption(parser);
    parser.addOption({QStringLiteral("sfen"), QStringLiteral("Tsume position (attacker to move)."), QStringLiteral("sfen")});
    parser.addOption({QStringLiteral("target-moves"), QStringLiteral("Expected mate length in plies (odd, 1-39)."), QStringLiteral("n")});
    parser.addOption({QStringLiteral("timeout-ms"), QStringLiteral("Total engine time budget (1000-120000)."), QStringLiteral("ms"), QStringLiteral("15000")});
    parser.addOption({QStringLiteral("no-final-alternatives"), QStringLiteral("Treat alternatives on the final mating move as a cook.")});
    if (!parser.parse(QStringList{QStringLiteral("shogiboardq-cli")} + args)) {
        return CliOutput::fail(kInvalidArgument, parser.errorText());
    }

    QString error;
    const auto engine = CliArgs::resolveEngine(parser, &error);
    if (!engine) return CliOutput::fail(QStringLiteral("engine_not_found"), error);
    if (!parser.isSet(QStringLiteral("sfen"))) return CliOutput::fail(kInvalidArgument, QStringLiteral("--sfen is required"));
    if (!parser.isSet(QStringLiteral("target-moves"))) return CliOutput::fail(kInvalidArgument, QStringLiteral("--target-moves is required"));
    const auto targetMoves = CliArgs::intOption(parser, QStringLiteral("target-moves"), 3, 1, 39, &error);
    if (!targetMoves) return CliOutput::fail(kInvalidArgument, error);
    if (*targetMoves % 2 == 0) return CliOutput::fail(kInvalidArgument, QStringLiteral("--target-moves must be odd"));
    const auto timeoutMs = CliArgs::intOption(parser, QStringLiteral("timeout-ms"), 15000, 1000, 120000, &error);
    if (!timeoutMs) return CliOutput::fail(kInvalidArgument, error);

    const SfenValidation validation = SfenValidationService::validate(parser.value(QStringLiteral("sfen")));
    if (!validation.valid) return CliOutput::fail(kInvalidArgument, QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; "))));

    TsumeVerificationRunner runner;
    QEventLoop loop;
    VerifyReporter reporter(&loop, validation.normalizedSfen, *targetMoves);
    QObject::connect(&runner, &TsumeVerificationRunner::finished, &reporter, &VerifyReporter::onFinished);
    QObject::connect(&runner, &TsumeVerificationRunner::errorOccurred, &reporter, &VerifyReporter::onError);
    CliStopWatcher watcher(false);
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &TsumeVerificationRunner::abort);

    TsumeVerificationRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.sfen = validation.normalizedSfen;
    request.targetMoves = *targetMoves;
    request.timeoutMs = *timeoutMs;
    request.allowFinalMoveAlternatives = !parser.isSet(QStringLiteral("no-final-alternatives"));

    if (!runner.start(request, &error)) return CliOutput::fail(QStringLiteral("engine_error"), error);
    loop.exec();
    return reporter.exitCode();
}

#include "clicommands_tsume.moc"
