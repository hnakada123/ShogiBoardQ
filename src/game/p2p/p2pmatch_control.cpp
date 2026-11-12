/// @file p2pmatch_control.cpp
/// @brief 開始後の対局の状態機械の実装（制御メッセージ・結果の提案と署名・再接続と照合）

#include "p2pmatch.h"

#include "p2precordkinds.h"

namespace P2p {

// ---------------------------------------------------------------- 制御メッセージ

void Match::sendControl(ControlKind kind, const CborValue& body)
{
    if (m_phase == Phase::Reconnecting && kind != ControlKind::Resume) return;
    ControlPayload payload;
    payload.control.gameId = m_gameId;
    payload.control.connectionId = m_connectionId;
    payload.control.sequence = ++m_controlOut;
    payload.control.kind = kind;
    payload.control.bodyHash = Cbor::hash(body);
    payload.body = body;
    payload.signature = m_sessionKey.sign(payload.control.toCbor());
    ++m_controlCount;
    const CborValue value = payload.toCbor();
    if (kind != ControlKind::Ready && kind != ControlKind::Resume) persist(RecordKinds::control(true, value));
    send(FrameType::Control, value);
}

void Match::handleControl(const ControlPayload& payload)
{
    const Control& c = payload.control;
    if (c.gameId != m_gameId || c.connectionId != m_connectionId) return;
    // 同じ接続での再送（同じ番号）は1回だけ扱う
    if (c.sequence <= m_controlIn) return;
    if (c.sequence != m_controlIn + 1) return failWithError(ErrorCode::StateConflict, 0);
    if (!Crypto::verify(opponentSessionKey(), c.toCbor(), payload.signature)) {
        return failWithError(ErrorCode::InvalidSignature, 0);
    }
    m_controlIn = c.sequence;
    if (++m_controlCount > 4096) return failWithError(ErrorCode::LimitExceeded, 0);
    if (c.kind != ControlKind::Ready && c.kind != ControlKind::Resume) {
        persist(RecordKinds::control(false, payload.toCbor()));
    }
    // 再接続中は、RESUME を確かめるまでほかの制御を受け付けない
    if (m_phase == Phase::Reconnecting && c.kind != ControlKind::Resume) return;

    switch (c.kind) {
    case ControlKind::Ready:
        handleReady(payload.body);
        return;
    case ControlKind::Resume:
        handleResume(payload.body);
        return;
    case ControlKind::ResultPropose:
        handleResultPropose(payload.body, c.bodyHash);
        return;
    case ControlKind::Abort:
        // ABORT は一方的な離脱の通知で、合意中断ではない。対局を止め、結果未確定として閉じる
        finish(Settlement::Aborted, ControlBody::parseAbort(payload.body).value_or(ErrorCode::Cancelled));
        return;
    case ControlKind::Error:
        finish(Settlement::Error, ControlBody::parseError(payload.body).value_or(ErrorCode::ConditionMismatch));
        return;
    case ControlKind::ResultDecline: {
        const std::optional<QByteArray> declined = ControlBody::parseDecline(payload.body);
        if (declined && m_myProposal && *declined == m_myProposalHash) {
            m_myProposal.reset();
            m_myProposalHash.clear();
            emitEvent(MatchEvent{MatchEvent::Type::ProposalDeclined, {}, 0, std::nullopt});
        }
        return;
    }
    }
}

void Match::handleReady(const CborValue& body)
{
    const std::optional<QByteArray> state0Hash = ControlBody::parseReady(body);
    // READY は後手が送る。先手は受けた時点で初手の計時を始める（再送の READY で時計を初期化し直さない）
    if (!m_setup.iAmBlack || m_readyDone) return;
    if (!state0Hash || m_confirmed.size() != 1 || *state0Hash != m_confirmed.front().state.hash()) {
        return failWithError(ErrorCode::StateConflict, 0);
    }
    m_readyDone = true;
    m_phase = Phase::Playing;
    m_timer.startMyTurn(now());
    emitEvent(MatchEvent{MatchEvent::Type::Ready, {}, 0, std::nullopt});
}

// ---------------------------------------------------------------- 結果

FinalResult Match::resultFor(Outcome outcome, Cause cause) const
{
    std::vector<GameState> states;
    states.reserve(m_confirmed.size());
    for (const SignedState& s : m_confirmed) states.push_back(s.state);
    FinalResult r;
    r.gameId = m_gameId;
    r.finalStateHash = lastState().hash();
    r.transcriptHash = transcriptHash(m_gameId, states);
    r.ply = lastState().ply;
    r.outcome = outcome;
    r.cause = cause;
    return r;
}

bool Match::resultIsCurrent(const FinalResult& result) const
{
    return result.gameId == m_gameId && result.ply == lastState().ply && result.finalStateHash == lastState().hash()
        && result == resultFor(result.outcome, result.cause);
}

bool Match::proposeResult(Outcome outcome, Cause cause)
{
    if (m_phase != Phase::Playing || m_agreedResult || m_confirmed.empty() || !m_readyDone) return false;
    // 自分の未確認の着手がある間は提案しない
    if (m_pending) return false;
    const FinalResult result = resultFor(outcome, cause);
    if (m_myProposal && *m_myProposal == result) return true;
    const CborValue body = result.toCbor();
    m_myProposal = result;
    m_myProposalHash = Cbor::hash(body);
    sendControl(ControlKind::ResultPropose, body);
    // 相手の同じ提案をすでに受けていれば、これで一致
    if (m_pendingOffer && *m_pendingOffer == result) {
        m_pendingOffer.reset();
        agree(result);
    }
    return true;
}

bool Match::resign()
{
    // 投了は負ける側だけ、自分の手番に限る
    if (!isMyTurn() || m_pending) return false;
    return proposeResult(GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::White : Turn::Black), Cause::Resign);
}

