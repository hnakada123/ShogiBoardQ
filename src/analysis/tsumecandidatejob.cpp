/// @file tsumecandidatejob.cpp
/// @brief 詰将棋の候補局面1つ分の処理の実装

#include "tsumecandidatejob.h"

#include <atomic>

#include "shogiposition.h"
#include "tsumeshogicandidatescreener.h"
#include "tsumeshogipositiongenerator.h"

namespace {
/// 駒除去の候補の事前選別は呼び出し元のスレッドで行うので短くする
constexpr TsumeshogiCandidateScreener::Limits kTrimScreenLimits{20, 5, 2};
/// 事前選別の中断フラグ（除去の事前選別は中断しない）
const std::atomic_bool kNeverStop{false};
} // namespace

TsumeCandidateJob::TsumeCandidateJob(const QString& sfen, const Settings& settings)
    : m_settings(settings)
    , m_candidateSfen(sfen)
{
}

TsumeCandidateJob::~TsumeCandidateJob() = default;

void TsumeCandidateJob::abort()
{
    m_verification.reset();
    m_outcome.reset();
    m_stage = Stage::Rejected;
}

std::optional<MateQuery> TsumeCandidateJob::nextQuery()
{
    switch (m_stage) {
    case Stage::CheckLength:
        return MateQuery{m_candidateSfen, m_settings.timeoutMs};
    case Stage::Verify:
    case Stage::VerifyTrimmed: {
        std::optional<MateQuery> query = m_verification->nextQuery();
        if (m_verification->isDone()) {
            finishVerification();
            // 検査の後に除去へ進んだら、続けて次の問い合わせを出す
            return isDone() ? std::nullopt : nextQuery();
        }
        return query;
    }
    case Stage::Trim:
        return nextTrimQuery();
    case Stage::Accepted:
    case Stage::Rejected:
        break;
    }
    return std::nullopt;
}

void TsumeCandidateJob::submit(const MateAnswer& answer, qint64 elapsedMs)
{
    switch (m_stage) {
    case Stage::CheckLength:
        // ちょうど目標手数で詰む候補だけを検査する（ほかは数えずに次の候補へ）
        if (answer.kind == MateAnswer::Kind::Mate && answer.pv.size() == m_settings.targetMoves) {
            m_passedLengthCheck = true;
            startVerification(m_candidateSfen, Stage::Verify);
        } else {
            m_stage = Stage::Rejected;
        }
        return;
    case Stage::Verify:
    case Stage::VerifyTrimmed:
        m_verification->submit(answer, elapsedMs);
        if (m_verification->isDone()) finishVerification();
        return;
    case Stage::Trim:
        onTrimAnswer(answer);
        return;
    case Stage::Accepted:
    case Stage::Rejected:
        return;
    }
}

// ============================================================
// 唯一解の検査
// ============================================================

void TsumeCandidateJob::startVerification(const QString& sfen, Stage stage)
{
    m_stage = stage;
    m_verification = std::make_unique<TsumeVerificationJob>(
        sfen, m_settings.targetMoves, TsumeshogiVerifier::Options{m_settings.allowFinalMoveAlternatives, false},
        m_settings.timeoutMs);
}

void TsumeCandidateJob::finishVerification()
{
    using Status = TsumeshogiVerifier::Status;
    const TsumeshogiVerifier::Result result = m_verification->result();
    const QString sfen = m_verification->sfen();
    const bool trimmed = m_stage == Stage::VerifyTrimmed;
    m_verification.reset();

    if (result.status == Status::Unique) {
        startTrimming(sfen, result.pv);
        return;
    }
    if (!trimmed) {
        reject(result.status == Status::Unknown);
        return;
    }
    // 除いた局面の判定ができなければ、その駒が要るとは言えないので候補全体を棄却する。
    // 内蔵探索で目標手数の詰みを確かめた局面への不詰の答えも、駒を残す根拠にしない
    if (result.status == Status::Unknown || result.status == Status::Invalid || result.status == Status::WrongLength
        || (result.status == Status::NoMate && m_trimProvenInTarget)) {
        reject(true);
        return;
    }
    // 余詰・駒余りになる除去は採らない（その駒は要る）
    ++m_rejected;
    ++m_trimIndex;
    m_stage = Stage::Trim;
}

// ============================================================
// 要らない駒の除去
// ============================================================

void TsumeCandidateJob::startTrimming(const QString& sfen, const QStringList& pv)
{
    m_stage = Stage::Trim;
    m_baseSfen = sfen;
    m_basePv = pv;
    m_trimCandidates = enumerateRemovablePieces(sfen);
    m_trimIndex = 0;
}

