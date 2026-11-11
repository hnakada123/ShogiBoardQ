/// @file p2pgamecoordinator.cpp
/// @brief P2P対局の司令塔の実装

#include "p2pgamecoordinator.h"

#include <QDateTime>
#include <QTimer>

#include "gameboard.h"
#include "logcategories.h"
#include "matchrecordsink.h"
#include "p2pknownpeers.h"
#include "p2precordkinds.h"
#include "p2precordstore.h"
#include "p2ptransport.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "gameinfokeys.h"

namespace {

using P2p::Cause;
using P2p::Outcome;

/// 仮予約の期限（条件の交換と双方の利用者による確認を終えるまで。実装仕様 3.3節）
constexpr qint64 kReservationMs = 60'000;
/// 再接続の間隔
constexpr qint64 kReconnectIntervalMs = 5'000;
constexpr int kTickMs = 250;

Turn turnOf(bool black) { return black ? Turn::Black : Turn::White; }

/// 終局の結果を棋譜の終局の行にする（行は終局した局面の手番側に書く）
RecordEntry endingEntry(const P2p::FinalResult& result)
{
    const Turn toMove = P2p::GameRules::sideToMove(result.ply);
    const Turn winner = result.outcome == Outcome::BlackWins ? Turn::Black : Turn::White;
    const Turn loser = winner == Turn::Black ? Turn::White : Turn::Black;
    switch (result.cause) {
    case Cause::Resign: return RecordEntry::ending(TerminalType::Resign, loser);
    case Cause::Checkmate: return RecordEntry::ending(TerminalType::Checkmate, toMove);
    case Cause::Timeout: return RecordEntry::ending(TerminalType::Timeout, loser);
    case Cause::Repetition: return RecordEntry::ending(TerminalType::Repetition, toMove);
    case Cause::PerpetualCheck: {
        // 連続王手の千日手は王手をかけていた側の反則負け
        const RecordEntry::Ending e = RecordEntry::forSideToMove(TerminalType::IllegalLoss, loser, toMove);
        return RecordEntry::ending(e.terminal, e.side);
    }
    case Cause::AgreedDraw: return RecordEntry::ending(TerminalType::Draw, toMove);
    case Cause::MaxPly: return RecordEntry::ending(TerminalType::MaxMoves, toMove);
    case Cause::AgreedAbort: return RecordEntry::ending(TerminalType::Interrupt, toMove);
    }
    return RecordEntry::ending(TerminalType::Interrupt, toMove);
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
    resetMatch();
    P2p::NegotiationConfig config;
    config.role = request.role;
    config.networkId = m_transport->networkId();
    config.myPeerId = m_transport->localPeerId();
    config.opponentPeerId = request.opponentPeerIdBytes;
    config.myDisplayName = request.myDisplayName;
    config.mySide = request.mySide;
    config.offerId = request.offerId.isEmpty() ? P2p::zeroHash() : request.offerId;
    config.hostTerms = request.hostTerms;
    config.expectedTerms = request.expectedTerms;
    config.expectedDisplayName = request.expectedDisplayName;
    config.expectedHostSide = request.expectedHostSide;
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
    m_opponentDisplayName.clear();
    m_myDisplayName = request.myDisplayName;
    m_reservationDeadline = now() + kReservationMs;
    m_phase = Phase::Negotiating;
    m_ticker->start();
    m_negotiation->begin();
    flushNegotiation();
    return true;
}

void P2pGameCoordinator::flushNegotiation()
{
    if (!m_negotiation) return;
    sendFrames(m_negotiation->takeOutgoing());
    if (m_negotiation->opponent()) m_opponentDisplayName = m_negotiation->opponent()->displayName;
    if (m_negotiation->applicantSigned() && m_role == P2p::kRoleHost) emit applicantVerified(m_opponentPeerId);
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
    const QByteArray verified = m_negotiation && m_negotiation->opponentVerified() ? m_negotiation->opponent()->playerKey
                                                                                  : QByteArray();
    // 開始に至らなかった相手からの再申込みを拒否する（Noise で確かめた PeerId。開始署名で確かめた ID だけを添える）
    emit negotiationAbandoned(m_opponentPeerId, verified);
    if (m_transport && m_connection != 0) m_transport->closeConnection(m_connection);
    m_connection = 0;
    m_phase = Phase::Idle;
    m_ticker->stop();
    emit negotiationEnded(false, code, byPeer);
}

// ---------------------------------------------------------------- 対局の開始

void P2pGameCoordinator::startMatch()
{
    P2p::Negotiation& n = *m_negotiation;
    P2p::MatchSetup setup;
    setup.agreement = *n.agreement();
    setup.terms = n.terms();
    setup.iAmBlack = n.iAmBlack();
    setup.blackStartSignature = n.blackStartSignature();
    setup.whiteStartSignature = n.whiteStartSignature();
    setup.myConnectionRandom = n.myConnectionRandom();
    setup.opponentConnectionRandom = n.opponentConnectionRandom();
    setup.nextRequestId = n.nextRequestId();
    const QByteArray gameId = setup.agreement.gameId();

    // 開始合意と両方の開始署名をジャーナルに書いてから始める
    m_journal = m_store ? m_store->createJournal(gameId) : nullptr;
    const auto persist = [this](const P2p::CborValue& record) { return m_journal && m_journal->append(record); };
    const quint8 mySide = setup.iAmBlack ? P2p::kSideBlack : P2p::kSideWhite;
    const bool journaled = persist(P2p::RecordKinds::start(setup.agreement, setup.terms, mySide, m_opponentDisplayName))
        && persist(P2p::RecordKinds::startSignature(P2p::kSideBlack, setup.blackStartSignature))
        && persist(P2p::RecordKinds::startSignature(P2p::kSideWhite, setup.whiteStartSignature));

    m_match = std::make_unique<P2p::Match>(setup, n.takeSessionKey(), m_playerKey, [this]() { return now(); }, persist);
    m_negotiation.reset();
    m_phase = Phase::Playing;
    m_recordFinished = false;
    m_previousTo = BoardSquare{};
    if (m_transport) m_transport->setGamePeer(m_opponentPeerId);

    // 相手の開始署名を確かめたので、プレイヤーID を既知の相手として数える
    if (m_knownPeers) {
        P2pKnownPeer& peer = m_knownPeers->upsert(setup.agreement.playerKey(!setup.iAmBlack));
        peer.lastDisplayName = m_opponentDisplayName;
        ++peer.games;
        m_knownPeers->save();
    }

    // 棋譜を作ってから P2P対局中にする（対局者名は双方が名乗った表示名。手元で付けた名前は入れない）
    if (m_deps.gameBoard) m_deps.gameBoard->newGame(QString::fromLatin1(P2p::Rules::initialSfen()));
    if (m_sink) {
        MatchRecordStart start;
        start.startSfen = QString::fromLatin1(P2p::Rules::initialSfen());
        start.players.seat(turnOf(setup.iAmBlack)).humanName = m_myDisplayName;
        start.players.seat(turnOf(!setup.iAmBlack)).humanName = m_opponentDisplayName;
        TimeControl tc;
        tc.black.mainMs = static_cast<qint64>(setup.terms.mainMs);
        tc.black.byoyomiMs = static_cast<qint64>(setup.terms.byoyomiMs);
        tc.white = tc.black;
        start.timeControl = tc;
        start.extraInfo.append({GameInfoKeys::kP2pGameId, QString::fromLatin1(gameId.toHex())});
        m_sink->beginRecord(start);
    }
    if (m_deps.clock) {
        TimeControl tc;
        tc.black.mainMs = static_cast<qint64>(setup.terms.mainMs);
        tc.black.byoyomiMs = static_cast<qint64>(setup.terms.byoyomiMs);
        tc.white = tc.black;
        m_deps.clock->setTimeControl(tc);
        // 時間切れは対局の状態機械が判定する（表示の時計では終局させない）
        m_deps.clock->setLoseOnTimeout(false);
        m_deps.clock->setCurrentPlayer(Turn::Black);
    }
    if (m_deps.activity) m_lease = m_deps.activity->begin(AppActivityState::Activity::DuringP2pGame, this);
    emit negotiationEnded(true, std::nullopt, false);
    emit gameStarted();

    if (!journaled) {
        m_match->abort(P2p::ErrorCode::StorageError);
        flushMatch();
        return;
    }
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
        // 開始前に切れたら交渉をやめる
        m_negotiation->cancel();
        m_negotiation->takeOutgoing();
        endNegotiation(false);
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
        // 仮予約の60秒を過ぎたら取りやめる
        if (now() > m_reservationDeadline) {
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
        return;
    case P2p::MatchEvent::Type::MoveCommitted:
        applyCommittedMove(event);
        return;
    case P2p::MatchEvent::Type::OfferReceived:
        if (event.result) emit offerReceived(event.result->cause);
        return;
    case P2p::MatchEvent::Type::ProposalDeclined:
        emit proposalDeclined();
        return;
    case P2p::MatchEvent::Type::ResultAgreed:
        // 棋譜を確定してから P2P対局中を手放す（相手の署名の待ちは棋譜の閲覧を止めない）
        if (event.result) {
            writeEndingRecord(&*event.result);
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
        releaseGame();
        saveEvidence();
        if (m_knownPeers && m_match->ending() != P2p::MatchEnding::MutuallySigned) {
            P2pKnownPeer& peer = m_knownPeers->upsert(opponentPlayerKey());
            ++peer.unconfirmed;
            m_knownPeers->save();
        }
        if (m_transport) {
            m_transport->setGamePeer(QString());
            if (m_connection != 0) m_transport->closeConnection(m_connection);
        }
        m_connection = 0;
        m_phase = Phase::Finished;
        m_ticker->stop();
        emit gameFinished(m_match->ending(), m_match->endingError(), m_lastEvidencePath);
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
    entry->time = RecordTime::fromMilliseconds(static_cast<qint64>(event.spentMs), total);
    if (m_sink) m_sink->appendRecord(*entry);
    syncClock();
    // 盤・棋譜・時計を更新し終えた後に知らせる（駒音）
    m_deps.gameBoard->notifyMoveCommitted(mover, static_cast<int>(state.ply));
    emit statusChanged();
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
        const quint32 ply = m_match ? m_match->lastState().ply : 0;
        if (result) {
            m_sink->appendRecord(endingEntry(*result));
        } else {
            // 結果が一致しないまま終わった: 中断（結果未確定）
            m_sink->appendRecord(RecordEntry::ending(TerminalType::Interrupt, P2p::GameRules::sideToMove(ply)));
        }
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
    info.localEnding = m_match->endingName();
    info.endedAt = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
    // 相手が同じ手数の異なる状態に署名した証明があれば、ジャーナルを残す
    bool conflict = false;
    if (m_journal) {
        const std::vector<P2p::CborValue> records = P2pJournal::readAll(m_journal->path());
        P2pRecordStore::evidenceFromJournal(records, nullptr, &conflict);
        m_journal->close();
    }
    m_lastEvidencePath = m_store->saveEvidence(m_match->evidence(), info, conflict);
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
    flushMatch();
    emit statusChanged();
    // 盤は相手の承認を受けてから動かす（確認待ちの手を棋譜へ先に入れない）
    return error == P2p::Match::MoveError::None ? GameChannel::MoveResult::Sent : GameChannel::MoveResult::Rejected;
}

bool P2pGameCoordinator::canResign() const
{
    return isHumanToMove();
}

void P2pGameCoordinator::resign()
{
    if (!m_match || !m_match->resign()) return;
    flushMatch();
}

void P2pGameCoordinator::offerDraw()
{
    if (!m_match || !m_match->offer(Cause::AgreedDraw)) return;
    flushMatch();
}

void P2pGameCoordinator::offerAbort()
{
    if (!m_match || !m_match->offer(Cause::AgreedAbort)) return;
    flushMatch();
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

bool P2pGameCoordinator::isHumanToMove() const
{
    if (!isInProgress()) return false;
    const P2p::Match::Phase phase = m_match->phase();
    return (phase == P2p::Match::Phase::Playing || phase == P2p::Match::Phase::Reconnecting) && m_match->isMyTurn()
        && !m_match->hasPendingMove();
}

Turn P2pGameCoordinator::sideToMove() const
{
    if (!m_match || m_match->confirmedStates().empty()) return Turn::Black;
    return P2p::GameRules::sideToMove(m_match->lastState().ply);
}