bool Match::offer(Cause cause)
{
    if (cause != Cause::AgreedDraw && cause != Cause::AgreedAbort) return false;
    quint32& offeredPly = cause == Cause::AgreedDraw ? m_offeredDrawPly : m_offeredAbortPly;
    if (offeredPly == lastState().ply) return false;   // 同じ手数で1回まで
    if (!proposeResult(cause == Cause::AgreedDraw ? Outcome::Draw : Outcome::Aborted, cause)) return false;
    offeredPly = lastState().ply;
    return true;
}

void Match::answerOffer(bool accept)
{
    if (!m_pendingOffer) return;
    const FinalResult offered = *m_pendingOffer;
    const QByteArray hash = m_pendingOfferHash;
    // 回線が切れている間は承諾を送れないので、一致させない（つなぎ直した後に相手が申し出を送り直し、もう一度尋ねる）
    if (accept && m_phase == Phase::Playing && resultIsCurrent(offered) && !m_pending) {
        m_myProposal = offered;
        m_myProposalHash = hash;
        m_pendingOffer.reset();
        m_pendingOfferHash.clear();
        sendControl(ControlKind::ResultPropose, offered.toCbor());
        agree(offered);
        return;
    }
    m_pendingOffer.reset();
    m_pendingOfferHash.clear();
    sendControl(ControlKind::ResultDecline, ControlBody::decline(hash));
}

