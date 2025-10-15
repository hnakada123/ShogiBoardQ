#include "shogiutils.h"
#include "errorbus.h"
#include <QStringList>
#include <QDebug>
#include <QAtomicInteger>
#include <QObject>

namespace ShogiUtils {

// 指した先のマスの段を漢字表記で出力する。
QString transRankTo(const int rankTo)
{
    static const QStringList rankStrings = { "", "一", "二", "三", "四", "五", "六", "七", "八", "九" };

    if (rankTo < 1 || rankTo > 9) {
        const QString msg = QObject::tr("The rank must be a value between 1 and 9. (got %1)")
        .arg(rankTo);
        qWarning().noquote() << "[ShogiUtils]" << msg;
        ErrorBus::instance().postError(msg);     // ← ここで通知
        return QString();                        // 呼び出し側で空チェックして早期return
    }
    return rankStrings.at(rankTo);               // 範囲内なので at() でも安全
}

// 指した先のマスの筋を漢字表記で出力する。
QString transFileTo(const int fileTo)
{
    static const QStringList fileStrings = { "", "１", "２", "３", "４", "５", "６", "７", "８", "９" };

    if (fileTo < 1 || fileTo > 9) {
        const QString msg = QObject::tr("The file must be a value between 1 and 9. (got %1)")
        .arg(fileTo);
        qWarning().noquote() << "[ShogiUtils]" << msg;
        ErrorBus::instance().postError(msg);     // ← 通知
        return QString();
    }
    return fileStrings.at(fileTo);
}

} // namespace ShogiUtils

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
