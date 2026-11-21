/// @file p2pmatch.cpp
/// @brief 開始後の対局の状態機械の実装（State0・着手の合意・期限・制御メッセージ・結果の提案と署名・再接続と照合）

#include "p2pmatch.h"

#include "p2precordkinds.h"

namespace P2p {

namespace {

/// State0 と READY を終えるまでの期限（実装仕様 3.3節）
constexpr qint64 kOpeningTimeoutMs = 30'000;
/// 相手の最終結果への署名を待つ時間（実装仕様 8.2節）
constexpr qint64 kResultSignatureWaitMs = 30'000;

} // namespace

void Match::ResultExchange::clearProposals()
{
    myProposal.reset();
    opponentOffer.reset();
    drawOfferAwaitingAnswer = false;
    abortOfferAwaitingAnswer = false;
}

Match::Match(MatchSetup setup, SigningKey sessionKey, const SigningKey* playerKey, MonotonicClock clock, Persist persist)
    : m_setup(std::move(setup))
    , m_sessionKey(std::move(sessionKey))
    , m_playerKey(playerKey)
    , m_clock(std::move(clock))
    , m_persist(std::move(persist))
    , m_gameId(m_setup.agreement.gameId())
    , m_line(m_gameId)
    , m_control(m_gameId, connectionId(m_gameId,
                                       m_setup.iAmBlack ? m_setup.myConnectionRandom : m_setup.opponentConnectionRandom,
                                       m_setup.iAmBlack ? m_setup.opponentConnectionRandom : m_setup.myConnectionRandom))
{
    m_requestId = qMax<quint64>(1, m_setup.nextRequestId);
}

// ---------------------------------------------------------------- 共通

std::vector<Frame> Match::takeOutgoing()
{
    std::vector<Frame> out;
    out.swap(m_outgoing);
    return out;
}

std::vector<MatchEvent> Match::takeEvents()
{
    std::vector<MatchEvent> out;
    out.swap(m_events);
    return out;
}

void Match::send(FrameType type, const CborValue& payload)
{
    // 再接続中に送るのは、照合の手順（再接続の HELLO・RESUME・SYNC）だけ
    if (m_phase == Phase::Reconnecting && type != FrameType::Hello && type != FrameType::Control
        && type != FrameType::SyncRequest && type != FrameType::SyncChunk) {
        return;
    }
    Frame frame;
    frame.type = type;
    frame.requestId = m_requestId++;
    frame.gameId = m_gameId;
    frame.payload = payload;
    m_outgoing.push_back(std::move(frame));
}

bool Match::persist(const CborValue& record)
{
    return m_persist && m_persist(record);
}

SignedState Match::signedByMe(const GameState& state) const
{
    SignedState s;
    s.state = state;
    const QByteArray signature = m_sessionKey.sign(stateApproval(m_gameId, state.ply, state.hash()));
    (m_setup.iAmBlack ? s.blackApproval : s.whiteApproval) = signature;
    return s;
}

const QByteArray& Match::myApproval(const SignedState& state) const
{
    return m_setup.iAmBlack ? state.blackApproval : state.whiteApproval;
}

bool Match::verifyApproval(const GameState& state, const QByteArray& signature, bool black) const
{
    return Crypto::verify(m_setup.agreement.sessionKey(black), stateApproval(m_gameId, state.ply, state.hash()),
                          signature);
}

quint64 Match::myMainMs() const
{
    return m_setup.iAmBlack ? lastState().blackMainMs : lastState().whiteMainMs;
}

quint64 Match::opponentMainMs() const
{
    return m_setup.iAmBlack ? lastState().whiteMainMs : lastState().blackMainMs;
}

void Match::finish(Settlement ending, std::optional<ErrorCode> error)
{
    if (m_phase == Phase::Finished) return;
    m_phase = Phase::Finished;
    m_settlement = ending;
    m_endingError = error;
    persist(RecordKinds::ending(m_settlement));
    emitEvent(MatchEvent{MatchEvent::Type::Finished, {}, 0, m_result.agreed});
}

void Match::failWithError(ErrorCode code, quint32 relatedPly)
{
    if (m_phase == Phase::Finished) return;
    // 検証の失敗では状態を変えずに中断へ移る（相手の負けとして記録しない）
    if (m_phase != Phase::Reconnecting) sendControl(ControlKind::Error, ControlBody::error(code, relatedPly));
    finish(Settlement::Error, code);
}

Evidence Match::evidence() const
{
    Evidence e;
    e.agreement = m_setup.agreement;
    e.blackStartSignature = m_setup.blackStartSignature;
    e.whiteStartSignature = m_setup.whiteStartSignature;
    e.rules = m_setup.rules;
    e.timeControl = m_setup.terms;
    e.states = m_line.states();
    // 相手の承認のない自分の最後の状態も残す（片方の署名だけの状態として）
    if (m_pending) e.states.push_back(m_pending->state);
    if (m_result.agreed) {
        e.result = m_result.agreed;
        (m_setup.iAmBlack ? e.blackResultSignature : e.whiteResultSignature) = m_result.mySignature;
        (m_setup.iAmBlack ? e.whiteResultSignature : e.blackResultSignature) = m_result.opponentSignature;
    }
    return e;
}

bool Match::isMyTurn() const
{
    if (m_line.isEmpty()) return false;
    return (m_line.sideToMove() == Turn::Black) == m_setup.iAmBlack;
}

qint64 Match::reconnectTotalMs() const
{
    return m_timer.disconnectedTotalMs(now());
}

qint64 Match::reconnectRemainingMs() const
{
    const qint64 disconnected = m_timer.currentDisconnectMs(now());
    if (disconnected < 0) return -1;
    return qMax<qint64>(0, static_cast<qint64>(NetworkPolicy::v1().reconnectWaitMs) - disconnected);
}

// ---------------------------------------------------------------- 開始

void Match::begin()
{
    m_startedAt = now();
    m_openingDeadline = m_startedAt + kOpeningTimeoutMs;
    // 開始合意と両方の開始署名をジャーナルに書いてから始める（書けなければ何も送らずに中断する）
    if (!persist(RecordKinds::start(m_setup.agreement, m_setup.rules, m_setup.terms, mySide(),
                                    m_setup.opponentDisplayName))
        || !persist(RecordKinds::startSignature(kSideBlack, m_setup.blackStartSignature))
        || !persist(RecordKinds::startSignature(kSideWhite, m_setup.whiteStartSignature))) {
        abort(ErrorCode::StorageError);
        return;
    }
    if (!m_setup.iAmBlack) return;
    // 先手が State0 を提案する（後手の承認と READY を待つ。再送はしない）
    const GameState state0 = GameRules::initialState(m_gameId, m_setup.terms);
    const SignedState mine = signedByMe(state0);
    if (!persist(RecordKinds::state(mine))) return failWithError(ErrorCode::StorageError, 0);
    m_pending = PendingMove{mine, std::nullopt, false, 0};
    send(FrameType::StatePropose, StateProposePayload{state0, mine.blackApproval}.toCbor());
}

// ---------------------------------------------------------------- 受信

void Match::receive(const Frame& frame)
{
    if (m_phase == Phase::Finished) return;
    if (frame.gameId != m_gameId) return failWithError(ErrorCode::ConditionMismatch, 0);
    switch (frame.type) {
    case FrameType::Hello: {
        const std::optional<ReconnectHello> hello = ReconnectHello::fromCbor(frame.payload);
        if (!hello) return failWithError(ErrorCode::ConditionMismatch, 0);
        handleReconnectHello(*hello);
        return;
    }
    case FrameType::Reject:
        // 両方の開始署名がそろった後の REJECT は無視する（対局中のエラーは CONTROL で伝える）
        return;
    default:
        break;
    }
    // 再接続中は、RESUME を確かめるまで着手を受け付けない
    if (m_phase == Phase::Reconnecting && frame.type != FrameType::Control && frame.type != FrameType::SyncRequest
        && frame.type != FrameType::SyncChunk) {
        return;
    }
    // 結果が一致した後の着手と承認は取り込まない。申し出た側の着手と相手の承諾が行き違うと、結果の指す手数より後の
    // 状態が届く。それを承認すると、双方の証跡が「結果が最後の状態を指す」検査に通らなくなる
    if (m_result.agreed && (frame.type == FrameType::StatePropose || frame.type == FrameType::StateAccept)) return;
    switch (frame.type) {
    case FrameType::StatePropose: {
        const std::optional<StateProposePayload> payload = StateProposePayload::fromCbor(frame.payload);
        if (!payload) return failWithError(ErrorCode::InvalidMove, lastState().ply);
        handleStatePropose(*payload);
        return;
    }
    case FrameType::StateAccept: {
        const std::optional<StateAcceptPayload> payload = StateAcceptPayload::fromCbor(frame.payload);
        if (!payload) return failWithError(ErrorCode::InvalidSignature, 0);
        handleStateAccept(*payload);
        return;
    }
    case FrameType::Control: {
        const std::optional<ControlPayload> payload = ControlPayload::fromCbor(frame.payload);
        if (!payload) return failWithError(ErrorCode::InvalidSignature, 0);
        handleControl(*payload);
        return;
    }
    case FrameType::SyncRequest: {
        const std::optional<SyncRequestPayload> request = SyncRequestPayload::fromCbor(frame.payload);
        if (!request) return failWithError(ErrorCode::StateConflict, 0);
        handleSyncRequest(*request);
        return;
    }
    case FrameType::SyncChunk: {
        const std::optional<SyncChunkPayload> chunk = SyncChunkPayload::fromCbor(frame.payload);
        if (!chunk) return failWithError(ErrorCode::StateConflict, 0);
        handleSyncChunk(*chunk);
        return;
    }
    case FrameType::ResultSignature: {
        const std::optional<ResultSignaturePayload> payload = ResultSignaturePayload::fromCbor(frame.payload);
        if (!payload) return failWithError(ErrorCode::InvalidSignature, 0);
        handleResultSignature(*payload);
        return;
    }
    default:
        // 開始の手順のフレームは、開始後には来ない
        return failWithError(ErrorCode::ConditionMismatch, 0);
    }
}

// ---------------------------------------------------------------- State0 と着手

void Match::handleStatePropose(const StateProposePayload& payload)
{
    const GameState& state = payload.state;
    const bool opponentIsBlack = !m_setup.iAmBlack;
    if (state.gameId != m_gameId) return failWithError(ErrorCode::ConditionMismatch, state.ply);

    // State0（先手が提案し、後手が受ける）
    if (state.ply == 0) {
        if (!m_line.isEmpty()) {
            // 対局中に新しい State0 を受けても再初期化しない（同じものなら承認を送り直す）
            if (state != m_line.at(0).state) return failWithError(ErrorCode::StateConflict, 0);
            send(FrameType::StateAccept, StateAcceptPayload{0, state.hash(), myApproval(m_line.at(0))}.toCbor());
            return;
        }
        if (m_setup.iAmBlack || state != GameRules::initialState(m_gameId, m_setup.terms)
            || !verifyApproval(state, payload.signature, true)) {
            return failWithError(ErrorCode::InvalidSignature, 0);
        }
        SignedState both = signedByMe(state);
        both.blackApproval = payload.signature;
        if (!persist(RecordKinds::state(both))) return failWithError(ErrorCode::StorageError, 0);
        m_line.append(both);
        send(FrameType::StateAccept, StateAcceptPayload{0, state.hash(), both.whiteApproval}.toCbor());
        sendControl(ControlKind::Ready, ControlBody::ready(state.hash()));
        // 後手は READY を送った時点から、先手の初手の時間を観測する
        m_phase = Phase::Playing;
        m_timer.startObserving(now());
        emitEvent(MatchEvent{MatchEvent::Type::Ready, {}, 0, std::nullopt});
        return;
    }
    if (m_line.isEmpty() || m_phase == Phase::Opening) return failWithError(ErrorCode::ConditionMismatch, state.ply);

    // 再送（同じ手数・同じ状態）には保存した署名を送り直す。盤・時計・棋譜は動かさない
    if (state.ply <= lastState().ply) {
        const SignedState& known = m_line.at(state.ply);
        if (known.state == state) {
            send(FrameType::StateAccept, StateAcceptPayload{state.ply, state.hash(), myApproval(known)}.toCbor());
            return;
        }
        // 同じ手数の異なる状態: 相手の2つの署名を証明として残して止める
        if (verifyApproval(state, payload.signature, opponentIsBlack)) {
            m_conflictProof = persist(RecordKinds::conflict(
                known.state, opponentIsBlack ? known.blackApproval : known.whiteApproval, state, payload.signature));
        }
        return failWithError(ErrorCode::StateConflict, state.ply);
    }
    if (state.ply != lastState().ply + 1 || isMyTurn() || m_pending) {
        return failWithError(ErrorCode::StateConflict, state.ply);
    }
    // 棋譜から終局が決まった局面の後の手は承認しない（承認すると、千日手・連続王手の千日手で決まった結果を、
    // 相手が次の手で消せる。手順は終局の局面で終わり、証跡は棋譜から決まる結果を示す）
    if (isDecidedByRecord()) return failWithError(ErrorCode::InvalidMove, state.ply);
    if (!verifyApproval(state, payload.signature, opponentIsBlack)) {
        return failWithError(ErrorCode::InvalidSignature, state.ply);
    }
    switch (GameRules::checkNext(lastState(), state, m_setup.terms)) {
    case GameRules::StateCheck::Ok:
        break;
    case GameRules::StateCheck::ClockMismatch:
        return failWithError(ErrorCode::ClockMismatch, state.ply);
    default:
        return failWithError(ErrorCode::InvalidMove, state.ply);
    }
    // 自己申告の消費時間と観測時間の差（実装仕様 7.1節）
    const qint64 observed = m_timer.observedMs(now());
    const qint64 disconnected = m_timer.disconnectedSinceObservingMs(now());
    const auto tolerance = static_cast<qint64>(NetworkPolicy::v1().toleranceMs);
    if (observed - static_cast<qint64>(state.spentMs) > tolerance + disconnected) {
        return failWithError(ErrorCode::ClockMismatch, state.ply);
    }
    SignedState both = signedByMe(state);
    (opponentIsBlack ? both.blackApproval : both.whiteApproval) = payload.signature;
    if (!persist(RecordKinds::state(both))) return failWithError(ErrorCode::StorageError, state.ply);
    send(FrameType::StateAccept, StateAcceptPayload{state.ply, state.hash(), myApproval(both)}.toCbor());
    // 自分の手番は、相手の着手を確かめて STATE_ACCEPT を送った時点から
    m_timer.startMyTurn(now());
    commit(both);
}

void Match::handleStateAccept(const StateAcceptPayload& payload)
{
    // 承認済みの手の承認の再送は無視する。自分に未承認の手があるときも同じ（提案の再送に相手が承認を送り直し、
    // それが、こちらが次の手を提案した後に届くことがある）
    if (!m_line.isEmpty() && payload.ply <= lastState().ply && m_line.at(payload.ply).state.hash() == payload.stateHash) {
        return;
    }
    if (!m_pending) return failWithError(ErrorCode::StateConflict, payload.ply);
    const GameState& state = m_pending->state.state;
    if (payload.ply != state.ply || payload.stateHash != state.hash()) {
        return failWithError(ErrorCode::StateConflict, payload.ply);
    }
    if (!verifyApproval(state, payload.signature, !m_setup.iAmBlack)) {
        return failWithError(ErrorCode::InvalidSignature, payload.ply);
    }
    SignedState both = m_pending->state;
    (m_setup.iAmBlack ? both.whiteApproval : both.blackApproval) = payload.signature;
    if (!persist(RecordKinds::state(both))) return failWithError(ErrorCode::StorageError, payload.ply);
    m_pending.reset();
    // State0 は READY を待つ
    if (both.state.ply == 0) m_line.append(both);
    else commit(both);
}

void Match::commit(const SignedState& state)
{
    m_line.append(state);
    // ply が進んだら、それまでの提案と申し出は無効
    m_result.clearProposals();
    emitEvent(MatchEvent{MatchEvent::Type::MoveCommitted, state.state.moveUsi, state.state.spentMs, std::nullopt});
    // 棋譜から決まる終局は、どちらの側も提案する（同じ提案を交わした時点で一致）
    if (const std::optional<GameRules::Terminal>& terminal = m_line.terminal()) {
        proposeResult(terminal->outcome, terminal->cause);
    }
}

Match::MoveError Match::playMove(const UsiMove& move)
{
    if (m_phase == Phase::Finished || m_phase == Phase::Finishing || m_result.agreed) return MoveError::Finished;
    if (m_phase == Phase::Opening || !isMyTurn()) return MoveError::NotMyTurn;
    // 棋譜から終局が決まった局面では、もう指さない（相手の結果の提案が届く前に指すと、千日手などの後の手を相手が
    // 承認し、双方の結果が食い違う。監査 2026-11-21 U1）
    if (isDecidedByRecord()) return MoveError::Finished;
    // 二重送信の禁止（切断中も1手だけ指せる）
    if (m_pending) return MoveError::Pending;
    // 消費時間は、状態を作る直前に固定する
    const auto spent = static_cast<quint64>(m_timer.mySpentMs(now()));
    if (!GameRules::canMoveWithin(myMainMs(), m_setup.terms.byoyomiMs, spent)) return MoveError::TimeUp;
    const std::optional<GameState> next = GameRules::nextState(lastState(), move, spent, m_setup.terms);
    // 合法手でなければ計時を続ける
    if (!next) return MoveError::Illegal;
    const SignedState mine = signedByMe(*next);
    if (!persist(RecordKinds::state(mine))) {
        failWithError(ErrorCode::StorageError, next->ply);
        return MoveError::Finished;
    }
    Frame frame;
    frame.type = FrameType::StatePropose;
    frame.requestId = m_requestId++;
    frame.gameId = m_gameId;
    frame.payload = StateProposePayload{*next, myApproval(mine)}.toCbor();
    m_pending = PendingMove{mine, frame, false, 0};
    // 申し出への答えを待っている間に指したら、申し出は断ったことにする
    if (m_result.opponentOffer) answerOffer(false);
    if (m_phase == Phase::Playing) resendPending();
    return MoveError::None;
}

void Match::resendPending()
{
    if (!m_pending || !m_pending->frame || m_phase != Phase::Playing) return;
    if (!m_pending->sent) {
        // 観測は、自分の STATE_PROPOSE を最初に送信キューへ渡した時点から
        m_pending->sent = true;
        m_timer.startObserving(now());
    }
    m_pending->sentAt = now();
    m_outgoing.push_back(*m_pending->frame);
}

// ---------------------------------------------------------------- 期限

void Match::tick()
{
    const qint64 t = now();
    const NetworkPolicy policy = NetworkPolicy::v1();
    switch (m_phase) {
    case Phase::Finished:
        return;
    case Phase::Opening:
        if (t > m_openingDeadline) finish(Settlement::Error, ErrorCode::Expired);
        return;
    case Phase::Finishing:
        if (t - m_result.agreedAt > kResultSignatureWaitMs) finish(Settlement::SignatureMissing, std::nullopt);
        return;
    case Phase::Reconnecting:
        if (m_timer.currentDisconnectMs(t) > static_cast<qint64>(policy.reconnectWaitMs)) {
            finish(Settlement::Disconnected, std::nullopt);
        } else if (m_timer.disconnectedTotalMs(t) > static_cast<qint64>(policy.disconnectTotalMs)) {
            finish(Settlement::Aborted, ErrorCode::DisconnectLimit);
        }
        return;
    case Phase::Playing:
        break;
    }
    if (m_control.isOverLimit()) return failWithError(ErrorCode::LimitExceeded, lastState().ply);
    // 1局の実時間の上限: 合意中断を提案する（合意できなければ未確定で終える）
    if (t - m_startedAt > static_cast<qint64>(policy.gameLimitMs)) {
        if (!m_result.myProposal) proposeResult(Outcome::Aborted, Cause::AgreedAbort);
        if (t - m_startedAt > static_cast<qint64>(policy.gameLimitMs + policy.reconnectWaitMs)) {
            abort(ErrorCode::GameTimeLimit);
        }
        return;
    }
    // 着手の確認: 5秒で同じメッセージを再送し、30秒で接続の復旧へ移る
    if (m_pending && m_pending->sent) {
        if (m_timer.observedMs(t) > static_cast<qint64>(policy.disconnectDetectMs)) {
            connectionLost();
            return;
        }
        if (t - m_pending->sentAt >= static_cast<qint64>(policy.resendMs)) resendPending();
    }
    if (m_result.agreed) return;
    const quint64 byoyomi = m_setup.terms.byoyomiMs;
    if (isMyTurn() && !m_pending) {
        // 自分の時計が切れたら、負けを申告する
        if (!m_myTimeoutDeclared && !GameRules::canMoveWithin(myMainMs(), byoyomi, static_cast<quint64>(m_timer.mySpentMs(t)))) {
            m_myTimeoutDeclared = true;
            proposeResult(GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::White : Turn::Black), Cause::Timeout);
        }
    } else if (!isMyTurn() && m_claimedTimeoutPly != lastState().ply) {
        // 相手の着手が届かないまま、相手の R + B に許容差を足した時間を超えたら、時間切れの勝ちを主張できる
        const qint64 observed = m_timer.observedMs(t) - m_timer.disconnectedSinceObservingMs(t);
        if (observed > static_cast<qint64>(opponentMainMs() + byoyomi + policy.toleranceMs)) {
            m_claimedTimeoutPly = lastState().ply;
            proposeResult(GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::Black : Turn::White), Cause::Timeout);
        }
    }
}