void Match::handleResultPropose(const CborValue& body, const QByteArray& bodyHash)
{
    if (m_agreedResult) return;   // 結果の一致の後は、別の結果に同意しない
    const std::optional<FinalResult> result = FinalResult::fromCbor(body);
    if (!result || m_confirmed.empty() || !resultIsCurrent(*result)) {
        // 古い手数の提案・形の合わない提案には同意しない（対局は続く）
        sendControl(ControlKind::ResultDecline, ControlBody::decline(bodyHash));
        return;
    }
    // 自分の提案と同じなら一致
    if (m_myProposal && *m_myProposal == *result) {
        agree(*result);
        return;
    }
    const bool opponentToMove = !isMyTurn();
    const Outcome iWin = GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::Black : Turn::White);
    const auto decline = [this, &bodyHash]() { sendControl(ControlKind::ResultDecline, ControlBody::decline(bodyHash)); };
    switch (result->cause) {
    case Cause::Resign:
        // 投了の提案は負ける側（相手）だけ、相手の手番に限る
        if (result->outcome != iWin || !opponentToMove) return decline();
        m_myProposal = *result;
        m_myProposalHash = bodyHash;
        sendControl(ControlKind::ResultPropose, body);
        agree(*result);
        return;
    case Cause::Checkmate:
    case Cause::Repetition:
    case Cause::PerpetualCheck:
    case Cause::MaxPly: {
        std::vector<GameState> states;
        for (const SignedState& s : m_confirmed) states.push_back(s.state);
        const std::optional<GameRules::Terminal> terminal = GameRules::judge(states);
        if (!terminal) return decline();
        // 判定の食い違いは実装の不一致。同意せずに中断し、証跡を残す
        if (terminal->outcome != result->outcome || terminal->cause != result->cause) {
            return failWithError(ErrorCode::ConditionMismatch, lastState().ply);
        }
        proposeResult(result->outcome, result->cause);
        agree(*result);
        return;
    }
    case Cause::Timeout: {
        if (result->outcome == iWin) {
            // 相手が自分の時間切れを申告した
            if (!opponentToMove) return decline();
        } else {
            // 相手が自分の時間切れを主張した: 自分の時計で確かめる
            const quint64 remaining = m_setup.iAmBlack ? lastState().blackMainMs : lastState().whiteMainMs;
            const auto elapsed = static_cast<quint64>(m_timer.mySpentMs(now()));
            if (!isMyTurn() || m_pending || GameRules::canMoveWithin(remaining, m_setup.terms.byoyomiMs, elapsed)) {
                return decline();
            }
        }
        m_myProposal = *result;
        m_myProposalHash = bodyHash;
        sendControl(ControlKind::ResultPropose, body);
        agree(*result);
        return;
    }
    case Cause::AgreedDraw:
    case Cause::AgreedAbort: {
        // 利用者に尋ねる（答えるまでに手数が進めば無効）
        m_pendingOffer = *result;
        m_pendingOfferHash = bodyHash;
        emitEvent(MatchEvent{MatchEvent::Type::OfferReceived, {}, 0, *result});
        return;
    }
    }
}

void Match::agree(const FinalResult& result)
{
    if (m_agreedResult) return;
    // 1. gameId -> resultDigest を永続化する（以後、この対局で別の結果に署名しない）
    if (!persist(RecordKinds::resultDigest(result))) return failWithError(ErrorCode::StorageError, result.ply);
    m_agreedResult = result;
    m_phase = Phase::Finishing;
    m_resultAgreedAt = now();
    emitEvent(MatchEvent{MatchEvent::Type::ResultAgreed, {}, 0, result});
    // 2. プレイヤー鍵で FinalResult に署名し、RESULT_SIGNATURE を送る
    m_myResultSignature = m_playerKey ? m_playerKey->sign(result.toCbor()) : QByteArray();
    persist(RecordKinds::resultSignature(mySide(), m_myResultSignature));
    ResultSignaturePayload payload;
    payload.result = result;
    payload.signature.side = mySide();
    payload.signature.signature = m_myResultSignature;
    send(FrameType::ResultSignature, payload.toCbor());
    // 相手の署名が先に届いていれば、ここでそろう
    if (!m_opponentResultSignature.isEmpty()) finish(Settlement::MutuallySigned, std::nullopt);
}

