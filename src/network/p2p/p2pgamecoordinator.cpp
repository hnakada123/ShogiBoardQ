/// @file p2pgamecoordinator.cpp
/// @brief P2P対局の司令塔の実装

#include "p2pgamecoordinator.h"

#include <QDateTime>
#include <QTimer>

#include "engineslots.h"
#include "gameboard.h"
#include "gameinfokeys.h"
#include "logcategories.h"
#include "matchending.h"
#include "matchengineseat.h"
#include "matchrecordsink.h"
#include "p2pknownpeers.h"
#include "p2precordstore.h"
#include "p2ptransport.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "usi.h"
#include "usiposition.h"

namespace {

using P2p::Cause;

/// 条件の確認と開始署名の期限（条件の交換と双方の利用者による確認を終えるまで。実装仕様 3.3節）
constexpr qint64 kConfirmationMs = 60'000;
/// 再接続の間隔
constexpr qint64 kReconnectIntervalMs = 5'000;
constexpr int kTickMs = 250;
/// エンジンの異常を、持ち時間の間に指し手が返らなかったものとして扱う残り時間（対局の状態機械が時間切れにする）
constexpr qint64 kEngineTimeoutSlackMs = 1000;

Turn turnOf(bool black) { return black ? Turn::Black : Turn::White; }

/// 合意した持ち時間を、棋譜と時計の持ち時間にする（先手・後手で同じ。v1 は加算なし）
TimeControl timeControlOf(const P2p::TimeControlTerms& terms)
{
    TimeControl tc;
    tc.black.mainMs = static_cast<qint64>(terms.mainMs);
    tc.black.byoyomiMs = static_cast<qint64>(terms.byoyomiMs);
    tc.white = tc.black;
    return tc;
}

} // namespace

P2pGameCoordinator::P2pGameCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_ticker(new QTimer(this))
{
    m_elapsed.start();
    m_ticker->setInterval(kTickMs);
    connect(m_ticker, &QTimer::timeout, this, &P2pGameCoordinator::onTick);
}

P2pGameCoordinator::~P2pGameCoordinator() = default;

void P2pGameCoordinator::setEnvironment(P2pTransport* transport, P2pRecordStore* store, P2pKnownPeers* knownPeers,
                                        const P2p::SigningKey* playerKey)
{
    m_transport = transport;
    m_store = store;
    m_knownPeers = knownPeers;
    m_playerKey = playerKey;
}

QByteArray P2pGameCoordinator::opponentPlayerKey() const
{
    if (m_match) return m_match->agreement().playerKey(!m_match->iAmBlack());
    if (m_negotiation && m_negotiation->opponent()) return m_negotiation->opponent()->playerKey;
    return {};
}

// ---------------------------------------------------------------- 交渉

bool P2pGameCoordinator::startNegotiation(const NegotiationRequest& request)
{
    if (isBusy() || !m_transport || !m_playerKey || !m_playerKey->isValid() || request.connection == 0) return false;
    // エンジンで指すなら、エンジンの枠と、登録エンジンの場所が要る
    if (request.player.isEngine() && (!m_deps.engines || request.player.enginePath.isEmpty())) return false;
    resetMatch();
    P2p::NegotiationConfig config;
    config.role = request.role;
    config.networkId = m_transport->networkId();
    config.myPeerId = m_transport->localPeerId();
    config.opponentPeerId = request.opponentPeerIdBytes;
    config.myDisplayName = request.myDisplayName;
    config.mySide = request.mySide;
    config.myKind = request.player.kind;
    config.hostTerms = request.hostTerms;
    config.expectedPlayerKey = request.friendPlayerKey;
    config.nowSeconds = []() { return static_cast<quint64>(QDateTime::currentSecsSinceEpoch()); };
    P2pKnownPeers* known = m_knownPeers;
    // ブロックは申込みで名乗ったプレイヤーID で判定してよい（名乗った側が断られるだけ。実装仕様 3.3節）
    config.isBlocked = [known](const QByteArray& playerKey) {
        const P2pKnownPeer* peer = known ? known->find(playerKey) : nullptr;
        return peer && peer->blocked;
    };
    m_negotiation = std::make_unique<P2p::Negotiation>(config, m_playerKey);
    m_connection = request.connection;
    m_opponentPeerId = request.opponentPeerId;
    m_role = request.role;
    m_viaRelay = request.viaRelay;
    m_friendNegotiation = !request.friendPlayerKey.isEmpty();
    m_opponentDisplayName.clear();
    m_myDisplayName = request.myDisplayName;
    m_player = request.player;
    m_confirmationDeadline = now() + kConfirmationMs;
    m_phase = Phase::Negotiating;
    m_ticker->start();
    // エンジンは、利用者が条件を確かめている間に起動しておく（readyok まで開始署名を送らない。起動の失敗は戻った後に届く）
    if (m_player.isEngine()) startEngine();
    m_negotiation->begin();
    flushNegotiation();
    return true;
}