// ---------------------------------------------------------------- 制御メッセージ

void Match::sendControl(ControlKind kind, const CborValue& body)
{
    if (m_phase == Phase::Reconnecting && kind != ControlKind::Resume) return;
    const CborValue value = m_control.make(kind, body, m_sessionKey).toCbor();
    if (kind != ControlKind::Ready && kind != ControlKind::Resume) persist(RecordKinds::control(true, value));
    send(FrameType::Control, value);
}

void Match::handleControl(const ControlPayload& payload)
{
    switch (m_control.receive(payload, opponentSessionKey())) {
    case ControlChannel::Receipt::Accepted:
        break;
    case ControlChannel::Receipt::Ignored:
        return;
    case ControlChannel::Receipt::OutOfOrder:
        return failWithError(ErrorCode::StateConflict, 0);
    case ControlChannel::Receipt::BadSignature:
        return failWithError(ErrorCode::InvalidSignature, 0);
    case ControlChannel::Receipt::OverLimit:
        return failWithError(ErrorCode::LimitExceeded, 0);
    }
    const ControlKind kind = payload.control.kind;
    // ジャーナルに残すのは、中身の形が合う制御メッセージだけ（bodyHash が合えば body は任意の配列なので、形を確かめずに
    // 書くと、相手が大きな body でジャーナルの上限を使わせられる）。書けなければ続けない（実装仕様4章 STORAGE_ERROR。
    // 監査 2026-11-13 P18）
    if (kind != ControlKind::Ready && kind != ControlKind::Resume && hasKnownShape(kind, payload.body)
        && !persist(RecordKinds::control(false, payload.toCbor()))) {
        return failWithError(ErrorCode::StorageError, lastState().ply);
    }
    // 再接続中は、RESUME を確かめるまでほかの制御を受け付けない
    if (m_phase == Phase::Reconnecting && kind != ControlKind::Resume) return;

    switch (kind) {
    case ControlKind::Ready:
        handleReady(payload.body);
        return;
    case ControlKind::Resume:
        handleResume(payload.body);
        return;
    case ControlKind::ResultPropose:
        handleResultPropose(payload.body, payload.control.bodyHash);
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
        if (declined && m_result.myProposal && *declined == m_result.myProposal->digest()) {
            // 相手が答えたので、同じ手数でももう一度申し出られる
            m_result.myProposal.reset();
            m_result.drawOfferAwaitingAnswer = false;
            m_result.abortOfferAwaitingAnswer = false;
            emitEvent(MatchEvent{MatchEvent::Type::ProposalDeclined, {}, 0, std::nullopt});
        }
        return;
    }
    }
}

