/// @file csaformatter.cpp
/// @brief CSA形式の文字列生成ヘルパ関数の実装

#include "csaformatter.h"

#include <QRegularExpression>

// ========================================
// CsaFormatter namespace
// ========================================

namespace CsaFormatter {

QString removeTurnMarker(const QString& move)
{
    QString result = move;
    if (result.startsWith(QStringLiteral("▲")) || result.startsWith(QStringLiteral("△"))) {
        result = result.mid(1);
    }
    return result;
}

bool isTerminalMove(const QString& move)
{
    return !csaResultCode(move).isEmpty()
        || move.contains(QStringLiteral("不戦勝")) || move.contains(QStringLiteral("不戦敗"));
}

QString extractCsaTimeToken(const QString& timeText)
{
    QString text = timeText.trimmed();

    if (text.startsWith(QLatin1Char('('))) text = text.mid(1);
    if (text.endsWith(QLatin1Char(')'))) text.chop(1);

    const qsizetype slashIdx = text.indexOf(QLatin1Char('/'));
    if (slashIdx < 0) return QStringLiteral("T0");

    const QString moveTime = text.left(slashIdx).trimmed();
    const QStringList parts = moveTime.split(QLatin1Char(':'));
    if (parts.size() != 2) return QStringLiteral("T0");

    bool ok1, ok2;
    const qint64 minutes = parts[0].toLongLong(&ok1);
    const int seconds = parts[1].section(QLatin1Char('.'), 0, 0).toInt(&ok2);
    if (!ok1 || !ok2) return QStringLiteral("T0");
    const qsizetype dot = parts[1].indexOf(QLatin1Char('.'));
    const QString fraction = dot < 0 ? QString() : parts[1].mid(dot);
    return QStringLiteral("T%1%2").arg(minutes * 60 + seconds).arg(fraction);
}

QString csaResultCode(const QString& terminalMove)
{
    const QString move = removeTurnMarker(terminalMove);

    if (move.contains(QStringLiteral("投了"))) return QStringLiteral("%TORYO");
    if (move.contains(QStringLiteral("中断"))) return QStringLiteral("%CHUDAN");
    if (move.contains(QStringLiteral("千日手"))) return QStringLiteral("%SENNICHITE");
    if (move.contains(QStringLiteral("持将棋"))) return QStringLiteral("%JISHOGI");
    if (move.contains(QStringLiteral("切れ負け")) || move.contains(QStringLiteral("時間切れ")))
        return QStringLiteral("%TIME_UP");
    if (move.contains(QStringLiteral("反則勝ち")))
        return terminalMove.startsWith(QStringLiteral("▲"))
            ? QStringLiteral("%-ILLEGAL_ACTION") : QStringLiteral("%+ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("反則負け"))) return QStringLiteral("%ILLEGAL_MOVE");
    if (move.contains(QStringLiteral("先手の反則"))) return QStringLiteral("%+ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("後手の反則"))) return QStringLiteral("%-ILLEGAL_ACTION");
    if (move.contains(QStringLiteral("反則"))) return QStringLiteral("%ILLEGAL_MOVE");
    if (move.contains(QStringLiteral("入玉勝ち")) || move.contains(QStringLiteral("宣言勝ち")))
        return QStringLiteral("%KACHI");
    if (move.contains(QStringLiteral("引き分け"))) return QStringLiteral("%HIKIWAKE");
    if (move.contains(QStringLiteral("最大手数"))) return QStringLiteral("%MAX_MOVES");
    if (move.contains(QStringLiteral("詰み"))) return QStringLiteral("%TSUMI");
    if (move.contains(QStringLiteral("不詰"))) return QStringLiteral("%FUZUMI");
    if (move.contains(QStringLiteral("エラー"))) return QStringLiteral("%ERROR");

    return QString();
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

QString convertToCsaTime(const QString& timeStr)
{
    // 対局情報の持ち時間（フィッシャー加算）: "mm:ss+加算秒秒加算" → "$TIME:秒+0+加算"
    static const QRegularExpression reIncrement(
        QStringLiteral("^(\\d+):(\\d{2})\\+(\\d+)秒加算$"));
    const QRegularExpressionMatch inc = reIncrement.match(timeStr);
    if (inc.hasMatch()) {
        const int totalSeconds = inc.captured(1).toInt() * 60 + inc.captured(2).toInt();
        return QStringLiteral("$TIME:%1+0+%2").arg(totalSeconds).arg(inc.captured(3).toInt());
    }

    // 対局情報の持ち時間（切れ負け）: "mm:ss" → "$TIME:秒+0+0"
    static const QRegularExpression reSuddenDeath(QStringLiteral("^(\\d+):(\\d{2})$"));
    const QRegularExpressionMatch sd = reSuddenDeath.match(timeStr);
    if (sd.hasMatch()) {
        const int totalSeconds = sd.captured(1).toInt() * 60 + sd.captured(2).toInt();
        return QStringLiteral("$TIME:%1+0+0").arg(totalSeconds);
    }

    // 対局情報の持ち時間: "mm:ss+秒読み" → "$TIME:秒+秒読み+0"
    static const QRegularExpression reV22(
        QStringLiteral("(\\d+):(\\d{2})\\+(\\d+)"));
    QRegularExpressionMatch m = reV22.match(timeStr);
    if (m.hasMatch()) {
        int minutes = m.captured(1).toInt();
        int seconds = m.captured(2).toInt();
        int byoyomi = m.captured(3).toInt();
        int totalSeconds = minutes * 60 + seconds;
        return QStringLiteral("$TIME:%1+%2+0").arg(totalSeconds).arg(byoyomi);
    }

    // 既にV3.0形式: "秒+秒読み+フィッシャー"
    static const QRegularExpression reV30(
        QStringLiteral("^(\\d+(?:\\.\\d+)?)\\+(\\d+(?:\\.\\d+)?)\\+(\\d+(?:\\.\\d+)?)$"));
    QRegularExpressionMatch m2 = reV30.match(timeStr);
    if (m2.hasMatch()) {
        return QStringLiteral("$TIME:%1").arg(timeStr);
    }

    // "○分" や "○秒" 形式
    static const QRegularExpression reMinSec(
        QStringLiteral("(\\d+)分(?:(\\d+)秒)?"));
    QRegularExpressionMatch m3 = reMinSec.match(timeStr);
    if (m3.hasMatch()) {
        int minutes = m3.captured(1).toInt();
        int seconds = m3.captured(2).isEmpty() ? 0 : m3.captured(2).toInt();
        int totalSeconds = minutes * 60 + seconds;
        return QStringLiteral("$TIME:%1+0+0").arg(totalSeconds);
    }

    // 秒読み形式: "○秒"
    static const QRegularExpression reSec(QStringLiteral("(\\d+)秒"));
    QRegularExpressionMatch m4 = reSec.match(timeStr);
    if (m4.hasMatch()) {
        int seconds = m4.captured(1).toInt();
        return QStringLiteral("$TIME:0+%1+0").arg(seconds);
    }

    return QStringLiteral("$TIME_LIMIT:%1").arg(timeStr);
}

QStringList convertToCsaTimeLines(const QString& timeStr)
{
    // 先後で違う持ち時間（KifuExportMetadataBuilder::timeControlText の
    // "先手 mm:ss+秒 / 後手 mm:ss+秒"、駒落ちは下手・上手）は CSA V3.0 の $TIME+ と $TIME- に分ける
    static const QRegularExpression rePerSide(
        QStringLiteral("^(?:先手|下手) (.+) / (?:後手|上手) (.+)$"));
    const QRegularExpressionMatch m = rePerSide.match(timeStr.trimmed());
    if (m.hasMatch()) {
        static const QString prefix = QStringLiteral("$TIME:");
        const QString black = convertToCsaTime(m.captured(1));
        const QString white = convertToCsaTime(m.captured(2));
        if (black.startsWith(prefix) && white.startsWith(prefix)) {
            return { QStringLiteral("$TIME+:") + black.mid(prefix.size()),
                     QStringLiteral("$TIME-:") + white.mid(prefix.size()) };
        }
    }
    return { convertToCsaTime(timeStr) };
}

} // namespace CsaFormatter
