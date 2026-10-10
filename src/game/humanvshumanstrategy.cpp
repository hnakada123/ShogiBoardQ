/// @file humanvshumanstrategy.cpp
/// @brief 人間 vs 人間モードの Strategy 実装

#include "humanvshumanstrategy.h"
#include "strategycontext.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "logcategories.h"

HumanVsHumanStrategy::HumanVsHumanStrategy(MatchCoordinator::StrategyContext& ctx)
    : m_ctx(ctx)
{
}

void HumanVsHumanStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvH";

    // --- 手番の単一ソースを確立：GC → TurnManager → m_cur → 表示
    ShogiGameController::Player side =
        m_ctx.gc() ? m_ctx.gc()->currentPlayer()
                   : ShogiGameController::NoPlayer;
    if (side == ShogiGameController::NoPlayer) {
        side = ShogiGameController::Player1;
        if (m_ctx.gc()) m_ctx.gc()->setCurrentPlayer(side);
    }

    m_ctx.setCurrentTurn(
        (side == ShogiGameController::Player2) ? MatchCoordinator::P2
                                               : MatchCoordinator::P1);

    // 盤描画は手番反映のあと（ハイライト/時計のズレ防止）
    if (m_ctx.hooks().ui.renderBoardFromGc)
        m_ctx.hooks().ui.renderBoardFromGc();
    m_ctx.updateTurnDisplay(m_ctx.currentTurn());
}

void HumanVsHumanStrategy::onHumanMove(const QPoint& /*from*/,
                                        const QPoint& /*to*/,
                                        const QString& prettyMove)
{
    // 着手後の現在手番は「次の手番」なので、着手者はその逆
    ShogiGameController::Player curAfterMove =
        m_ctx.gc() ? m_ctx.gc()->currentPlayer()
                   : ShogiGameController::NoPlayer;
    const MatchCoordinator::Player moverP =
        (curAfterMove == ShogiGameController::Player1) ? MatchCoordinator::P2
                                                       : MatchCoordinator::P1;

    // 直前手の消費時間（consideration）を確定
    finishTurnTimerAndSetConsideration(static_cast<int>(moverP));

    // HvH でも秒読み/インクリメントを適用し、総考慮へ加算して表示値を確定
    ShogiClock* clock = m_ctx.clock();
    if (clock) {
        if (moverP == MatchCoordinator::P1) {
            clock->applyByoyomiAndResetConsideration1();
        } else {
            clock->applyByoyomiAndResetConsideration2();
        }
    }

    // 指し手＋考慮時間を棋譜欄に追記する。千日手・持将棋の終局行より先に記録する
    if (m_ctx.hooks().game.appendKifuLine) {
        QString elapsed = QStringLiteral("00:00/00:00:00");
        if (clock) {
            elapsed = (moverP == MatchCoordinator::P1) ? clock->player1ConsiderationAndTotalTime()
                                                       : clock->player2ConsiderationAndTotalTime();
        }
        m_ctx.hooks().game.appendKifuLine(prettyMove, elapsed);
    }

    // 表示更新（時計ラベル等）
    qCDebug(lcGame) << "[Match] HvH: finalize previous turn";
    if (clock) m_ctx.pokeTimeUpdateNow();

    // 千日手チェック（SFENは呼び出し前に追加済み）
    if (m_ctx.checkAndHandleSennichite()) return;

    // 最大手数チェック
    const QStringList* rec = m_ctx.sfenHistory();
    const int movesPlayed = rec ? static_cast<int>(rec->size() - 1) : m_ctx.currentMoveIndex();
    if (m_ctx.maxMoves() > 0 && movesPlayed >= m_ctx.maxMoves()) {
        m_ctx.handleMaxMovesJishogi();
        return;
    }

    // 次手番の計測と UI 準備
    armTurnTimerIfNeeded();
}

void HumanVsHumanStrategy::armTurnTimerIfNeeded()
{
    if (!m_turnTimerArmed) {
        m_turnTimer.start();
        m_turnTimerArmed = true;
    }
}

void HumanVsHumanStrategy::disarmTurnTimer()
{
    // 「待った」や終局で計測を打ち切る。次の armTurnTimerIfNeeded() で測り直す。
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}

void HumanVsHumanStrategy::finishTurnTimerAndSetConsideration(int moverPlayer)
{
    if (!m_turnTimerArmed) return;

    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;
    const auto mover = static_cast<MatchCoordinator::Player>(moverPlayer);

    ShogiClock* clock = m_ctx.clock();
    if (clock) {
        if (mover == MatchCoordinator::P1)
            clock->setMeasuredConsiderationTime(1, ms);
        else
            clock->setMeasuredConsiderationTime(2, ms);
    }
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}