bool Match::hasKnownShape(ControlKind kind, const CborValue& body)
{
    switch (kind) {
    case ControlKind::Ready: return ControlBody::parseReady(body).has_value();
    case ControlKind::Resume: return ControlBody::parseResume(body).has_value();
    case ControlKind::ResultPropose: return FinalResult::fromCbor(body).has_value();
    case ControlKind::Abort: return ControlBody::parseAbort(body).has_value();
    case ControlKind::Error: return ControlBody::parseError(body).has_value();
    case ControlKind::ResultDecline: return ControlBody::parseDecline(body).has_value();
    }
    return false;
}

void Match::handleReady(const CborValue& body)
{
    const std::optional<QByteArray> state0Hash = ControlBody::parseReady(body);
    // READY は後手が送る。先手は受けた時点で初手の計時を始める（再送の READY で時計を初期化し直さない）
    if (!m_setup.iAmBlack || m_phase != Phase::Opening) return;
    if (!state0Hash || m_line.size() != 1 || *state0Hash != m_line.at(0).state.hash()) {
        return failWithError(ErrorCode::StateConflict, 0);
    }
    m_phase = Phase::Playing;
    m_timer.startMyTurn(now());
    emitEvent(MatchEvent{MatchEvent::Type::Ready, {}, 0, std::nullopt});
}

