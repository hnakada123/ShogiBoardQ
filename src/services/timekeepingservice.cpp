/// @file timekeepingservice.cpp
/// @brief 時間管理サービスクラスの実装

#include "timekeepingservice.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "matchcoordinator.h"

// ============================================================
// 秒読み・消費時間
// ============================================================

QString TimekeepingService::applyByoyomiAndCollectElapsed(ShogiClock* clock, bool nextIsP1)
{
    if (!clock) return QString();

    // 直前に指した側へ秒読みを適用し、消費/累計文字列を取得
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

// ============================================================
// 時計制御・後処理
// ============================================================

void TimekeepingService::finalizeTurnPresentation(ShogiClock* clock,
                                                  MatchCoordinator* match,
                                                  ShogiGameController* gc,
                                                  bool nextIsP1,
                                                  bool isReplayMode)
{
    if (match) match->pokeTimeUpdateNow();

    if (!isReplayMode && clock) {
        clock->startClock();
    }

    if (match) {
        match->markTurnEpochNowFor(nextIsP1 ? MatchCoordinator::P1 : MatchCoordinator::P2);
        const bool humanTurn =
            gc && ((gc->currentPlayer() == ShogiGameController::Player1) ||
                   (gc->currentPlayer() == ShogiGameController::Player2));
        if (humanTurn) {
            match->armTurnTimerIfNeeded();
        } else {
            match->disarmHumanTimerIfNeeded();
        }
    }
}

// ============================================================
// 統合 API
// ============================================================

void TimekeepingService::updateTurnAndTimekeepingDisplay(
    ShogiClock* clock,
    MatchCoordinator* match,
    ShogiGameController* gc,
    bool isReplayMode,
    const std::function<void(const QString&)>& appendElapsedLine,
    const std::function<void(int)>& updateTurnStatus)
{
    // 処理フロー:
    // 1. KIF再生中は時計を止めて即リターン
    // 2. 終局後も時計を止めて即リターン
    // 3. 直前手の秒読み/加算を適用し消費時間を棋譜欄へ
    // 4. UIの手番表示を更新
    // 5. 時計/MatchCoordinatorの後処理

    // 1) KIF再生中は時計を動かさない
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

    // 次に指す側を判定
    const bool nextIsP1 = (gc && gc->currentPlayer() == ShogiGameController::Player2);

    // 3) 直前手の byoyomi/increment を適用し、消費/累計テキストを返す
    const QString elapsed = TimekeepingService::applyByoyomiAndCollectElapsed(clock, nextIsP1);
    if (!elapsed.isEmpty() && appendElapsedLine) {
        appendElapsedLine(elapsed);
    }

    // 4) UIの手番表示（1:先手, 2:後手）
    if (updateTurnStatus) {
        updateTurnStatus(nextIsP1 ? 1 : 2);
    }

    // 5) 時計/MatchCoordinatorの後処理
    TimekeepingService::finalizeTurnPresentation(clock, match, gc, nextIsP1, isReplayMode);
}
