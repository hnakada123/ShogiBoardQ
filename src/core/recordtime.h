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
