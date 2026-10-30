/// @file csagamesummary.cpp
/// @brief CSA の対局条件（Game_Summary）の値の実装

#include "csagamesummary.h"
#include "logcategories.h"

#include <QtGlobal>

namespace {
constexpr int kMsPerSecond = 1000;
constexpr int kMsPerMinute = 60000;
} // namespace

int CsaGameSummary::Time::unitMs() const
{
    if (timeUnit == QStringLiteral("1msec") || timeUnit == QStringLiteral("msec")) {
        return 1;
    } else if (timeUnit == QStringLiteral("1min") || timeUnit == QStringLiteral("min")) {
        return kMsPerMinute;
    }
    // デフォルトは秒
    return kMsPerSecond;
}

int CsaGameSummary::Time::toMs(qint64 units) const
{
    return static_cast<int>(qBound(qint64(0), units * unitMs(), qint64(kMaxTimeMs)));
}

TimeControl CsaGameSummary::timeControl() const
{
    TimeControl tc;
    if (isTimeUnlimited()) return tc;
    for (const bool black : {true, false}) {
        const Time& t = timeFor(black);
        TimeControlSide& side = black ? tc.black : tc.white;
        side.mainMs = t.totalTimeMs();
        side.byoyomiMs = t.byoyomiMs();
        side.incrementMs = t.incrementMs();
        side.delayMs = t.delayMs();
    }
    return tc;
}

int CsaGameSummary::positionMoveConsumedMs(int index) const
{
    if (index < 0 || index >= moves.size()) return 0;
    const bool black = moves.at(index).startsWith(QLatin1Char('+'));
    return unitsToMs(black, moveConsumedTimes.value(index, 0));
}

int CsaGameSummary::consumedUnits(const QString& line)
{
    const qsizetype commaPos = line.indexOf(QLatin1Char(','));
    if (commaPos <= 0) return 0;
    const QStringView rest = QStringView(line).mid(commaPos + 1);
    if (!rest.startsWith(QLatin1Char('T'))) return 0;
    bool ok = false;
    const int value = rest.mid(1).toInt(&ok);
    if (!ok || value < 0) {
        qCWarning(lcNetwork) << "Invalid consumed time token:" << rest;
        return 0;
    }
    return value;
}
