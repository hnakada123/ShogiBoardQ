#include "tsumecollection.h"
#include "position.h"
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// Q_INIT_RESOURCE は名前空間の外で宣言し、静的ライブラリからもリソースをリンクする。
static const QJsonObject& trustedCollectionReport()
{
    static const QJsonObject report = []() {
        Q_INIT_RESOURCE(tsumeshogi);
        QFile file(QStringLiteral(":/tsumeshogi/validated-collections.json"));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    return report;
}

int TsumeCollection::verifiedMateLength(const QByteArray& contents, const Result& parsed)
{
    if (parsed.problems.isEmpty() || !parsed.invalidLines.isEmpty()
        || parsed.positionIds.size() != parsed.problems.size()) return 0;
    // 隣接するJSONやファイル名を根拠にしない。同梱した監査記録だけを信頼する。
    const auto& report = trustedCollectionReport();
    if (report.value(QStringLiteral("method")).toString() != QStringLiteral("all_single_piece_removals_to_defender_hand")
        || report.value(QStringLiteral("independent_shortest_search")).toString()
           != QStringLiteral("Hayanagi TsumeSearch with a fresh table per SFEN")
        || !report.value(QStringLiteral("allow_final_move_alternatives")).toBool()) return 0;
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex());
    const QJsonArray files = report.value(QStringLiteral("files")).toArray();
    for (const auto& entry : files) {
        const auto file = entry.toObject();
        if (file.value(QStringLiteral("sha256")).toString() != hash) continue;
        const int count = file.value(QStringLiteral("positions")).toInt();
        const int plies = file.value(QStringLiteral("target_plies")).toInt();
        if (count != parsed.problems.size() || plies < 1 || plies % 2 != 1
            || file.value(QStringLiteral("unique_positions")).toInt() != count
            || file.value(QStringLiteral("legal_mate_lines")).toInt() != count
            || file.value(QStringLiteral("shortest_mate_exact_target")).toInt() != count
            || file.value(QStringLiteral("positions_with_removable_piece")).toInt(-1) != 0
            || file.value(QStringLiteral("inconclusive_positions")).toInt(-1) != 0) return 0;
        for (const auto& problem : parsed.problems)
            if (problem.referenceMoves.size() != plies) return 0;
        return plies;
    }
    return 0;
}

QString TsumeCollection::positionId(const QString& sfen)
{
    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) return {};
    return QString::fromStdString(position.to_sfen()).section(QLatin1Char(' '), 0, 2);
}

bool TsumeCollection::validMateLine(const QString& sfen, const QStringList& pv)
{
    if (pv.isEmpty() || pv.size() % 2 != 1) return false;
    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) return false;
    const auto attacker = position.side_to_move();
    for (const auto& move : pv) {
        const bool attack = position.side_to_move() == attacker;
        if (!position.apply_usi_move(move.toStdString())) return false;
        if (attack && !position.is_in_check(position.side_to_move())) return false;
    }
    return position.is_in_check(position.side_to_move()) && position.generate_legal_moves().empty();
}

TsumeCollection::Result TsumeCollection::parse(const QString& text)
{
    Result result;
    const auto lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression movePattern(QStringLiteral("^(?:[1-9][a-i][1-9][a-i]\\+?|[PLNSGBR]\\*[1-9][a-i])$"));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.startsWith(QChar(0xfeff))) line.remove(0, 1);
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        auto fields = line.simplified().split(QLatin1Char(' '));
        if (fields.first() == QStringLiteral("position")) fields.removeFirst();
        if (!fields.isEmpty() && fields.first() == QStringLiteral("sfen")) fields.removeFirst();
        const int lineNumber = static_cast<int>(i + 1);
        if (fields.size() < 4) { result.invalidLines.append(lineNumber); continue; }
        TsumeProblem problem{fields.mid(0, 4).join(QLatin1Char(' ')), {}, lineNumber};
        shogi::Position position;
        bool valid = position.set_sfen(problem.sfen.toStdString(), true);
        if (valid) {
            const auto defender = shogi::opposite(position.side_to_move());
            valid = position.find_king(defender) >= 0 && !position.is_in_check(defender);
        }
        if (fields.size() > 4) {
            valid = valid && fields[4] == QStringLiteral("moves") && fields.size() > 5;
            problem.referenceMoves = fields.mid(5);
            for (const auto& move : std::as_const(problem.referenceMoves))
                valid = valid && movePattern.match(move).hasMatch();
        }
        if (valid) {
            result.positionIds.append(QString::fromStdString(position.to_sfen()).section(QLatin1Char(' '), 0, 2));
            result.problems.append(problem);
        }
        else result.invalidLines.append(lineNumber);
    }
    return result;
}
