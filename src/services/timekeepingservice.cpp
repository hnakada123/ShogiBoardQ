#include "timekeepingservice.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "matchcoordinator.h"

QString TimekeepingService::applyByoyomiAndCollectElapsed(ShogiClock* clock, bool nextIsP1)
{
    if (!clock) return QString();

    // 直前に指した側へ適用し、今回手の経過/累計文字列を取得
    if (nextIsP1) {
        clock->applyByoyomiAndResetConsideration2();
        const QString elapsed = clock->getPlayer2ConsiderationAndTotalTime();
        clock->setPlayer2ConsiderationTime(0);
        return elapsed;
    } else {
        clock->applyByoyomiAndResetConsideration1();
        const QString elapsed = clock->getPlayer1ConsiderationAndTotalTime();
        clock->setPlayer1ConsiderationTime(0);
        return elapsed;
    }
}

void TimekeepingService::finalizeTurnPresentation(ShogiClock* clock,
                                                  MatchCoordinator* match,
                                                  ShogiGameController* gc,
                                                  bool nextIsP1,
                                                  bool isReplayMode)
{
    if (match) match->pokeTimeUpdateNow();

    // 手番表示（UI側は MainWindow 側で別途実施してOK）
    if (!isReplayMode && clock) {
        clock->startClock();
    }

    if (match) {
        match->markTurnEpochNowFor(nextIsP1 ? MatchCoordinator::P1 : MatchCoordinator::P2);
        // HvH は共通、HvE は人間側のみ計測
        const bool humanTurn =
            gc && ((gc->currentPlayer() == ShogiGameController::Player1) ||
                   (gc->currentPlayer() == ShogiGameController::Player2)); // MainWindow 側の isHumanTurn に合わせてもOK
        if (humanTurn) {
            // プレイモードの分岐は司令塔側ユーティリティで吸収
            match->armHumanTimerIfNeeded();
            match->armTurnTimerIfNeeded();
        } else {
            match->disarmHumanTimerIfNeeded();
        }
    }
}
