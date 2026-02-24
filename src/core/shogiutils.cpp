/// @file shogiutils.cpp
/// @brief 将棋関連の共通ユーティリティ関数群の実装

#include "shogiutils.h"
#include "shogimove.h"
#include "errorbus.h"
#include "logcategories.h"
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QStringList>
#include "shogiboard.h"
#include <QAtomicInteger>
#include <QObject>

namespace ShogiUtils {

// ============================================================
// 座標表記変換
// ============================================================

QString transRankTo(const int rankTo)
{
    static const QStringList rankStrings = { "", "一", "二", "三", "四", "五", "六", "七", "八", "九" };

    if (rankTo < 1 || rankTo > 9) {
        // -1 はセンチネル値（未設定/無効）として使われるため、エラーダイアログは表示しない
        if (rankTo == -1) {
            qCDebug(lcCore, "transRankTo: sentinel value -1 (no valid coordinate)");
            return QString();
        }
        const QString msg = QObject::tr("The rank must be a value between 1 and 9. (got %1)")
        .arg(rankTo);
        qCWarning(lcCore, "%s", qUtf8Printable(msg));
        ErrorBus::instance().postError(msg);
        return QString();
    }
    return rankStrings.at(rankTo);
}

QString transFileTo(const int fileTo)
{
    static const QStringList fileStrings = { "", "１", "２", "３", "４", "５", "６", "７", "８", "９" };

    if (fileTo < 1 || fileTo > 9) {
        // -1 はセンチネル値（未設定/無効）として使われるため、エラーダイアログは表示しない
        if (fileTo == -1) {
            qCDebug(lcCore, "transFileTo: sentinel value -1 (no valid coordinate)");
            return QString();
        }
        const QString msg = QObject::tr("The file must be a value between 1 and 9. (got %1)")
        .arg(fileTo);
        qCWarning(lcCore, "%s", qUtf8Printable(msg));
        ErrorBus::instance().postError(msg);
        return QString();
    }
    return fileStrings.at(fileTo);
}

// ============================================================
// USI変換
// ============================================================

QString moveToUsi(const ShogiMove& move)
{
    const int fromX = move.fromSquare.x();
    const int fromY = move.fromSquare.y();
    const int toX = move.toSquare.x();
    const int toY = move.toSquare.y();

    if (toX < 0 || toX > 8 || toY < 0 || toY > 8) {
        return QString();
    }

    const int toFile = toX + 1;
    const QChar toRank = QChar('a' + toY);

    // 駒打ち: fromXが9（先手駒台）または10（後手駒台）
    if (fromX == 9 || fromX == 10) {
        QChar piece = pieceToChar(toBlack(move.movingPiece));
        return QStringLiteral("%1*%2%3").arg(piece).arg(toFile).arg(toRank);
    }

    if (fromX < 0 || fromX > 8 || fromY < 0 || fromY > 8) {
        return QString();
    }

    const int fromFile = fromX + 1;
    const QChar fromRank = QChar('a' + fromY);

    QString usi = QStringLiteral("%1%2%3%4").arg(fromFile).arg(fromRank).arg(toFile).arg(toRank);

    if (move.isPromotion) {
        usi += QLatin1Char('+');
    }

    return usi;
}

// ============================================================
// 漢字座標解析（逆変換）
// ============================================================

int parseFullwidthFile(QChar ch)
{
    // 全角数字 '１' (0xFF11) 〜 '９' (0xFF19)
    if (ch >= QChar(0xFF11) && ch <= QChar(0xFF19)) {
        return ch.unicode() - 0xFF11 + 1;
    }
    return 0;
}

int parseKanjiRank(QChar ch)
{
    static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
    const auto idx = kanjiRanks.indexOf(ch);
    if (idx >= 0) {
        return static_cast<int>(idx) + 1;
    }
    return 0;
}

bool parseMoveLabel(const QString& moveLabel, int* outFile, int* outRank)
{
    if (!outFile || !outRank) return false;
    *outFile = 0;
    *outRank = 0;

    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark = QStringLiteral("△");

    qsizetype markPos = moveLabel.indexOf(senteMark);
    if (markPos < 0) {
        markPos = moveLabel.indexOf(goteMark);
    }
    if (markPos < 0 || moveLabel.length() <= markPos + 2) {
        return false;
    }

    const QString afterMark = moveLabel.mid(markPos + 1);

    // 「同」で始まる場合は前の手を参照する必要がある
    if (afterMark.startsWith(QStringLiteral("同"))) {
        return false;
    }

    const QChar fileChar = afterMark.at(0);
    const QChar rankChar = afterMark.at(1);

    const int file = parseFullwidthFile(fileChar);
    const int rank = parseKanjiRank(rankChar);

    if (file >= 1 && file <= 9 && rank >= 1 && rank <= 9) {
        *outFile = file;
        *outRank = rank;
        return true;
    }

    return false;
}

bool parseMoveCoordinateFromModel(const QAbstractItemModel* model, int row,
                                   int* outFile, int* outRank)
{
    if (!model || !outFile || !outRank) return false;
    *outFile = 0;
    *outRank = 0;

    if (row <= 0 || row >= model->rowCount()) {
        return false;
    }

    const QModelIndex idx = model->index(row, 0);
    const QString moveLabel = model->data(idx, Qt::DisplayRole).toString();

    if (parseMoveLabel(moveLabel, outFile, outRank)) {
        return true;
    }

    // 「同」の場合は前の手を遡って座標を取得
    for (int i = row - 1; i > 0; --i) {
        const QModelIndex prevIdx = model->index(i, 0);
        const QString prevLabel = model->data(prevIdx, Qt::DisplayRole).toString();

        if (parseMoveLabel(prevLabel, outFile, outRank)) {
            return true;
        }
    }

    return false;
}

} // namespace ShogiUtils

// ============================================================
// 対局タイマー
// ============================================================

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
