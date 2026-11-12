#ifndef P2PMATCHTIMER_H
#define P2PMATCHTIMER_H

/// @file p2pmatchtimer.h
/// @brief P2P対局の計時（自分の手番の消費時間・相手の手番の観測時間・切断の時間。実装仕様 7.1節・7.3節）

#include <QtGlobal>

namespace P2p {

/**
 * @brief 1局の計時の記録（単調増加時計のミリ秒を引数で受ける値。規則は持たない）
 *
 * 自分の消費時間は、自分の手番が始まった時点から数える。相手の手番は、相手の着手が届くまでの時間を観測する
 * （後手の初手は READY を送った時点、以後は自分の提案を最初に送った時点から）。回線が切れていた時間は別に数え、
 * 観測を始めてから切れていた時間を、相手の自己申告と比べるときの許容に足す。時間の比べ方は対局の状態機械
 * （Match）が決める。
 */
class MatchTimer
{
public:
    /// 自分の手番が始まった
    void startMyTurn(qint64 now) { m_myTurnStart = now; }
    /// 自分の手番の消費時間（負にしない）
    qint64 mySpentMs(qint64 now) const { return qMax<qint64>(0, now - m_myTurnStart); }

    /// 相手の手番の観測を始める
    void startObserving(qint64 now)
    {
        m_observeStart = now;
        m_disconnectedAtObserveStart = disconnectedTotalMs(now);
    }
    /// 観測を始めてからの時間（切れていた時間を含む）
    qint64 observedMs(qint64 now) const { return now - m_observeStart; }
    /// 観測を始めてから回線が切れていた時間
    qint64 disconnectedSinceObservingMs(qint64 now) const { return disconnectedTotalMs(now) - m_disconnectedAtObserveStart; }

    /// 回線が切れた（切れている間に呼んでも始まりは変えない）
    void disconnected(qint64 now)
    {
        if (m_disconnectStart < 0) m_disconnectStart = now;
    }
    /// 回線を復旧した（切れていた時間を合計に足す）
    void reconnected(qint64 now)
    {
        if (m_disconnectStart < 0) return;
        m_disconnectedTotal += now - m_disconnectStart;
        m_disconnectStart = -1;
    }
    /// 今の切断の時間（切れていなければ -1）
    qint64 currentDisconnectMs(qint64 now) const { return m_disconnectStart >= 0 ? now - m_disconnectStart : -1; }
    /// 1局で回線が切れていた時間の合計（今の切断を含む）
    qint64 disconnectedTotalMs(qint64 now) const
    {
        return m_disconnectedTotal + (m_disconnectStart >= 0 ? now - m_disconnectStart : 0);
    }

private:
    qint64 m_myTurnStart = 0;
    qint64 m_observeStart = 0;
    qint64 m_disconnectedAtObserveStart = 0;
    qint64 m_disconnectedTotal = 0;    ///< 終わった切断の時間の合計
    qint64 m_disconnectStart = -1;     ///< 今の切断の始まり（切れていなければ -1）
};

} // namespace P2p

#endif // P2PMATCHTIMER_H
