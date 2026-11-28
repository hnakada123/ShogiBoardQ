/// @file clicommands_tsume.cpp
/// @brief shogiboardq-cli の詰将棋コマンド（generate-tsume / grow-tsume / verify-tsume）

#include "clicommands.h"

#include "cliargs.h"
#include "clioutput.h"
#include "clistopwatcher.h"
#include "inputpath.h"
#include "matesearchposition.h"
#include "outputpath.h"
#include "sfenvalidator.h"
#include "tsumeretrogradegenerator.h"
#include "tsumeshogigenerator.h"
#include "tsumeshogisettings.h"
#include "tsumeverificationrunner.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTextStream>

#include <algorithm>
#include <optional>

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
        // 生成器は失敗の後にも finished を出す。読み手には error を最後のイベントとして見せる
        if (!m_failed) {
            QJsonObject obj = progressFields(m_timer.elapsed());
            obj[QStringLiteral("stopped")] = m_stopped;
            CliOutput::printEvent(QStringLiteral("finished"), obj);
        }
        m_loop->quit();
    }
    void onError(const QString& message)
    {
        m_failed = true;
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
    bool m_failed = false;   ///< エンジンの失敗を error で知らせた
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

/// 逆算・成長型詰将棋生成の進捗・発見局面・チェックポイントを JSON Lines で書く中継役
class GrowReporter : public QObject
{
    Q_OBJECT
public:
    GrowReporter(TsumeRetrogradeGenerator* generator, QEventLoop* loop, QString outputPath,
                 QString engineName, int targetMoves)
        : m_generator(generator)
        , m_loop(loop)
        , m_outputPath(std::move(outputPath))
        , m_engineName(std::move(engineName))
        , m_targetMoves(targetMoves)
    {
    }
    int exitCode() const { return m_exitCode; }

public slots:
    void onPositionFound(const QString& sfen, const QStringList& pv, int removedPieces, int verifyQueries)
    {
        ++m_found;
        const auto prog = m_generator->progress();
        if (!m_outputPath.isEmpty()) appendPositionToFile(sfen, pv);

        QJsonObject obj;
        obj[QStringLiteral("index")] = m_found;
        obj[QStringLiteral("sfen")] = sfen;
        obj[QStringLiteral("pv")] = QJsonArray::fromStringList(pv);
        obj[QStringLiteral("plies")] = static_cast<int>(pv.size());
        obj[QStringLiteral("removed_pieces")] = removedPieces;
        obj[QStringLiteral("verify_queries")] = verifyQueries;
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(prog.elapsedMs);
        CliOutput::printEvent(QStringLiteral("position"), obj);
    }

    void onProgress(const TsumeRetrogradeGenerator::Progress& prog)
    {
        m_lastProgress = prog;
        if (prog.elapsedMs - m_lastProgressMs < kProgressIntervalMs) return;
        m_lastProgressMs = prog.elapsedMs;
        CliOutput::printEvent(QStringLiteral("progress"), progressFields(prog));
    }

    void onCheckpointSaved(const QString& path)
    {
        const auto prog = m_generator->progress();
        QJsonObject obj;
        obj[QStringLiteral("path")] = path;
        obj[QStringLiteral("max_plies_reached")] = prog.maxPliesReached;
        obj[QStringLiteral("found")] = prog.found;
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(prog.elapsedMs);
        CliOutput::printEvent(QStringLiteral("checkpoint"), obj);
    }

    void onStopRequested()
    {
        m_stopped = true;
        m_generator->stop();
    }

    void onControlLine(const QString& line)
    {
        if (line.startsWith(QLatin1String("exclude "))) {
            m_generator->addExcludeLine(line.mid(8).trimmed());
        } else if (line.startsWith(QLatin1String("seed "))) {
            m_generator->addSeedLine(line.mid(5).trimmed());
        }
    }

    void onFinished(bool stopped)
    {
        if (!m_failed) {
            const auto prog = m_generator->progress();
            QJsonObject obj = progressFields(prog);
            obj[QStringLiteral("stopped")] = stopped || m_stopped;
            CliOutput::printEvent(QStringLiteral("finished"), obj);
        }
        m_loop->quit();
    }

    void onError(const QString& message)
    {
        m_failed = true;
        m_exitCode = CliOutput::failEvent(ErrorKind::EngineError, message);
    }

private:
    static QJsonObject progressFields(const TsumeRetrogradeGenerator::Progress& p)
    {
        QJsonObject obj;
        obj[QStringLiteral("phase")] = p.phase;
        obj[QStringLiteral("max_plies_reached")] = p.maxPliesReached;
        obj[QStringLiteral("frontier_nodes")] = p.frontierNodes;
        obj[QStringLiteral("repair_queue")] = p.repairQueueSize;
        obj[QStringLiteral("pending_certify")] = p.pendingCertifySize;
        obj[QStringLiteral("generated")] = static_cast<double>(p.generatedCandidates);
        obj[QStringLiteral("queries")] = static_cast<double>(p.engineQueries);
        obj[QStringLiteral("found")] = p.found;
        obj[QStringLiteral("rejected")] = p.certifiedRejected;
        obj[QStringLiteral("inconclusive")] = p.certifiedInconclusive;
        obj[QStringLiteral("repair_succeeded")] = p.repairSucceeded;
        obj[QStringLiteral("engine_restarts")] = p.engineRestarts;
        if (!p.certifierStage.isEmpty()) {
            obj[QStringLiteral("certifier_stage")] = p.certifierStage;
            obj[QStringLiteral("certifier_queries")] = p.certifierQueries;
            obj[QStringLiteral("trim_index")] = p.trimIndex;
            obj[QStringLiteral("trim_total")] = p.trimTotal;
        }
        obj[QStringLiteral("elapsed_ms")] = static_cast<double>(p.elapsedMs);
        return obj;
    }

    void appendPositionToFile(const QString& sfen, const QStringList& pv)
    {
        const bool needHeader = !QFileInfo::exists(m_outputPath) || QFileInfo(m_outputPath).size() == 0;
        QFile file(m_outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
        QTextStream out(&file);
        if (needHeader) {
            out << "# ShogiBoardQ grow-tsume " << QStringLiteral(APP_VERSION)
                << " engine=" << m_engineName
                << " target_moves=" << m_targetMoves
                << " started=" << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
        }
        out << sfen << " moves " << pv.join(QLatin1Char(' ')) << '\n';
        out.flush();
    }

    TsumeRetrogradeGenerator* m_generator;
    QEventLoop* m_loop;
    QString m_outputPath;
    QString m_engineName;
    int m_targetMoves = 0;
    TsumeRetrogradeGenerator::Progress m_lastProgress;
    qint64 m_lastProgressMs = -kProgressIntervalMs;
    int m_found = 0;
    bool m_failed = false;
    bool m_stopped = false;
    int m_exitCode = 0;
};

/// grow-tsume の途中保存と出力先の指定を確かめる。問題があれば、失敗を出力して終了コードを返す
std::optional<int> checkGrowPaths(const QString& checkpointPath, bool resume, const QString& outputPath)
{
    if (resume && checkpointPath.isEmpty()) {
        return CliOutput::failEvent(ErrorKind::InvalidArgument,
                                    QStringLiteral("--resume requires --checkpoint-path"));
    }
    if (!checkpointPath.isEmpty()) {
        if (resume) {
            if (const auto failure = InputPath::check(checkpointPath)) {
                return CliOutput::failEvent(failure->kind, failure->message);
            }
        }
        if (const auto failure = OutputPath::check(checkpointPath, true, QString())) {
            return CliOutput::failEvent(failure->kind, failure->message);
        }
    }
    if (!outputPath.isEmpty()) {
        if (const auto failure = OutputPath::check(outputPath, true, QString())) {
            return CliOutput::failEvent(failure->kind, failure->message);
        }
    }
    return std::nullopt;
}

/// 種局面・除外のファイル（引数 paramName）の行を lines に足す。指定が無ければ何もしない。
/// 読めなければ、失敗を出力して終了コードを返す
std::optional<int> appendInputLines(const QString& path, const QString& paramName, QStringList& lines)
{
    if (path.isEmpty()) return std::nullopt;
    if (const auto failure = InputPath::check(path)) {
        return CliOutput::failEvent(failure->kind, failure->message);
    }
    QString fileError;
    // "SFEN" と "SFEN moves ..." の両方をそのまま渡す（PV付きシードは直接フロンティア投入される）
    const auto read = TsumeRetrogradeGenerator::readInputLines(path, &fileError);
    if (!read) {
        return CliOutput::failEvent(ErrorKind::InvalidContent,
                                    QStringLiteral("Cannot open %1: %2").arg(paramName, fileError));
    }
    lines.append(*read);
    return std::nullopt;
}

/// grow-tsume の引数（範囲と既定値は CliCommandSpecs の表）を生成器の設定にする。
/// 種局面・除外・途中保存は呼び出し側が入れる
TsumeRetrogradeGenerator::Settings growSettings(const QJsonObject& params, const EngineEntry& engine)
{
    TsumeRetrogradeGenerator::Settings settings;
    settings.enginePath = engine.path;
    settings.engineName = engine.name;
    settings.targetMoves = params.value(QStringLiteral("target_moves")).toInt();
    settings.maxPositionsToFind = params.value(QStringLiteral("max_positions")).toInt();
    settings.beamWidth = params.value(QStringLiteral("beam_width")).toInt();
    settings.branchFactor = params.value(QStringLiteral("branch_factor")).toInt();
    settings.maxAttackPieces = params.value(QStringLiteral("max_attack_pieces")).toInt();
    settings.maxDefendPieces = params.value(QStringLiteral("max_defend_pieces")).toInt();
    settings.maxTotalBoardPieces = params.value(QStringLiteral("max_total_board_pieces")).toInt();
    settings.stepTimeoutMs = params.value(QStringLiteral("step_timeout_ms")).toInt();
    settings.verifyQueryTimeoutMs = params.value(QStringLiteral("verify_query_timeout_ms")).toInt();
    settings.verifyBudgetMs = static_cast<qint64>(params.value(QStringLiteral("verify_budget_seconds")).toInt()) * 1000LL;
    settings.engineHashMb = params.value(QStringLiteral("hash_mb")).toInt();
    settings.engineThreads = params.value(QStringLiteral("threads")).toInt();
    settings.addRemainingToDefenderHand = params.value(QStringLiteral("add_remaining_to_defender_hand")).toBool();
    settings.allowFinalMoveAlternatives = params.value(QStringLiteral("allow_final_move_alternatives")).toBool();
    settings.requireNoSurplus = params.value(QStringLiteral("require_no_surplus")).toBool();
    settings.strictMinLength = params.value(QStringLiteral("strict_min_length")).toBool();
    settings.excludeSimilar = params.value(QStringLiteral("exclude_similar")).toBool();
    settings.checkpointIntervalSec = params.value(QStringLiteral("checkpoint_interval_seconds")).toInt();
    settings.rngSeed = static_cast<std::uint64_t>(std::max(0, params.value(QStringLiteral("rng_seed")).toInt()));
    return settings;
}

/// grow-tsume の started イベントの中身
QJsonObject growStartedEvent(const TsumeRetrogradeGenerator::Settings& settings, const QJsonObject& params,
                             const QString& outputPath)
{
    QJsonObject started;
    started[QStringLiteral("engine")] = settings.engineName;
    started[QStringLiteral("target_moves")] = settings.targetMoves;
    started[QStringLiteral("max_positions")] = settings.maxPositionsToFind;
    started[QStringLiteral("beam_width")] = settings.beamWidth;
    started[QStringLiteral("branch_factor")] = settings.branchFactor;
    started[QStringLiteral("max_attack_pieces")] = settings.maxAttackPieces;
    started[QStringLiteral("max_defend_pieces")] = settings.maxDefendPieces;
    started[QStringLiteral("max_total_board_pieces")] = settings.maxTotalBoardPieces;
    started[QStringLiteral("step_timeout_ms")] = settings.stepTimeoutMs;
    started[QStringLiteral("verify_query_timeout_ms")] = settings.verifyQueryTimeoutMs;
    started[QStringLiteral("verify_budget_seconds")] = params.value(QStringLiteral("verify_budget_seconds")).toInt();
    started[QStringLiteral("allow_final_move_alternatives")] = settings.allowFinalMoveAlternatives;
    started[QStringLiteral("require_no_surplus")] = settings.requireNoSurplus;
    started[QStringLiteral("strict_min_length")] = settings.strictMinLength;
    started[QStringLiteral("resume")] = settings.resumeFromCheckpoint;
    if (!settings.checkpointPath.isEmpty()) started[QStringLiteral("checkpoint_path")] = settings.checkpointPath;
    if (!outputPath.isEmpty()) started[QStringLiteral("output_path")] = outputPath;
    return started;
}

} // namespace

int CliCommands::generateTsume(const QJsonObject& params)
{

    QString error;
    const auto engine = CliArgs::resolveEngine(params, &error);
    if (!engine) return CliOutput::failEvent(ErrorKind::EngineNotFound, error);
    // 範囲と既定値は CliCommandSpecs の表で決まる
    const int targetMoves = params.value(QStringLiteral("target_moves")).toInt();
    if (!TsumeshogiSettings::GeneratorLimits::isTargetMoves(targetMoves))
        return CliOutput::failEvent(ErrorKind::InvalidArgument, QStringLiteral("target_moves must be odd"));
    const int maxPositions = params.value(QStringLiteral("max_positions")).toInt();
    const int timeoutMs = params.value(QStringLiteral("timeout_ms")).toInt();
    const int maxAttack = params.value(QStringLiteral("max_attack_pieces")).toInt();
    const int maxDefend = params.value(QStringLiteral("max_defend_pieces")).toInt();
    const int attackRange = params.value(QStringLiteral("attack_range")).toInt();

    TsumeshogiGenerator::Settings settings;
    settings.enginePath = engine->path;
    settings.engineName = engine->name;
    settings.targetMoves = targetMoves;
    settings.timeoutMs = timeoutMs;
    settings.maxPositionsToFind = maxPositions;
    settings.allowFinalMoveAlternatives = params.value(QStringLiteral("allow_final_move_alternatives")).toBool();
    settings.posGenSettings.maxAttackPieces = maxAttack;
    settings.posGenSettings.maxDefendPieces = maxDefend;
    settings.posGenSettings.attackRange = attackRange;
    settings.posGenSettings.addRemainingToDefenderHand = params.value(QStringLiteral("add_remaining_to_defender_hand")).toBool();

    TsumeshogiGenerator generator;
    QEventLoop loop;
    GenerateReporter reporter(&generator, &loop);
    QObject::connect(&generator, &TsumeshogiGenerator::positionFound, &reporter, &GenerateReporter::onPositionFound);
    QObject::connect(&generator, &TsumeshogiGenerator::progressUpdated, &reporter, &GenerateReporter::onProgress);
    QObject::connect(&generator, &TsumeshogiGenerator::verificationStatsUpdated, &reporter, &GenerateReporter::onVerificationStats);
    QObject::connect(&generator, &TsumeshogiGenerator::finished, &reporter, &GenerateReporter::onFinished);
    QObject::connect(&generator, &TsumeshogiGenerator::errorOccurred, &reporter, &GenerateReporter::onError);
    CliStopWatcher watcher(params.value(QStringLiteral("stdin_control")).toBool());
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

int CliCommands::growTsume(const QJsonObject& params)
{
    QString error;
    const auto engine = CliArgs::resolveEngine(params, &error);
    if (!engine) return CliOutput::failEvent(ErrorKind::EngineNotFound, error);

    const int targetMoves = params.value(QStringLiteral("target_moves")).toInt();
    if (!TsumeshogiSettings::GeneratorLimits::isTargetMoves(targetMoves)) {
        return CliOutput::failEvent(ErrorKind::InvalidArgument, QStringLiteral("target_moves must be odd"));
    }

    const QString checkpointPath = params.value(QStringLiteral("checkpoint_path")).toString();
    const bool resume = params.value(QStringLiteral("resume")).toBool();
    const QString outputPath = params.value(QStringLiteral("output_path")).toString();
    if (const auto failed = checkGrowPaths(checkpointPath, resume, outputPath)) return *failed;

    QStringList seedSfens;
    const QString seedSfen = params.value(QStringLiteral("seed_sfen")).toString().trimmed();
    if (!seedSfen.isEmpty()) seedSfens.append(seedSfen);
    if (const auto failed = appendInputLines(params.value(QStringLiteral("seed_file")).toString(),
                                             QStringLiteral("seed_file"), seedSfens)) {
        return *failed;
    }
    QStringList excludeLines;
    if (const auto failed = appendInputLines(params.value(QStringLiteral("exclude_file")).toString(),
                                             QStringLiteral("exclude_file"), excludeLines)) {
        return *failed;
    }

    TsumeRetrogradeGenerator::Settings settings = growSettings(params, *engine);
    settings.seedSfens = seedSfens;
    settings.excludeLines = excludeLines;
    settings.checkpointPath = checkpointPath;
    settings.resumeFromCheckpoint = resume;

    TsumeRetrogradeGenerator generator;
    QEventLoop loop;
    GrowReporter reporter(&generator, &loop, outputPath, engine->name, targetMoves);
    QObject::connect(&generator, &TsumeRetrogradeGenerator::positionFound, &reporter, &GrowReporter::onPositionFound);
    QObject::connect(&generator, &TsumeRetrogradeGenerator::progressUpdated, &reporter, &GrowReporter::onProgress);
    QObject::connect(&generator, &TsumeRetrogradeGenerator::checkpointSaved, &reporter, &GrowReporter::onCheckpointSaved);
    QObject::connect(&generator, &TsumeRetrogradeGenerator::finished, &reporter, &GrowReporter::onFinished);
    QObject::connect(&generator, &TsumeRetrogradeGenerator::errorOccurred, &reporter, &GrowReporter::onError);
    CliStopWatcher watcher(params.value(QStringLiteral("stdin_control")).toBool());
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &reporter, &GrowReporter::onStopRequested);
    QObject::connect(&watcher, &CliStopWatcher::lineReceived, &reporter, &GrowReporter::onControlLine);

    CliOutput::printEvent(QStringLiteral("started"), growStartedEvent(settings, params, outputPath));

    if (!generator.start(settings)) {
        return reporter.exitCode() != 0 ? reporter.exitCode() : 1;
    }
    if (generator.isRunning()) loop.exec();
    return reporter.exitCode();
}

int CliCommands::verifyTsume(const QJsonObject& params)
{

    QString error;
    const auto engine = CliArgs::resolveEngine(params, &error);
    if (!engine) return CliOutput::fail(ErrorKind::EngineNotFound, error);
    const int targetMoves = params.value(QStringLiteral("target_moves")).toInt();
    // 範囲は表で確かめてある（生成と同じ GeneratorLimits）。奇数の規則も生成と同じ
    if (!TsumeshogiSettings::GeneratorLimits::isTargetMoves(targetMoves))
        return CliOutput::fail(ErrorKind::InvalidArgument, QStringLiteral("target_moves must be odd"));
    const int timeoutMs = params.value(QStringLiteral("timeout_seconds")).toInt() * 1000;

    const SfenValidation validation = SfenValidator::validate(params.value(QStringLiteral("sfen")).toString());
    if (!validation.valid) return CliOutput::fail(ErrorKind::InvalidArgument, QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; "))));

    // 問題の局面は mate・GUI の詰み探索と同じく、玉が片方だけなら玉の無い側（攻方）の手番にする
    const QString sfen = MateSearchPosition::attackerToMove(*validation.position).toSfen();

    TsumeVerificationRunner runner;
    QEventLoop loop;
    VerifyReporter reporter(&loop, sfen, targetMoves);
    QObject::connect(&runner, &TsumeVerificationRunner::finished, &reporter, &VerifyReporter::onFinished);
    QObject::connect(&runner, &TsumeVerificationRunner::errorOccurred, &reporter, &VerifyReporter::onError);
    CliStopWatcher watcher(false);
    QObject::connect(&watcher, &CliStopWatcher::stopRequested, &runner, &TsumeVerificationRunner::abort);

    TsumeVerificationRunner::Request request;
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.sfen = sfen;
    request.targetMoves = targetMoves;
    request.timeoutMs = timeoutMs;
    request.allowFinalMoveAlternatives = params.value(QStringLiteral("allow_final_move_alternatives")).toBool();

    // 詰将棋の局面でなければ（受方の玉がない・王手がかかっているなど）、エンジンを起動せずに status invalid を返す
    if (!runner.start(request)) {
        reporter.onFinished(TsumeshogiVerifier::Status::Invalid, {}, 0, 0);
        return reporter.exitCode();
    }
    loop.exec();
    return reporter.exitCode();
}

#include "clicommands_tsume.moc"
