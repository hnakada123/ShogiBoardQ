/// @file tsumeshogi_diversity_sampler.cpp
/// @brief 問題集補充用の未検証候補をJSONLに出力する。採択判断はPython収集側で一元管理する。
#include "tsumeshogi_seedmutation.h"
#include "tsumeshogigenerator.h"
#include <position.h>
#include <tsume.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <atomic>
#include <iostream>

namespace {
bool reaches(int piece, int source, int destination, const seed_mutation::Position& position)
{
    const int dx = destination % 9 - source % 9;
    const int dy = destination / 9 - source / 9;
    if (dx == 0 && dy == 0) return false;
    if (piece >= 11 && piece <= 14) piece = 5;
    bool sliding = false;
    switch (piece) {
    case 1: return dx == 0 && dy == -1;
    case 2: sliding = dx == 0 && dy < 0; break;
    case 3: return std::abs(dx) == 1 && dy == -2;
    case 4: return (dy == -1 && std::abs(dx) <= 1) || (dy == 1 && std::abs(dx) == 1);
    case 5: return (dy == -1 && std::abs(dx) <= 1) || (dy == 0 && std::abs(dx) == 1)
                || (dy == 1 && dx == 0);
    case 16:
        if (std::abs(dx) + std::abs(dy) == 1) return true;
        [[fallthrough]];
    case 6: sliding = std::abs(dx) == std::abs(dy); break;
    case 17:
        if (std::abs(dx) == 1 && std::abs(dy) == 1) return true;
        [[fallthrough]];
    case 7: sliding = dx == 0 || dy == 0; break;
    default: return false;
    }
    if (!sliding) return false;
    const int step = (dx > 0 ? 1 : dx < 0 ? -1 : 0) + (dy > 0 ? 9 : dy < 0 ? -9 : 0);
    for (int sq = source + step; sq != destination; sq += step)
        if (position.board[sq] != 0) return false;
    return true;
}

// 既知局面の直前の王手と、玉移動または合駒を逆算する。
QString predecessor(const QList<seed_mutation::Position>& seeds,
                    const TsumeshogiPositionGenerator::Settings& settings)
{
    if (seeds.isEmpty()) return {};
    auto* rng = QRandomGenerator::global();
    auto position = seeds.at(rng->bounded(static_cast<int>(seeds.size())));
    int king = -1;
    QList<int> attackers;
    for (int i = 0; i < 81; ++i) {
        if (position.board[i] == -8) king = i;
        if (position.board[i] > 0) attackers.append(i);
    }
    if (king < 0 || attackers.isEmpty()) return {};
    QList<std::pair<int, int>> interpositions;
    for (int sq = 0; sq < 81; ++sq) {
        // 成駒は打てない。合駒を戻すと、未使用駒の計算により玉方持駒へ戻る。
        if (position.board[sq] >= 0 || position.board[sq] <= -8) continue;
        auto without = position;
        without.board[sq] = 0;
        for (const int attacker : attackers)
            if (reaches(without.board[attacker], attacker, king, without))
                interpositions.append({sq, attacker});
    }
    QList<std::pair<int, int>> checks;
    position.board[king] = 0;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int df = -1; df <= 1; ++df) {
            const int rank = king / 9 + dr;
            const int file = king % 9 + df;
            if ((dr == 0 && df == 0) || rank < 0 || rank >= 9 || file < 0 || file >= 9) continue;
            const int previous = rank * 9 + file;
            if (position.board[previous] != 0) continue;
            for (const int attacker : attackers)
                if (reaches(position.board[attacker], attacker, previous, position))
                    checks.append({previous, attacker});
        }
    }
    position.board[king] = -8;
    int destination = -1;
    if (!interpositions.isEmpty() && (checks.isEmpty() || rng->bounded(2) == 0)) {
        const auto selected = interpositions.at(rng->bounded(static_cast<int>(interpositions.size())));
        position.board[selected.first] = 0;
        destination = selected.second;
    } else {
        if (checks.isEmpty()) return {};
        const auto selected = checks.at(rng->bounded(static_cast<int>(checks.size())));
        const int previousKing = selected.first;
        destination = selected.second;
        position.board[king] = 0;
        position.board[previousKing] = -8;
        // 玉が直前に攻め駒を取った場合も候補に含める。
        if (rng->bounded(100) < 20) position.board[king] = rng->bounded(1, 8);
    }
    const int piece = position.board[destination];
    const QString afterCheck = seed_mutation::sfen(position, settings);
    shogi::Position probe;
    if (afterCheck.isEmpty() || !probe.set_sfen(afterCheck.toStdString(), true)
        || !probe.is_in_check(shogi::Color::White)) return {};
    position.board[destination] = 0;
    if (piece < 10 && rng->bounded(100) < 25) {
        ++position.hand[piece - 1]; // 直前の駒打ちを戻す。
    } else {
        const int previousPiece = piece > 10 && rng->bounded(2) == 0 ? piece % 10 : piece;
        QList<int> sources;
        for (int sq = 0; sq < 81; ++sq) {
            if (position.board[sq] != 0 || sq == destination) continue;
            if (previousPiece != piece && sq / 9 > 2 && destination / 9 > 2) continue;
            if (reaches(previousPiece, sq, destination, position)) sources.append(sq);
        }
        if (sources.isEmpty()) return {};
        const int source = sources.at(rng->bounded(static_cast<int>(sources.size())));
        position.board[source] = previousPiece;
        // 捕獲を伴う王手を戻す場合は、その駒を攻方持駒から盤上の玉方駒へ戻す。
        if (rng->bounded(100) < 20) {
            QList<int> held;
            for (int i = 0; i < 7; ++i) if (position.hand[i] > 0) held.append(i);
            if (!held.isEmpty()) {
                const int captured = held.at(rng->bounded(static_cast<int>(held.size())));
                --position.hand[captured];
                position.board[destination] = -(captured + 1);
            }
        }
    }
    const QString candidate = seed_mutation::sfen(position, settings);
    if (candidate.isEmpty() || !probe.set_sfen(candidate.toStdString(), true)
        || probe.is_in_check(shogi::Color::White)) return {};
    return candidate;
}

