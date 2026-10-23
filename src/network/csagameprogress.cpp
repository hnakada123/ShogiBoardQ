/// @file csagameprogress.cpp
/// @brief CSA通信対局1局ぶんの進行を表す値型の実装

#include "csagameprogress.h"

CsaGameProgress CsaGameProgress::forSummary(const CsaClient::GameSummary& summary)
{
    CsaGameProgress progress;
    progress.summary = summary;
    progress.isBlackSide = summary.isBlackSide();
    return progress;
}

void CsaGameProgress::startClockFromSummary()
{
    const CsaClient::GameSummary& s = summary;
    int remainingBlack = s.totalTimeMs(true);
    int remainingWhite = s.totalTimeMs(false);
    int consumedBlack = 0;
    int consumedWhite = 0;
    for (int i = 0; i < s.moves.size(); ++i) {
        const bool black = s.moves.at(i).startsWith(QLatin1Char('+'));
        int& remaining = black ? remainingBlack : remainingWhite;
        int& consumed = black ? consumedBlack : consumedWhite;
        const int consumedMs = s.positionMoveConsumedMs(i);
        remaining += s.incrementMs(black);
        consumed += consumedMs;
        // 残り時間を超えた分は秒読みで指したものとして、残りは 0 にする（仕様 3.2.2.1）
        remaining = qMax(0, remaining - consumedMs);
    }

    if (s.toMove == Turn::Black) remainingBlack += s.incrementMs(true);
    else remainingWhite += s.incrementMs(false);

    blackTotalMs = consumedBlack;
    whiteTotalMs = consumedWhite;
    blackRemainingMs = remainingBlack;
    whiteRemainingMs = remainingWhite;
}

void CsaGameProgress::recordMoveTime(bool blackMove, int consumedMs)
{
    int& total = blackMove ? blackTotalMs : whiteTotalMs;
    int& remaining = blackMove ? blackRemainingMs : whiteRemainingMs;
    total += consumedMs;
    remaining = qMax(0, remaining - consumedMs);
    // CSAの加算は次の手番の開始時に、その手番側の加算時間で行う。
    if (blackMove) whiteRemainingMs += summary.incrementMs(false);
    else blackRemainingMs += summary.incrementMs(true);
}

RecordTime CsaGameProgress::initialMoveTime(int index) const
{
    // 再開局面の各手の消費時間と、その側の累計（「１手 / 合計」の表記）
    const CsaClient::GameSummary& s = summary;
    if (index < 0 || index >= s.moves.size()) return {};
    const bool black = s.moves.at(index).startsWith(QLatin1Char('+'));
    qint64 total = 0;
    for (int i = 0; i <= index; ++i) {
        if (s.moves.at(i).startsWith(QLatin1Char('+')) == black) total += s.positionMoveConsumedMs(i);
    }
    return RecordTime::fromMilliseconds(s.positionMoveConsumedMs(index), total);
}

UsiGoTimeInput CsaGameProgress::engineTiming(int marginMs) const
{
    // 仕様 3.2.2.1: 1手に使える時間は「遅延時間＋残り時間＋秒読み」。遅延時間はサーバーが消費時間から
    // 差し引くので、今の手の思考時間に足すだけでよい。サーバーは LF の到着時刻で消費時間を測るので、
    // 通信の遅延に備えて余裕を差し引く。go に載せる値は UsiGoTimeCalculator が求める。
    const TimeControl tc = summary.timeControl().normalized();
    const TimeControlSide& mine = tc.side(isBlackSide ? Turn::Black : Turn::White);
    qint64 black = qMax(0, blackRemainingMs);
    qint64 white = qMax(0, whiteRemainingMs);
    qint64& own = isBlackSide ? black : white;
    own = qMax<qint64>(0, own - mine.incrementMs);

    UsiGoTimeInput timing;
    timing.timeControl = tc;
    timing.btimeMs = black;
    timing.wtimeMs = white;
    timing.extraMs = mine.delayMs;
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

RecordEntry CsaGameProgress::endRecord(CsaClient::GameResult result, CsaClient::GameEndCause cause,
                                       bool blackToMove, int consumedMs) const
{
    using Cause = CsaClient::GameEndCause;
    const bool isDraw = (result == CsaClient::GameResult::Draw);
    const Turn sideToMove = blackToMove ? Turn::Black : Turn::White;
    // 勝敗のない終局（中断など）の負けた側は使われない
    const bool iAmLoser = (result == CsaClient::GameResult::Lose);
    const Turn loser = (iAmLoser == isBlackSide) ? Turn::Black : Turn::White;

    TerminalType type = TerminalType::Interrupt;
    switch (cause) {
    case Cause::Resign: type = TerminalType::Resign; break;
    case Cause::TimeUp: type = TerminalType::Timeout; break;
    case Cause::IllegalMove:
    case Cause::IllegalAction: type = TerminalType::IllegalLoss; break;
    case Cause::OuteSennichite: type = RecordEntry::perpetualCheckTerminal(loser, sideToMove); break;
    case Cause::Sennichite: type = TerminalType::Repetition; break;
    // 入玉宣言は宣言した勝者の行。点数による引き分けは持将棋
    case Cause::Jishogi: type = isDraw ? TerminalType::Impasse : TerminalType::DeclarationWin; break;
    case Cause::MaxMoves: type = TerminalType::MaxMoves; break;
    case Cause::Chudan:
    case Cause::Unknown: break;
    }

    // 消費時間は終局行を行った側の累計に足す
    const Turn side = RecordEntry::endingSide(type, sideToMove, loser);
    const RecordTime time =
        RecordTime::fromMilliseconds(consumedMs, qint64(totalMs(side == Turn::Black)) + consumedMs);
    return RecordEntry::ending(type, side, time);
}