// ---------------------------------------------------------------- 結果

bool Match::proposeResult(Outcome outcome, Cause cause)
{
    if (m_phase != Phase::Playing || m_result.agreed || m_line.isEmpty()) return false;
    // 自分の未確認の着手がある間は提案しない
    if (m_pending) return false;
    // 棋譜から終局が決まっていれば、その結果のほかは提案しない（投了・時間切れ・合意の申し出で、棋譜と食い違う結果に
    // 署名しない。監査 2026-11-13 P7）
    if (const std::optional<GameRules::Terminal>& terminal = m_line.terminal();
        terminal && (terminal->outcome != outcome || terminal->cause != cause)) {
        return false;
    }
    const FinalResult result = m_line.resultFor(outcome, cause);
    if (m_result.myProposal && *m_result.myProposal == result) return true;
    m_result.myProposal = result;
    sendControl(ControlKind::ResultPropose, result.toCbor());
    // 相手の同じ提案をすでに受けていれば、これで一致
    if (m_result.opponentOffer && *m_result.opponentOffer == result) {
        m_result.opponentOffer.reset();
        agree(result);
    }
    return true;
}

void Match::acceptProposal(const FinalResult& result)
{
    m_result.myProposal = result;
    sendControl(ControlKind::ResultPropose, result.toCbor());
    agree(result);
}

