/// @file csagameprogress.cpp
/// @brief CSA通信対局1局ぶんの進行を表す値型の実装

#include "csagameprogress.h"

#include <limits>

CsaGameProgress CsaGameProgress::forSummary(const CsaGameSummary& summary)
{
    CsaGameProgress progress;
    progress.summary = summary;
    progress.isBlackSide = summary.isBlackSide();
    return progress;
}

Turn CsaGameProgress::sideToMove() const
{
    if (record.entries.isEmpty()) return summary.toMove;
    return opponent(record.entries.constLast().mover);
}

namespace {

/// 時間の足し算（int の範囲で止める。サーバーが送る値を何手分足しても桁あふれさせない）
int addMs(int a, qint64 b)
{
    return static_cast<int>(qBound(qint64(std::numeric_limits<int>::min()), a + b, qint64(std::numeric_limits<int>::max())));
}

} // namespace

void CsaGameProgress::startClockFromSummary()
{
    const CsaGameSummary& s = summary;
    int remainingBlack = s.totalTimeMs(true);
    int remainingWhite = s.totalTimeMs(false);
    int consumedBlack = 0;
    int consumedWhite = 0;
    for (int i = 0; i < s.moves.size(); ++i) {
        const bool black = s.moves.at(i).startsWith(QLatin1Char('+'));
        int& remaining = black ? remainingBlack : remainingWhite;
        int& consumed = black ? consumedBlack : consumedWhite;
        const int consumedMs = s.positionMoveConsumedMs(i);
        remaining = addMs(remaining, s.incrementMs(black));
        consumed = addMs(consumed, consumedMs);
        // 残り時間を超えた分は秒読みで指したものとして、残りは 0 にする（仕様 3.2.2.1）
        remaining = qMax(0, remaining - consumedMs);
    }

    if (s.toMove == Turn::Black) remainingBlack = addMs(remainingBlack, s.incrementMs(true));
    else remainingWhite = addMs(remainingWhite, s.incrementMs(false));

    blackTotalMs = consumedBlack;
    whiteTotalMs = consumedWhite;
    blackRemainingMs = remainingBlack;
    whiteRemainingMs = remainingWhite;
}

void CsaGameProgress::recordMoveTime(bool blackMove, int consumedMs)
{
    int& total = blackMove ? blackTotalMs : whiteTotalMs;
    int& remaining = blackMove ? blackRemainingMs : whiteRemainingMs;
    total = addMs(total, consumedMs);
    remaining = qMax(0, remaining - consumedMs);
    // CSAの加算は次の手番の開始時に、その手番側の加算時間で行う。
    if (blackMove) whiteRemainingMs = addMs(whiteRemainingMs, summary.incrementMs(false));
    else blackRemainingMs = addMs(blackRemainingMs, summary.incrementMs(true));
}

RecordTime CsaGameProgress::initialMoveTime(int index) const
{
    // 再開局面の各手の消費時間と、その側の累計（「１手 / 合計」の表記）
    const CsaGameSummary& s = summary;
    if (index < 0 || index >= s.moves.size()) return {};
    const bool black = s.moves.at(index).startsWith(QLatin1Char('+'));
    qint64 total = 0;
    for (int i = 0; i <= index; ++i) {
        if (s.moves.at(i).startsWith(QLatin1Char('+')) == black) total += s.positionMoveConsumedMs(i);
    }
    return RecordTime::fromMilliseconds(s.positionMoveConsumedMs(index), total);
}

UsiGoTimeInput CsaGameProgress::engineTiming(bool pondering, qint64 elapsedThisTurnMs, int marginMs) const
{
    // 仕様 3.2.2.1: 1手に使える時間は「遅延時間＋残り時間＋秒読み」。遅延時間はサーバーが消費時間から
    // 差し引くので、今の手の思考時間に足すだけでよい。サーバーは LF の到着時刻で消費時間を測るので、
    // 通信の遅延に備えて余裕を差し引く。go に載せる値は UsiGoTimeCalculator が求める。
    const TimeControl tc = summary.timeControl();
    const Turn mine = isBlackSide ? Turn::Black : Turn::White;
    const Turn toMove = sideToMove();
    // 手番側の残りだけが今の手番の加算を含み、今の手番の経過時間はまだ引いていない
    const auto clockSide = [&](Turn player) {
        const bool onTurn = player == toMove;
        return UsiClockSide{qMax(0, remainingMs(player == Turn::Black)), onTurn, onTurn ? elapsedThisTurnMs : 0};
    };
    UsiGoTimeInput timing = UsiGoTimeCalculator::fromClock(tc, mine, pondering, clockSide(Turn::Black),
                                                           clockSide(Turn::White));
    timing.extraMs = tc.normalized().side(mine).delayMs;
    timing.marginMs = qMax(0, marginMs);
    return timing;
}

RecordEntry CsaGameProgress::moveRecord(bool blackMove, const QString& displayText, int consumedMs) const
{
    RecordEntry entry;
    entry.displayText = displayText;
    entry.mover = blackMove ? Turn::Black : Turn::White;
    entry.time = RecordTime::fromMilliseconds(consumedMs, totalMs(blackMove));
    return entry;
}

MatchGameEndInfo CsaGameProgress::endInfo(CsaClient::GameResult result, CsaClient::GameEndCause cause) const
{
    using Cause = CsaClient::GameEndCause;
    // 勝敗のない終局（中断など）の負けた側は使われない
    const bool iAmLoser = (result == CsaClient::GameResult::Lose);
    const Turn loser = (iAmLoser == isBlackSide) ? Turn::Black : Turn::White;
    switch (cause) {
    case Cause::Resign: return {MatchCause::Resignation, loser};
    case Cause::TimeUp: return {MatchCause::Timeout, loser};
    case Cause::IllegalMove:
    case Cause::IllegalAction: return {MatchCause::IllegalMove, loser};
    case Cause::OuteSennichite: return {MatchCause::OuteSennichite, loser};
    case Cause::Sennichite: return {MatchCause::Sennichite, loser};
    // 入玉宣言は宣言した側の勝ち。点数による引き分けは持将棋
    case Cause::Jishogi:
        return {result == CsaClient::GameResult::Draw ? MatchCause::Jishogi : MatchCause::NyugyokuWin, loser};
    case Cause::MaxMoves: return {MatchCause::MaxMoves, loser};
    case Cause::Chudan:
    case Cause::Unknown: break;
    }
    return {MatchCause::BreakOff, loser};
}

RecordEntry CsaGameProgress::endRecord(CsaClient::GameResult result, CsaClient::GameEndCause cause,
                                       bool blackToMove, int consumedMs) const
{
    // 終局行は手番側の行にそろえ（手番でない側の反則負けは手番側の反則勝ち）、消費時間はその側の累計に足す
    const Turn sideToMove = blackToMove ? Turn::Black : Turn::White;
    const RecordEntry::Ending ending = MatchEnding::endingOf(endInfo(result, cause), sideToMove);
    const RecordTime time =
        RecordTime::fromMilliseconds(consumedMs, qint64(totalMs(ending.side == Turn::Black)) + consumedMs);
    return RecordEntry::ending(ending.terminal, ending.side, time);
}
