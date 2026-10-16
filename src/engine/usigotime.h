#ifndef USIGOTIME_H
#define USIGOTIME_H

/// @file usigotime.h
/// @brief USI の go コマンドに載せる時間を求める純粋関数

#include <QString>

#include "shogitypes.h"
#include "timecontrol.h"

/// go コマンドに載せる時間（ミリ秒）
struct UsiGoTime {
    qint64 btime = 0;
    qint64 wtime = 0;
    qint64 byoyomi = 0;
    qint64 binc = 0;
    qint64 winc = 0;
    bool useByoyomi = false;  ///< byoyomi を送るか（false なら binc/winc を送る）

    /// "go [ponder ]btime .. wtime .. byoyomi .." または "... binc .. winc .." を返す
    QString toCommand(bool ponder) const;
};

/// go コマンド（探索の種類と時間）
struct UsiGo {
    enum class Kind {
        Clock,     ///< 対局の手番の探索（go btime ... wtime ...）
        Ponder,    ///< 先読み（go ponder btime ...）
        Infinite,  ///< stop まで探索する（検討・棋譜解析）
        Mate,      ///< 詰み探索（go mate <ms> / go mate infinite）
    };
    Kind kind = Kind::Infinite;
    UsiGoTime time;       ///< Clock・Ponder の持ち時間
    int mateTimeMs = 0;   ///< Mate の制限時間（0 以下は infinite）

    static UsiGo clock(const UsiGoTime& t) { return {Kind::Clock, t, 0}; }
    static UsiGo ponder(const UsiGoTime& t) { return {Kind::Ponder, t, 0}; }
    static UsiGo infinite() { return {Kind::Infinite, {}, 0}; }
    static UsiGo mate(int timeMs) { return {Kind::Mate, {}, timeMs}; }

    /// 送る go コマンド
    QString command() const;
};

/**
 * @brief go の時間を求める入力
 *
 * btime/wtime は USI（将棋所の仕様）の意味で渡す。すなわち、その側の次の手番（手番側は今の手番）で
 * 加わる加算を含まない残り持ち時間で、btime + binc がその手に使える持ち時間になる。
 * 時計の残り時間に加算を先に足す方式（ローカル対局）か、手番の始めに足す方式（CSA）かは、
 * 呼び出し側で換算してから渡す。
 */
struct UsiGoTimeInput {
    TimeControl timeControl;   ///< 秒読み・加算の決まり（持ち時間は使わない）
    qint64 btimeMs = 0;        ///< 先手の残り持ち時間（USI の btime の意味）
    qint64 wtimeMs = 0;        ///< 後手の残り持ち時間（USI の wtime の意味）
    Turn sideToMove = Turn::Black;
    qint64 moverByoyomiMs = -1;  ///< 手番側の秒読みの残り（-1 なら決まりの秒読みをそのまま使う）
    qint64 extraMs = 0;        ///< 今の手だけに足す時間（CSA の遅延時間）
    qint64 marginMs = 0;       ///< 通信の遅延に備えて差し引く時間
};

namespace UsiGoTimeCalculator {

/// GUI の期限より先に返せるよう、毎手差し引く余裕の上限（予算の半分を超えない）
inline constexpr qint64 kEngineReserveMs = 250;

/**
 * @brief 手番側がこの手に使える時間から、go に載せる値を求める
 *
 * 予算（持ち時間＋加算＋秒読み＋extraMs）から marginMs と kEngineReserveMs を秒読み側から先に差し引く。
 * 加算方式では予算のうち加算分を binc/winc で伝え、btime + binc が予算を超えないようにする。
 * 短い設定でも最低限の思考予算を残す。
 */
UsiGoTime compute(const UsiGoTimeInput& input);

/// 手番側がこの手に使える時間の合計（差し引く前の予算。応答待ちの期限に使う）
qint64 budgetMs(const UsiGoTimeInput& input);

} // namespace UsiGoTimeCalculator

#endif // USIGOTIME_H
