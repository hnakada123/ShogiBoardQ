/// @file humanvshumanstrategy.cpp
/// @brief 人間 vs 人間モードの Strategy 実装

#include "humanvshumanstrategy.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"

HumanVsHumanStrategy::HumanVsHumanStrategy(MatchCoordinator* coordinator)
    : m_coordinator(coordinator)
{
}

void HumanVsHumanStrategy::start()
{
    if (m_coordinator->m_hooks.log)
        m_coordinator->m_hooks.log(QStringLiteral("[Match] Start HvH"));

    // --- 手番の単一ソースを確立：GC → TurnManager → m_cur → 表示
    ShogiGameController::Player side =
        m_coordinator->m_gc ? m_coordinator->m_gc->currentPlayer()
                            : ShogiGameController::NoPlayer;
    if (side == ShogiGameController::NoPlayer) {
        side = ShogiGameController::Player1;
        if (m_coordinator->m_gc) m_coordinator->m_gc->setCurrentPlayer(side);
    }

    m_coordinator->m_cur =
        (side == ShogiGameController::Player2) ? MatchCoordinator::P2
                                               : MatchCoordinator::P1;

    // 盤描画は手番反映のあと（ハイライト/時計のズレ防止）
    if (m_coordinator->m_hooks.renderBoardFromGc)
        m_coordinator->m_hooks.renderBoardFromGc();
    m_coordinator->updateTurnDisplay(m_coordinator->m_cur);
}

void HumanVsHumanStrategy::onHumanMove(const QPoint& /*from*/,
                                        const QPoint& /*to*/,
                                        const QString& /*prettyMove*/)
{
    // 千日手チェック（SFENは呼び出し前に追加済み）
    if (m_coordinator->checkAndHandleSennichite()) return;

    // 着手後の現在手番は「次の手番」なので、着手者はその逆
    ShogiGameController::Player curAfterMove =
        m_coordinator->m_gc ? m_coordinator->m_gc->currentPlayer()
                            : ShogiGameController::NoPlayer;
    const MatchCoordinator::Player moverP =
        (curAfterMove == ShogiGameController::Player1) ? MatchCoordinator::P2
                                                       : MatchCoordinator::P1;

    // 直前手の消費時間（consideration）を確定
    finishTurnTimerAndSetConsideration(static_cast<int>(moverP));

    // HvH でも秒読み/インクリメントを適用し、総考慮へ加算して表示値を確定
    ShogiClock* clock = m_coordinator->m_clock;
    if (clock) {
        if (moverP == MatchCoordinator::P1) {
            clock->applyByoyomiAndResetConsideration1();
        } else {
            clock->applyByoyomiAndResetConsideration2();
        }
    }

    // 表示更新（時計ラベル等）
    if (m_coordinator->m_hooks.log)
        m_coordinator->m_hooks.log(QStringLiteral("[Match] HvH: finalize previous turn"));
    if (clock) m_coordinator->handleTimeUpdated();

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

void HumanVsHumanStrategy::finishTurnTimerAndSetConsideration(int moverPlayer)
{
    if (!m_turnTimerArmed) return;

    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;
    const auto mover = static_cast<MatchCoordinator::Player>(moverPlayer);

    ShogiClock* clock = m_coordinator->m_clock;
    if (clock) {
        if (mover == MatchCoordinator::P1)
            clock->setPlayer1ConsiderationTime(static_cast<int>(ms));
        else
            clock->setPlayer2ConsiderationTime(static_cast<int>(ms));
    }
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}
