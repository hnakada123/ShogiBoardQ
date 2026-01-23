#include "shogiutils.h"
#include "shogimove.h"
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
        // -1 はセンチネル値（未設定/無効）として使われるため、エラーダイアログは表示しない
        if (rankTo == -1) {
            qDebug().noquote() << "[ShogiUtils] transRankTo: sentinel value -1 (no valid coordinate)";
            return QString();
        }
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
        // -1 はセンチネル値（未設定/無効）として使われるため、エラーダイアログは表示しない
        if (fileTo == -1) {
            qDebug().noquote() << "[ShogiUtils] transFileTo: sentinel value -1 (no valid coordinate)";
            return QString();
        }
        const QString msg = QObject::tr("The file must be a value between 1 and 9. (got %1)")
        .arg(fileTo);
        qWarning().noquote() << "[ShogiUtils]" << msg;
        ErrorBus::instance().postError(msg);     // ← 通知
        return QString();
    }
    return fileStrings.at(fileTo);
}

QString moveToUsi(const ShogiMove& move)
{
    const int fromX = move.fromSquare.x();  // 0-indexed (0-8 for board, 9/10 for piece stands)
    const int fromY = move.fromSquare.y();  // 0-indexed (0-8)
    const int toX = move.toSquare.x();      // 0-indexed (0-8)
    const int toY = move.toSquare.y();      // 0-indexed (0-8)

    // 移動先が盤外なら無効
    if (toX < 0 || toX > 8 || toY < 0 || toY > 8) {
        return QString();
    }

    // USI形式: file=1-9, rank=a-i
    const int toFile = toX + 1;             // 1-9
    const QChar toRank = QChar('a' + toY);  // a-i

    // 駒打ち判定: fromX が 9（先手駒台）または 10（後手駒台）の場合
    if (fromX == 9 || fromX == 10) {
        // 駒種（大文字に変換）
        QChar piece = move.movingPiece.toUpper();
        return QStringLiteral("%1*%2%3").arg(piece).arg(toFile).arg(toRank);
    }

    // 通常の盤上移動
    if (fromX < 0 || fromX > 8 || fromY < 0 || fromY > 8) {
        return QString();
    }

    const int fromFile = fromX + 1;           // 1-9
    const QChar fromRank = QChar('a' + fromY);// a-i

    QString usi = QStringLiteral("%1%2%3%4").arg(fromFile).arg(fromRank).arg(toFile).arg(toRank);

    // 成りの場合は "+" を追加
    if (move.isPromotion) {
        usi += QLatin1Char('+');
    }

    return usi;
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
