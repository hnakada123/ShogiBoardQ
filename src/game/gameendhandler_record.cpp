/// @file gameendhandler_record.cpp
/// @brief 終局時の棋譜追記・消費時間の確定と結果表示

#include "gameendhandler.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "logcategories.h"

// --- 棋譜追記 ---

void GameEndHandler::appendBreakOffLineAndMark()
{
    if (!m_refs.session->gameOver().isOver) return;
    if (m_refs.session->gameOver().moveAppended) return;

    const Turn gcTurn = m_refs.gc ? m_refs.gc->currentPlayer() : Turn::White;

    if (m_refs.clock) {
        // 止めた時点までの経過を手番側の考慮時間に精算し、その考慮時間で確定する
        m_refs.clock->stopClock();
        m_refs.clock->commitMove(gcTurn);
    }

    RecordTime elapsed;
    if (m_refs.clock) {
        elapsed = m_refs.clock->recordTime(gcTurn);
    }

    if (m_hooks.appendRecord) m_hooks.appendRecord(RecordEntry::ending(TerminalType::Interrupt, gcTurn, elapsed));
    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();
    markGameOverMoveAppended();
}

void GameEndHandler::appendGameOverLineAndMark(Cause cause, Turn loser)
{
    if (!m_refs.session->gameOver().isOver) return;
    if (m_refs.session->gameOver().moveAppended) return;
    if (!m_refs.clock || !m_hooks.appendRecord) {
        markGameOverMoveAppended();
        return;
    }

    m_refs.clock->stopClock();

    // 千日手・持将棋（引き分け）と連続王手の千日手の終局行は、終局した局面で手番の側の行として記録する。
    // 棋譜ファイルを読み込んだときも終局行には手番側の印が付くので、それと合わせる
    Turn lineOwner = loser;
    if ((cause == Cause::Sennichite || cause == Cause::Jishogi || cause == Cause::OuteSennichite) && m_refs.gc) {
        lineOwner = m_refs.gc->currentPlayer();
    }

    TerminalType terminal = TerminalType::None;
    switch (cause) {
    case Cause::Jishogi: terminal = TerminalType::Impasse; break;
    case Cause::NyugyokuWin: terminal = TerminalType::DeclarationWin; break;
    case Cause::IllegalMove: terminal = TerminalType::IllegalLoss; break;
    case Cause::Sennichite: terminal = TerminalType::Repetition; break;
    case Cause::OuteSennichite:
        terminal = loser == lineOwner ? TerminalType::IllegalLoss : TerminalType::IllegalWin;
        break;
    case Cause::BreakOff: terminal = TerminalType::Interrupt; break;
    case Cause::Resignation: terminal = TerminalType::Resign; break;
    case Cause::Timeout: terminal = TerminalType::Timeout; break;
    }

    // 終局行の消費時間は、その行（投了・宣言など）を行った手番側のもの。
    // 入玉宣言勝ちでは宣言した勝者が手番側になる。
    const Turn mover = (cause == Cause::NyugyokuWin)
                             ? (loser == Turn::Black ? Turn::White : Turn::Black)
                             : lineOwner;
    // 時計は上で止めて考慮時間を精算済み。その考慮時間で確定する
    m_refs.clock->commitMove(mover);

    const RecordTime elapsed = m_refs.clock->recordTime(mover);

    m_hooks.appendRecord(RecordEntry::ending(terminal, mover, elapsed));
    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();
    markGameOverMoveAppended();
}

// --- 結果表示 ---

bool GameEndHandler::usesHandicapNames() const
{
    return m_hooks.usesHandicapNames && m_hooks.usesHandicapNames();
}

QString GameEndHandler::resultMessage(const GameEndInfo& info) const
{
    // 駒落ちは棋譜の局面図・終局行と同じく下手・上手と呼ぶ
    const bool handicapNames = usesHandicapNames();
    const QString blackJP = handicapNames ? tr("下手") : tr("先手");
    const QString whiteJP = handicapNames ? tr("上手") : tr("後手");
    const QString loserJP = (info.loser == Turn::Black) ? blackJP : whiteJP;
    const QString winnerJP = (info.loser == Turn::Black) ? whiteJP : blackJP;

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