void P2pGameCoordinator::flushNegotiation()
{
    if (!m_negotiation) return;
    sendFrames(m_negotiation->takeOutgoing());
    if (m_negotiation->opponent()) m_opponentDisplayName = m_negotiation->opponent()->displayName;
    switch (m_negotiation->stage()) {
    case P2p::Negotiation::Stage::Started:
        endNegotiation(true);
        return;
    case P2p::Negotiation::Stage::Failed:
        endNegotiation(false);
        return;
    default:
        emit negotiationChanged();
        return;
    }
}

void P2pGameCoordinator::sendFrames(const std::vector<P2p::Frame>& frames)
{
    if (!m_transport || m_connection == 0) return;
    for (const P2p::Frame& frame : frames) m_transport->sendFrame(m_connection, frame.encode());
}

void P2pGameCoordinator::confirmStart()
{
    if (m_phase != Phase::Negotiating || !m_negotiation) return;
    // 対局が始まるとすぐ時計が動くので、エンジンが指せるようになるまで開始署名を送らない
    if (isEngineStarting()) return;
    m_negotiation->confirm();
    flushNegotiation();
}

void P2pGameCoordinator::cancelNegotiation()
{
    if (m_phase != Phase::Negotiating || !m_negotiation) return;
    m_negotiation->cancel();
    flushNegotiation();
}

void P2pGameCoordinator::endNegotiation(bool started)
{
    if (started) {
        startMatch();
        return;
    }
    const std::optional<P2p::ErrorCode> code = m_negotiation ? m_negotiation->failure() : std::nullopt;
    const bool byPeer = m_negotiation && m_negotiation->failedByPeer();
    if (m_transport && m_connection != 0) m_transport->closeConnection(m_connection);
    m_connection = 0;
    m_phase = Phase::Idle;
    m_ticker->stop();
    // 対局は始まらないので、起動したエンジンを手放す
    releaseEngine();
    emit negotiationEnded(false, code, byPeer);
}

// ---------------------------------------------------------------- 対局の開始

void P2pGameCoordinator::startMatch()
{
    P2p::Negotiation& n = *m_negotiation;
    P2p::MatchSetup setup;
    setup.agreement = *n.agreement();
    setup.rules = n.rules();
    setup.terms = n.terms();
    setup.iAmBlack = n.iAmBlack();
    setup.blackStartSignature = n.blackStartSignature();
    setup.whiteStartSignature = n.whiteStartSignature();
    setup.myConnectionRandom = n.myConnectionRandom();
    setup.opponentConnectionRandom = n.opponentConnectionRandom();
    setup.nextRequestId = n.nextRequestId();
    setup.opponentDisplayName = m_opponentDisplayName;
    const QByteArray gameId = setup.agreement.gameId();

    // ジャーナルに書けなければ、対局の状態機械が開始の件を書くところで中断する
    m_journal = m_store ? m_store->createJournal(gameId) : nullptr;
    const auto persist = [this](const P2p::CborValue& record) { return m_journal && m_journal->append(record); };
    m_match = std::make_unique<P2p::Match>(setup, n.takeSessionKey(), m_playerKey, [this]() { return now(); }, persist);
    m_negotiation.reset();
    m_phase = Phase::Playing;
    m_recordFinished = false;
    m_previousTo = BoardSquare{};
    m_engineRequestedPly = -1;
    if (m_transport) m_transport->setGamePeer(m_opponentPeerId);

    // 相手の開始署名を確かめたので、プレイヤーID を既知の相手として数える
    if (m_knownPeers) m_knownPeers->recordGameStarted(setup.agreement.playerKey(!setup.iAmBlack), m_opponentDisplayName);

    // 棋譜を作ってから P2P対局中にする（対局者名は双方が名乗った表示名。手元で付けた名前は入れない。エンジンで
    // 指していても表示名で記録する。通信対局と同じ）
    if (m_deps.gameBoard) m_deps.gameBoard->newGame(QString::fromLatin1(P2p::Rules::initialSfen()));
    if (m_sink) {
        MatchRecordStart start;
        start.startSfen = QString::fromLatin1(P2p::Rules::initialSfen());
        start.players.seat(turnOf(setup.iAmBlack)).humanName = m_myDisplayName;
        start.players.seat(turnOf(!setup.iAmBlack)).humanName = m_opponentDisplayName;
        start.timeControl = timeControlOf(setup.terms);
        start.extraInfo.append({GameInfoKeys::kP2pGameId, QString::fromLatin1(gameId.toHex())});
        m_sink->beginRecord(start);
    }
    if (m_deps.clock) {
        m_deps.clock->setTimeControl(timeControlOf(setup.terms));
        // 時間切れは対局の状態機械が判定する（表示の時計では終局させない）
        m_deps.clock->setLoseOnTimeout(false);
        m_deps.clock->setCurrentPlayer(Turn::Black);
    }
    if (m_deps.activity) m_lease = m_deps.activity->begin(AppActivityState::Activity::DuringP2pGame, this);
    emit negotiationEnded(true, std::nullopt, false);
    emit gameStarted();
    if (m_player.isEngine()) emit matchEngineStarted(turnOf(setup.iAmBlack), m_player.engineName);

    m_match->begin();
    flushMatch();
}

