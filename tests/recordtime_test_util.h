#ifndef RECORDTIME_TEST_UTIL_H
#define RECORDTIME_TEST_UTIL_H

/// @file recordtime_test_util.h
/// @brief テストで棋譜の時間を表記（"0:01/00:00:01"・"(0:01/00:00:01)"）から作る補助
///
/// アプリの時間の表記の読み取りは棋譜の取り込み境界（KIF の KifLexer など）だけにあり、RecordTime は読み方を持たない。
/// テストで手を足すときに表記で時間を書けるよう、ここに置く。数として読めない表記は表記のまま残す（valid は false）。

#include <QString>
#include <limits>

#include "recordtime.h"

inline RecordTime recordTimeFromText(const QString& text)
{
    RecordTime result;
    result.importedText = text;
    QString clean = text.trimmed();
    if (clean.startsWith(QLatin1Char('(')) && clean.endsWith(QLatin1Char(')')))
        clean = clean.mid(1, clean.size() - 2).trimmed();
    const auto parts = clean.split(QLatin1Char('/'));
    if (parts.size() != 2) return result;
    auto parse = [](const QString& part, int fields, qint64& value) {
        const auto components = part.trimmed().split(QLatin1Char(':'));
        if (components.size() != fields) return false;
        qint64 seconds = 0;
        for (int i = 0; i < fields; ++i) {
            QString component = components.at(i).trimmed();
            QString fraction;
            const auto dot = component.indexOf(QLatin1Char('.'));
            if (dot >= 0) {
                if (i != fields - 1) return false;
                fraction = component.mid(dot + 1);
                component.truncate(dot);
                if (fraction.isEmpty()) return false;
                for (QChar c : std::as_const(fraction)) if (c < QLatin1Char('0') || c > QLatin1Char('9')) return false;
            }
            if (component.isEmpty()) return false;
            for (QChar c : std::as_const(component)) if (c < QLatin1Char('0') || c > QLatin1Char('9')) return false;
            bool ok = false;
            const qint64 number = component.toLongLong(&ok);
            if (!ok || number > std::numeric_limits<qint64>::max() / 1000 || (i > 0 && number >= 60)
                || seconds > (std::numeric_limits<qint64>::max() / 1000 - number) / 60) return false;
            seconds = seconds * 60 + number;
            if (i == fields - 1) {
                const qint64 millis = fraction.isEmpty() ? 0 : fraction.left(3).leftJustified(3, QLatin1Char('0')).toInt();
                if (seconds > (std::numeric_limits<qint64>::max() - millis) / 1000) return false;
                value = seconds * 1000 + millis;
            }
        }
        return true;
    };
    result.valid = parse(parts.at(0), 2, result.elapsedMs) && parse(parts.at(1), 3, result.totalMs);
    return result;
}

#endif // RECORDTIME_TEST_UTIL_H
