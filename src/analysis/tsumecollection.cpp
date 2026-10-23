#include "tsumecollection.h"
#include "enginemovevalidator.h"
#include "positioncollectionfile.h"
#include "positionvalidator.h"
#include <algorithm>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// Q_INIT_RESOURCE は名前空間の外で宣言し、静的ライブラリからもリソースをリンクする。
// 公開した版ごとの監査記録（20260926版と、駒余りを検査して差し替えた20261001版）を同梱する。
static const QList<QJsonObject>& trustedCollectionReports()
{
    static const QList<QJsonObject> reports = []() {
        Q_INIT_RESOURCE(tsumeshogi);
        QList<QJsonObject> loaded;
        for (const auto* name : {"validated-collections-20260926.json", "validated-collections-20261001.json"}) {
            QFile file(QStringLiteral(":/tsumeshogi/") + QLatin1String(name));
            if (file.open(QIODevice::ReadOnly)) loaded.append(QJsonDocument::fromJson(file.readAll()).object());
        }
        return loaded;
    }();
    return reports;
}

/// 監査記録 report に、内容のハッシュ hash が一致するファイルがあれば、その詰み手数を返す（なければ0）
static int verifiedMateLengthIn(const QJsonObject& report, const QString& hash,
                                const TsumeCollection::Result& parsed)
{
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

int TsumeCollection::verifiedMateLength(const QByteArray& contents, const Result& parsed)
{
    if (parsed.problems.isEmpty() || !parsed.invalidLines.isEmpty()
        || parsed.positionIds.size() != parsed.problems.size()) return 0;
    // 隣接するJSONやファイル名を根拠にしない。同梱した監査記録だけを信頼する。
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex());
    for (const auto& report : trustedCollectionReports()) {
        if (report.value(QStringLiteral("method")).toString() != QStringLiteral("all_single_piece_removals_to_defender_hand")
            || report.value(QStringLiteral("independent_shortest_search")).toString()
               != QStringLiteral("Hayanagi TsumeSearch with a fresh table per SFEN")
            || !report.value(QStringLiteral("allow_final_move_alternatives")).toBool()) continue;
        const int plies = verifiedMateLengthIn(report, hash, parsed);
        if (plies > 0) return plies;
    }
    return 0;
}

QString TsumeCollection::positionId(const QString& sfen)
{
    const std::optional<Position> position = Position::fromSfen(sfen);
    if (!position || PositionValidator::boardProblem(*position) != PositionValidator::Problem::None) return {};
    return position->positionKey();
}

bool TsumeCollection::validMateLine(const QString& sfen, const QStringList& pv)
{
    if (pv.isEmpty() || pv.size() % 2 != 1) return false;
    std::optional<Position> position = Position::fromSfen(sfen);
    if (!position || PositionValidator::boardProblem(*position) != PositionValidator::Problem::None) return false;
    const Turn attacker = position->sideToMove();
    for (const QString& text : pv) {
        // 攻方の手はすべて王手で、どの手も合法手
        const bool attack = position->sideToMove() == attacker;
        const std::optional<UsiMove> move = UsiMove::parse(text);
        if (!move || !EngineMoveValidator::applyIfLegal(*position, *move)) return false;
        if (attack && !EngineMoveValidator::isInCheck(*position, position->sideToMove())) return false;
    }
    const EngineMoveValidator::Context end = EngineMoveValidator::load(*position);
    return EngineMoveValidator::checkCount(end) > 0 && EngineMoveValidator::legalMoveCount(end) == 0;
}

TsumeCollection::Result TsumeCollection::parse(const QString& text)
{
    Result result;
    const PositionCollectionFile::Contents contents = PositionCollectionFile::parse(text);
    result.invalidLines = contents.invalidLines;
    for (const auto& line : contents.lines) {
        // position コマンドの行は指した後の局面を問題にし、SFEN に添えた手順は参考手順として持つ
        const bool played = line.movesPlayed && !line.moves.isEmpty();
        TsumeProblem problem{played ? line.position() : line.startSfen, line.referenceMoves(), line.lineNumber};
        // 玉方（手番でない側）の玉があり、王手がかかっていない局面だけを問題にする
        const std::optional<Position> position = Position::fromSfen(problem.sfen);
        bool valid = position.has_value();
        if (valid) {
            const PositionValidator::Report report = PositionValidator::check(*position);
            valid = report.isValid() && (position->sideToMove() == Turn::Black ? report.whiteKing : report.blackKing);
        }
        if (valid) {
            result.positionIds.append(position->positionKey());
            result.problems.append(problem);
        } else {
            result.invalidLines.append(line.lineNumber);
        }
    }
    std::sort(result.invalidLines.begin(), result.invalidLines.end());
    return result;
}
