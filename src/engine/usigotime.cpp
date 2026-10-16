/// @file usigotime.cpp
/// @brief USI の go コマンドに載せる時間を求める純粋関数の実装

#include "usigotime.h"

#include <QtGlobal>

QString UsiGoTime::toCommand(bool ponder) const
{
    QString command = ponder ? QStringLiteral("go ponder") : QStringLiteral("go");
    command += QStringLiteral(" btime %1 wtime %2").arg(btime).arg(wtime);
    if (useByoyomi) {
        command += QStringLiteral(" byoyomi %1").arg(byoyomi);
    } else {
        command += QStringLiteral(" binc %1 winc %2").arg(binc).arg(winc);
    }
    return command;
}

QString UsiGo::command() const
{
    switch (kind) {
    case Kind::Clock: return time.toCommand(false);
    case Kind::Ponder: return time.toCommand(true);
    case Kind::Infinite: return QStringLiteral("go infinite");
    case Kind::Mate:
        return mateTimeMs > 0 ? QStringLiteral("go mate %1").arg(mateTimeMs) : QStringLiteral("go mate infinite");
    }
    return QStringLiteral("go infinite");
}

namespace UsiGoTimeCalculator {
namespace {

struct Budget {
    qint64 main = 0;     ///< 持ち時間（手番側の加算を含む）
    qint64 byoyomi = 0;  ///< 秒読み
};

qint64 incrementOf(const TimeControl& tc, Turn side)
{
    return tc.usesByoyomi() ? 0 : qMax<qint64>(0, tc.side(side).incrementMs);
}

Budget moverBudget(const UsiGoTimeInput& input, const TimeControl& tc)
{
    const Turn mover = input.sideToMove;
    Budget b;
    b.main = qMax<qint64>(0, mover == Turn::Black ? input.btimeMs : input.wtimeMs) + incrementOf(tc, mover);
    if (tc.usesByoyomi()) {
        b.byoyomi = input.moverByoyomiMs >= 0 ? input.moverByoyomiMs : qMax<qint64>(0, tc.side(mover).byoyomiMs);
        // 遅延時間は秒読み側に足す（秒読みの対局で手番の始めに使える時間）
        b.byoyomi += qMax<qint64>(0, input.extraMs);
    } else {
        b.main += qMax<qint64>(0, input.extraMs);
    }
    return b;
}

/// 秒読み側から先に amount を差し引く
void take(Budget& b, qint64 amount)
{
    const qint64 fromByoyomi = qMin(b.byoyomi, amount);
    b.byoyomi -= fromByoyomi;
    b.main = qMax<qint64>(0, b.main - (amount - fromByoyomi));
}

} // namespace

qint64 budgetMs(const UsiGoTimeInput& input)
{
    const TimeControl tc = input.timeControl.normalized();
    const Budget b = moverBudget(input, tc);
    return b.main + b.byoyomi;
}

UsiGoTime compute(const UsiGoTimeInput& input)
{
    const TimeControl tc = input.timeControl.normalized();
    const Turn mover = input.sideToMove;

    UsiGoTime result;
    result.useByoyomi = tc.usesByoyomi();
    result.btime = qMax<qint64>(0, input.btimeMs);
    result.wtime = qMax<qint64>(0, input.wtimeMs);
    result.binc = incrementOf(tc, Turn::Black);
    result.winc = incrementOf(tc, Turn::White);

    Budget b = moverBudget(input, tc);
    take(b, qMax<qint64>(0, input.marginMs));
    // 通信・探索停止の余裕。短い設定でも予算の半分は残す
    take(b, qMin(kEngineReserveMs, (b.main + b.byoyomi) / 2));

    if (!result.useByoyomi) {
        // 予算のうち加算分は binc/winc で伝える。余裕を引くと加算分に届かないときは、
        // btime + binc が予算を超えないよう手番側の加算を減らす。
        const qint64 increment = qMin(incrementOf(tc, mover), b.main);
        (mover == Turn::Black ? result.binc : result.winc) = increment;
        b.main -= increment;
    }
    (mover == Turn::Black ? result.btime : result.wtime) = b.main;
    result.byoyomi = b.byoyomi;
    return result;
}

} // namespace UsiGoTimeCalculator