std::optional<MateQuery> TsumeCandidateJob::nextTrimQuery()
{
    while (m_trimIndex < m_trimCandidates.size()) {
        const QString sfen = removePiece(m_baseSfen, m_trimCandidates.at(m_trimIndex));
        // 除くと開始局面で玉に王手がかかる（詰将棋として不正）除去はエンジンに送らない
        if (TsumeshogiPositionGenerator::isDefenderKingInCheck(sfen)) {
            ++m_trimIndex;
            continue;
        }
        // 内蔵探索で目標手数の詰みがない・短く詰む・初手が複数ある除去はエンジンに送らない
        const auto verdict = TsumeshogiCandidateScreener::screen(sfen, m_settings.targetMoves, kTrimScreenLimits,
                                                                 kNeverStop);
        if (verdict == TsumeshogiCandidateScreener::Verdict::NotMateInTarget) {
            ++m_trimIndex;
            continue;
        }
        if (verdict == TsumeshogiCandidateScreener::Verdict::MultipleFirstMoves) {
            ++m_rejected;
            ++m_trimIndex;
            continue;
        }
        m_trimSfen = sfen;
        m_trimProvenInTarget = verdict == TsumeshogiCandidateScreener::Verdict::Candidate;
        return MateQuery{sfen, m_settings.timeoutMs};
    }
    // 全ての1枚除去を判定し終えた局面だけを採択する
    m_outcome = Outcome{m_baseSfen, m_basePv};
    m_stage = Stage::Accepted;
    return std::nullopt;
}

void TsumeCandidateJob::onTrimAnswer(const MateAnswer& answer)
{
    switch (answer.kind) {
    case MateAnswer::Kind::Mate:
        // 除去後の手順の長さだけでは最短手数が変わった証明にならないので、違う長さは判定できないものとする
        if (answer.pv.size() != m_settings.targetMoves) {
            reject(true);
            return;
        }
        startVerification(m_trimSfen, Stage::VerifyTrimmed);
        return;
    case MateAnswer::Kind::NoMate:
        // 内蔵探索と食い違う答えを、駒を残す根拠にしない
        if (m_trimProvenInTarget) {
            reject(true);
            return;
        }
        ++m_trimIndex;   // 除くと詰まない駒は要る
        return;
    case MateAnswer::Kind::Unknown:
    case MateAnswer::Kind::Unsupported:
        // 判定できないのは「その駒が要る」という証明ではない
        reject(true);
        return;
    }
}

void TsumeCandidateJob::reject(bool inconclusive)
{
    ++m_rejected;
    if (inconclusive) ++m_inconclusive;
    m_verification.reset();
    m_stage = Stage::Rejected;
}

// ============================================================
// 駒を1枚除いた局面
// ============================================================

QList<TsumeCandidateJob::TrimCandidate> TsumeCandidateJob::enumerateRemovablePieces(const QString& sfen)
{
    QList<TrimCandidate> candidates;
    const std::optional<Position> position = Position::fromSfen(sfen);
    if (!position) return candidates;
    const Position& board = *position;

    // 盤上の駒（玉以外）。上の段から、段の中は9筋から順に並べる。盤上の守方の駒も除ける
    for (int rank = 1; rank <= 9; ++rank) {
        for (int file = 9; file >= 1; --file) {
            const BoardSquare square{file, rank};
            const Piece piece = board.pieceAt(square);
            if (piece == Piece::None || toBlack(piece) == Piece::BlackKing) continue;
            candidates.append(TrimCandidate{TrimCandidate::Board, square, piece});
        }
    }

    // 攻方（先手）の持駒（歩・香・桂・銀・金・角・飛の順）。守方の持駒は守りの資源なので除かない
    static constexpr Piece kHandOrder[] = {
        Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
        Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook};
    for (const Piece piece : kHandOrder) {
        if (board.handCount(piece) > 0) candidates.append(TrimCandidate{TrimCandidate::Hand, BoardSquare{}, piece});
    }
    return candidates;
}

QString TsumeCandidateJob::removePiece(const QString& sfen, const TrimCandidate& candidate)
{
    std::optional<Position> position = Position::fromSfen(sfen);
    if (!position) return sfen;
    Position& board = *position;

    // 駒の総数は変えない（除いた駒は生駒にして後手の持駒へ移す）
    const Piece toDefenderHand = toWhite(toBlack(demote(candidate.piece)));
    if (candidate.location == TrimCandidate::Board) {
        if (board.pieceAt(candidate.square) == candidate.piece) {
            board.setPieceAt(candidate.square, Piece::None);
            board.addToHand(toDefenderHand);
        }
    } else if (board.takeFromHand(candidate.piece)) {
        board.addToHand(toDefenderHand);
    }

    // 詰将棋は常に攻方（先手）の手番の1手目から始まる
    return board.toSfen(Turn::Black, 1);
}

QStringList TsumeCandidateJob::onePieceRemovedPositions(const QString& sfen)
{
    QStringList positions;
    const QList<TrimCandidate> candidates = enumerateRemovablePieces(sfen);
    for (const TrimCandidate& candidate : candidates) positions.append(removePiece(sfen, candidate));
    return positions;
}
