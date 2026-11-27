/// @file tsumestrictcertifier.cpp
/// @brief 長手数詰将棋の厳密証明ジョブの実装

#include "tsumestrictcertifier.h"

#include "tsumecandidatejob.h"
#include "tsumecollection.h"
#include "tsumeshogicandidatescreener.h"
#include "tsumeshogipositiongenerator.h"
#include "tsumeshogisettings.h"

#include <QJsonArray>

#include <algorithm>
#include <atomic>

namespace {
constexpr TsumeshogiCandidateScreener::Limits kTrimScreenLimits{20, 5, 2};
const std::atomic_bool kNeverStop{false};

QString replyToString(TsumeshogiVerifier::Reply reply)
{
    switch (reply) {
    case TsumeshogiVerifier::Reply::Mate: return QStringLiteral("m");
    case TsumeshogiVerifier::Reply::NoMate: return QStringLiteral("n");
    case TsumeshogiVerifier::Reply::Unknown: return QStringLiteral("u");
    }
    return QStringLiteral("u");
}

TsumeshogiVerifier::Reply replyFromString(const QString& text)
{
    if (text == QLatin1String("m")) return TsumeshogiVerifier::Reply::Mate;
    if (text == QLatin1String("n")) return TsumeshogiVerifier::Reply::NoMate;
    return TsumeshogiVerifier::Reply::Unknown;
}
} // namespace

TsumeStrictCertifier::TsumeStrictCertifier(const QString& sfen, const Settings& settings)
    : m_settings(settings)
    , m_candidateSfen(sfen)
{
    if (!TsumeshogiSettings::GeneratorLimits::isTargetMoves(m_settings.targetMoves)
        || m_candidateSfen.isEmpty()
        || TsumeshogiPositionGenerator::isDefenderKingInCheck(m_candidateSfen)) {
        reject(RejectionReason::Invalid);
    }
}

TsumeStrictCertifier::~TsumeStrictCertifier() = default;

QString TsumeStrictCertifier::stageName(Stage stage)
{
    switch (stage) {
    case Stage::CheckBase: return QStringLiteral("check_base");
    case Stage::VerifyBase: return QStringLiteral("verify_base");
    case Stage::TrimProbe: return QStringLiteral("trim_probe");
    case Stage::VerifyTrimmed: return QStringLiteral("verify_trimmed");
    case Stage::Certified: return QStringLiteral("certified");
    case Stage::Rejected: return QStringLiteral("rejected");
    }
    return QStringLiteral("rejected");
}

QString TsumeStrictCertifier::rejectionReasonName(RejectionReason reason)
{
    switch (reason) {
    case RejectionReason::None: return QStringLiteral("none");
    case RejectionReason::Invalid: return QStringLiteral("invalid");
    case RejectionReason::WrongLength: return QStringLiteral("wrong_length");
    case RejectionReason::NoMate: return QStringLiteral("nomate");
    case RejectionReason::Multiple: return QStringLiteral("multiple");
    case RejectionReason::Surplus: return QStringLiteral("surplus");
    case RejectionReason::Inconclusive: return QStringLiteral("inconclusive");
    }
    return QStringLiteral("inconclusive");
}

TsumeshogiVerifier::Options TsumeStrictCertifier::verifierOptions() const
{
    TsumeshogiVerifier::Options opt;
    opt.allowFinalMoveAlternatives = m_settings.allowFinalMoveAlternatives;
    opt.requireNoSurplus = m_settings.requireNoSurplus;
    opt.maxVisitedNodes = m_settings.maxVisitedNodes;
    opt.maxCacheEntries = m_settings.maxCacheEntries;
    opt.skipImmediateMateQueries = true;
    return opt;
}

int TsumeStrictCertifier::queryLimitMs() const
{
    const qint64 remaining = m_settings.totalBudgetMs - m_spentMs;
    if (remaining <= 0) return 0;
    return static_cast<int>(std::min<qint64>(remaining, std::max(1, m_settings.queryMaxMs)));
}

void TsumeStrictCertifier::seedVerifiedCache(const std::map<std::string, TsumeshogiVerifier::CacheEntry>& entries)
{
    const std::size_t limit = m_settings.maxCacheEntries > 0
        ? static_cast<std::size_t>(m_settings.maxCacheEntries) : 0;
    for (const auto& [key, val] : entries) {
        if (val.reply == TsumeshogiVerifier::Reply::Unknown) continue;
        if (limit > 0 && m_verifiedCache.size() >= limit) break;
        m_verifiedCache.emplace(key, val);
    }
    if (m_verification) m_verification->verifier().seedCache(m_verifiedCache);
}