// ---------------------------------------------------------------- 通信の知らせ

void P2pGameCoordinator::onConnectionOpened(quint64 connection, const QString& peerId, bool viaRelay)
{
    // 対局中の相手の再接続だけを受ける（交渉の接続は startNegotiation で渡される）
    if (m_phase != Phase::Playing || !m_match || peerId != m_opponentPeerId) return;
    if (m_connection != 0 && m_connection != connection && m_transport) m_transport->closeConnection(m_connection);
    // 相手が接続し直してきた（こちらはまだ切れたと気づいていない）: 古い接続を捨てて照合からやり直す
    if (m_match->phase() == P2p::Match::Phase::Playing) m_match->connectionLost();
    m_connection = connection;
    m_viaRelay = viaRelay;
    m_match->connectionEstablished();
    flushMatch();
}

void P2pGameCoordinator::onFrame(quint64 connection, const QByteArray& bytes)
{
    if (!ownsConnection(connection)) return;
    // 通信プロセスが確かめた外枠も、ここで決定的CBORからすべて確かめ直す
    QString error;
    const std::optional<P2p::Frame> frame = P2p::Frame::decode(bytes, &error);
    if (m_phase == Phase::Negotiating && m_negotiation) {
        if (!frame) {
            m_negotiation->cancel();
        } else {
            m_negotiation->receive(*frame);
        }
        flushNegotiation();
        return;
    }
    if (m_phase == Phase::Playing && m_match) {
        if (!frame) {
            qCWarning(lcGame) << "P2P: malformed frame:" << error;
            m_match->abort(P2p::ErrorCode::ConditionMismatch);
        } else {
            m_match->receive(*frame);
        }
        flushMatch();
    }
}

void P2pGameCoordinator::onConnectionClosed(quint64 connection)
{
    if (!ownsConnection(connection)) return;
    m_connection = 0;
    if (m_phase == Phase::Negotiating && m_negotiation) {
        // 招待で接続した側が、相手の応答を受ける前に閉じられたら、招待が受け付けられなかった
        const bool refused = m_role == P2p::kRoleApplicant
                             && m_negotiation->stage() == P2p::Negotiation::Stage::AwaitingHello;
        // 開始前に切れたら交渉をやめる
        m_negotiation->cancel();
        m_negotiation->takeOutgoing();
        endNegotiation(false);
        if (refused) emit inviteRefused();
        return;
    }
    if (m_phase == Phase::Playing && m_match) {
        m_match->connectionLost();
        m_nextReconnectAt = now();
        flushMatch();
    }
}

void P2pGameCoordinator::onRouteChanged(quint64 connection, bool viaRelay)
{
    if (!ownsConnection(connection)) return;
    m_viaRelay = viaRelay;
    emit statusChanged();
}

