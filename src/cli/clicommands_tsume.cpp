/// @file clicommands_tsume.cpp
/// @brief shogiboardq-cli の詰将棋コマンド（generate-tsume / verify-tsume）

#include "clicommands.h"

#include "cliargs.h"
#include "clioutput.h"
#include "clistopwatcher.h"
#include "sfenvalidationservice.h"
#include "tsumeshogigenerator.h"
#include "tsumeverificationrunner.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>

namespace {

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
        m_exitCode = CliOutput::failEvent(ErrorKind::EngineError, message);
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
        m_exitCode = CliOutput::fail(ErrorKind::EngineError, message);
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
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("generate-tsume"), args, true, &exitCode);
    if (!params) return exitCode;

    QString error;
    const auto engine = CliArgs::resolveEngine(*params, &error);
    if (!engine) return CliOutput::failEvent(ErrorKind::EngineNotFound, error);
    // 範囲と既定値は CliCommandSpecs の表で決まる
    const int targetMoves = params->value(QStringLiteral("target_moves")).toInt();
    if (targetMoves % 2 == 0) return CliOutput::failEvent(ErrorKind::InvalidArgument, QStringLiteral("target_moves must be odd"));
    const int maxPositions = params->value(QStringLiteral("max_positions")).toInt();
    const int timeoutMs = params->value(QStringLiteral("timeout_ms")).toInt();
    const int maxAttack = params->value(QStringLiteral("max_attack_pieces")).toInt();
    const int maxDefend = params->value(QStringLiteral("max_defend_pieces")).toInt();
    const int attackRange = params->value(QStringLiteral("attack_range")).toInt();

    TsumeshogiGenerator::Settings settings;
    settings.enginePath = engine->path;
    settings.engineName = engine->name;
    settings.targetMoves = targetMoves;
    settings.timeoutMs = timeoutMs;
    settings.maxPositionsToFind = maxPositions;
    settings.allowFinalMoveAlternatives = params->value(QStringLiteral("allow_final_move_alternatives")).toBool();
    settings.posGenSettings.maxAttackPieces = maxAttack;
    settings.posGenSettings.maxDefendPieces = maxDefend;
    settings.posGenSettings.attackRange = attackRange;
    settings.posGenSettings.addRemainingToDefenderHand = params->value(QStringLiteral("add_remaining_to_defender_hand")).toBool();

    TsumeshogiGenerator generator;
    QEventLoop loop;
    GenerateReporter reporter(&generator, &loop);
    QObject::connect(&generator, &TsumeshogiGenerator::positionFound, &reporter, &GenerateReporter::onPositionFound);
    QObject::connect(&generator, &TsumeshogiGenerator::progressUpdated, &reporter, &GenerateReporter::onProgress);
    QObject::connect(&generator, &TsumeshogiGenerator::verificationStatsUpdated, &reporter, &GenerateReporter::onVerificationStats);
    QObject::connect(&generator, &TsumeshogiGenerator::finished, &reporter, &GenerateReporter::onFinished);
    QObject::connect(&generator, &TsumeshogiGenerator::errorOccurred, &reporter, &GenerateReporter::onError);
    CliStopWatcher watcher(params->value(QStringLiteral("stdin_control")).toBool());
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &reporter, &GenerateReporter::onStopRequested);

    QJsonObject started;
    started[QStringLiteral("engine")] = engine->name;
    started[QStringLiteral("target_moves")] = targetMoves;
    started[QStringLiteral("max_positions")] = maxPositions;
    started[QStringLiteral("timeout_ms")] = timeoutMs;
    started[QStringLiteral("max_attack_pieces")] = maxAttack;
    started[QStringLiteral("max_defend_pieces")] = maxDefend;
    started[QStringLiteral("attack_range")] = attackRange;
    started[QStringLiteral("add_remaining_to_defender_hand")] = settings.posGenSettings.addRemainingToDefenderHand;
    started[QStringLiteral("allow_final_move_alternatives")] = settings.allowFinalMoveAlternatives;
    CliOutput::printEvent(QStringLiteral("started"), started);

    generator.start(settings);
    if (generator.isRunning()) loop.exec();
    return reporter.exitCode();
}

int CliCommands::verifyTsume(const QStringList& args)
{
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("verify-tsume"), args, false, &exitCode);
    if (!params) return exitCode;

    QString error;
    const auto engine = CliArgs::resolveEngine(*params, &error);
    if (!engine) return CliOutput::fail(ErrorKind::EngineNotFound, error);
    const int targetMoves = params->value(QStringLiteral("target_moves")).toInt();
    if (targetMoves % 2 == 0) return CliOutput::fail(ErrorKind::InvalidArgument, QStringLiteral("target_moves must be odd"));
    const int timeoutMs = params->value(QStringLiteral("timeout_seconds")).toInt() * 1000;

    const SfenValidation validation = SfenValidationService::validate(params->value(QStringLiteral("sfen")).toString());
    if (!validation.valid) return CliOutput::fail(ErrorKind::InvalidArgument, QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; "))));

    TsumeVerificationRunner runner;
    QEventLoop loop;
    VerifyReporter reporter(&loop, validation.normalizedSfen, targetMoves);
    QObject::connect(&runner, &TsumeVerificationRunner::finished, &reporter, &VerifyReporter::onFinished);
    QObject::connect(&runner, &TsumeVerificationRunner::errorOccurred, &reporter, &VerifyReporter::onError);
    CliStopWatcher watcher(false);
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &TsumeVerificationRunner::abort);

    TsumeVerificationRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.sfen = validation.normalizedSfen;
    request.targetMoves = targetMoves;
    request.timeoutMs = timeoutMs;
    request.allowFinalMoveAlternatives = params->value(QStringLiteral("allow_final_move_alternatives")).toBool();

    // 詰将棋の局面でなければ（受方の玉がない・王手がかかっているなど）、エンジンを起動せずに status invalid を返す
    if (!runner.start(request)) {
        reporter.onFinished(TsumeshogiVerifier::Status::Invalid, {}, 0, 0);
        return reporter.exitCode();
    }
    loop.exec();
    return reporter.exitCode();
}

#include "clicommands_tsume.moc"
