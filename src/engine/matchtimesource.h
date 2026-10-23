#ifndef MATCHTIMESOURCE_H
#define MATCHTIMESOURCE_H

/// @file matchtimesource.h
/// @brief 対局中のエンジンの思考に使う時間の出どころ（ローカル対局は対局時計、通信対局はサーバーの残り時間）

#include "shogitypes.h"
#include "usigotime.h"

class ShogiClock;

/// エンジンに思考を頼むときの時間の条件と、応答を待つ期限
struct MatchSearchTiming {
    UsiGoTimeInput input;
    int timeoutMs = 30 * 60 * 1000;
    bool expired = false;   ///< 時間切れで、もう思考を頼まない

    /// 時間の条件から作る（応答を待つ期限は、その手に使える時間）
    static MatchSearchTiming fromInput(const UsiGoTimeInput& input);
};

/**
 * @brief 対局中のエンジンが思考するときの時間の出どころ
 *
 * エンジンとの対局のやり取り（UsiMatchHandler）はこの型だけから時間を読む。
 * ローカル対局は対局時計（ClockTimeSource）、通信対局はサーバーが示した残り時間（CsaGameCoordinator）が実装する。
 */
class MatchTimeSource
{
public:
    virtual ~MatchTimeSource() = default;

    /// side が思考を始める（pondering なら先読みする）時点の時間の条件。stop を待った後も呼び直し、期限を延ばさない
    virtual MatchSearchTiming snapshot(Turn side, bool pondering) const = 0;
    /// side の今の手番の考慮時間（手番でなければ 0）
    virtual qint64 elapsedThisTurn(Turn side) const = 0;
    /// エンジンの指し手を受けて手番の時間を精算する。時間切れなら false
    virtual bool finishTurn() { return true; }
};

/// 決まった残り時間を返す出どころ（時計のない対局・テスト）
class FixedTimeSource final : public MatchTimeSource
{
public:
    /// timing の手番（sideToMove）は使わず、思考する側にする
    explicit FixedTimeSource(const UsiGoTimeInput& timing = {}) : m_timing(timing) {}
    void setTiming(const UsiGoTimeInput& timing) { m_timing = timing; }

    MatchSearchTiming snapshot(Turn side, bool pondering) const override;
    qint64 elapsedThisTurn(Turn) const override { return 0; }

private:
    UsiGoTimeInput m_timing;
};

/// ローカル対局の対局時計から読む出どころ
class ClockTimeSource final : public MatchTimeSource
{
public:
    explicit ClockTimeSource(ShogiClock* clock) : m_clock(clock) {}

    MatchSearchTiming snapshot(Turn side, bool pondering) const override;
    qint64 elapsedThisTurn(Turn side) const override;
    bool finishTurn() override;

private:
    ShogiClock* m_clock = nullptr;   ///< 非所有
};

#endif // MATCHTIMESOURCE_H