void P2pGameCoordinator::onTick()
{
    if (m_phase == Phase::Negotiating && m_negotiation) {
        // 確認の期限（60秒）を過ぎたら取りやめる
        if (now() > m_confirmationDeadline) {
            m_negotiation->cancel();
            flushNegotiation();
        }
        return;
    }
    if (m_phase != Phase::Playing || !m_match) {
        m_ticker->stop();
        return;
    }
    // スリープ・時計の飛びで計時の連続性を確かめられなくなったら、対局を中断する（結果未確定）
    if (clockDiscontinuity() && m_match->phase() != P2p::Match::Phase::Finished) {
        qCWarning(lcGame) << "P2P: clock discontinuity (sleep?), aborting the game";
        m_match->abort(P2p::ErrorCode::ClockDiscontinuity);
        flushMatch();
        return;
    }
    m_match->tick();
    // 回線が切れている間は、申込側（最初に接続した側）が接続し直す
    if (m_match->phase() == P2p::Match::Phase::Reconnecting && m_connection == 0 && m_role == P2p::kRoleApplicant
        && now() >= m_nextReconnectAt && m_transport) {
        m_nextReconnectAt = now() + kReconnectIntervalMs;
        m_transport->reconnectGamePeer(m_opponentPeerId);
    }
    flushMatch();
    emit statusChanged();
}

// ---------------------------------------------------------------- 対局の知らせ

void P2pGameCoordinator::flushMatch()
{
    if (!m_match) return;
    sendFrames(m_match->takeOutgoing());
    for (const P2p::MatchEvent& event : m_match->takeEvents()) handleMatchEvent(event);
}

void P2pGameCoordinator::handleMatchEvent(const P2p::MatchEvent& event)
{
    switch (event.type) {
    case P2p::MatchEvent::Type::Ready:
        syncClock();
        emit statusChanged();
        // 先手のエンジンは、初手の計時が始まったここから考える
        requestEngineMove();
        return;
    case P2p::MatchEvent::Type::MoveCommitted:
        applyCommittedMove(event);
        return;
    case P2p::MatchEvent::Type::OfferReceived:
        if (event.result) emit offerReceived(event.result->cause);
        return;
    case P2p::MatchEvent::Type::ProposalDeclined:
        // 答えを待っている申し出がなくなった（P2P対局のウィンドウの状態の行と、待っている知らせを先に消す）
        emit statusChanged();
        emit proposalDeclined();
        return;
    case P2p::MatchEvent::Type::ResultAgreed:
        // 棋譜を確定してから P2P対局中を手放す（相手の署名の待ちは棋譜の閲覧を止めない）
        if (event.result) {
            writeEndingRecord(&*event.result);
            finishEngine(&*event.result);
            releaseGame();
            emit resultAgreed(*event.result);
        }
        return;
    case P2p::MatchEvent::Type::Reconnecting:
        if (m_transport && m_connection != 0) m_transport->closeConnection(m_connection);
        m_connection = 0;
        m_nextReconnectAt = now();
        if (m_deps.clock) m_deps.clock->updateClock();
        emit statusChanged();
        return;
    case P2p::MatchEvent::Type::Resumed:
        emit statusChanged();
        return;
    case P2p::MatchEvent::Type::Finished: {
        writeEndingRecord(nullptr);
        // 結果が一致して終わったなら、エンジンにはもう知らせてある
        finishEngine(nullptr);
        releaseGame();
        saveEvidence();
        if (m_knownPeers && m_match->settlement() != P2p::Settlement::MutuallySigned) {
            m_knownPeers->recordUnconfirmed(opponentPlayerKey());
        }
        if (m_transport) {
            m_transport->setGamePeer(QString());
            if (m_connection != 0) m_transport->closeConnection(m_connection);
        }
        m_connection = 0;
        m_phase = Phase::Finished;
        m_ticker->stop();
        emit gameFinished(m_match->settlement(), m_match->endingError(), m_lastEvidencePath);
        return;
    }
    }
}

void P2pGameCoordinator::applyCommittedMove(const P2p::MatchEvent& event)
{
    if (!m_deps.gameBoard || m_recordFinished) return;
    const std::optional<UsiMove> move = UsiMove::parse(QString::fromLatin1(event.moveUsi));
    if (!move) return;
    // 両方の署名がそろった手だけを、合法手を確かめて盤に指す（サーバー裁定前提の applyRecordedMove は使わない）
    std::optional<RecordEntry> entry = m_deps.gameBoard->applyMove(*move, m_previousTo);
    if (!entry) {
        // 状態機械が確かめた手が盤に指せないのは実装の不一致
        m_match->abort(P2p::ErrorCode::InvalidMove);
        flushMatch();
        return;
    }
    m_previousTo = move->to;
    const P2p::GameState& state = m_match->lastState();
    const Turn mover = P2p::GameRules::sideToMove(state.ply - 1);
    qint64 total = 0;
    for (const P2p::SignedState& s : m_match->confirmedStates()) {
        if (s.state.ply > 0 && P2p::GameRules::sideToMove(s.state.ply - 1) == mover) {
            total += static_cast<qint64>(s.state.spentMs);
        }
    }
    // 署名した消費時間はミリ秒だが（証跡にそのまま残る）、棋譜にはローカル対局と同じく秒の単位で書く
    entry->time = RecordTime::wholeSeconds(total - static_cast<qint64>(event.spentMs), total);
    if (m_sink) m_sink->appendRecord(*entry);
    syncClock();
    // 盤・棋譜・時計を更新し終えた後に知らせる（駒音）。人間が盤で指した自分の手は、送ったとき（playMove）に
    // 知らせている。エンジンが指した自分の手は、盤に出るここで知らせる（通信対局と同じ）
    const bool mine = (mover == Turn::Black) == m_match->iAmBlack();
    if (!mine || m_player.isEngine()) m_deps.gameBoard->notifyPiecePlaced(mover, static_cast<int>(state.ply));
    emit statusChanged();
    // 相手の手を盤に指したら、自分のエンジンに次の手を頼む
    requestEngineMove();
}

