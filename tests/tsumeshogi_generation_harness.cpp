/// @file tsumeshogi_generation_harness.cpp
/// @brief 詰将棋局面生成の採択率を実エンジンで測る再現ハーネス（ctest 対象外）
///
/// ランダム候補を `go mate` で調べ、目標手数のPVが返った局面に `TsumeshogiVerifier` を
/// 生成側と同じ予算配分（総予算 timeout、1問い合わせ最大1秒）で適用し、採択・棄却の内訳と
/// 棄却理由（詰む攻手の一覧）を表示する。実行例:
///
///   ./build/tests/tsumeshogi_generation_harness --engine /path/to/KomoringHeights-by-gcc --count 3000 --explain
///   ./build/tests/tsumeshogi_generation_harness --engine ... --sfen "<SFEN>" --target 3
///   ./build/tests/tsumeshogi_generation_harness --engine ... --sfen-file candidates.sfen

#include "threadtypes.h"
#include "tsumeshogipositiongenerator.h"
#include "tsumeshogiverifier.h"

#include <position.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cstdio>
#include <map>

namespace {

using Status = TsumeshogiVerifier::Status;
using Reply = TsumeshogiVerifier::Reply;

QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

struct EngineReply {
    enum Kind { Mate, NoMate, Timeout, Error } kind = Error;
    QStringList pv;
    QString raw;
    qint64 ms = 0;
};

/// USIエンジンを同期的に操作する（検証専用）
class Engine
{
public:
    bool start(const QString& path, const QStringList& options)
    {
        m_process.setProcessChannelMode(QProcess::MergedChannels);
        m_process.start(path, {});
        if (!m_process.waitForStarted(5000)) return false;
        send(QStringLiteral("usi"));
        if (readUntil(QStringLiteral("usiok"), 10000).isEmpty()) return false;
        for (const auto& option : options) send(QStringLiteral("setoption name ") + option);
        send(QStringLiteral("isready"));
        if (readUntil(QStringLiteral("readyok"), 60000).isEmpty()) return false;
        send(QStringLiteral("usinewgame"));
        return true;
    }
    void quit()
    {
        send(QStringLiteral("quit"));
        m_process.waitForFinished(3000);
    }
    EngineReply goMate(const QString& sfen, int ms)
    {
        EngineReply reply;
        QElapsedTimer timer;
        timer.start();
        send(QStringLiteral("position sfen ") + sfen);
        send(QStringLiteral("go mate %1").arg(ms));
        const QString line = readUntil(QStringLiteral("checkmate"), ms + 10000);
        reply.ms = timer.elapsed();
        reply.raw = line;
        ++m_queries;
        if (line.isEmpty()) return reply;
        const QString rest = line.mid(9).trimmed();
        if (rest == QLatin1String("nomate")) reply.kind = EngineReply::NoMate;
        else if (rest == QLatin1String("timeout") || rest.isEmpty()) reply.kind = EngineReply::Timeout;
        else if (rest == QLatin1String("notimplemented")) reply.kind = EngineReply::Error;
        else {
            reply.kind = EngineReply::Mate;
            reply.pv = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
        return reply;
    }
    /// 同一局面の再問い合わせを避ける（1局面の検査・説明の間だけ有効）
    EngineReply cachedGoMate(const QString& sfen, int ms)
    {
        const auto found = m_cache.find(sfen);
        if (found != m_cache.end()) return found->second;
        const auto reply = goMate(sfen, ms);
        m_cache[sfen] = reply;
        return reply;
    }
    void clearCache() { m_cache.clear(); }
    int queries() const { return m_queries; }

private:
    void send(const QString& line)
    {
        m_process.write(line.toUtf8() + '\n');
        m_process.waitForBytesWritten(1000);
    }
    QString readUntil(const QString& prefix, int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            while (m_process.canReadLine()) {
                const QString line = QString::fromUtf8(m_process.readLine()).trimmed();
                if (line.startsWith(prefix)) return line;
            }
            if (m_process.state() != QProcess::Running) return {};
            m_process.waitForReadyRead(20);
        }
        return {};
    }

