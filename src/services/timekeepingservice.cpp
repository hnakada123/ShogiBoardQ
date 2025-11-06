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

void TimekeepingService::updateTurnAndTimekeepingDisplay(
    ShogiClock* clock,
    MatchCoordinator* match,
    ShogiGameController* gc,
    bool isReplayMode,
    const std::function<void(const QString&)>& appendElapsedLine,
    const std::function<void(int)>& updateTurnStatus)
{
    // 1) KIF再生中は時計を動かさない（統一）
    if (isReplayMode) {
        if (clock) clock->stopClock();
        if (match) match->pokeTimeUpdateNow();
        return;
    }

    // 2) 終局後は時計を止めて整えるのみ
    const bool gameOver = (match && match->gameOverState().isOver);
    if (gameOver) {
        if (clock) clock->stopClock();
        if (match) match->pokeTimeUpdateNow();
        return;
    }

    // 次に指すのは誰か（UIの次手番＝ now から見た next）
    const bool nextIsP1 = (gc && gc->currentPlayer() == ShogiGameController::Player2);

    // 3) 直前手の byoyomi/increment を適用し、消費/累計テキストを返す
    const QString elapsed = TimekeepingService::applyByoyomiAndCollectElapsed(clock, nextIsP1);
    if (!elapsed.isEmpty() && appendElapsedLine) {
        appendElapsedLine(elapsed); // 「mm:ss/HH:MM:SS」を棋譜欄へ
    }

    // 4) UIの手番表示（1:先手, 2:後手）
    if (updateTurnStatus) {
        updateTurnStatus(nextIsP1 ? 1 : 2);
    }

    // 5) 時計/司令塔の後処理（poke, start, epoch 記録、人間タイマのアーム/解除）
    TimekeepingService::finalizeTurnPresentation(clock, match, gc, nextIsP1, isReplayMode);
}
