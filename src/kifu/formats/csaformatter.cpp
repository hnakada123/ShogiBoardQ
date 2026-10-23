/// @file csaformatter.cpp
/// @brief CSA形式の文字列生成ヘルパ関数の実装

#include "csaformatter.h"
#include "timecontroltext.h"
#include "gameinfokeys.h"
#include "kifuexportcommon.h"

#include <QRegularExpression>

// ========================================
// CsaFormatter namespace
// ========================================

namespace CsaFormatter {

QString timeToken(const RecordTime& time)
{
    if (!time.valid) return QStringLiteral("T0");
    const qint64 seconds = time.elapsedMs / 1000;
    const qint64 millis = time.elapsedMs % 1000;
    if (millis == 0) return QStringLiteral("T%1").arg(seconds);
    return QStringLiteral("T%1.%2").arg(seconds).arg(millis, 3, 10, QLatin1Char('0'));
}

QString convertToCsaDateTime(const QString& dateTimeStr)
{
    if (dateTimeStr.contains(QLatin1Char('/')) &&
        (dateTimeStr.length() == 10 || dateTimeStr.length() == 19)) {
        return dateTimeStr;
    }

    // KIF形式1: "2024年05月05日（月）15時05分40秒"
    static const QRegularExpression reKif1(
        QStringLiteral("(\\d{4})年(\\d{1,2})月(\\d{1,2})日[^\\d]*(\\d{1,2})時(\\d{1,2})分(\\d{1,2})秒"));
    QRegularExpressionMatch m1 = reKif1.match(dateTimeStr);
    if (m1.hasMatch()) {
        return QStringLiteral("%1/%2/%3 %4:%5:%6")
            .arg(m1.captured(1),
                 m1.captured(2).rightJustified(2, QLatin1Char('0')),
                 m1.captured(3).rightJustified(2, QLatin1Char('0')),
                 m1.captured(4).rightJustified(2, QLatin1Char('0')),
                 m1.captured(5).rightJustified(2, QLatin1Char('0')),
                 m1.captured(6).rightJustified(2, QLatin1Char('0')));
    }

    // KIF形式2: "2024年05月05日" (時刻なし)
    static const QRegularExpression reKif2(
        QStringLiteral("(\\d{4})年(\\d{1,2})月(\\d{1,2})日"));
    QRegularExpressionMatch m2 = reKif2.match(dateTimeStr);
    if (m2.hasMatch()) {
        return QStringLiteral("%1/%2/%3")
            .arg(m2.captured(1),
                 m2.captured(2).rightJustified(2, QLatin1Char('0')),
                 m2.captured(3).rightJustified(2, QLatin1Char('0')));
    }

    // ISO形式: "2024-05-05T15:05:40" や "2024-05-05 15:05:40"
    static const QRegularExpression reIso(
        QStringLiteral("(\\d{4})-(\\d{2})-(\\d{2})[T ](\\d{2}):(\\d{2}):(\\d{2})"));
    QRegularExpressionMatch m3 = reIso.match(dateTimeStr);
    if (m3.hasMatch()) {
        return QStringLiteral("%1/%2/%3 %4:%5:%6")
            .arg(m3.captured(1), m3.captured(2), m3.captured(3),
                 m3.captured(4), m3.captured(5), m3.captured(6));
    }

    // ISO形式（日付のみ）: "2024-05-05"
    static const QRegularExpression reIsoDate(
        QStringLiteral("(\\d{4})-(\\d{2})-(\\d{2})"));
    QRegularExpressionMatch m4 = reIsoDate.match(dateTimeStr);
    if (m4.hasMatch()) {
        return QStringLiteral("%1/%2/%3")
            .arg(m4.captured(1), m4.captured(2), m4.captured(3));
    }

    return dateTimeStr;
}

namespace {

/// 持ち時間の決まりを CSA の $TIME の値（"秒+秒読み+加算"）にする
QString csaTimeValue(const TimeControlSide& side)
{
    return QStringLiteral("%1+%2+%3").arg(side.mainMs / 1000).arg(side.byoyomiMs / 1000).arg(side.incrementMs / 1000);
}

} // namespace

QString convertToCsaTime(const QString& timeStr)
{
    // このアプリの書式（"mm:ss"・"mm:ss+秒読み"・"mm:ss+加算秒秒加算"）は TimeControlText で読む
    if (const std::optional<TimeControlSide> side = TimeControlText::parseSide(timeStr))
        return QStringLiteral("$TIME:") + csaTimeValue(*side);

    // 既にV3.0形式: "秒+秒読み+フィッシャー"
    static const QRegularExpression reV30(
        QStringLiteral("^(\\d+(?:\\.\\d+)?)\\+(\\d+(?:\\.\\d+)?)\\+(\\d+(?:\\.\\d+)?)$"));
    if (reV30.match(timeStr).hasMatch()) return QStringLiteral("$TIME:%1").arg(timeStr);

    // ほかのソフトの書式: "○分" や "○分○秒"
    static const QRegularExpression reMinSec(QStringLiteral("(\\d+)分(?:(\\d+)秒)?"));
    if (const QRegularExpressionMatch m = reMinSec.match(timeStr); m.hasMatch()) {
        const int minutes = m.captured(1).toInt();
        const int seconds = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
        return QStringLiteral("$TIME:%1+0+0").arg(minutes * 60 + seconds);
    }

    // 秒読みだけ: "○秒"
    static const QRegularExpression reSec(QStringLiteral("(\\d+)秒"));
    if (const QRegularExpressionMatch m = reSec.match(timeStr); m.hasMatch())
        return QStringLiteral("$TIME:0+%1+0").arg(m.captured(1).toInt());

    return QStringLiteral("$TIME_LIMIT:%1").arg(timeStr);
}

QStringList convertToCsaTimeLines(const QString& timeStr)
{
    // 先後で違う持ち時間（"先手 mm:ss+秒 / 後手 mm:ss+秒"、駒落ちは下手・上手）は CSA V3.0 の $TIME+ と $TIME- に分ける
    if (const std::optional<TimeControlText::Parsed> parsed = TimeControlText::parse(timeStr); parsed && parsed->perSide) {
        return {QStringLiteral("$TIME+:") + csaTimeValue(parsed->timeControl.black),
                QStringLiteral("$TIME-:") + csaTimeValue(parsed->timeControl.white)};
    }
    return {convertToCsaTime(timeStr)};
}

namespace {

struct CsaInfoKey {
    const char* csa;
    const QString& gameInfo;
};

// 読むときは語の最初の行、書くときは項目名の最初の行を使う（後ろの行は別名）
const CsaInfoKey kInfoKeys[] = {
    {"EVENT", GameInfoKeys::kEvent},
    {"SITE", GameInfoKeys::kSite},
    {"START_TIME", GameInfoKeys::kStartDateTime},
    {"END_TIME", GameInfoKeys::kEndDateTime},
    {"TIME_LIMIT", GameInfoKeys::kTimeControl},
    {"TIME", GameInfoKeys::kCsaTime},
    {"TIME+", GameInfoKeys::kCsaBlackTime},
    {"TIME-", GameInfoKeys::kCsaWhiteTime},
    {"MAX_MOVES", GameInfoKeys::kMaxMoves},
    {"SMAX_MOVES", GameInfoKeys::kMaxMoves},
    {"JISHOGI", GameInfoKeys::kJishogiPoints},
    {"JISHOGI_POINTS", GameInfoKeys::kJishogiPoints},
    {"JISHOGI_POINT", GameInfoKeys::kJishogiPoints},
    {"JISHOGI", GameInfoKeys::kJishogi},
    {"OPENING", GameInfoKeys::kOpening},
    {"NOTE", GameInfoKeys::kNote},
};

} // namespace

QString gameInfoKeyFromCsa(const QString& csaKey)
{
    for (const auto& entry : kInfoKeys) {
        if (csaKey == QLatin1String(entry.csa)) return entry.gameInfo;
    }
    return {};
}

QString csaKeyFromGameInfo(const QString& gameInfoKey)
{
    for (const auto& entry : kInfoKeys) {
        if (gameInfoKey == entry.gameInfo) return QString::fromLatin1(entry.csa);
    }
    return {};
}

QString escapeNote(const QString& note)
{
    QString value = note;
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    return value;
}

QString unescapeNote(const QString& value)
{
    QString decoded;
    for (qsizetype i = 0; i < value.size(); ++i) {
        if (value.at(i) == QLatin1Char('\\') && i + 1 < value.size()) {
            if (value.at(i + 1) == QLatin1Char('n')) { decoded += QLatin1Char('\n'); ++i; continue; }
            if (value.at(i + 1) == QLatin1Char('\\')) { decoded += QLatin1Char('\\'); ++i; continue; }
        }
        decoded += value.at(i);
    }
    return decoded;
}

} // namespace CsaFormatter