    QProcess m_process;
    std::map<QString, EngineReply> m_cache;
    int m_queries = 0;
};

const char* statusName(Status status)
{
    switch (status) {
    case Status::Running: return "Running";
    case Status::Unique: return "Unique";
    case Status::Multiple: return "Multiple";
    case Status::NoMate: return "NoMate";
    case Status::WrongLength: return "WrongLength";
    case Status::Unknown: return "Unknown";
    case Status::Invalid: return "Invalid";
    }
    return "?";
}

struct VerifyOutcome {
    Status status = Status::Invalid;
    int queries = 0;
    qint64 ms = 0;
    QStringList pv;
    QStringList log;
};

/// TsumeshogiGenerator::continueVerification() と同じ予算配分で検査する
VerifyOutcome verify(Engine& engine, const QString& sfen, int target, int budgetMs, bool allowFinal)
{
    VerifyOutcome outcome;
    TsumeshogiVerifier verifier;
    verifier.start(sfen, target, {allowFinal});
    QElapsedTimer timer;
    timer.start();
    while (verifier.result().status == Status::Running) {
        if (timer.elapsed() >= budgetMs) verifier.abort();
        const QString query = verifier.nextPosition();
        if (verifier.result().status != Status::Running) break;
        if (query.isEmpty()) continue;
        const qint64 remaining = budgetMs - timer.elapsed();
        if (remaining <= 0) {
            verifier.abort();
            break;
        }
        const int timeout = static_cast<int>(std::min<qint64>(remaining, 1000));
        ++outcome.queries;
        const auto reply = engine.cachedGoMate(query, timeout);
        outcome.log.append(query + QStringLiteral("  => ") + reply.raw);
        if (reply.kind == EngineReply::Mate) verifier.submit(Reply::Mate, reply.pv);
        else if (reply.kind == EngineReply::NoMate) verifier.submit(Reply::NoMate);
        else verifier.submit(Reply::Unknown);
    }
    outcome.status = verifier.result().status;
    outcome.pv = verifier.result().pv;
    outcome.ms = timer.elapsed();
    return outcome;
}

/// 攻方手番の局面で「全応手が詰む攻手」を列挙し、唯一なら主手順の応手ごとに再帰する（棄却理由の説明用）
void explainNode(Engine& engine, const shogi::Position& position, int remaining, const QString& indent)
{
    struct Winner {
        QString usi;
        bool immediate = false;
        int longest = 0;
        std::vector<std::pair<shogi::Position, QString>> children;
    };
    std::vector<Winner> winners;
    int unknownMoves = 0;
    const auto moves = position.generate_checking_moves();
    for (const auto& move : moves) {
        auto after = position;
        after.do_move(move);
        const auto defenses = after.generate_legal_moves();
        Winner winner;
        winner.usi = QString::fromStdString(position.move_to_usi(move));
        winner.immediate = defenses.empty();
        bool fail = false;
        bool unknown = false;
        for (const auto& defense : defenses) {
            auto child = after;
            child.do_move(defense);
            const auto reply = engine.cachedGoMate(QString::fromStdString(child.to_sfen()), 1000);
            if (reply.kind == EngineReply::NoMate) {
                fail = true;
                break;
            }
            if (reply.kind != EngineReply::Mate) {
                unknown = true;
                continue;
            }
            winner.longest = std::max<int>(winner.longest, static_cast<int>(reply.pv.size()));
            winner.children.emplace_back(child, QString::fromStdString(after.move_to_usi(defense)));
        }
        if (fail) continue;
        if (unknown) {
            ++unknownMoves;
            continue;
        }
        winners.push_back(std::move(winner));
    }
    QStringList names;
    for (const auto& winner : winners)
        names << (winner.usi + (winner.immediate ? QStringLiteral("(即詰)")
                                                 : QStringLiteral("(%1手)").arg(winner.longest + 2)));
    out() << indent << "王手" << moves.size() << "手 / 詰む攻手" << winners.size() << "手"
          << (unknownMoves ? QStringLiteral(" / 不明%1手").arg(unknownMoves) : QString())
          << ": " << names.join(QStringLiteral(", ")) << "\n";
    if (winners.size() != 1 || remaining <= 1) return;
    const auto& winner = winners.front();
    for (const auto& [child, defense] : winner.children) {
        out() << indent << "  " << winner.usi << " " << defense << " の後:\n";
        explainNode(engine, child, remaining - 2, indent + QStringLiteral("    "));
    }
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("engine"), QStringLiteral("USI mate engine path"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("count"), QStringLiteral("random positions to try"), QStringLiteral("n"), QStringLiteral("300")});
    parser.addOption({QStringLiteral("target"), QStringLiteral("target moves"), QStringLiteral("n"), QStringLiteral("3")});
    parser.addOption({QStringLiteral("timeout"), QStringLiteral("ms per search and per verification"), QStringLiteral("ms"), QStringLiteral("5000")});
    parser.addOption({QStringLiteral("hash"), QStringLiteral("USI_Hash MB"), QStringLiteral("mb"), QStringLiteral("1024")});
    parser.addOption({QStringLiteral("threads"), QStringLiteral("Threads"), QStringLiteral("n"), QStringLiteral("4")});
    parser.addOption({QStringLiteral("attack"), QStringLiteral("maxAttackPieces"), QStringLiteral("n"), QStringLiteral("4")});
    parser.addOption({QStringLiteral("defend"), QStringLiteral("maxDefendPieces"), QStringLiteral("n"), QStringLiteral("1")});
    parser.addOption({QStringLiteral("range"), QStringLiteral("attackRange"), QStringLiteral("n"), QStringLiteral("3")});
    parser.addOption({QStringLiteral("sfen"), QStringLiteral("verify only this sfen (prints every query)"), QStringLiteral("sfen")});
    parser.addOption({QStringLiteral("sfen-file"), QStringLiteral("verify the sfens listed in file"), QStringLiteral("file")});
    parser.addOption({QStringLiteral("explain"), QStringLiteral("list the mating moves of rejected positions")});
    parser.process(app);
    if (!parser.isSet(QStringLiteral("engine"))) {
        out() << "--engine is required\n";
        return 1;
    }