void Match::decline(const QByteArray& resultHash)
{
    sendControl(ControlKind::ResultDecline, ControlBody::decline(resultHash));
}

bool Match::resign()
{
    // 投了は負ける側だけ、自分の手番に限る
    if (!isMyTurn() || m_pending) return false;
    return proposeResult(GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::White : Turn::Black), Cause::Resign);
}

Match::OfferResult Match::offer(Cause cause)
{
    if (cause != Cause::AgreedDraw && cause != Cause::AgreedAbort) return OfferResult::Unavailable;
    // 相手が答えるまでの間に申し出られるのは、引き分け・中断それぞれ1回まで（答えを待たずに送り直して、相手に何度も
    // 尋ねない）。相手が断れば、同じ手数でももう一度申し出られる（手数が進んだときも。commit）
    bool& awaitingAnswer = cause == Cause::AgreedDraw ? m_result.drawOfferAwaitingAnswer : m_result.abortOfferAwaitingAnswer;
    if (awaitingAnswer) return OfferResult::AwaitingAnswer;
    if (!proposeResult(cause == Cause::AgreedDraw ? Outcome::Draw : Outcome::Aborted, cause)) return OfferResult::Unavailable;
    awaitingAnswer = true;
    return OfferResult::Sent;
}

std::optional<Cause> Match::offerAwaitingAnswer() const
{
    // 自分の提案は1つだけ持つ（申し出の後で時間切れなどを提案したら、申し出はもう待っていない）。再接続の間は、
    // つなぎ直した後に送り直すので、待っているものとして扱う
    if ((m_phase != Phase::Playing && m_phase != Phase::Reconnecting) || m_result.agreed || !m_result.myProposal) {
        return std::nullopt;
    }
    const Cause cause = m_result.myProposal->cause;
    if (cause != Cause::AgreedDraw && cause != Cause::AgreedAbort) return std::nullopt;
    return cause;
}

