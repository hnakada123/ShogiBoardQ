/// @file gameendhandler_record.cpp
/// @brief 終局時の棋譜追記・消費時間の確定と結果表示

#include "gameendhandler.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "parsecommon.h"
#include "logcategories.h"

#include <QDateTime>

// --- 棋譜追記 ---

void GameEndHandler::appendBreakOffLineAndMark()
{
    if (!m_refs.gameOver->isOver) return;
    if (m_refs.gameOver->moveAppended) return;

    const auto gcTurn = m_refs.gc ? m_refs.gc->currentPlayer() : ShogiGameController::NoPlayer;
    const Player curP = (gcTurn == ShogiGameController::Player1) ? Player::P1 : Player::P2;
    const QString line = (curP == Player::P1) ? QStringLiteral("▲中断") : QStringLiteral("△中断");

    if (m_refs.clock) {
        m_refs.clock->stopClock();

        const qint64 epochMs = m_hooks.turnEpochFor ? m_hooks.turnEpochFor(curP) : -1;
        qint64 considerMs = 0;
        if (epochMs > 0) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            considerMs = now - epochMs;
            if (considerMs < 0) considerMs = 0;
        } else {
            considerMs = (curP == Player::P1) ? m_refs.clock->player1ConsiderationMs()
                                               : m_refs.clock->player2ConsiderationMs();
        }

        if (curP == Player::P1) m_refs.clock->setPlayer1ConsiderationTime(int(considerMs));
        else                    m_refs.clock->setPlayer2ConsiderationTime(int(considerMs));

        if (curP == Player::P1) m_refs.clock->applyByoyomiAndResetConsideration1();
        else                    m_refs.clock->applyByoyomiAndResetConsideration2();
    }

    QString elapsed;
    if (m_refs.clock) {
        elapsed = (curP == Player::P1)
            ? m_refs.clock->player1ConsiderationAndTotalTime()
            : m_refs.clock->player2ConsiderationAndTotalTime();
    }

    if (m_hooks.appendKifuLine) m_hooks.appendKifuLine(line, elapsed);
    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();
    markGameOverMoveAppended();
}

void GameEndHandler::appendGameOverLineAndMark(Cause cause, Player loser)
{
    if (!m_refs.gameOver->isOver) return;
    if (m_refs.gameOver->moveAppended) return;
    if (!m_refs.clock || !m_hooks.appendKifuLine) {
        markGameOverMoveAppended();
        return;
    }

    m_refs.clock->stopClock();

    // 千日手・持将棋（引き分け）と連続王手の千日手の終局行は、終局した局面で手番の側の行として記録する。
    // 棋譜ファイルを読み込んだときも終局行には手番側の印が付くので、それと合わせる
    Player lineOwner = loser;
    if ((cause == Cause::Sennichite || cause == Cause::Jishogi || cause == Cause::OuteSennichite) && m_refs.gc) {
        lineOwner = (m_refs.gc->currentPlayer() == ShogiGameController::Player2) ? Player::P2 : Player::P1;
    }

    QString line;
    const QString mark = (lineOwner == Player::P1) ? QStringLiteral("▲") : QStringLiteral("△");
    const QString winMark = (loser == Player::P1) ? QStringLiteral("△") : QStringLiteral("▲");

    switch (cause) {
    case Cause::Jishogi:        line = QStringLiteral("%1持将棋").arg(mark); break;
    case Cause::NyugyokuWin:    line = QStringLiteral("%1入玉勝ち").arg(winMark); break;
    case Cause::IllegalMove:    line = QStringLiteral("%1反則負け").arg(mark); break;
    case Cause::Sennichite:     line = QStringLiteral("%1千日手").arg(mark); break;
    // 棋譜形式に連続王手の千日手の終局語は無いので、王手を続けた側の反則として記録する
    case Cause::OuteSennichite:
        line = KifuParseCommon::foulTerminalMove(loser == Player::P1, lineOwner == Player::P1);
        break;
    case Cause::BreakOff:       line = QStringLiteral("%1中断").arg(mark); break;
    case Cause::Resignation:    line = QStringLiteral("%1投了").arg(mark); break;
    case Cause::Timeout:        line = QStringLiteral("%1時間切れ").arg(mark); break;
    }

    // 終局行の消費時間は、その行（投了・宣言など）を行った手番側のもの。
    // 入玉宣言勝ちでは宣言した勝者が手番側になる。
    const Player mover = (cause == Cause::NyugyokuWin)
                             ? (loser == Player::P1 ? Player::P2 : Player::P1)
                             : lineOwner;
    const qint64 epochMs = m_hooks.turnEpochFor ? m_hooks.turnEpochFor(mover) : -1;
    qint64 considerMs = 0;
    if (epochMs > 0) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        considerMs = now - epochMs;
        if (considerMs < 0) considerMs = 0;
    } else {
        considerMs = (mover == Player::P1) ? m_refs.clock->player1ConsiderationMs()
                                            : m_refs.clock->player2ConsiderationMs();
    }

    if (mover == Player::P1) m_refs.clock->setPlayer1ConsiderationTime(int(considerMs));
    else                     m_refs.clock->setPlayer2ConsiderationTime(int(considerMs));

    if (mover == Player::P1) m_refs.clock->applyByoyomiAndResetConsideration1();
    else                     m_refs.clock->applyByoyomiAndResetConsideration2();

    const QString elapsed = (mover == Player::P1)
                                ? m_refs.clock->player1ConsiderationAndTotalTime()
                                : m_refs.clock->player2ConsiderationAndTotalTime();

    m_hooks.appendKifuLine(line, elapsed);
    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();
    markGameOverMoveAppended();
}

// --- 結果表示 ---

QString GameEndHandler::resultMessage(const GameEndInfo& info) const
{
    const bool loserIsP1 = (info.loser == Player::P1);
    const QString loserJP = loserIsP1 ? tr("先手") : tr("後手");
    const QString winnerJP = loserIsP1 ? tr("後手") : tr("先手");

    QString msg;
    switch (info.cause) {
    case Cause::Resignation:
        msg = tr("%1の投了。%2の勝ちです。").arg(loserJP, winnerJP); break;
    case Cause::Timeout:
        msg = tr("%1の時間切れ。%2の勝ちです。").arg(loserJP, winnerJP); break;
    case Cause::Jishogi:
        msg = tr("最大手数に達しました。持将棋です。"); break;
    case Cause::NyugyokuWin:
        msg = tr("%1の入玉宣言。%2の勝ちです。").arg(winnerJP, winnerJP); break;
    case Cause::IllegalMove:
        msg = tr("%1の反則負け。%2の勝ちです。").arg(loserJP, winnerJP); break;
    case Cause::Sennichite:
        msg = tr("千日手が成立しました。"); break;
    case Cause::OuteSennichite:
        msg = tr("%1の連続王手の千日手。%2の勝ちです。").arg(loserJP, winnerJP); break;
    case Cause::BreakOff:
    default:
        msg = tr("対局が終了しました。"); break;
    }
    return msg;
}

void GameEndHandler::displayResultsAndUpdateGui(const GameEndInfo& info)
{
    const QString msg = resultMessage(info);
    if (m_hooks.showGameOverDialog) m_hooks.showGameOverDialog(tr("対局終了"), msg);
    qCDebug(lcGame) << "Game ended";

    if (m_hooks.autoSaveKifuIfEnabled) {
        m_hooks.autoSaveKifuIfEnabled();
    }
    emit gameEndProcessed(info);
}
