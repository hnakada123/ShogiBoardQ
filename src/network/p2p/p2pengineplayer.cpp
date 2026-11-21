/// @file p2pengineplayer.cpp
/// @brief P2P対局で自分の手を指すエンジンの実装

#include "p2pengineplayer.h"

#include "matchending.h"
#include "matchengineseat.h"
#include "p2pgamerules.h"
#include "p2pmatch.h"
#include "timecontrol.h"
#include "usi.h"
#include "usigotime.h"
#include "usiposition.h"

namespace {

/// エンジンの異常を、持ち時間の間に指し手が返らなかったものとして扱う残り時間
constexpr qint64 kTimeoutSlackMs = 1000;

} // namespace

P2pEnginePlayer::P2pEnginePlayer(EngineSlots* engines, QObject* parent)
    : QObject(parent)
    , m_seat(new MatchEngineSeat(engines, this))
{
    connect(m_seat, &MatchEngineSeat::initialized, this, &P2pEnginePlayer::initialized);
    connect(m_seat, &MatchEngineSeat::errorOccurred, this, &P2pEnginePlayer::errorOccurred);
    connect(m_seat, &MatchEngineSeat::moveReady, this, &P2pEnginePlayer::moveReady);
    connect(m_seat, &MatchEngineSeat::resigned, this, &P2pEnginePlayer::onResigned);
    connect(m_seat, &MatchEngineSeat::declaredWin, this, &P2pEnginePlayer::onDeclaredWin);
}

P2pEnginePlayer::~P2pEnginePlayer() = default;

void P2pEnginePlayer::start(const QString& enginePath, const QString& engineName)
{
    m_match = nullptr;
    m_requestedPly = -1;
    MatchEngineSeat::Params params;
    params.slot = 1;
    params.enginePath = enginePath;
    params.engineName = engineName;
    // 先後はまだ決まっていない（手番は送る局面で決まる）
    params.role = QStringLiteral("P2P");
    params.timeSource = this;
    m_seat->start(params);
}

bool P2pEnginePlayer::isReady() const
{
    return m_seat->isReady();
}

void P2pEnginePlayer::beginMatch(const P2p::Match* match)
{
    m_match = match;
    m_requestedPly = -1;
}

void P2pEnginePlayer::requestMove()
{
    if (!m_match || !m_seat->isReady()) return;
    const std::vector<P2p::SignedState>& states = m_match->confirmedStates();
    if (states.empty()) return;
    const qint64 ply = static_cast<qint64>(states.back().state.ply);
    if (ply == m_requestedPly) return;
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
    m_requestedPly = ply;
    m_seat->engine()->requestMatchMove(*position);
}

void P2pEnginePlayer::moveNow()
{
    if (Usi* engine = m_seat->engine()) engine->requestImmediateMove();
}

void P2pEnginePlayer::cancelThinking()
{
    if (Usi* engine = m_seat->engine()) engine->cancelCurrentOperation();
}

bool P2pEnginePlayer::hasRunOutOfTime() const
{
    if (!m_match || !m_match->isMyTurn() || m_match->hasPendingMove()) return false;
    const P2p::GameState& state = m_match->lastState();
    const qint64 budget = static_cast<qint64>(m_match->iAmBlack() ? state.blackMainMs : state.whiteMainMs)
        + static_cast<qint64>(m_match->terms().byoyomiMs);
    return m_match->myTurnSpentMs() + kTimeoutSlackMs >= budget;
}

void P2pEnginePlayer::finish(const P2p::FinalResult* result)
{
    if (m_seat->engine()) {
        // ローカル対局・通信対局と同じ手順と同じ対応（MatchEnding）。結果の決まらない終局は draw
        const MatchGameEndInfo info =
            result ? P2p::GameRules::endInfo(*result) : MatchGameEndInfo{MatchCause::BreakOff, Turn::Black};
        m_seat->finish(MatchEnding::engineResult(info, mySide()));
    }
    release();
}

void P2pEnginePlayer::release()
{
    m_seat->release();
    m_match = nullptr;
    m_requestedPly = -1;
}

MatchSearchTiming P2pEnginePlayer::snapshot(Turn side, bool pondering) const
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
    UsiGoTimeInput input = UsiGoTimeCalculator::fromClock(P2p::GameRules::timeControl(terms), side, pondering,
                                                          clockSide(Turn::Black), clockSide(Turn::White));
    // 基本の持ち時間を使い切った後は、秒読みの残りだけを使える
    const qint64 byoyomi = static_cast<qint64>(terms.byoyomiMs);
    if (!pondering && side == mine && byoyomi > 0) {
        input.moverByoyomiMs = qMax<qint64>(0, byoyomi - qMax<qint64>(0, unsettled - remainingOf(mine)));
    }
    return MatchSearchTiming::fromInput(input);
}

void P2pEnginePlayer::onResigned(Turn /*side*/)
{
    emit resigned();
}

void P2pEnginePlayer::onDeclaredWin(Turn /*side*/)
{
    emit declaredWin();
}

Turn P2pEnginePlayer::mySide() const
{
    return m_match && !m_match->iAmBlack() ? Turn::White : Turn::Black;
}