void Match::handleResultSignature(const ResultSignaturePayload& payload)
{
    const quint8 opponentSide = m_setup.iAmBlack ? kSideWhite : kSideBlack;
    if (payload.signature.side != opponentSide) return failWithError(ErrorCode::InvalidSignature, 0);
    if (!Crypto::verify(opponentPlayerKey(), payload.result.toCbor(), payload.signature.signature)) {
        return failWithError(ErrorCode::InvalidSignature, 0);
    }
    if (m_agreedResult && payload.result != *m_agreedResult) return failWithError(ErrorCode::StateConflict, 0);
    // 同じ内容の署名の再送は許す
    if (!m_opponentResultSignature.isEmpty()) return;
    if (!m_agreedResult) {
        // 相手の提案より先に署名は届かない（フレームは順に届く）。合わない順序は受け付けない
        return;
    }
    m_opponentResultSignature = payload.signature.signature;
    persist(RecordKinds::resultSignature(opponentSide, m_opponentResultSignature));
    finish(Settlement::MutuallySigned, std::nullopt);
}

void Match::abort(ErrorCode reason)
{
    if (m_phase == Phase::Finished) return;
    if (m_phase != Phase::Reconnecting && !m_confirmed.empty()) {
        sendControl(ControlKind::Abort, ControlBody::abort(lastState().hash(), reason));
    }
    finish(Settlement::Aborted, reason);
}

// ---------------------------------------------------------------- 再接続

void Match::connectionLost()
{
    if (m_phase == Phase::Finished) return;
    if (m_phase == Phase::Opening) {
        finish(Settlement::Error, ErrorCode::Expired);
        return;
    }
    if (m_phase == Phase::Finishing) {
        // 結果は一致している。相手の署名を待たずに、署名のない証跡として閉じる
        finish(Settlement::SignatureMissing, std::nullopt);
        return;
    }
    if (m_phase == Phase::Reconnecting) return;
    m_phase = Phase::Reconnecting;
    m_timer.disconnected(now());
    m_resumeSent = false;
    m_resumeReceived = false;
    m_syncing = false;
    m_myRandom.clear();
    m_opponentRandom.clear();
    emitEvent(MatchEvent{MatchEvent::Type::Reconnecting, {}, 0, std::nullopt});
}

void Match::connectionEstablished()
{
    if (m_phase != Phase::Reconnecting) return;
    m_myRandom = Crypto::randomBytes(kNonceSize);
    m_opponentRandom.clear();
    m_connectionId.clear();
    m_controlOut = 0;
    m_controlIn = 0;
    m_resumeSent = false;
    m_resumeReceived = false;
    send(FrameType::Hello, ReconnectHello{mySide(), m_myRandom}.toCbor());
}

void Match::handleReconnectHello(const ReconnectHello& hello)
{
    const quint8 opponentSide = m_setup.iAmBlack ? kSideWhite : kSideBlack;
    if (m_phase != Phase::Reconnecting || hello.side != opponentSide || m_myRandom.isEmpty()
        || hello.connectionRandom == m_myRandom) {
        return;
    }
    m_opponentRandom = hello.connectionRandom;
    m_connectionId = connectionId(m_gameId, m_setup.iAmBlack ? m_myRandom : m_opponentRandom,
                                  m_setup.iAmBlack ? m_opponentRandom : m_myRandom);
    // 新しい connectionId に対する RESUME の一局用鍵の署名で、相手を確かめ直す
    m_resumeSent = true;
    sendControl(ControlKind::Resume, ControlBody::resume(lastState().ply, lastState().hash()));
}

