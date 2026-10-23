/// @file timecontroltext.cpp
/// @brief 対局情報の持ち時間の書式の実装

#include "timecontroltext.h"

#include "gameinfokeys.h"

#include <QRegularExpression>

namespace TimeControlText {

QString format(const TimeControlSide& side)
{
    const qint64 mainSec = side.mainMs / 1000;
    QString text = QStringLiteral("%1:%2")
        .arg(mainSec / 60, 2, 10, QLatin1Char('0'))
        .arg(mainSec % 60, 2, 10, QLatin1Char('0'));
    if (side.byoyomiMs >= 1000) {
        text += QStringLiteral("+%1").arg(side.byoyomiMs / 1000);
    } else if (side.incrementMs >= 1000) {
        text += QStringLiteral("+%1秒加算").arg(side.incrementMs / 1000);
    }
    return text;
}

QString format(const TimeControl& timeControl, bool handicapNames)
{
    QString blackText = format(timeControl.black);
    const QString whiteText = format(timeControl.white);
    if (blackText == whiteText) return blackText;
    return QStringLiteral("%1 %2 / %3 %4")
        .arg(handicapNames ? GameInfoKeys::kShitatePlayer : GameInfoKeys::kBlackPlayer, blackText,
             handicapNames ? GameInfoKeys::kUwatePlayer : GameInfoKeys::kWhitePlayer, whiteText);
}

std::optional<TimeControlSide> parseSide(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("^(\\d+):(\\d{2})(?:\\+(\\d+)(秒加算)?)?$"));
    const QRegularExpressionMatch m = re.match(text.trimmed());
    if (!m.hasMatch() || m.captured(2).toInt() >= 60) return std::nullopt;
    TimeControlSide side;
    side.mainMs = (m.captured(1).toLongLong() * 60 + m.captured(2).toLongLong()) * 1000;
    if (!m.captured(3).isEmpty()) {
        const qint64 extraMs = m.captured(3).toLongLong() * 1000;
        if (m.captured(4).isEmpty()) side.byoyomiMs = extraMs;
        else side.incrementMs = extraMs;
    }
    return side;
}

std::optional<Parsed> parse(const QString& text)
{
    const QString trimmed = text.trimmed();
    static const QRegularExpression perSide(
        QStringLiteral("^(先手|下手) (.+) / (後手|上手) (.+)$"));
    if (const QRegularExpressionMatch m = perSide.match(trimmed); m.hasMatch()) {
        const std::optional<TimeControlSide> black = parseSide(m.captured(2));
        const std::optional<TimeControlSide> white = parseSide(m.captured(4));
        if (!black || !white) return std::nullopt;
        return Parsed{{*black, *white}, true, m.captured(1) == GameInfoKeys::kShitatePlayer};
    }
    const std::optional<TimeControlSide> both = parseSide(trimmed);
    if (!both) return std::nullopt;
    return Parsed{{*both, *both}, false, false};
}

} // namespace TimeControlText
