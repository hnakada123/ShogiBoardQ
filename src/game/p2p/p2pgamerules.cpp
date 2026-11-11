/// @file p2pgamerules.cpp
/// @brief P2P対局の将棋の規則の実装

#include "p2pgamerules.h"

#include <QStringList>

#include "enginemovevalidator.h"
#include "sennichitedetector.h"
#include "shogiposition.h"

namespace P2p {

namespace GameRules {

namespace {

std::optional<Position> positionOf(const GameState& state)
{
    return Position::fromSfen(QString::fromLatin1(state.sfen));
}

void setError(QString* error, const QString& text)
{
    if (error) *error = text;
}

} // namespace

QByteArray sfenFor(const Position& position, quint32 ply)
{
    return position.toSfen(sideToMove(ply), static_cast<int>(ply) + 1).toLatin1();
}

GameState initialState(const QByteArray& gameId, const TimeControlTerms& terms)
{
    GameState s;
    s.gameId = gameId;
    s.ply = 0;
    s.previousStateHash = zeroHash();
    s.sfen = Rules::initialSfen();
    s.spentMs = 0;
    s.blackMainMs = terms.mainMs;
    s.whiteMainMs = terms.mainMs;
    return s;
}

bool canMoveWithin(quint64 remainingMainMs, quint64 byoyomiMs, quint64 spentMs)
{
    // 境界と同値なら時間切れ（R と B は上限があるので足しても桁あふれしない）
    return spentMs < remainingMainMs + byoyomiMs;
}

std::optional<GameState> nextState(const GameState& previous, const UsiMove& move, quint64 spentMs,
                                   const TimeControlTerms& terms, QString* error)
{
    if (previous.ply >= kMaxPly) {
        setError(error, QStringLiteral("ply limit reached"));
        return std::nullopt;
    }
    std::optional<Position> position = positionOf(previous);
    if (!position || position->sideToMove() != sideToMove(previous.ply)) {
        setError(error, QStringLiteral("previous state has no valid position"));
        return std::nullopt;
    }
    const Turn mover = sideToMove(previous.ply);
    const quint64 remaining = mover == Turn::Black ? previous.blackMainMs : previous.whiteMainMs;
    if (!canMoveWithin(remaining, terms.byoyomiMs, spentMs)) {
        setError(error, QStringLiteral("time is up"));
        return std::nullopt;
    }
    if (!move.isValid() || !EngineMoveValidator::applyIfLegal(*position, move)) {
        setError(error, QStringLiteral("illegal move"));
        return std::nullopt;
    }
    GameState next;
    next.gameId = previous.gameId;
    next.ply = previous.ply + 1;
    next.previousStateHash = previous.hash();
    next.moveUsi = move.toString().toLatin1();
    next.sfen = sfenFor(*position, next.ply);
    next.spentMs = spentMs;
    next.blackMainMs = previous.blackMainMs;
    next.whiteMainMs = previous.whiteMainMs;
    const quint64 left = remaining > spentMs ? remaining - spentMs : 0;
    (mover == Turn::Black ? next.blackMainMs : next.whiteMainMs) = left;
    return next;
}

StateCheck checkNext(const GameState& previous, const GameState& next, const TimeControlTerms& terms)
{
    if (next.gameId != previous.gameId) return StateCheck::WrongGame;
    if (next.ply != previous.ply + 1) return StateCheck::WrongPly;
    if (next.previousStateHash != previous.hash()) return StateCheck::WrongPrevious;
    const std::optional<UsiMove> move = UsiMove::parse(QString::fromLatin1(next.moveUsi));
    // 読めた手を書き直して元の文字列と一致すること（前後の空白・小文字の駒打ちを通さない）
    if (!move || move->toString().toLatin1() != next.moveUsi) return StateCheck::IllegalMove;
    const Turn mover = sideToMove(previous.ply);
    const quint64 remaining = mover == Turn::Black ? previous.blackMainMs : previous.whiteMainMs;
    if (!canMoveWithin(remaining, terms.byoyomiMs, next.spentMs)) return StateCheck::ClockMismatch;
    QString error;
    const std::optional<GameState> expected = nextState(previous, *move, next.spentMs, terms, &error);
    if (!expected) return StateCheck::IllegalMove;
    if (expected->sfen != next.sfen) return StateCheck::SfenMismatch;
    if (expected->blackMainMs != next.blackMainMs || expected->whiteMainMs != next.whiteMainMs) {
        return StateCheck::ClockMismatch;
    }
    return StateCheck::Ok;
}

std::optional<Terminal> judge(const std::vector<GameState>& states)
{
    if (states.empty()) return std::nullopt;
    const GameState& last = states.back();
    const std::optional<Position> position = positionOf(last);
    if (!position) return std::nullopt;

    // 合法手のない手番側の負け（王手がかかっていなくても詰みとして扱う）
    const EngineMoveValidator::Context ctx = EngineMoveValidator::load(*position);
    if (last.ply > 0 && EngineMoveValidator::legalMoveCount(ctx) == 0) {
        return Terminal{winnerOutcome(opponent(position->sideToMove())), Cause::Checkmate};
    }

    QStringList sfens;
    sfens.reserve(static_cast<qsizetype>(states.size()));
    for (const GameState& s : states) sfens.append(QString::fromLatin1(s.sfen));
    switch (SennichiteDetector::check(sfens)) {
    case SennichiteDetector::Result::ContinuousCheckByBlack:
        return Terminal{Outcome::WhiteWins, Cause::PerpetualCheck};
    case SennichiteDetector::Result::ContinuousCheckByWhite:
        return Terminal{Outcome::BlackWins, Cause::PerpetualCheck};
    case SennichiteDetector::Result::Draw:
        return Terminal{Outcome::Draw, Cause::Repetition};
    case SennichiteDetector::Result::None:
        break;
    }

    if (last.ply >= kMaxPly) return Terminal{Outcome::Draw, Cause::MaxPly};
    return std::nullopt;
}

} // namespace GameRules

} // namespace P2p