void Match::handleResume(const CborValue& body)
{
    const std::optional<ControlBody::Resume> resume = ControlBody::parseResume(body);
    if (!resume || m_phase != Phase::Reconnecting || !m_resumeSent) return failWithError(ErrorCode::StateConflict, 0);
    m_resumeReceived = true;
    const quint32 myPly = lastState().ply;
    if (resume->latestPly <= myPly) {
        // 相手の最新は自分も持っている: 同じ状態であること
        if (m_confirmed[resume->latestPly].state.hash() != resume->latestStateHash) {
            return failWithError(ErrorCode::StateConflict, resume->latestPly);
        }
        // 相手が古ければ、相手の SYNC_REQUEST に応える。同じなら照合は終わり
        if (resume->latestPly == myPly) finishResume();
        else m_syncing = true;   // 相手の要求を待つ（応えた後に finishResume）
        return;
    }
    // 自分が古い: 足りない分を求める
    m_syncing = true;
    send(FrameType::SyncRequest, SyncRequestPayload{myPly + 1, lastState().hash()}.toCbor());
}

void Match::handleSyncRequest(const SyncRequestPayload& request)
{
    if (!m_resumeReceived || request.fromPly == 0 || request.fromPly > lastState().ply
        || m_confirmed[request.fromPly - 1].state.hash() != request.previousStateHash) {
        return failWithError(ErrorCode::StateConflict, request.fromPly);
    }
    // 連続した State を両署名とともに返す（要求の外のデータは送らない）
    quint32 ply = request.fromPly;
    while (ply <= lastState().ply) {
        SyncChunkPayload chunk;
        while (ply <= lastState().ply && chunk.states.size() < kMaxSyncChunkStates) chunk.states.push_back(m_confirmed[ply++]);
        chunk.final = ply > lastState().ply;
        send(FrameType::SyncChunk, chunk.toCbor());
    }
    finishResume();
}

void Match::handleSyncChunk(const SyncChunkPayload& chunk)
{
    if (!m_syncing || m_phase != Phase::Reconnecting) return failWithError(ErrorCode::StateConflict, 0);
    for (const SignedState& s : chunk.states) {
        if (s.state.ply != lastState().ply + 1 || !verifyApproval(s.state, s.blackApproval, true)
            || !verifyApproval(s.state, s.whiteApproval, false)
            || GameRules::checkNext(lastState(), s.state, m_setup.terms) != GameRules::StateCheck::Ok) {
            return failWithError(ErrorCode::StateConflict, s.state.ply);
        }
        // 確認待ちだった自分の手と違う状態は受け入れない（部分署名だけの別の分岐へ巻き戻さない）
        if (m_pending && s.state.ply == m_pending->state.ply && s.state != m_pending->state) {
            return failWithError(ErrorCode::StateConflict, s.state.ply);
        }
        if (!persist(RecordKinds::state(s))) return failWithError(ErrorCode::StorageError, s.state.ply);
        const bool byMe = (GameRules::sideToMove(lastState().ply) == Turn::Black) == m_setup.iAmBlack;
        if (m_pending && s.state.ply == m_pending->state.ply) {
            m_pending.reset();
            m_pendingFrame.clear();
            m_pendingSent = false;
        }
        // 復旧で初めて受理した相手の着手では、自分の手番は今から始める（失われた時刻を推測しない）
        if (!byMe) m_timer.startMyTurn(now());
        commit(s);
        if (m_phase == Phase::Finished) return;
    }
    if (chunk.final) finishResume();
}

void Match::finishResume()
{
    if (m_phase != Phase::Reconnecting) return;
    m_syncing = false;
    m_timer.reconnected(now());
    m_phase = Phase::Playing;
    emitEvent(MatchEvent{MatchEvent::Type::Resumed, {}, 0, std::nullopt});
    // 相手の承認のない自分の手は、同じバイト列で送り直す
    if (m_pending) {
        m_pendingSent = false;
        resendPending();
    }
    // 結果を提案していたなら送り直す
    if (m_myProposal) {
        const FinalResult proposal = *m_myProposal;
        m_myProposal.reset();
        proposeResult(proposal.outcome, proposal.cause);
    }
    if (m_controlOut + m_controlIn > 4096) failWithError(ErrorCode::LimitExceeded, lastState().ply);
}

} // namespace P2p