void Match::answerOffer(bool accept)
{
    if (!m_result.opponentOffer) return;
    const FinalResult offered = *m_result.opponentOffer;
    m_result.opponentOffer.reset();
    // 回線が切れている間は承諾を送れないので、一致させない（つなぎ直した後に相手が申し出を送り直し、もう一度尋ねる）
    if (accept && m_phase == Phase::Playing && m_line.isCurrent(offered) && !m_pending) {
        acceptProposal(offered);
        return;
    }
    decline(offered.digest());
}

void Match::handleResultPropose(const CborValue& body, const QByteArray& bodyHash)
{
    if (m_result.agreed) return;   // 結果の一致の後は、別の結果に同意しない
    // 結果の提案を受けるのは、READY を交わして対局が始まった後だけ。開始の手続きの途中の提案（先手が READY を待つ
    // 間の「先手の時間切れ」など）には同意しない
    if (m_phase != Phase::Playing) return decline(bodyHash);
    const std::optional<FinalResult> result = FinalResult::fromCbor(body);
    // 古い手数の提案・形の合わない提案には同意しない（対局は続く）
    if (!result || m_line.isEmpty() || !m_line.isCurrent(*result)) return decline(bodyHash);
    // 自分の提案と同じなら一致
    if (m_result.myProposal && *m_result.myProposal == *result) return agree(*result);

    const bool opponentToMove = !isMyTurn();
    const Outcome iWin = GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::Black : Turn::White);
    // 棋譜から終局（詰み・千日手・512手など）が決まった局面では、投了・時間切れ・合意の申し出に同意しない（利用者にも
    // 尋ねない）。同意すると、署名した結果が棋譜と食い違い、証跡の検証に通らなくなる（監査 2026-11-13 P7）
    const std::optional<GameRules::Terminal>& terminal = m_line.terminal();
    switch (result->cause) {
    case Cause::Resign:
        // 投了の提案は負ける側（相手）だけ、相手の手番に限る
        if (terminal || result->outcome != iWin || !opponentToMove) return decline(bodyHash);
        return acceptProposal(*result);
    case Cause::Checkmate:
    case Cause::Repetition:
    case Cause::PerpetualCheck:
    case Cause::MaxPly:
        if (!terminal) return decline(bodyHash);
        // 判定の食い違いは実装の不一致。同意せずに中断し、証跡を残す
        if (terminal->outcome != result->outcome || terminal->cause != result->cause) {
            return failWithError(ErrorCode::ConditionMismatch, lastState().ply);
        }
        proposeResult(result->outcome, result->cause);
        return agree(*result);
    case Cause::Timeout:
        if (terminal) return decline(bodyHash);
        if (result->outcome == iWin) {
            // 相手が自分の時間切れを申告した
            if (!opponentToMove) return decline(bodyHash);
        } else {
            // 相手が自分の時間切れを主張した: 自分の時計で確かめる
            const auto elapsed = static_cast<quint64>(m_timer.mySpentMs(now()));
            if (!isMyTurn() || m_pending || !m_timer.myTurnStarted()
                || GameRules::canMoveWithin(myMainMs(), m_setup.terms.byoyomiMs, elapsed)) {
                return decline(bodyHash);
            }
        }
        return acceptProposal(*result);
    case Cause::AgreedDraw:
    case Cause::AgreedAbort:
        if (terminal) return decline(bodyHash);
        // 利用者に尋ねる（答えるまでに手数が進めば無効）
        m_result.opponentOffer = result;
        emitEvent(MatchEvent{MatchEvent::Type::OfferReceived, {}, 0, *result});
        return;
    }
}

