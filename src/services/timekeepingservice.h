#ifndef TIMEKEEPINGSERVICE_H
#define TIMEKEEPINGSERVICE_H

/// @file timekeepingservice.h
/// @brief 時間管理サービスクラスの定義

#include <QString>

class ShogiClock;
class ShogiGameController;
class MatchCoordinator;

/**
 * @brief 対局中の時間管理（秒読み適用・時計制御・手番表示更新）を統合するサービス
 *
 * ShogiClock への秒読み/加算適用、MatchCoordinator の時間エポック記録、
 * UI手番表示の更新を一括で実行する静的メソッド群を提供する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class TimekeepingService {
public:
    // --- 秒読み・消費時間 ---

    /**
     * @brief 直前に指した側の秒読みを適用し、消費/累計時間文字列を返す
     * @param clock 将棋時計
     * @param nextIsP1 次に指す側がP1（先手）かどうか
     * @return 直前手の "mm:ss/HH:MM:SS" 形式の消費/累計文字列（未取得時は空）
     */
    static QString applyByoyomiAndCollectElapsed(ShogiClock* clock, bool nextIsP1);

    // --- 時計制御・後処理 ---

    /**
     * @brief 時計の停止/再開、MatchCoordinatorのpoke/arm/disarm等の後処理を一括実行
     * @param clock 将棋時計
     * @param match MatchCoordinator（nullable）
     * @param gc ゲームコントローラ（nullable）
     * @param nextIsP1 次に指す側がP1かどうか
     * @param isReplayMode 棋譜再生モード中かどうか
     */
    static void finalizeTurnPresentation(ShogiClock* clock,
                                         MatchCoordinator* match,
                                         ShogiGameController* gc,
                                         bool nextIsP1,
                                         bool isReplayMode);

    // --- 統合 API ---

    /**
     * @brief 時計・MatchCoordinator・UI更新まで一括で実行する統合関数
     * @param clock 将棋時計
     * @param match MatchCoordinator（nullable）
     * @param gc ゲームコントローラ（nullable）
     * @param isReplayMode 棋譜再生モード中かどうか
     * @param appendElapsedLine 棋譜欄に消費時間行を追記するコールバック
     * @param updateTurnStatus UI手番表示を更新するコールバック（1:先手, 2:後手）
     */
    static void updateTurnAndTimekeepingDisplay(
        ShogiClock* clock,
        MatchCoordinator* match,
        ShogiGameController* gc,
        bool isReplayMode,
        const std::function<void(const QString&)>& appendElapsedLine,
        const std::function<void(int)>& updateTurnStatus
        );
};

#endif // TIMEKEEPINGSERVICE_H
