#ifndef MATCHTIMERECORD_H
#define MATCHTIMERECORD_H

/// @file matchtimerecord.h
/// @brief 対局の持ち時間と開始・終了の日時（対局情報と棋譜に書く記録）

#include <QDateTime>

#include "timecontrol.h"

/**
 * @brief 最後に始めた対局の持ち時間と開始・終了の日時
 *
 * 時計（ShogiClock）とは別の、対局情報と棋譜の書き出しに使う記録。ローカル対局と通信対局の両方が書く。
 */
class MatchTimeRecord
{
public:
    /// 対局を始めた（持ち時間を記録し、開始日時を今にして終了日時を消す）
    void begin(const TimeControl& timeControl);
    /// 終局した（終了日時がまだ無ければ今にする）
    void recordEnd();
    /// 終了日時を消す（中断した対局を再開した・新規にした）
    void clearEnd() { m_endDateTime = QDateTime(); }
    /// 記録をすべて消す（別の棋譜を読み込んだ。前の対局の持ち時間・日時を書き出しに使わない）
    void clear() { *this = MatchTimeRecord(); }

    const TimeControl& timeControl() const { return m_timeControl; }
    bool hasTimeControl() const { return !m_timeControl.isUnlimited(); }
    QDateTime startDateTime() const { return m_startDateTime; }
    QDateTime endDateTime() const { return m_endDateTime; }

private:
    TimeControl m_timeControl;
    QDateTime m_startDateTime;
    QDateTime m_endDateTime;
};

#endif // MATCHTIMERECORD_H