void P2pGameCoordinator::syncClock()
{
    if (!m_deps.clock || !m_match || m_recordFinished) return;
    const P2p::GameState& state = m_match->lastState();
    m_deps.clock->setRemainingTimes(static_cast<qint64>(state.blackMainMs), static_cast<qint64>(state.whiteMainMs));
    m_deps.clock->setCurrentPlayer(P2p::GameRules::sideToMove(state.ply));
    m_deps.clock->startClock();
}

void P2pGameCoordinator::writeEndingRecord(const P2p::FinalResult* result)
{
    if (m_recordFinished) return;
    m_recordFinished = true;
    if (m_deps.clock) m_deps.clock->stopClock();
    if (m_sink) {
        const quint32 ply = result ? result->ply : (m_match ? m_match->lastState().ply : 0);
        // 結果が一致しないまま終わったら中断（結果未確定）。行は終局した局面の手番側にそろえる
        const MatchGameEndInfo info = result ? P2p::GameRules::endInfo(*result) : MatchGameEndInfo{MatchCause::BreakOff, Turn::Black};
        const RecordEntry::Ending ending = MatchEnding::endingOf(info, P2p::GameRules::sideToMove(ply));
        m_sink->appendRecord(RecordEntry::ending(ending.terminal, ending.side));
        m_sink->finishRecord(/*autoSave=*/false);
    }
}

void P2pGameCoordinator::releaseGame()
{
    m_lease.reset();
}

void P2pGameCoordinator::saveEvidence()
{
    if (!m_match || !m_store) return;
    P2pHistoryInfo info;
    info.mySide = m_match->iAmBlack() ? P2p::kSideBlack : P2p::kSideWhite;
    info.opponentDisplayName = m_opponentDisplayName;
    info.settlement = m_match->settlement();
    info.endedAt = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
    // 相手が同じ手数の異なる状態に署名した証明があれば、ジャーナルを残す
    if (m_journal) m_journal->close();
    m_lastEvidencePath = m_store->saveEvidence(m_match->evidence(), info, m_match->hasConflictProof());
    m_journal.reset();
}

bool P2pGameCoordinator::clockDiscontinuity()
{
    // 単調増加時計はスリープの間に進まない（Linux の CLOCK_MONOTONIC）。OS の時刻だけが大きく進んだらスリープ
    constexpr qint64 kAllowedDriftMs = 15'000;
    const qint64 mono = now();
    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
    const bool jumped = m_lastTickMono >= 0
        && qAbs((wall - m_lastTickWallMs) - (mono - m_lastTickMono)) > kAllowedDriftMs;
    m_lastTickMono = mono;
    m_lastTickWallMs = wall;
    return jumped;
}

void P2pGameCoordinator::resetMatch()
{
    releaseEngine();
    m_engineRequestedPly = -1;
    m_lastTickMono = -1;
    m_match.reset();
    m_negotiation.reset();
    m_journal.reset();
    m_lease.reset();
    m_connection = 0;
    m_phase = Phase::Idle;
}

// ---------------------------------------------------------------- 対局の操作

GameChannel::MoveResult P2pGameCoordinator::playMove(const UsiMove& move)
{
    if (!isHumanToMove()) return GameChannel::MoveResult::Rejected;
    const P2p::Match::MoveError error = m_match->playMove(move);
    if (error == P2p::Match::MoveError::None) {
        onMoveSent();
        // 駒を置いた時点に合わせて、駒音もここで鳴らす（盤の表示は盤の操作が指した後にする）
        const P2p::GameState* sent = m_match->pendingState();
        if (sent && m_deps.gameBoard && !m_recordFinished) {
            m_deps.gameBoard->notifyPiecePlaced(P2p::GameRules::sideToMove(sent->ply - 1), static_cast<int>(sent->ply));
        }
    }
    flushMatch();
    emit statusChanged();
    // 盤は相手の承認を受けてから動かす（確認待ちの手を棋譜へ先に入れない）
    return error == P2p::Match::MoveError::None ? GameChannel::MoveResult::Sent : GameChannel::MoveResult::Rejected;
}

