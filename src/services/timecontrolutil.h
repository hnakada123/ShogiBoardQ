#ifndef TIMECONTROLUTIL_H
#define TIMECONTROLUTIL_H

/// @file timecontrolutil.h
/// @brief 持ち時間設定をShogiClockへ適用するユーティリティの定義
/// @todo remove コメントスタイルガイド適用済み

#include "gamestartcoordinator.h"
#include <QString>

class ShogiClock;
class GameStartCoordinator;

/**
 * @brief 持ち時間設定をShogiClockへ適用するユーティリティ名前空間
 *
 * GameStartCoordinator::TimeControl の設定値を ShogiClock に変換・適用する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
namespace TimeControlUtil {

/**
 * @brief GameStartCoordinator::TimeControl を ShogiClock へ適用する
 * @param clock 対象の将棋時計
 * @param tc 持ち時間設定
 * @param startSfenStr 開始局面のSFEN（初期手番の推定にのみ使用）
 * @param currentSfenStr 現在局面のSFEN（初期手番の推定にのみ使用）
 */
void applyToClock(ShogiClock* clock,
                  const GameStartCoordinator::TimeControl& tc,
                  const QString& startSfenStr,
                  const QString& currentSfenStr);

} // namespace TimeControlUtil

#endif // TIMECONTROLUTIL_H