QString grow(const QList<seed_mutation::Position>& seeds,
             const TsumeshogiPositionGenerator::Settings& settings, int depth, int budget,
             const QSet<QString>& known, QSet<QString>& tried)
{
    if (seeds.isEmpty()) return {};
    auto* rng = QRandomGenerator::global();
    auto seed = seeds.at(rng->bounded(static_cast<int>(seeds.size())));
    if (seed.matePlies >= depth) return {};
    const int steps = std::clamp((depth - seed.matePlies) / 2, 1, 3);
    int currentPlies = seed.matePlies;
    const std::atomic_bool stop{false};
    QString candidate;
    for (int step = 0; step < steps; ++step) {
        QString previous;
        for (int attempt = 0; attempt < 16 && previous.isEmpty(); ++attempt) {
            const QString proposed = predecessor({seed}, settings);
            if (proposed.isEmpty() || known.contains(proposed) || tried.contains(proposed)) continue;
            tried.insert(proposed);
            shogi::Position position;
            if (!position.set_sfen(proposed.toStdString(), true)) continue;
            shogi::TsumeSearch search;
            const int expected = currentPlies + 2;
            const auto proof = search.solve(position, shogi::Color::Black, expected, std::min(10, budget), stop);
            if (proof.status != shogi::TsumeStatus::Mate || proof.plies != expected) continue;
            int mating = 0;
            for (const auto& move : position.generate_checking_moves()) {
                auto child = position;
                child.do_move(move);
                const auto reply = search.solve(child, shogi::Color::Black, expected - 1, std::min(10, budget), stop);
                if (reply.status == shogi::TsumeStatus::Mate && ++mating >= 2) break;
            }
            if (mating < 2) previous = proposed;
        }
        if (previous.isEmpty()) break;
        candidate = previous;
        currentPlies += 2;
        const auto positions = seed_mutation::loadContents(previous);
        if (positions.isEmpty()) break;
        seed = positions.first();
    }
    return candidate;
}

