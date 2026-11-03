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

QString CsaGameSummary::unsupportedReason(bool playedByEngine) const
{
    // Format は Position の書式と指し手表記を決める。将棋以外は解釈できない
    if (!format.isEmpty() && !format.startsWith(QStringLiteral("Shogi"))) {
        return tr("未対応の Format です: %1").arg(format);
    }
    // 時間無制限ではエンジンに思考時間を与えられない（ローカル対局と同じ方針）
    if (playedByEngine && isTimeUnlimited()) {
        return tr("時間無制限の対局はエンジンでは行えません");
    }
    return QString();
}

QList<CsaGameSummary::ConditionLine> CsaGameSummary::conditionLines() const
{
    QList<ConditionLine> lines;
    if (isTimeUnlimited()) {
        lines.append({tr("持時間: 無制限")});
    } else {
        for (const bool black : {true, false}) {
            if (!black && !hasIndividualTime) break;
            const Time& t = timeFor(black);
            QString text = tr("持時間: %1秒, 秒読み: %2秒, 加算: %3秒")
                               .arg(t.totalTimeMs() / 1000)
                               .arg(t.byoyomiMs() / 1000)
                               .arg(t.incrementMs() / 1000);
            if (t.delay > 0) text += tr(", 遅延: %1秒").arg(t.delayMs() / 1000);
            if (t.leastTimePerMove > 0) text += tr(", 最少消費: %1秒").arg(t.leastTimePerMoveMs() / 1000);
            if (hasIndividualTime) text = (black ? tr("先手の") : tr("後手の")) + text;
            lines.append({text});
        }
    }
    if (maxMoves > 0) lines.append({tr("最大手数: %1手").arg(maxMoves)});
    lines.append({allowsJishogiDeclaration() ? tr("入玉宣言ルール: %1").arg(declaration) : tr("入玉宣言ルール: なし")});
    if (!protocolVersion.isEmpty() && !protocolVersion.startsWith(QStringLiteral("1."))) {
        lines.append({tr("未知のプロトコルバージョンです: %1").arg(protocolVersion), true});
    }
    if (protocolMode != QStringLiteral("Server")) {
        lines.append({tr("Protocol_Mode が Server ではありません: %1").arg(protocolMode), true});
    }
    return lines;
}
