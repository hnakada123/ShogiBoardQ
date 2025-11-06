#ifndef TIMEKEEPINGSERVICE_H
#define TIMEKEEPINGSERVICE_H

#include <QString>

class ShogiClock;
class ShogiGameController;
class MatchCoordinator;

class TimekeepingService {
public:
    // 「これから指す側」が P1 かどうかを引数で渡す。
    // 返り値は「直前に指した側の 1手の消費/累計」を "mm:ss/HH:MM:SS" で返す（未取得時は空）。
    static QString applyByoyomiAndCollectElapsed(ShogiClock* clock, bool nextIsP1);

    // 時計の停止/再開、司令塔の poke/arm/disarm など、時間面の後処理をまとめて実行。
    static void finalizeTurnPresentation(ShogiClock* clock,
                                         MatchCoordinator* match,
                                         ShogiGameController* gc,
                                         bool nextIsP1,
                                         bool isReplayMode);

    // 時計・司令塔・UI更新まで一括で実行する統合関数
    static void updateTurnAndTimekeepingDisplay(
        ShogiClock* clock,
        MatchCoordinator* match,
        ShogiGameController* gc,
        bool isReplayMode,
        const std::function<void(const QString&)>& appendElapsedLine, // 棋譜欄追記
        const std::function<void(int)>& updateTurnStatus               // UI 手番表示 (1:先手 / 2:後手)
        );
};

#endif // TIMEKEEPINGSERVICE_H