bool hasMultipleMates(shogi::Position& position, shogi::TsumeSearch& search,
                      int plies, int budget, const std::atomic_bool& stop)
{
    int mating = 0;
    for (const auto& move : position.generate_checking_moves()) {
        auto child = position;
        child.do_move(move);
        const auto reply = search.solve(child, shogi::Color::Black, plies - 1, budget, stop);
        if (reply.status == shogi::TsumeStatus::Mate && ++mating >= 2) return true;
    }
    return false;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc == 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--removals")) {
        std::string line;
        while (std::getline(std::cin, line)) {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(line), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) return 2;
            const auto sfen = document.object().value(QStringLiteral("sfen")).toString();
            const auto positions = TsumeshogiGenerator::onePieceRemovedPositions(sfen);
            std::cout << QJsonDocument(QJsonArray::fromStringList(positions)).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        }
        return 0;
    }
    if (argc != 6) {
        std::cerr << "Usage: tsumeshogi_diversity_sampler seed-file max-plies screen-ms worker seconds\n";
        return 2;
    }
    const QString seedFile = QString::fromLocal8Bit(argv[1]);
    const int depth = QString::fromLocal8Bit(argv[2]).toInt();
    const int budget = QString::fromLocal8Bit(argv[3]).toInt();
    const int worker = QString::fromLocal8Bit(argv[4]).toInt();
    const int seconds = QString::fromLocal8Bit(argv[5]).toInt();
    if (depth < 3 || depth > 19 || depth % 2 == 0 || budget < 1 || worker < 0 || seconds < 1)
        return 2;
    TsumeshogiPositionGenerator::Settings settings;
    // 4ワーカーごとの種の補充担当にも、異なる駒数設定が割り当たるようにする。
    settings.maxAttackPieces = 5 + (worker * 7 + worker / 4) % 4;
    settings.maxDefendPieces = 1 + worker % 3;
    settings.attackRange = 2 + worker % 3;
    TsumeshogiPositionGenerator generator;
    generator.setSettings(settings);
    auto mutationSettings = settings;
    // 他ワーカーの種を、ランダム配置用の小さい駒数上限だけで捨てない。
    // 駒が多い候補も、採択時には全1枚除去の検査を必ず通す。
    mutationSettings.maxAttackPieces = 9;
    mutationSettings.maxDefendPieces = 4;
    QList<seed_mutation::Position> seeds;
    QList<seed_mutation::Position> mutationSeeds;
    QList<seed_mutation::Position> growSeeds;
    QSet<QString> seen;
    QSet<QString> known;
    QSet<QString> growthTried;
    qint64 knownSize = -1;
    const std::atomic_bool stop{false};
    QElapsedTimer timer;
    timer.start();
    int generated = 0;
    int candidates = 0;
    int minimumPlies = 3;
    int growthTarget = depth;
    while (timer.elapsed() < static_cast<qint64>(seconds) * 1000) {
        if (generated % 1000 == 0) {
            seeds = seed_mutation::load(seedFile);
            growSeeds.clear();
            QFile targetFile(seedFile + QStringLiteral(".target"));
            if (targetFile.open(QIODevice::ReadOnly))
                growthTarget = std::clamp(QString::fromUtf8(targetFile.readAll()).toInt(), 3, depth);
            mutationSeeds.clear();
            for (const auto& seed : seeds) {
                if (seed.matePlies < growthTarget - 2) continue;
                mutationSeeds.append(seed);
                if (seed.matePlies == growthTarget) mutationSeeds.append(seed);
            }
            for (const auto& seed : seed_mutation::load(seedFile + QStringLiteral(".certified")))
                if (seed.matePlies >= growthTarget - 4 && seed.matePlies < growthTarget)
                    growSeeds.append(seed);
            QFile minimumFile(seedFile + QStringLiteral(".minimum"));
            if (minimumFile.open(QIODevice::ReadOnly))
                minimumPlies = std::clamp(QString::fromUtf8(minimumFile.readAll()).toInt(), 3, depth);
            QFile knownFile(QFileInfo(seedFile).absolutePath() + QStringLiteral("/known_sfens.txt"));
            if (knownFile.exists() && knownFile.size() != knownSize && knownFile.open(QIODevice::ReadOnly)) {
                const auto lines = QString::fromUtf8(knownFile.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                known = QSet<QString>(lines.cbegin(), lines.cend());
                knownSize = knownFile.size();
            }
            if (seen.size() > 200000) seen.clear();
            if (growthTried.size() > 200000) growthTried.clear();
            std::cerr << "generated=" << generated << " candidates=" << candidates
                      << " seeds=" << seeds.size() << " ms=" << timer.elapsed() << '\n';
        }
        ++generated;
        const int mode = seeds.isEmpty() ? 99 : QRandomGenerator::global()->bounded(100);
        // 長手数の補充では同じ手数の種を多めに変更する。短い種の逆算と新規配置も併用する。
        // 13手詰は9手詰から2段階の逆算も行い、11手詰の既知の接頭辞だけに偏らせない。
        const int growthLimit = growthTarget >= 13 ? (worker % 3 == 0 ? 70 : 35) : 40;
        const int mutationLimit = growthTarget >= 13 ? 90 : 75;
        const QString sfen = mode < growthLimit ? grow(growSeeds, mutationSettings, growthTarget, budget, known, growthTried) : mode < mutationLimit
            ? seed_mutation::sample(mutationSeeds.isEmpty() ? seeds : mutationSeeds, mutationSettings) : generator.generate();
        if (sfen.isEmpty() || seen.contains(sfen) || known.contains(sfen)) continue;
        seen.insert(sfen);
        shogi::Position position;
        if (!position.set_sfen(sfen.toStdString(), true) || position.is_in_check(shogi::Color::White)) continue;
        shogi::TsumeSearch search; // SFENをまたいだ探索表の再利用をしない。
        auto result = search.solve(position, shogi::Color::Black, depth, budget, stop);
        if (result.status != shogi::TsumeStatus::Mate || result.plies < minimumPlies) continue;
        const int plies = result.plies;
        if (hasMultipleMates(position, search, plies, budget, stop)) continue;
        // 最短手数と主手順の候補だけ。余詰・駒の必要性は collection_auditor が別途確認する。
        QJsonArray pv;
        auto after = position;
        for (int remaining = plies; remaining > 0; --remaining) {
            if (result.status != shogi::TsumeStatus::Mate || result.plies != remaining) break;
            // 主手順の途中で判明する余詰も早めに捨てる。最終手の複数解は許容する。
            // 判定不能はここでは除外せず、採択時の実エンジン検査へ渡す。
            if (remaining < plies && remaining > 1 && remaining % 2 == 1
                && hasMultipleMates(after, search, remaining, std::min(5, budget), stop)) break;
            pv.append(QString::fromStdString(after.move_to_usi(result.move)));
            if (!after.do_move(result.move)) break;
            if (remaining > 1) result = search.solve(after, shogi::Color::Black, remaining - 1, budget, stop);
        }
        if (pv.size() != plies) continue;
        ++candidates;
        const QJsonObject event{{QStringLiteral("sfen"), sfen}, {QStringLiteral("pv"), pv},
                                {QStringLiteral("target"), plies},
                                {QStringLiteral("source"), mode < growthLimit ? QStringLiteral("predecessor") : mode < mutationLimit
                                    ? QStringLiteral("mutation") : QStringLiteral("random")}};
        std::cout << QJsonDocument(event).toJson(QJsonDocument::Compact).toStdString() << std::endl;
    }
}