void Match::agree(const FinalResult& result)
{
    // 結果に署名するのは、対局が始まった後に1回だけ（呼ぶ側も確かめている）
    if (m_result.agreed || m_phase != Phase::Playing) return;
    // 1. gameId -> resultDigest を永続化する（以後、この対局で別の結果に署名しない）
    if (!persist(RecordKinds::resultDigest(result))) return failWithError(ErrorCode::StorageError, result.ply);
    m_result.agreed = result;
    m_phase = Phase::Finishing;
    m_result.agreedAt = now();
    emitEvent(MatchEvent{MatchEvent::Type::ResultAgreed, {}, 0, result});
    // 2. プレイヤー鍵で FinalResult に署名し、RESULT_SIGNATURE を送る
    m_result.mySignature = m_playerKey ? m_playerKey->sign(result.toCbor()) : QByteArray();
    persist(RecordKinds::resultSignature(mySide(), m_result.mySignature));
    ResultSignaturePayload payload;
    payload.result = result;
    payload.signature.side = mySide();
    payload.signature.signature = m_result.mySignature;
    send(FrameType::ResultSignature, payload.toCbor());
    // 相手の署名が先に届いていれば、ここでそろう
    if (!m_result.opponentSignature.isEmpty()) finish(Settlement::MutuallySigned, std::nullopt);
}

void Match::handleResultSignature(const ResultSignaturePayload& payload)
{
    if (payload.signature.side != opponentSide()) return failWithError(ErrorCode::InvalidSignature, 0);
    if (!Crypto::verify(opponentPlayerKey(), payload.result.toCbor(), payload.signature.signature)) {
        return failWithError(ErrorCode::InvalidSignature, 0);
    }
    if (m_result.agreed && payload.result != *m_result.agreed) return failWithError(ErrorCode::StateConflict, 0);
    // 同じ内容の署名の再送は許す
    if (!m_result.opponentSignature.isEmpty()) return;
    // 相手の提案より先に署名は届かない（フレームは順に届く）。合わない順序は受け付けない
    if (!m_result.agreed) return;
    m_result.opponentSignature = payload.signature.signature;
    persist(RecordKinds::resultSignature(opponentSide(), m_result.opponentSignature));
    finish(Settlement::MutuallySigned, std::nullopt);
}

void Match::abort(ErrorCode reason)
{
    if (m_phase == Phase::Finished) return;
    if (m_phase != Phase::Reconnecting && !m_line.isEmpty()) {
        sendControl(ControlKind::Abort, ControlBody::abort(lastState().hash(), reason));
    }
    finish(Settlement::Aborted, reason);
}

// ---------------------------------------------------------------- 再接続

void Match::connectionLost()
{
    switch (m_phase) {
    case Phase::Finished:
    case Phase::Reconnecting:
        return;
    case Phase::Opening:
        finish(Settlement::Error, ErrorCode::Expired);
        return;
    case Phase::Finishing:
        // 結果は一致している。相手の署名を待たずに、署名のない証跡として閉じる
        finish(Settlement::SignatureMissing, std::nullopt);
        return;
    case Phase::Playing:
        break;
    }
    m_phase = Phase::Reconnecting;
    m_resync = Resync::AwaitingConnection;
    m_myRandom.clear();
    m_timer.disconnected(now());
    emitEvent(MatchEvent{MatchEvent::Type::Reconnecting, {}, 0, std::nullopt});
}

