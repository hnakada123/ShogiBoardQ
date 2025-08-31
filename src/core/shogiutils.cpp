#include "shogiutils.h"
#include <QStringList>
#include <QDebug>
#include <QAtomicInteger>

// GUIで使用する共通関数
namespace ShogiUtils {
// 指した先のマスの段を漢字表記で出力する。
QString transRankTo(const int rankTo)
{
    static const QStringList rankStrings = { "", "一", "二", "三", "四", "五", "六", "七", "八", "九" };

    // 指定された段が1から9の範囲外の場合
    if (rankTo < 1 || rankTo > 9) {
        // エラーメッセージを表示する。
        const QString errorMessage = "The rank must be a value between 1 and 9.";

        logAndThrowError(errorMessage);
    }
    return rankStrings[rankTo];
}

// 指した先のマスの筋を漢字表記で出力する。
QString transFileTo(const int fileTo)
{
    static const QStringList fileStrings = { "", "１", "２", "３", "４", "５", "６", "７", "８", "９" };

    // 指定された筋が1から9の範囲外の場合
    if (fileTo < 1 || fileTo > 9) {
        // エラーメッセージを表示する。
        const QString errorMessage = "The file must be a value between 1 and 9.";

        logAndThrowError(errorMessage);
    }
    return fileStrings[fileTo];
}

// エラーメッセージをログ出力し、例外をスローする。
void logAndThrowError(const QString& errorMessage)
{
    qWarning() << errorMessage;

    throw std::runtime_error(errorMessage.toStdString());
}
}

namespace {
QElapsedTimer g_gameEpoch;
QAtomicInteger<bool> g_epochStarted(false);
}

void ShogiUtils::startGameEpoch() {
    g_gameEpoch.start();
    g_epochStarted.storeRelease(true);
}

qint64 ShogiUtils::nowMs() {
    if (!g_epochStarted.loadAcquire()) return 0;
    return g_gameEpoch.elapsed();
}
