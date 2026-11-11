/// @file matchverificationservice.cpp
/// @brief P2P対局の証跡ファイルの検証の実装

#include "matchverificationservice.h"

#include <QJsonArray>

#include "boundedfile.h"
#include "inputpath.h"
#include "p2pevidenceverifier.h"

namespace MatchVerificationService {

namespace {

QString hex(const QByteArray& bytes) { return QString::fromLatin1(bytes.toHex()); }

QString categoryName(P2p::EvidenceReport::Category category)
{
    switch (category) {
    case P2p::EvidenceReport::Category::MutuallySigned: return QStringLiteral("mutually_signed");
    case P2p::EvidenceReport::Category::DeterminedByRecord: return QStringLiteral("determined_by_record");
    case P2p::EvidenceReport::Category::Unconfirmed: return QStringLiteral("unconfirmed");
    }
    return QStringLiteral("unconfirmed");
}

QString outcomeName(P2p::Outcome outcome)
{
    switch (outcome) {
    case P2p::Outcome::BlackWins: return QStringLiteral("black_wins");
    case P2p::Outcome::WhiteWins: return QStringLiteral("white_wins");
    case P2p::Outcome::Draw: return QStringLiteral("draw");
    case P2p::Outcome::Aborted: return QStringLiteral("aborted");
    }
    return QString();
}

QString causeName(P2p::Cause cause)
{
    switch (cause) {
    case P2p::Cause::Resign: return QStringLiteral("resign");
    case P2p::Cause::Checkmate: return QStringLiteral("checkmate");
    case P2p::Cause::Timeout: return QStringLiteral("timeout");
    case P2p::Cause::Repetition: return QStringLiteral("repetition");
    case P2p::Cause::PerpetualCheck: return QStringLiteral("perpetual_check");
    case P2p::Cause::AgreedDraw: return QStringLiteral("agreed_draw");
    case P2p::Cause::MaxPly: return QStringLiteral("max_ply");
    case P2p::Cause::AgreedAbort: return QStringLiteral("agreed_abort");
    }
    return QString();
}

QJsonObject toJson(const P2p::EvidenceReport& report)
{
    QJsonObject checks;
    checks[QStringLiteral("conditions_match")] = report.conditionsMatch;
    checks[QStringLiteral("start_signatures")] = report.blackStartSigned && report.whiteStartSigned;
    checks[QStringLiteral("signatures")] = report.signaturesValid;
    checks[QStringLiteral("record_legal")] = report.recordLegal;
    checks[QStringLiteral("clock_arithmetic")] = report.clockArithmeticValid;
    checks[QStringLiteral("result_refers_to_record")] = !report.result || report.resultMatchesRecord;
    checks[QStringLiteral("result_conflicts_with_record")] = report.resultConflictsWithRecord;

    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("valid")] = report.isValid();
    obj[QStringLiteral("category")] = categoryName(report.category);
    obj[QStringLiteral("game_id")] = hex(report.gameId);
    obj[QStringLiteral("network_id")] = hex(report.networkId);
    if (!report.networkName.isEmpty()) obj[QStringLiteral("network")] = report.networkName;
    obj[QStringLiteral("black_player_id")] = hex(report.blackPlayerKey);
    obj[QStringLiteral("white_player_id")] = hex(report.whitePlayerKey);
    obj[QStringLiteral("main_time_ms")] = static_cast<qint64>(report.timeControl.mainMs);
    obj[QStringLiteral("byoyomi_ms")] = static_cast<qint64>(report.timeControl.byoyomiMs);
    obj[QStringLiteral("states")] = report.stateCount;
    obj[QStringLiteral("last_confirmed_ply")] = report.lastMutualPly;
    obj[QStringLiteral("checks")] = checks;
    if (report.terminal) {
        QJsonObject terminal;
        terminal[QStringLiteral("outcome")] = outcomeName(report.terminal->outcome);
        terminal[QStringLiteral("cause")] = causeName(report.terminal->cause);
        obj[QStringLiteral("record_result")] = terminal;
    }
    if (report.result) {
        QJsonObject result;
        result[QStringLiteral("outcome")] = outcomeName(report.result->outcome);
        result[QStringLiteral("cause")] = causeName(report.result->cause);
        result[QStringLiteral("ply")] = static_cast<int>(report.result->ply);
        result[QStringLiteral("result_digest")] = hex(report.resultDigest);
        result[QStringLiteral("black_signed")] = report.blackResultSigned;
        result[QStringLiteral("white_signed")] = report.whiteResultSigned;
        obj[QStringLiteral("result")] = result;
    }
    if (!report.error.isEmpty()) obj[QStringLiteral("problem")] = report.error;
    // 確かめられないことを明示する（実装仕様 9章: 思考時間・人間の着手・対局の日時は示さない）
    obj[QStringLiteral("not_verifiable")] = QJsonArray{QStringLiteral("actual_thinking_time"),
                                                       QStringLiteral("human_play"), QStringLiteral("date_played")};
    return obj;
}

} // namespace

std::variant<QJsonObject, Failure> verifyFile(const QString& path)
{
    if (const std::optional<Failure> problem = InputPath::check(path)) return *problem;
    QByteArray bytes;
    switch (BoundedFile::read(path, P2p::Evidence::kMaxBytes, bytes)) {
    case BoundedFile::Result::Ok:
        break;
    case BoundedFile::Result::TooLarge:
        return Failure{ErrorKind::InvalidContent, QStringLiteral("The evidence file exceeds 8 MiB."), QString()};
    case BoundedFile::Result::NotRegularFile:
    case BoundedFile::Result::OpenFailed:
        return Failure{ErrorKind::FileError, QStringLiteral("Cannot read %1.").arg(path), QString()};
    }
    QString error;
    const std::optional<P2p::Evidence> evidence = P2p::Evidence::decode(bytes, &error);
    if (!evidence) {
        return Failure{ErrorKind::InvalidContent, QStringLiteral("Not a ShogiBoardQ evidence file: %1").arg(error),
                       QString()};
    }
    return toJson(P2p::EvidenceVerifier::verify(*evidence));
}

} // namespace MatchVerificationService