void TsumeStrictCertifier::syncVerifierCache()
{
    if (!m_verification) return;
    const auto exported = m_verification->verifier().exportCache();
    const std::size_t limit = m_settings.maxCacheEntries > 0
        ? static_cast<std::size_t>(m_settings.maxCacheEntries) : 0;
    for (const auto& [key, val] : exported) {
        if (val.reply == TsumeshogiVerifier::Reply::Unknown) continue;
        if (limit > 0 && m_verifiedCache.size() >= limit) break;
        m_verifiedCache.emplace(key, val);
    }
}

void TsumeStrictCertifier::abort()
{
    syncVerifierCache();
    m_verification.reset();
    m_outcome.reset();
    m_stage = Stage::Rejected;
    m_rejectionReason = RejectionReason::Inconclusive;
}

std::optional<MateQuery> TsumeStrictCertifier::nextQuery()
{
    if (isDone()) return std::nullopt;
    if (m_spentMs >= m_settings.totalBudgetMs) {
        reject(RejectionReason::Inconclusive);
        return std::nullopt;
    }

    switch (m_stage) {
    case Stage::CheckBase: {
        const int limit = queryLimitMs();
        if (limit <= 0) {
            reject(RejectionReason::Inconclusive);
            return std::nullopt;
        }
        return MateQuery{m_candidateSfen, limit};
    }
    case Stage::VerifyBase:
    case Stage::VerifyTrimmed: {
        std::optional<MateQuery> query = m_verification->nextQuery();
        if (m_verification->isDone()) {
            finishVerification();
            return isDone() ? std::nullopt : nextQuery();
        }
        return query;
    }
    case Stage::TrimProbe:
        return nextTrimQuery();
    case Stage::Certified:
    case Stage::Rejected:
        break;
    }
    return std::nullopt;
}

void TsumeStrictCertifier::submit(const MateAnswer& answer, qint64 elapsedMs)
{
    if (isDone()) return;
    m_spentMs += std::max<qint64>(0, elapsedMs);
    ++m_queries;

    switch (m_stage) {
    case Stage::CheckBase:
        if (answer.kind == MateAnswer::Kind::Mate) {
            if (!TsumeCollection::validMateLine(m_candidateSfen, answer.pv)) {
                reject(RejectionReason::Inconclusive);
                return;
            }
            if (static_cast<int>(answer.pv.size()) != m_settings.targetMoves) {
                reject(RejectionReason::WrongLength);
                return;
            }
            startVerification(m_candidateSfen, Stage::VerifyBase);
        } else if (answer.kind == MateAnswer::Kind::NoMate) {
            reject(RejectionReason::NoMate);
        } else {
            reject(RejectionReason::Inconclusive);
        }
        return;

    case Stage::VerifyBase:
    case Stage::VerifyTrimmed:
        m_verification->submit(answer, elapsedMs);
        if (m_verification->isDone()) finishVerification();
        return;

    case Stage::TrimProbe:
        onTrimProbeAnswer(answer);
        return;

    case Stage::Certified:
    case Stage::Rejected:
        return;
    }
}

void TsumeStrictCertifier::startVerification(const QString& sfen, Stage stage)
{
    m_stage = stage;
    const qint64 remaining = std::max<qint64>(1, m_settings.totalBudgetMs - m_spentMs);
    m_verification = std::make_unique<TsumeVerificationJob>(
        sfen, m_settings.targetMoves, verifierOptions(), remaining, m_settings.queryMaxMs);
    m_verification->verifier().seedCache(m_verifiedCache);
}

void TsumeStrictCertifier::finishVerification()
{
    using Status = TsumeshogiVerifier::Status;
    syncVerifierCache();
    const TsumeshogiVerifier::Result result = m_verification->result();
    const QString sfen = m_verification->sfen();
    const bool trimmed = (m_stage == Stage::VerifyTrimmed);
    m_verification.reset();

    if (result.status == Status::Unique) {
        if (!TsumeCollection::validMateLine(sfen, result.pv)
            || static_cast<int>(result.pv.size()) != m_settings.targetMoves) {
            reject(RejectionReason::Inconclusive);
            return;
        }
        if (trimmed) ++m_removedPieces;
        startTrimming(sfen, result.pv);
        return;
    }

    if (!trimmed) {
        switch (result.status) {
        case Status::Multiple:
            if (!result.cookMove.isEmpty()) {
                m_cookInfo = CookInfo{result.mainMove, result.cookMove, result.cookPly};
            }
            reject(RejectionReason::Multiple);
            return;
        case Status::Surplus:
            reject(RejectionReason::Surplus);
            return;
        case Status::NoMate:
            reject(RejectionReason::NoMate);
            return;
        case Status::WrongLength:
            reject(RejectionReason::WrongLength);
            return;
        case Status::Invalid:
            reject(RejectionReason::Invalid);
            return;
        case Status::Unknown:
        case Status::Running:
        case Status::Unique:
            reject(RejectionReason::Inconclusive);
            return;
        }
    }

    // 除去後の厳密検証結果の扱い：
    // 判定不能・不正・内蔵探索と食い違う不詰は「その駒が必要」という証明にならないので候補全体を棄却。
    if (result.status == Status::Unknown || result.status == Status::Invalid
        || (result.status == Status::NoMate && m_trimProvenInTarget)) {
        reject(RejectionReason::Inconclusive);
        return;
    }
    if (result.status == Status::WrongLength && !m_settings.strictMinLength) {
        reject(RejectionReason::Inconclusive);
        return;
    }
    // Multiple / Surplus / NoMate / (strictMinLength下の WrongLength) は除去すると不成立 → その駒は必須
    ++m_trimIndex;
    m_stage = Stage::TrimProbe;
}

