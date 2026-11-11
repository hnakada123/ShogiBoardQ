#ifndef RECORDTIME_H
#define RECORDTIME_H

#include <QString>
#include <QStringList>
#include <limits>
#include <utility>

/// 棋譜の一手の消費時間と累計。文字列の解析は棋譜の取り込み境界（KIF の KifLexer など）でだけ行う。
struct RecordTime {
    qint64 elapsedMs = 0;
    qint64 totalMs = 0;
    bool valid = false;
    QString importedText; ///< 未知の表記を含め、取り込んだ表記は保存時まで保持する

    static RecordTime fromMilliseconds(qint64 elapsed, qint64 total)
    {
        return {qMax<qint64>(0, elapsed), qMax<qint64>(0, total), true, {}};
    }

    /// 対局で指した手の時間を、棋譜に秒の単位で書く値にする（ローカル対局の時計と P2P対局で共通）。
    /// 累計はミリ秒を秒に切り捨て、1手の秒数はその手の前と後の累計の秒の差にする（1手の秒数を足すと累計に合う）。
    /// 棋譜欄は「00:45/00:00:45」のように秒までで出し、ミリ秒の端数で幅が変わらない
    static RecordTime wholeSeconds(qint64 totalBeforeMs, qint64 totalMs)
    {
        const qint64 totalSec = qMax<qint64>(0, totalMs) / 1000;
        const qint64 beforeSec = qMax<qint64>(0, totalBeforeMs) / 1000;
        return fromMilliseconds(qMax<qint64>(0, totalSec - beforeSec) * 1000, totalSec * 1000);
    }

    /// 消費時間が書いてあるか。消費時間のない形式（KI2・USI・USEN など）を読んだ手と、消費時間を書いていない手には
    /// 0 秒の時間が入るので、0 秒だけなら書いてないとみなす（読めない表記は取り込んだ文があれば書いてある）
    bool isRecorded() const
    {
        return valid ? (elapsedMs > 0 || totalMs > 0) : !importedText.trimmed().isEmpty();
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
