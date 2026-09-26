/// @file tsumeshogi_collection_auditor.cpp
/// @brief 保存済み問題の不要駒を除去し、全1枚除去の確定判定をJSON Linesで記録する実エンジン用ツール。
/// stdin: {"sfen":"...", "target":13, "timeout_ms":15000}
/// stdout: status=minimal のときだけ、最短手数・余詰・全1枚除去の確認が完了している。
#include "tsumecollection.h"
#include "tsumeshogigenerator.h"
#include "tsumeshogipositiongenerator.h"
#include "tsumeshogiverifier.h"

#include <position.h>
#include <tsume.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <map>
#include <string>

namespace {
using Status = TsumeshogiVerifier::Status;
using Reply = TsumeshogiVerifier::Reply;
struct Answer { Reply reply = Reply::Unknown; QStringList pv; };
struct Verdict { Status status = Status::Unknown; QStringList pv; };

QString statusName(Status status)
{
    switch (status) {
    case Status::Unique: return QStringLiteral("unique");
    case Status::Multiple: return QStringLiteral("multiple");
    case Status::NoMate: return QStringLiteral("nomate");
    case Status::WrongLength: return QStringLiteral("wrong_length");
    case Status::Invalid: return QStringLiteral("invalid");
    default: return QStringLiteral("unknown");
    }
}

class Auditor
{
public:
    ~Auditor()
    {
        if (m_engine.state() == QProcess::NotRunning) return;
        send(QStringLiteral("quit"));
        if (!m_engine.waitForFinished(3000)) {
            m_engine.kill();
            m_engine.waitForFinished();
        }
    }

    bool start(const QString& path)
    {
        m_engine.setProcessChannelMode(QProcess::MergedChannels);
        m_engine.start(path, {});
        if (!m_engine.waitForStarted(5000)) return false;
        send(QStringLiteral("usi"));
        if (readUntil(QStringLiteral("usiok"), 10000).isEmpty()) return false;
        for (const auto& option : {"Threads value 1", "USI_Hash value 128",
                                   "PostSearchLevel value MinLength", "GenerateAllLegalMoves value true"})
            send(QStringLiteral("setoption name ") + QString::fromLatin1(option));
        send(QStringLiteral("isready"));
        if (readUntil(QStringLiteral("readyok"), 60000).isEmpty()) return false;
        send(QStringLiteral("usinewgame"));
        return true;
    }

