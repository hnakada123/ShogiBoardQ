#ifndef MATCHTYPES_H
#define MATCHTYPES_H

/// @file matchtypes.h
/// @brief MatchCoordinator の基本型定義（Cause, GameEndInfo, GameOverState）
///
/// MatchCoordinator に依存せずに終局情報・時間パラメータ等を参照できる軽量ヘッダ。
/// matchcoordinator.h は本ファイルをインクルードし、using エイリアスで
/// MatchCoordinator::Cause 等を再公開する。対局者（先手・後手）は Turn で表す。

#include <QDateTime>

#include "shogitypes.h"

/// @brief 終局原因
/// @note MatchCoordinator::Cause は本型の using エイリアス。
enum class MatchCause : int {
    Resignation    = 0,  ///< 投了
    Timeout        = 1,  ///< 時間切れ
    BreakOff       = 2,  ///< 中断
    Jishogi        = 3,  ///< 持将棋／最大手数到達
    NyugyokuWin    = 4,  ///< 入玉宣言勝ち
    IllegalMove    = 5,  ///< 反則負け（入玉宣言失敗など）
    Sennichite     = 6,  ///< 千日手（引き分け）
    OuteSennichite = 7   ///< 連続王手の千日手（反則負け）
};

/// @brief 終局情報
/// @note MatchCoordinator::GameEndInfo は本型の using エイリアス。
struct MatchGameEndInfo {
    MatchCause  cause = MatchCause::Resignation;  ///< 終局原因
    Turn        loser = Turn::Black;              ///< 敗者
};

/// @brief 直近の終局状態
/// @note MatchCoordinator::GameOverState は本型の using エイリアス。
struct MatchGameOverState {
    bool             isOver        = false;  ///< 終局済みか
    bool             moveAppended  = false;  ///< 棋譜に終局手を追記済みか
    bool             hasLast       = false;  ///< 直近の終局情報があるか
    MatchGameEndInfo lastInfo;               ///< 直近の終局情報
    QDateTime        when;                   ///< 終局日時
};

#endif // MATCHTYPES_H
