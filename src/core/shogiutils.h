#ifndef SHOGIUTILS_H
#define SHOGIUTILS_H

#include <QString>
#include <QElapsedTimer>

struct ShogiMove;

// ShogibanQで使用する共通関数
namespace ShogiUtils {
    // 指した先のマスの段を漢字表記で出力する。
    QString transRankTo(const int rankTo);

    // 指した先のマスの筋を漢字表記で出力する。
    QString transFileTo(const int fileTo);

    // エラーメッセージをログ出力し、例外をスローする。
    void logAndThrowError(const QString& errorMessage);

    /**
     * @brief ShogiMoveをUSI形式の文字列に変換する
     * @param move 変換する指し手
     * @return USI形式の文字列（例: "7g7f", "8h2b+", "P*3d"）
     *
     * 座標系:
     * - move.fromSquare/toSquare は0-indexed (0-8)
     * - 駒台: fromSquare.x() == 9 (先手駒台) or 10 (後手駒台)
     * - USI形式: file=1-9, rank=a-i
     */
    QString moveToUsi(const ShogiMove& move);
}

namespace ShogiUtils {
    void startGameEpoch();      // 新規対局の開始時に呼ぶ
    qint64 nowMs();             // 対局開始からの経過ms（モノトニック）
}

#endif // SHOGIUTILS_H