void P2pGameCoordinator::onMoveSent()
{
    // 駒を置いた時点に合わせる。消費時間は送る状態を作った時点で固定しているので、自分の時計をその残り時間で
    // 止める（相手の時計は承認を受けてから進める。syncClock）
    const P2p::GameState* sent = m_match->pendingState();
    if (!sent || m_recordFinished) return;
    if (m_deps.clock) {
        m_deps.clock->stopClock();
        m_deps.clock->setRemainingTimes(static_cast<qint64>(sent->blackMainMs), static_cast<qint64>(sent->whiteMainMs));
    }
}

bool P2pGameCoordinator::canResign() const
{
    // エンジンに指させている対局でも、自分の手番なら人が投了できる（通信対局と同じ）
    return isMyMoveExpected();
}

void P2pGameCoordinator::resign()
{
    if (!m_match || !m_match->resign()) return;
    // エンジンに指させている対局で人が投了したら、考えているエンジンの指し手はもう受けない（投了を送った後に指さない）
    if (Usi* engine = m_engineSeat ? m_engineSeat->engine() : nullptr) engine->cancelCurrentOperation();
    flushMatch();
}

void P2pGameCoordinator::offerDraw()
{
    offer(Cause::AgreedDraw);
}

void P2pGameCoordinator::offerAbort()
{
    offer(Cause::AgreedAbort);
}

void P2pGameCoordinator::offer(P2p::Cause cause)
{
    if (!m_match) return;
    const P2p::Match::OfferResult result = m_match->offer(cause);
    if (result == P2p::Match::OfferResult::Sent) flushMatch();
    // 申し出た側にも、申し出たことを知らせる（相手には尋ねる箱が出るが、申し出た側には何も出ず、押せたのかが分からな
    // かった）。答えを待っている間に押し直したときは送り直さないが、相手が尋ねられている申し出を知らせ直す
    const std::optional<Cause> waiting = m_match->offerAwaitingAnswer();
    if (result != P2p::Match::OfferResult::Unavailable && waiting) {
        emit statusChanged();
        emit offerPending(*waiting);
        return;
    }
    // 送れなかったことも知らせる。相手の同じ申し出と行き違って結果が一致したときは、対局はもう進行中でない
    // （終局の知らせが出る）
    if (isInProgress()) emit offerNotSent();
}

void P2pGameCoordinator::answerOffer(bool accept)
{
    if (!m_match) return;
    m_match->answerOffer(accept);
    flushMatch();
}

void P2pGameCoordinator::leaveGame()
{
    if (m_phase == Phase::Negotiating) {
        cancelNegotiation();
        return;
    }
    if (!m_match || m_match->phase() == P2p::Match::Phase::Finished) return;
    m_match->abort(P2p::ErrorCode::Cancelled);
    flushMatch();
}

bool P2pGameCoordinator::isInProgress() const
{
    return m_phase == Phase::Playing && m_match && !m_recordFinished;
}

bool P2pGameCoordinator::isMyMoveExpected() const
{
    if (!isInProgress()) return false;
    const P2p::Match::Phase phase = m_match->phase();
    return (phase == P2p::Match::Phase::Playing || phase == P2p::Match::Phase::Reconnecting) && m_match->isMyTurn()
        && !m_match->hasPendingMove();
}

bool P2pGameCoordinator::isHumanToMove() const
{
    // エンジンで指す対局では、盤の手を受け付けない（Rules に書いた種別のとおりに指す）
    return !m_player.isEngine() && isMyMoveExpected();
}

Turn P2pGameCoordinator::mySide() const
{
    return turnOf(m_match && m_match->iAmBlack());
}

std::optional<P2p::PlayerKind> P2pGameCoordinator::opponentKind() const
{
    if (m_match) return m_match->rules().kind(!m_match->iAmBlack());
    if (m_negotiation) return m_negotiation->opponentKind();
    return std::nullopt;
}

// ---------------------------------------------------------------- エンジン

bool P2pGameCoordinator::isEngineStarting() const
{
    return m_phase == Phase::Negotiating && m_player.isEngine() && !(m_engineSeat && m_engineSeat->isReady());
}

