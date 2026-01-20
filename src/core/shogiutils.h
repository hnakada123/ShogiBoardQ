#ifndef SHOGIUTILS_H
#define SHOGIUTILS_H

#include <QString>
#include <QElapsedTimer>

// ShogibanQで使用する共通関数
namespace ShogiUtils {
    // 指した先のマスの段を漢字表記で出力する。
    QString transRankTo(const int rankTo);

    // 指した先のマスの筋を漢字表記で出力する。
    QString transFileTo(const int fileTo);

    // エラーメッセージをログ出力し、例外をスローする。
    void logAndThrowError(const QString& errorMessage);
}

namespace ShogiUtils {
    void startGameEpoch();      // 新規対局の開始時に呼ぶ
    qint64 nowMs();             // 対局開始からの経過ms（モノトニック）
}

#endif // SHOGIUTILS_H