void TsumeStrictCertifier::startTrimming(const QString& sfen, const QStringList& pv)
{
    m_stage = Stage::TrimProbe;
    m_baseSfen = sfen;
    m_basePv = pv;
    m_trimPositions = TsumeCandidateJob::onePieceRemovedPositions(sfen);
    m_trimIndex = 0;
    m_trimSfen.clear();
    m_trimProvenInTarget = false;
}

std::optional<MateQuery> TsumeStrictCertifier::nextTrimQuery()
{
    while (m_trimIndex < m_trimPositions.size()) {
        const QString sfen = m_trimPositions.at(m_trimIndex);
        if (TsumeshogiPositionGenerator::isDefenderKingInCheck(sfen)) {
            ++m_trimIndex;
            continue;
        }
        // 63手以下のときのみ内蔵探索による高速事前却下を利用する（63手超は Unknown を返す）
        const auto verdict = TsumeshogiCandidateScreener::screen(sfen, m_settings.targetMoves,
                                                                 kTrimScreenLimits, kNeverStop);
        if (verdict == TsumeshogiCandidateScreener::Verdict::NotMateInTarget
            || verdict == TsumeshogiCandidateScreener::Verdict::MultipleFirstMoves) {
            ++m_trimIndex;
            continue;
        }
        m_trimSfen = sfen;
        m_trimProvenInTarget = (verdict == TsumeshogiCandidateScreener::Verdict::Candidate);
        const int limit = queryLimitMs();
        if (limit <= 0) {
            reject(RejectionReason::Inconclusive);
            return std::nullopt;
        }
        return MateQuery{sfen, limit};
    }

    // 全ての1枚除去について確定判定が完了
    if (!TsumeCollection::validMateLine(m_baseSfen, m_basePv)
        || static_cast<int>(m_basePv.size()) != m_settings.targetMoves) {
        reject(RejectionReason::Inconclusive);
        return std::nullopt;
    }
    m_outcome = Outcome{m_baseSfen, m_basePv, m_removedPieces, m_queries, m_spentMs};
    m_stage = Stage::Certified;
    return std::nullopt;
}

void TsumeStrictCertifier::onTrimProbeAnswer(const MateAnswer& answer)
{
    switch (answer.kind) {
    case MateAnswer::Kind::Mate: {
        if (!TsumeCollection::validMateLine(m_trimSfen, answer.pv)) {
            reject(RejectionReason::Inconclusive);
            return;
        }
        const int len = static_cast<int>(answer.pv.size());
        if (len < m_settings.targetMoves) {
            // 目標より短い合法な詰み手順が実在する → 除いた局面は絶対に目標手数詰ではない（その駒は必須）
            ++m_trimIndex;
            return;
        }
        if (len > m_settings.targetMoves) {
            if (m_settings.strictMinLength) {
                // 自然終了した MinLength が目標より長い詰みを返した → 最短手数が目標と一致しない（その駒は必須）
                ++m_trimIndex;
                return;
            }
            reject(RejectionReason::Inconclusive);
            return;
        }
        // ちょうど目標手数で詰む → 厳密検証へ（余詰なし・駒余りなしなら不要駒として除去確定）
        startVerification(m_trimSfen, Stage::VerifyTrimmed);
        return;
    }
    case MateAnswer::Kind::NoMate:
        if (m_trimProvenInTarget) {
            reject(RejectionReason::Inconclusive);
            return;
        }
        ++m_trimIndex; // 除くと不詰 → その駒は必須
        return;
    case MateAnswer::Kind::Unknown:
    case MateAnswer::Kind::Unsupported:
        reject(RejectionReason::Inconclusive);
        return;
    }
}

