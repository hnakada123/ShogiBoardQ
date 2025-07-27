#ifndef SHOGIUTILS_H
#define SHOGIUTILS_H

#include <QString>

// ShogibanQで使用する共通関数
namespace ShogiUtils {
    // 指した先のマスの段を漢字表記で出力する。
    QString transRankTo(const int rankTo);

    // 指した先のマスの筋を漢字表記で出力する。
    QString transFileTo(const int fileTo);

    // エラーメッセージをログ出力し、例外をスローする。
    void logAndThrowError(const QString& errorMessage);
}

#endif // SHOGIUTILS_H