void P2pGameCoordinator::startEngine()
{
    if (!m_engineSeat) {
        m_engineSeat = new MatchEngineSeat(m_deps.engines, this);
        connect(m_engineSeat, &MatchEngineSeat::initialized, this, &P2pGameCoordinator::onEngineInitialized);
        connect(m_engineSeat, &MatchEngineSeat::errorOccurred, this, &P2pGameCoordinator::onEngineError);
        connect(m_engineSeat, &MatchEngineSeat::moveReady, this, &P2pGameCoordinator::onEngineMoveReady);
        connect(m_engineSeat, &MatchEngineSeat::resigned, this, &P2pGameCoordinator::onEngineResigned);
        connect(m_engineSeat, &MatchEngineSeat::declaredWin, this, &P2pGameCoordinator::onEngineDeclaredWin);
    }
    emit engineStarting();
    MatchEngineSeat::Params params;
    params.slot = 1;
    params.enginePath = m_player.enginePath;
    params.engineName = m_player.engineName;
    // 先後はまだ決まっていない（手番は送る局面で決まる）
    params.role = QStringLiteral("P2P");
    // 持ち時間はこの司令塔（MatchTimeSource）が、双方が署名した残り時間から返す
    params.timeSource = this;
    m_engineSeat->start(params);
}

void P2pGameCoordinator::releaseEngine()
{
    if (m_engineSeat) m_engineSeat->release();
}

void P2pGameCoordinator::finishEngine(const P2p::FinalResult* result)
{
    if (!m_engineSeat || !m_engineSeat->engine()) return;
    // ローカル対局・通信対局と同じ手順と同じ対応（MatchEnding）。結果の決まらない終局は draw
    const MatchGameEndInfo info = result ? P2p::GameRules::endInfo(*result) : MatchGameEndInfo{MatchCause::BreakOff, Turn::Black};
    m_engineSeat->finish(MatchEnding::engineResult(info, mySide()));
    m_engineSeat->release();
}

void P2pGameCoordinator::requestEngineMove()
{
    if (!m_player.isEngine() || !isMyMoveExpected() || !m_engineSeat || !m_engineSeat->isReady()) return;
    const std::vector<P2p::SignedState>& states = m_match->confirmedStates();
    if (states.empty()) return;
    const qint64 ply = static_cast<qint64>(states.back().state.ply);
    if (ply == m_engineRequestedPly) return;
    // エンジンに送る局面は、双方が署名した手順（開始局面からの手の列）
    QList<UsiMove> moves;
    moves.reserve(static_cast<qsizetype>(states.size()));
    for (std::size_t i = 1; i < states.size(); ++i) {
        const std::optional<UsiMove> move = UsiMove::parse(QString::fromLatin1(states[i].state.moveUsi));
        if (!move) return;
        moves.append(*move);
    }
    const std::optional<UsiPosition> position =
        UsiPosition::fromMoves(QString::fromLatin1(P2p::Rules::initialSfen()), moves);
    if (!position) return;
    m_engineRequestedPly = ply;
    m_engineSeat->engine()->requestMatchMove(*position);
}

MatchSearchTiming P2pGameCoordinator::snapshot(Turn side, bool pondering) const
{
    if (!m_match) return MatchSearchTiming::fromInput(UsiGoTimeInput{});
    const P2p::TimeControlTerms terms = m_match->terms();
    // 残り時間は、双方が署名した最新の状態のもの。自分の手を送って承認を待つ間は、その手の消費を引いた残り
    const P2p::GameState* pending = m_match->pendingState();
    const P2p::GameState& state = pending ? *pending : m_match->lastState();
    const Turn mine = mySide();
    // 自分の今の手番の経過時間は、まだ残りから引いていない（先読みを始めるのは、指し手を送る直前。その手の消費を
    // 引いた残りが、次の自分の手番の持ち時間になる）。相手の手番の経過時間は、相手の申告が届くまで分からない
    const qint64 unsettled = !pending && m_match->isMyTurn() ? m_match->myTurnSpentMs() : 0;
    const auto remainingOf = [&state](Turn player) {
        return static_cast<qint64>(player == Turn::Black ? state.blackMainMs : state.whiteMainMs);
    };
    const auto clockSide = [&](Turn player) {
        return UsiClockSide{remainingOf(player), false, player == mine ? unsettled : 0};
    };
    UsiGoTimeInput input = UsiGoTimeCalculator::fromClock(timeControlOf(terms), side, pondering, clockSide(Turn::Black),
                                                          clockSide(Turn::White));
    // 基本の持ち時間を使い切った後は、秒読みの残りだけを使える
    const qint64 byoyomi = static_cast<qint64>(terms.byoyomiMs);
    if (!pondering && side == mine && byoyomi > 0) {
        input.moverByoyomiMs = qMax<qint64>(0, byoyomi - qMax<qint64>(0, unsettled - remainingOf(mine)));
    }
    return MatchSearchTiming::fromInput(input);
}