    Engine engine;
    const QStringList options{
        QStringLiteral("Threads value %1").arg(parser.value(QStringLiteral("threads"))),
        QStringLiteral("USI_Hash value %1").arg(parser.value(QStringLiteral("hash"))),
        QStringLiteral("MultiPV value 1"), QStringLiteral("GenerateAllLegalMoves value true"),
        QStringLiteral("PostSearchLevel value MinLength"), QStringLiteral("NodesLimit value 0"),
        QStringLiteral("RootIsAndNodeIfChecked value true")};
    if (!engine.start(parser.value(QStringLiteral("engine")), options)) {
        out() << "engine start failed\n";
        return 1;
    }
    const int target = parser.value(QStringLiteral("target")).toInt();
    const int timeout = parser.value(QStringLiteral("timeout")).toInt();

    QStringList candidates;
    if (parser.isSet(QStringLiteral("sfen"))) {
        candidates << parser.value(QStringLiteral("sfen"));
    } else if (parser.isSet(QStringLiteral("sfen-file"))) {
        QFile file(parser.value(QStringLiteral("sfen-file")));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (!line.isEmpty()) candidates << line;
            }
        }
    } else {
        TsumeshogiPositionGenerator::Settings settings;
        settings.maxAttackPieces = parser.value(QStringLiteral("attack")).toInt();
        settings.maxDefendPieces = parser.value(QStringLiteral("defend")).toInt();
        settings.attackRange = parser.value(QStringLiteral("range")).toInt();
        const QStringList all = TsumeshogiPositionGenerator::generateBatch(
            settings, parser.value(QStringLiteral("count")).toInt(), CancelFlag{});
        int nomate = 0, mateOther = 0, timeouts = 0, errors = 0;
        qint64 ms = 0;
        std::map<int, int> lengths;
        for (const auto& sfen : all) {
            const auto reply = engine.goMate(sfen, timeout);
            ms += reply.ms;
            if (reply.kind == EngineReply::NoMate) ++nomate;
            else if (reply.kind == EngineReply::Timeout) ++timeouts;
            else if (reply.kind == EngineReply::Error) ++errors;
            else {
                ++lengths[static_cast<int>(reply.pv.size())];
                if (reply.pv.size() == target) candidates << sfen;
                else ++mateOther;
            }
        }
        out() << "=== 候補探索: " << all.size() << "局面, nomate=" << nomate << " mate(" << target << ")=" << candidates.size()
              << " mate(other)=" << mateOther << " timeout=" << timeouts << " error=" << errors
              << " 平均" << (all.isEmpty() ? 0 : ms / all.size()) << "ms\n    PV長分布:";
        for (const auto& [length, n] : lengths) out() << " " << length << "手:" << n;
        out() << "\n";
        out().flush();
    }

    std::map<Status, int> allowFinal, strictFinal;
    for (const auto& sfen : candidates) {
        engine.clearCache();
        const auto tolerant = verify(engine, sfen, target, timeout, true);
        ++allowFinal[tolerant.status];
        out() << "--- " << sfen << "\n    最終手許容=" << statusName(tolerant.status) << " queries=" << tolerant.queries
              << " ms=" << tolerant.ms << " pv=" << tolerant.pv.join(QLatin1Char(' ')) << "\n";
        if (parser.isSet(QStringLiteral("sfen")))
            for (const auto& line : tolerant.log) out() << "      " << line << "\n";
        const auto strict = verify(engine, sfen, target, timeout, false);
        ++strictFinal[strict.status];
        out() << "    最終手厳格=" << statusName(strict.status) << " queries=" << strict.queries
              << " ms=" << strict.ms << " pv=" << strict.pv.join(QLatin1Char(' ')) << "\n";
        if (parser.isSet(QStringLiteral("explain")) && tolerant.status != Status::Unique) {
            shogi::Position root;
            if (root.set_sfen(sfen.toStdString(), true)) explainNode(engine, root, target, QStringLiteral("    "));
        }
        out().flush();
    }
    out() << "=== 検査結果(最終手許容):";
    for (const auto& [status, n] : allowFinal) out() << " " << statusName(status) << "=" << n;
    out() << "\n=== 検査結果(最終手厳格):";
    for (const auto& [status, n] : strictFinal) out() << " " << statusName(status) << "=" << n;
    out() << "\n";
    engine.quit();
    return 0;
}
