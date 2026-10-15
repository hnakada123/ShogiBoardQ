#ifndef RECORDTIME_H
#define RECORDTIME_H

#include <QString>
#include <QStringList>
#include <limits>
#include <utility>

/// 棋譜の一手の消費時間と累計。文字列の解析は棋譜の取り込み境界でだけ行う。
struct RecordTime {
    qint64 elapsedMs = 0;
    qint64 totalMs = 0;
    bool valid = false;
    QString importedText; ///< 未知の表記を含め、取り込んだ表記は保存時まで保持する

    static RecordTime fromMilliseconds(qint64 elapsed, qint64 total)
    {
        return {qMax<qint64>(0, elapsed), qMax<qint64>(0, total), true, {}};
    }

    static RecordTime fromText(const QString& text)
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

    QString toText() const
    {
        if (!importedText.isEmpty() || !valid) return importedText;
        auto pad = [](qint64 n) { return QString::number(n).rightJustified(2, QLatin1Char('0')); };
        auto seconds = [&pad](qint64 ms) {
            QString text = pad(ms / 1000 % 60);
            if (ms % 1000) text += QLatin1Char('.') + QString::number(ms % 1000).rightJustified(3, QLatin1Char('0'));
            return text;
        };
        return pad(elapsedMs / 60000) + QLatin1Char(':') + seconds(elapsedMs) + QLatin1Char('/')
            + pad(totalMs / 3600000) + QLatin1Char(':') + pad(totalMs / 60000 % 60)
            + QLatin1Char(':') + seconds(totalMs);
    }
};

#endif // RECORDTIME_H
