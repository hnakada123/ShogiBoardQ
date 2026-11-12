/// @file p2pmatch.cpp
/// @brief 開始後の対局の状態機械の実装（State0・着手の合意・時計・期限）

#include "p2pmatch.h"

#include "p2precordkinds.h"

namespace P2p {

namespace {

/// State0 と READY を終えるまでの期限（実装仕様 3.3節）
constexpr qint64 kOpeningTimeoutMs = 30'000;
/// 相手の最終結果への署名を待つ時間（実装仕様 8.2節）
constexpr qint64 kResultSignatureWaitMs = 30'000;
/// 1局の制御メッセージの上限（実装仕様 4章）
constexpr quint64 kMaxControlMessages = 4096;

} // namespace

Match::Match(MatchSetup setup, SigningKey sessionKey, const SigningKey* playerKey, MonotonicClock clock, Persist persist)
    : m_setup(std::move(setup))
    , m_sessionKey(std::move(sessionKey))
    , m_playerKey(playerKey)
    , m_clock(std::move(clock))
    , m_persist(std::move(persist))
    , m_gameId(m_setup.agreement.gameId())
    , m_myRandom(m_setup.myConnectionRandom)
    , m_opponentRandom(m_setup.opponentConnectionRandom)
{
    m_requestId = qMax<quint64>(1, m_setup.nextRequestId);
    m_connectionId = connectionId(m_gameId, m_setup.iAmBlack ? m_myRandom : m_opponentRandom,
                                  m_setup.iAmBlack ? m_opponentRandom : m_myRandom);
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

bool Match::verifyApproval(const GameState& state, const QByteArray& signature, bool black) const
{
    return Crypto::verify(m_setup.agreement.sessionKey(black), stateApproval(m_gameId, state.ply, state.hash()),
                          signature);
}

void Match::finish(Settlement ending, std::optional<ErrorCode> error, bool byMe)
{
    if (m_phase == Phase::Finished) return;
    m_phase = Phase::Finished;
    m_settlement = ending;
    m_endingError = error;
    persist(RecordKinds::ending(m_settlement));
    MatchEvent event;
    event.type = MatchEvent::Type::Finished;
    event.byMe = byMe;
    event.result = m_agreedResult;
    event.error = error;
    emitEvent(event);
}

void Match::failWithError(ErrorCode code, quint32 relatedPly)
{
    if (m_phase == Phase::Finished) return;
    // 検証の失敗では状態を変えずに中断へ移る（相手の負けとして記録しない）
    if (m_phase != Phase::Reconnecting) sendControl(ControlKind::Error, ControlBody::error(code, relatedPly));
    finish(Settlement::Error, code, true);
}

Evidence Match::evidence() const
{
    Evidence e;
    e.agreement = m_setup.agreement;
    e.blackStartSignature = m_setup.blackStartSignature;
    e.whiteStartSignature = m_setup.whiteStartSignature;
    e.timeControl = m_setup.terms;
    e.states = m_confirmed;
    // 相手の承認のない自分の最後の状態も残す（片方の署名だけの状態として）
    if (m_pending) e.states.push_back(*m_pending);
    if (m_agreedResult) {
        e.result = m_agreedResult;
        (m_setup.iAmBlack ? e.blackResultSignature : e.whiteResultSignature) = m_myResultSignature;
        (m_setup.iAmBlack ? e.whiteResultSignature : e.blackResultSignature) = m_opponentResultSignature;
    }
    return e;
}

// ---------------------------------------------------------------- 開始

void Match::begin()
{
    m_startedAt = now();
    m_openingDeadline = m_startedAt + kOpeningTimeoutMs;
    // 開始合意と両方の開始署名をジャーナルに書いてから始める（書けなければ何も送らずに中断する）
    if (!persist(RecordKinds::start(m_setup.agreement, m_setup.terms, mySide(), m_setup.opponentDisplayName))
        || !persist(RecordKinds::startSignature(kSideBlack, m_setup.blackStartSignature))
        || !persist(RecordKinds::startSignature(kSideWhite, m_setup.whiteStartSignature))) {
        abort(ErrorCode::StorageError);
        return;
    }
    if (!m_setup.iAmBlack) return;
    // 先手が State0 を提案する
    const GameState state0 = GameRules::initialState(m_gameId, m_setup.terms);
    const SignedState mine = signedByMe(state0);
    if (!persist(RecordKinds::state(mine))) return failWithError(ErrorCode::StorageError, 0);
    m_pending = mine;
    StateProposePayload payload;
    payload.state = state0;
    payload.signature = mine.blackApproval;
    send(FrameType::StatePropose, payload.toCbor());
}

const GameState& Match::lastState() const
{
    static const GameState kNoState;
    return m_confirmed.empty() ? kNoState : m_confirmed.back().state;
}

bool Match::isMyTurn() const
{
    if (m_confirmed.empty()) return false;
    return (GameRules::sideToMove(lastState().ply) == Turn::Black) == m_setup.iAmBlack;
}

qint64 Match::currentTurnElapsedMs() const
{
    if (m_phase == Phase::Opening || m_confirmed.empty()) return 0;
    if (isMyTurn()) return now() - m_myTurnStart;
    return now() - m_observeStart;
}

qint64 Match::reconnectTotalMs() const
{
    return m_reconnectTotal + (m_reconnectStart >= 0 ? now() - m_reconnectStart : 0);
}

qint64 Match::reconnectRemainingMs() const
{
    if (m_reconnectStart < 0) return -1;
    return qMax<qint64>(0, static_cast<qint64>(NetworkPolicy::v1().reconnectWaitMs) - (now() - m_reconnectStart));
}

void Match::startMyTurn()
{
    m_myTurnStart = now();
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
    if (m_agreedResult && (frame.type == FrameType::StatePropose || frame.type == FrameType::StateAccept)) return;
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

void Match::handleStatePropose(const StateProposePayload& payload)
{
    const GameState& state = payload.state;
    const bool opponentIsBlack = !m_setup.iAmBlack;
    if (state.gameId != m_gameId) return failWithError(ErrorCode::ConditionMismatch, state.ply);

    // State0（先手が提案し、後手が受ける）
    if (state.ply == 0) {
        if (!m_confirmed.empty()) {
            // 対局中に新しい State0 を受けても再初期化しない（同じものなら承認を送り直す）
            if (state == m_confirmed.front().state) {
                StateAcceptPayload accept{0, state.hash(), m_setup.iAmBlack ? m_confirmed.front().blackApproval
                                                                            : m_confirmed.front().whiteApproval};
                send(FrameType::StateAccept, accept.toCbor());
                return;
            }
            return failWithError(ErrorCode::StateConflict, 0);
        }
        if (m_setup.iAmBlack || state != GameRules::initialState(m_gameId, m_setup.terms)
            || !verifyApproval(state, payload.signature, true)) {
            return failWithError(ErrorCode::InvalidSignature, 0);
        }
        SignedState both = signedByMe(state);
        both.blackApproval = payload.signature;
        if (!persist(RecordKinds::state(both))) return failWithError(ErrorCode::StorageError, 0);
        m_confirmed.push_back(both);
        send(FrameType::StateAccept, StateAcceptPayload{0, state.hash(), both.whiteApproval}.toCbor());
        sendControl(ControlKind::Ready, ControlBody::ready(state.hash()));
        // 後手は READY を送った時点から、先手の初手の時間を観測する
        m_readyDone = true;
        m_phase = Phase::Playing;
        m_observeStart = now();
        m_reconnectAtObserveStart = reconnectTotalMs();
        emitEvent(MatchEvent{MatchEvent::Type::Ready, 0, {}, false, 0, std::nullopt, std::nullopt});
        return;
    }
    if (m_confirmed.empty() || !m_readyDone) return failWithError(ErrorCode::ConditionMismatch, state.ply);

    // 再送（同じ手数・同じ状態）には保存した署名を送り直す。盤・時計・棋譜は動かさない
    if (state.ply <= lastState().ply) {
        const SignedState& known = m_confirmed[state.ply];
        if (known.state == state) {
            send(FrameType::StateAccept,
                 StateAcceptPayload{state.ply, state.hash(), m_setup.iAmBlack ? known.blackApproval : known.whiteApproval}
                     .toCbor());
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
    const qint64 observed = now() - m_observeStart;
    const qint64 disconnected = reconnectTotalMs() - m_reconnectAtObserveStart;
    const auto tolerance = static_cast<qint64>(NetworkPolicy::v1().toleranceMs);
    if (observed - static_cast<qint64>(state.spentMs) > tolerance + disconnected) {
        return failWithError(ErrorCode::ClockMismatch, state.ply);
    }
    SignedState both = signedByMe(state);
    (opponentIsBlack ? both.blackApproval : both.whiteApproval) = payload.signature;
    if (!persist(RecordKinds::state(both))) return failWithError(ErrorCode::StorageError, state.ply);
    send(FrameType::StateAccept,
         StateAcceptPayload{state.ply, state.hash(), m_setup.iAmBlack ? both.blackApproval : both.whiteApproval}.toCbor());
    // 自分の手番は、相手の着手を確かめて STATE_ACCEPT を送った時点から
    startMyTurn();
    commit(both, false);
}

void Match::handleStateAccept(const StateAcceptPayload& payload)
{
    // 承認済みの手の承認の再送は無視する。自分に未承認の手があるときも同じ（提案の再送に相手が承認を送り直し、
    // それが、こちらが次の手を提案した後に届くことがある）
    if (!m_confirmed.empty() && payload.ply <= lastState().ply && m_confirmed[payload.ply].state.hash() == payload.stateHash) {
        return;
    }
    if (!m_pending) return failWithError(ErrorCode::StateConflict, payload.ply);
    const GameState& state = m_pending->state;
    if (payload.ply != state.ply || payload.stateHash != state.hash()) {
        return failWithError(ErrorCode::StateConflict, payload.ply);
    }
    if (!verifyApproval(state, payload.signature, !m_setup.iAmBlack)) {
        return failWithError(ErrorCode::InvalidSignature, payload.ply);
    }
    SignedState both = *m_pending;
    (m_setup.iAmBlack ? both.whiteApproval : both.blackApproval) = payload.signature;
    if (!persist(RecordKinds::state(both))) return failWithError(ErrorCode::StorageError, payload.ply);
    m_pending.reset();
    m_pendingFrame.clear();
    m_pendingSent = false;
    if (state.ply == 0) {
        // State0 は READY を待つ
        m_confirmed.push_back(both);
        return;
    }
    commit(both, true);
}

void Match::commit(const SignedState& state, bool byMe)
{
    m_confirmed.push_back(state);
    // ply が進んだら、それまでの提案と申し出は無効
    m_myProposal.reset();
    m_myProposalHash.clear();
    m_pendingOffer.reset();
    m_pendingOfferHash.clear();
    MatchEvent event;
    event.type = MatchEvent::Type::MoveCommitted;
    event.ply = state.state.ply;
    event.moveUsi = state.state.moveUsi;
    event.byMe = byMe;
    event.spentMs = state.state.spentMs;
    emitEvent(event);
    checkTerminal();
}

void Match::checkTerminal()
{
    std::vector<GameState> states;
    states.reserve(m_confirmed.size());
    for (const SignedState& s : m_confirmed) states.push_back(s.state);
    const std::optional<GameRules::Terminal> terminal = GameRules::judge(states);
    // 棋譜から決まる終局は、どちらの側も提案する（同じ提案を交わした時点で一致）
    if (terminal) proposeResult(terminal->outcome, terminal->cause);
}

// ---------------------------------------------------------------- 着手

Match::MoveError Match::playMove(const UsiMove& move)
{
    if (m_phase == Phase::Finished || m_phase == Phase::Finishing || m_agreedResult) return MoveError::Finished;
    if (m_phase == Phase::Opening || !isMyTurn()) return MoveError::NotMyTurn;
    // 二重送信の禁止（切断中も1手だけ指せる）
    if (m_pending) return MoveError::Pending;
    // 消費時間は、状態を作る直前に固定する
    const auto spent = static_cast<quint64>(qMax<qint64>(0, now() - m_myTurnStart));
    const quint64 remaining = m_setup.iAmBlack ? lastState().blackMainMs : lastState().whiteMainMs;
    if (!GameRules::canMoveWithin(remaining, m_setup.terms.byoyomiMs, spent)) return MoveError::TimeUp;
    const std::optional<GameState> next = GameRules::nextState(lastState(), move, spent, m_setup.terms);
    // 合法手でなければ計時を続ける
    if (!next) return MoveError::Illegal;
    const SignedState mine = signedByMe(*next);
    if (!persist(RecordKinds::state(mine))) {
        failWithError(ErrorCode::StorageError, next->ply);
        return MoveError::Finished;
    }
    m_pending = mine;
    StateProposePayload payload;
    payload.state = *next;
    payload.signature = m_setup.iAmBlack ? mine.blackApproval : mine.whiteApproval;
    Frame frame;
    frame.type = FrameType::StatePropose;
    frame.requestId = m_requestId++;
    frame.gameId = m_gameId;
    frame.payload = payload.toCbor();
    m_pendingFrame = frame.encode();
    m_pendingSent = false;
    // 申し出への答えを待っている間に指したら、申し出は断ったことにする
    if (m_pendingOffer) answerOffer(false);
    if (m_phase == Phase::Playing) resendPending();
    return MoveError::None;
}

void Match::resendPending()
{
    if (!m_pending || m_pendingFrame.isEmpty() || m_phase != Phase::Playing) return;
    const std::optional<Frame> frame = Frame::decode(m_pendingFrame);
    if (!frame) return;
    if (!m_pendingSent) {
        // 観測は、自分の STATE_PROPOSE を最初に送信キューへ渡した時点から
        m_pendingSent = true;
        m_observeStart = now();
        m_reconnectAtObserveStart = reconnectTotalMs();
    }
    m_pendingSentAt = now();
    m_outgoing.push_back(*frame);
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
        if (t > m_openingDeadline) finish(Settlement::Error, ErrorCode::Expired, true);
        return;
    case Phase::Finishing:
        if (t - m_resultAgreedAt > kResultSignatureWaitMs) finish(Settlement::SignatureMissing, std::nullopt, false);
        return;
    case Phase::Reconnecting:
        if (t - m_reconnectStart > static_cast<qint64>(policy.reconnectWaitMs)) {
            finish(Settlement::Disconnected, std::nullopt, false);
        } else if (reconnectTotalMs() > static_cast<qint64>(policy.disconnectTotalMs)) {
            finish(Settlement::Aborted, ErrorCode::DisconnectLimit, true);
        }
        return;
    case Phase::Playing:
        break;
    }
    if (m_controlCount > kMaxControlMessages) return failWithError(ErrorCode::LimitExceeded, lastState().ply);
    // 1局の実時間の上限: 合意中断を提案する（合意できなければ未確定で終える）
    if (t - m_startedAt > static_cast<qint64>(policy.gameLimitMs)) {
        if (!m_myProposal) proposeResult(Outcome::Aborted, Cause::AgreedAbort);
        if (t - m_startedAt > static_cast<qint64>(policy.gameLimitMs + policy.reconnectWaitMs)) {
            abort(ErrorCode::GameTimeLimit);
        }
        return;
    }
    // 着手の確認: 5秒で同じメッセージを再送し、30秒で接続の復旧へ移る
    if (m_pending && m_pendingSent) {
        if (t - m_observeStart > static_cast<qint64>(policy.disconnectDetectMs)) {
            connectionLost();
            return;
        }
        if (t - m_pendingSentAt >= static_cast<qint64>(policy.resendMs)) resendPending();
    }
    if (m_agreedResult) return;
    const quint64 byoyomi = m_setup.terms.byoyomiMs;
    if (isMyTurn() && !m_pending) {
        // 自分の時計が切れたら、負けを申告する
        const quint64 remaining = m_setup.iAmBlack ? lastState().blackMainMs : lastState().whiteMainMs;
        if (!m_myTimeoutDeclared && !GameRules::canMoveWithin(remaining, byoyomi, static_cast<quint64>(t - m_myTurnStart))) {
            m_myTimeoutDeclared = true;
            proposeResult(GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::White : Turn::Black), Cause::Timeout);
        }
    } else if (!isMyTurn() && m_claimedTimeoutPly != lastState().ply) {
        // 相手の着手が届かないまま、相手の R + B に許容差を足した時間を超えたら、時間切れの勝ちを主張できる
        const quint64 remaining = m_setup.iAmBlack ? lastState().whiteMainMs : lastState().blackMainMs;
        const qint64 observed = t - m_observeStart - (reconnectTotalMs() - m_reconnectAtObserveStart);
        if (observed > static_cast<qint64>(remaining + byoyomi + policy.toleranceMs)) {
            m_claimedTimeoutPly = lastState().ply;
            proposeResult(GameRules::winnerOutcome(m_setup.iAmBlack ? Turn::Black : Turn::White), Cause::Timeout);
        }
    }
}

} // namespace P2p