void Match::connectionEstablished()
{
    if (m_phase != Phase::Reconnecting) return;
    // 接続ごとに新しい乱数で connectionId を決め直し、制御メッセージの番号を数え直す
    m_myRandom = Crypto::randomBytes(kNonceSize);
    m_control.restart();
    m_resync = Resync::AwaitingHello;
    send(FrameType::Hello, ReconnectHello{mySide(), m_myRandom}.toCbor());
}

void Match::handleReconnectHello(const ReconnectHello& hello)
{
    if (m_phase != Phase::Reconnecting || m_resync != Resync::AwaitingHello || hello.side != opponentSide()
        || hello.connectionRandom == m_myRandom) {
        return;
    }
    m_control.setConnectionId(connectionId(m_gameId, m_setup.iAmBlack ? m_myRandom : hello.connectionRandom,
                                           m_setup.iAmBlack ? hello.connectionRandom : m_myRandom));
    // 新しい connectionId に対する RESUME の一局用鍵の署名で、相手を確かめ直す
    m_resync = Resync::AwaitingResume;
    sendControl(ControlKind::Resume, ControlBody::resume(lastState().ply, lastState().hash()));
}

void Match::handleResume(const CborValue& body)
{
    const std::optional<ControlBody::Resume> resume = ControlBody::parseResume(body);
    if (!resume || m_phase != Phase::Reconnecting || m_resync != Resync::AwaitingResume) {
        return failWithError(ErrorCode::StateConflict, 0);
    }
    const quint32 myPly = lastState().ply;
    if (resume->latestPly > myPly) {
        // 自分が古い: 足りない分を求める
        m_resync = Resync::AwaitingSyncChunks;
        send(FrameType::SyncRequest, SyncRequestPayload{myPly + 1, lastState().hash()}.toCbor());
        return;
    }
    // 相手の最新は自分も持っている: 同じ状態であること
    if (m_line.at(resume->latestPly).state.hash() != resume->latestStateHash) {
        return failWithError(ErrorCode::StateConflict, resume->latestPly);
    }
    // 同じなら照合は終わり。相手が古ければ、相手の SYNC_REQUEST に応えてから終える
    if (resume->latestPly == myPly) finishResume();
    else m_resync = Resync::AwaitingSyncRequest;
}

void Match::handleSyncRequest(const SyncRequestPayload& request)
{
    // 応えるのは、再接続の照合で相手の要求を待っている間の1回だけ（照合の後も何度でも全状態を送り返して、中継の量の
    // 上限を使わせない。監査 2026-11-13 P17）。それ以外の要求は無視する
    if (m_phase != Phase::Reconnecting || m_resync != Resync::AwaitingSyncRequest) return;
    if (request.fromPly == 0 || request.fromPly > lastState().ply
        || m_line.at(request.fromPly - 1).state.hash() != request.previousStateHash) {
        return failWithError(ErrorCode::StateConflict, request.fromPly);
    }
    // 連続した State を両署名とともに返す（要求の外のデータは送らない）
    quint32 ply = request.fromPly;
    while (ply <= lastState().ply) {
        SyncChunkPayload chunk;
        while (ply <= lastState().ply && chunk.states.size() < kMaxSyncChunkStates) chunk.states.push_back(m_line.at(ply++));
        chunk.final = ply > lastState().ply;
        send(FrameType::SyncChunk, chunk.toCbor());
    }
    finishResume();
}

void Match::handleSyncChunk(const SyncChunkPayload& chunk)
{
    if (m_phase != Phase::Reconnecting || m_resync != Resync::AwaitingSyncChunks) {
        return failWithError(ErrorCode::StateConflict, 0);
    }
    for (const SignedState& s : chunk.states) {
        // 棋譜から終局が決まった局面の後の状態は取り込まない（着手の提案と同じ）
        if (isDecidedByRecord()) return failWithError(ErrorCode::StateConflict, s.state.ply);
        if (s.state.ply != lastState().ply + 1 || !verifyApproval(s.state, s.blackApproval, true)
            || !verifyApproval(s.state, s.whiteApproval, false)
            || GameRules::checkNext(lastState(), s.state, m_setup.terms) != GameRules::StateCheck::Ok) {
            return failWithError(ErrorCode::StateConflict, s.state.ply);
        }
        // 確認待ちだった自分の手と違う状態は受け入れない（部分署名だけの別の分岐へ巻き戻さない）
        const bool replacesPending = m_pending && s.state.ply == m_pending->state.state.ply;
        if (replacesPending && s.state != m_pending->state.state) return failWithError(ErrorCode::StateConflict, s.state.ply);
        if (!persist(RecordKinds::state(s))) return failWithError(ErrorCode::StorageError, s.state.ply);
        const bool byMe = isMyTurn();
        if (replacesPending) m_pending.reset();
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
    m_timer.reconnected(now());
    m_phase = Phase::Playing;
    emitEvent(MatchEvent{MatchEvent::Type::Resumed, {}, 0, std::nullopt});
    // 相手の承認のない自分の手は、同じバイト列で送り直す
    if (m_pending) {
        m_pending->sent = false;
        resendPending();
    }
    // 結果を提案していたなら送り直す
    if (m_result.myProposal) {
        const FinalResult proposal = *m_result.myProposal;
        m_result.myProposal.reset();
        proposeResult(proposal.outcome, proposal.cause);
    }
}

} // namespace P2p