void TsumeStrictCertifier::reject(RejectionReason reason)
{
    syncVerifierCache();
    m_verification.reset();
    m_outcome.reset();
    m_stage = Stage::Rejected;
    m_rejectionReason = reason;
}

QJsonObject TsumeStrictCertifier::toJson() const
{
    // 進行中の検証キャッシュも取り込んで保存する（再開時に既検証局面を 0ms で通過できる）
    std::map<std::string, TsumeshogiVerifier::CacheEntry> merged = m_verifiedCache;
    if (m_verification) {
        for (const auto& [k, v] : m_verification->verifier().exportCache()) {
            if (v.reply != TsumeshogiVerifier::Reply::Unknown) merged.emplace(k, v);
        }
    }

    QJsonObject obj;
    obj[QStringLiteral("stage")] = stageName(m_stage);
    obj[QStringLiteral("candidate_sfen")] = m_candidateSfen;
    obj[QStringLiteral("base_sfen")] = m_baseSfen;
    obj[QStringLiteral("base_pv")] = QJsonArray::fromStringList(m_basePv);
    obj[QStringLiteral("trim_positions")] = QJsonArray::fromStringList(m_trimPositions);
    obj[QStringLiteral("trim_index")] = m_trimIndex;
    obj[QStringLiteral("trim_sfen")] = m_trimSfen;
    obj[QStringLiteral("trim_proven_in_target")] = m_trimProvenInTarget;
    obj[QStringLiteral("removed_pieces")] = m_removedPieces;
    obj[QStringLiteral("queries")] = m_queries;
    obj[QStringLiteral("spent_ms")] = static_cast<double>(m_spentMs);

    QJsonArray cacheArr;
    for (const auto& [k, v] : merged) {
        QJsonArray entry;
        entry.append(QString::fromStdString(k));
        entry.append(replyToString(v.reply));
        entry.append(v.len);
        cacheArr.append(entry);
    }
    obj[QStringLiteral("verified_cache")] = cacheArr;
    return obj;
}

std::unique_ptr<TsumeStrictCertifier> TsumeStrictCertifier::fromJson(const QJsonObject& json, const Settings& settings)
{
    const QString candidate = json.value(QStringLiteral("candidate_sfen")).toString();
    if (candidate.isEmpty()) return nullptr;

    auto job = std::unique_ptr<TsumeStrictCertifier>(new TsumeStrictCertifier());
    job->m_settings = settings;
    job->m_candidateSfen = candidate;
    job->m_baseSfen = json.value(QStringLiteral("base_sfen")).toString();
    for (const auto& v : json.value(QStringLiteral("base_pv")).toArray()) job->m_basePv.append(v.toString());
    for (const auto& v : json.value(QStringLiteral("trim_positions")).toArray()) {
        job->m_trimPositions.append(v.toString());
    }
    job->m_trimIndex = std::max(0, json.value(QStringLiteral("trim_index")).toInt());
    job->m_trimSfen = json.value(QStringLiteral("trim_sfen")).toString();
    job->m_trimProvenInTarget = json.value(QStringLiteral("trim_proven_in_target")).toBool();
    job->m_removedPieces = std::max(0, json.value(QStringLiteral("removed_pieces")).toInt());
    job->m_queries = std::max(0, json.value(QStringLiteral("queries")).toInt());
    job->m_spentMs = std::max<qint64>(0, static_cast<qint64>(json.value(QStringLiteral("spent_ms")).toDouble()));

    for (const auto& item : json.value(QStringLiteral("verified_cache")).toArray()) {
        const QJsonArray arr = item.toArray();
        if (arr.size() != 3) continue;
        const auto reply = replyFromString(arr.at(1).toString());
        if (reply == TsumeshogiVerifier::Reply::Unknown) continue;
        job->m_verifiedCache.emplace(arr.at(0).toString().toStdString(),
                                     TsumeshogiVerifier::CacheEntry{reply, arr.at(2).toInt()});
    }

    const QString stageStr = json.value(QStringLiteral("stage")).toString();
    if (stageStr == QLatin1String("check_base")) {
        job->m_stage = Stage::CheckBase;
    } else if (stageStr == QLatin1String("verify_base")) {
        job->startVerification(job->m_candidateSfen, Stage::VerifyBase);
    } else if (stageStr == QLatin1String("trim_probe")) {
        if (job->m_baseSfen.isEmpty()) job->m_stage = Stage::CheckBase;
        else job->m_stage = Stage::TrimProbe;
    } else if (stageStr == QLatin1String("verify_trimmed")) {
        if (job->m_trimSfen.isEmpty() || job->m_baseSfen.isEmpty()) job->m_stage = Stage::CheckBase;
        else job->startVerification(job->m_trimSfen, Stage::VerifyTrimmed);
    } else {
        return nullptr;
    }
    return job;
}
