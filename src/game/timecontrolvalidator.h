#ifndef TIMECONTROLVALIDATOR_H
#define TIMECONTROLVALIDATOR_H

/// @file timecontrolvalidator.h
/// @brief 対局開始前の時間設定の検証（GUI非依存）

/**
 * @brief 対局ダイアログの時間設定で対局を開始できるかを判定する
 *
 * 時間無制限は、双方の持ち時間・秒読み・加算がすべて0秒の場合に限る。
 * 片方だけ全項目0秒の設定と、エンジンが参加する時間無制限の対局は開始させない。
 * 仕様: docs/dev/game-time-control-spec.md
 */
namespace TimeControlValidator {

/// 対局者1人分の時間設定（ダイアログの入力値）
struct PlayerTime {
    int hours = 0;         ///< 持ち時間（時間）
    int minutes = 0;       ///< 持ち時間（分）
    int byoyomiSec = 0;    ///< 秒読み（秒）
    int incrementSec = 0;  ///< 1手ごとの加算（秒）
};

/// 対局を開始できない時間設定の種類
enum class Issue {
    None,                ///< 開始できる
    Player1HasNoTime,    ///< 先手／下手だけ全項目0秒
    Player2HasNoTime,    ///< 後手／上手だけ全項目0秒
    EngineWithoutLimit,  ///< エンジンが参加する時間無制限の対局
};

/**
 * @brief 時間設定を検証する
 * @param p1 先手／下手の時間設定
 * @param p2 後手／上手の時間設定
 * @param hasEngine どちらかの対局者がエンジンなら true
 */
Issue validate(const PlayerTime& p1, const PlayerTime& p2, bool hasEngine);

} // namespace TimeControlValidator

#endif // TIMECONTROLVALIDATOR_H
