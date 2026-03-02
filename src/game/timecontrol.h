#ifndef TIMECONTROL_H
#define TIMECONTROL_H

/// @file timecontrol.h
/// @brief 時間制御設定構造体の定義
///
/// MatchCoordinator に依存せずに時間制御設定を参照できる軽量ヘッダ。
/// MatchCoordinator::TimeControl は本型の using エイリアス。

/// @brief 時間制御の設定
/// @note MatchCoordinator::TimeControl は本型の using エイリアス。
struct MatchTimeControl {
    bool useByoyomi    = false;  ///< 秒読み使用フラグ
    int  byoyomiMs1    = 0;      ///< 先手秒読み（ms）
    int  byoyomiMs2    = 0;      ///< 後手秒読み（ms）
    int  incMs1        = 0;      ///< 先手加算（ms）
    int  incMs2        = 0;      ///< 後手加算（ms）
    bool loseOnTimeout = false;  ///< 時間切れ負けフラグ
};

#endif // TIMECONTROL_H