void P2pGameCoordinator::forceImmediateMove()
{
    if (!m_player.isEngine() || !isMyMoveExpected() || !m_engineSeat) return;
    if (Usi* engine = m_engineSeat->engine()) engine->requestImmediateMove();
}

void P2pGameCoordinator::onEngineInitialized()
{
    // 条件確認画面の「開始」を押せるようにする
    if (m_phase == Phase::Negotiating) emit negotiationChanged();
}

void P2pGameCoordinator::onEngineError(const QString& message)
{
    releaseEngine();
    if (m_phase == Phase::Negotiating && m_negotiation) {
        // エンジンが指せないので、対局を始めない
        cancelNegotiation();
        emit engineNotice(tr("エンジンを使えないので、対局の開始を取りやめました。") + QLatin1Char('\n') + message);
        return;
    }
    if (m_phase != Phase::Playing || !m_match || m_recordFinished) return;
    if (isMyMoveExpected()) {
        // 持ち時間の間に指し手が返らなかったときは、対局の状態機械が時間切れとして相手に知らせる
        const P2p::GameState& state = m_match->lastState();
        const qint64 budget = static_cast<qint64>(m_match->iAmBlack() ? state.blackMainMs : state.whiteMainMs)
            + static_cast<qint64>(m_match->terms().byoyomiMs);
        if (m_match->myTurnSpentMs() + kEngineTimeoutSlackMs >= budget) {
            emit engineNotice(tr("エンジンが持ち時間の間に指し手を返しませんでした。"));
            return;
        }
    }
    // エンジンが指せなくなったら、対局を中断する（結果未確定。通信対局と同じ）
    m_match->abort(P2p::ErrorCode::Cancelled);
    flushMatch();
    emit engineNotice(tr("エンジンが止まったので、対局を中断しました（結果未確定）。") + QLatin1Char('\n') + message);
}

void P2pGameCoordinator::onEngineMoveReady(const UsiMatchMove& result)
{
    if (!m_player.isEngine() || !isMyMoveExpected()) return;
    const Turn side = mySide();
    const int ply = static_cast<int>(m_match->lastState().ply) + 1;
    const P2p::Match::MoveError error =
        result.move.isValid() ? m_match->playMove(result.move) : P2p::Match::MoveError::Illegal;
    QString notice;
    if (error == P2p::Match::MoveError::None) {
        onMoveSent();
        emit engineMovePlayed(side, ply, result.score);
    } else if (error == P2p::Match::MoveError::Illegal) {
        // 指せない手は相手に送らない（相手は不正な手として対局を止める）。反則は負けなので、投了にする
        notice = tr("エンジンが指せない手（%1）を返したので、投了しました。").arg(result.move.toString());
        m_match->resign();
    }
    // 時間切れ（TimeUp）は、対局の状態機械が相手に知らせる
    flushMatch();
    emit statusChanged();
    if (!notice.isEmpty()) emit engineNotice(notice);
}

void P2pGameCoordinator::onEngineResigned()
{
    if (!m_player.isEngine() || !isMyMoveExpected()) return;
    if (m_match->resign()) flushMatch();
}

void P2pGameCoordinator::onEngineDeclaredWin()
{
    if (!m_player.isEngine() || !isMyMoveExpected()) return;
    // P2P対局は入玉宣言を扱わない（実装仕様 1.1節。双方の合意による引き分けはできる）
    // 申し出たことは下の知らせに書くので、申し出の知らせ（offerPending）は重ねて出さない
    if (m_match->offer(Cause::AgreedDraw) == P2p::Match::OfferResult::Sent) flushMatch();
    emit statusChanged();
    emit engineNotice(tr("エンジンが入玉宣言勝ちを選びましたが、P2P対局は入玉宣言を扱いません。相手に引き分けを申し出ました。"
                         "相手が断ったときは、投了するか、対局をやめてください。"));
}

Turn P2pGameCoordinator::sideToMove() const
{
    if (!m_match || m_match->confirmedStates().empty()) return Turn::Black;
    return P2p::GameRules::sideToMove(m_match->lastState().ply);
}