    QJsonObject audit(QString sfen, int target, int budget)
    {
        QJsonObject result{{QStringLiteral("original"), sfen}, {QStringLiteral("target"), target}};
        shogi::Position position;
        if (target < 1 || target > 19 || target % 2 == 0 || budget < 1
            || !position.set_sfen(sfen.toStdString(), true)
            || position.side_to_move() != shogi::Color::Black) {
            result[QStringLiteral("status")] = QStringLiteral("invalid");
            return result;
        }
        auto base = verify(sfen, target, budget);
        if (base.status != Status::Unique) {
            result[QStringLiteral("status")] = statusName(base.status);
            return result;
        }
        int removed = 0;
        for (;;) {
            bool changed = false;
            bool unresolved = false;
            QJsonArray checks;
            for (const auto& candidate : TsumeshogiGenerator::onePieceRemovedPositions(sfen)) {
                QString reason;
                if (TsumeshogiPositionGenerator::isDefenderKingInCheck(candidate)) {
                    reason = QStringLiteral("initial_check");
                } else {
                    const int length = minimum(candidate, target, std::min(budget, 200));
                    if (length >= 0 && length != target) {
                        reason = QStringLiteral("wrong_length");
                    } else {
                        const auto verdict = verify(candidate, target, budget);
                        if (verdict.status == Status::Unique) {
                            sfen = candidate;
                            base = verdict;
                            ++removed;
                            changed = true;
                            break; // 1枚除去したら残り全駒の必要性を調べ直す。
                        }
                        reason = statusName(verdict.status);
                        // PV長だけでは最短手数の不一致を証明できない。
                        // 内蔵探索と不詰応答が食い違う場合も、駒を残す根拠にしない。
                        unresolved |= verdict.status == Status::Unknown || verdict.status == Status::Invalid
                            || verdict.status == Status::WrongLength
                            || (verdict.status == Status::NoMate && length == target);
                    }
                }
                checks.append(QJsonObject{{QStringLiteral("sfen"), candidate}, {QStringLiteral("reason"), reason}});
            }
            if (changed) continue;
            result[QStringLiteral("sfen")] = sfen;
            result[QStringLiteral("pv")] = QJsonArray::fromStringList(base.pv);
            result[QStringLiteral("removed")] = removed;
            result[QStringLiteral("checks")] = checks;
            if (unresolved) {
                result[QStringLiteral("status")] = QStringLiteral("unknown");
                return result;
            }
            // SFENごとに新しい探索表を使う有限深さ探索で最短手数も独立に確認する。
            const int length = minimum(sfen, target, budget);
            result[QStringLiteral("shortest_plies")] = length;
            const bool legal = base.pv.size() == target && TsumeCollection::validMateLine(sfen, base.pv);
            result[QStringLiteral("legal_pv")] = legal;
            result[QStringLiteral("status")] = length == target && legal
                ? QStringLiteral("minimal") : QStringLiteral("unknown");
            return result;
        }
    }

private:
    void send(const QString& line)
    {
        m_engine.write(line.toUtf8() + '\n');
        m_engine.waitForBytesWritten(1000);
    }
    QString readUntil(const QString& prefix, int budget)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < budget) {
            while (m_engine.canReadLine()) {
                const auto line = QString::fromUtf8(m_engine.readLine()).trimmed();
                if (line.startsWith(prefix)) return line;
            }
            if (m_engine.state() != QProcess::Running) return {};
            m_engine.waitForReadyRead(10);
        }
        return {};
    }
    Answer query(const QString& sfen, int budget)
    {
        const auto found = m_replies.find(sfen);
        if (found != m_replies.end()) return found->second;
        send(QStringLiteral("position sfen ") + sfen);
        send(QStringLiteral("go mate %1").arg(budget));
        const auto line = readUntil(QStringLiteral("checkmate"), budget + 5000);
        if (line.isEmpty()) {
            // 応答が欠落したエンジンを使い続け、遅延応答を別局面に適用しない。
            m_engine.kill();
            m_engine.waitForFinished();
            return {};
        }
        const auto text = line.mid(9).trimmed();
        Answer answer;
        if (text == QLatin1String("nomate")) answer.reply = Reply::NoMate;
        else if (text != QLatin1String("timeout") && text != QLatin1String("notimplemented") && !text.isEmpty()) {
            answer.reply = Reply::Mate;
            answer.pv = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
        if (answer.reply != Reply::Unknown) m_replies.emplace(sfen, answer);
        return answer;
    }
    Verdict verify(const QString& sfen, int target, int budget)
    {
        const auto key = std::make_pair(sfen, target);
        const auto found = m_verdicts.find(key);
        if (found != m_verdicts.end()) return found->second;
        TsumeshogiVerifier verifier;
        verifier.start(sfen, target, {true});
        QElapsedTimer timer;
        timer.start();
        while (verifier.result().status == Status::Running) {
            if (timer.elapsed() >= budget) verifier.abort();
            const auto next = verifier.nextPosition();
            if (verifier.result().status != Status::Running) break;
            if (next.isEmpty()) continue;
            const auto remaining = budget - timer.elapsed();
            if (remaining <= 0) {
                verifier.abort();
                break;
            }
            const auto answer = query(next, static_cast<int>(std::min<qint64>(remaining, 1000)));
            verifier.submit(answer.reply, answer.pv);
        }
        Verdict verdict{verifier.result().status, verifier.result().pv};
        if (verdict.status != Status::Unknown) m_verdicts.emplace(key, verdict);
        return verdict;
    }
    int minimum(const QString& sfen, int target, int budget)
    {
        const auto key = std::make_pair(sfen, target);
        const auto found = m_lengths.find(key);
        if (found != m_lengths.end()) return found->second;
        shogi::Position position;
        if (!position.set_sfen(sfen.toStdString(), true)) return -1;
        shogi::TsumeSearch search;
        const std::atomic_bool stop{false};
        const auto result = search.solve(position, position.side_to_move(), target, budget, stop);
        int length = -1;
        if (result.status == shogi::TsumeStatus::Mate) length = result.plies;
        if (result.status == shogi::TsumeStatus::NoMate || result.status == shogi::TsumeStatus::Limit) length = 0;
        if (length >= 0) m_lengths.emplace(key, length);
        return length;
    }

    QProcess m_engine;
    std::map<QString, Answer> m_replies;
    std::map<std::pair<QString, int>, Verdict> m_verdicts;
    std::map<std::pair<QString, int>, int> m_lengths;
};
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        std::cerr << "Usage: tsumeshogi_collection_auditor /path/to/USI-mate-engine\n";
        return 2;
    }
    Auditor auditor;
    if (!auditor.start(QString::fromLocal8Bit(argv[1]))) return 2;
    std::string line;
    while (std::getline(std::cin, line)) {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(line), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) return 2;
        const auto request = document.object();
        const auto result = auditor.audit(request.value(QStringLiteral("sfen")).toString(),
                                         request.value(QStringLiteral("target")).toInt(),
                                         request.value(QStringLiteral("timeout_ms")).toInt(15000));
        std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << std::endl;
    }
}
