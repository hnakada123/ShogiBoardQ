/// @file matchtimerecord.cpp
/// @brief 対局の持ち時間と開始・終了の日時の記録

#include "matchtimerecord.h"

void MatchTimeRecord::begin(const TimeControl& timeControl)
{
    m_timeControl = timeControl;
    m_startDateTime = QDateTime::currentDateTime();
    m_endDateTime = QDateTime();
}

void MatchTimeRecord::recordEnd()
{
    if (!m_endDateTime.isValid()) m_endDateTime = QDateTime::currentDateTime();
}
