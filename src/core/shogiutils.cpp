#include "shogiutils.h"
#include "shogimove.h"
#include "errorbus.h"
#include <QAbstractItemModel>
#include <QModelIndex>
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

// ========================================
// 漢字座標解析（逆変換）
// ========================================

int parseFullwidthFile(QChar ch)
{
    // 全角数字 '１' (0xFF11) 〜 '９' (0xFF19) を 1-9 に変換
    if (ch >= QChar(0xFF11) && ch <= QChar(0xFF19)) {
        return ch.unicode() - 0xFF11 + 1;
    }
    return 0;  // 無効
}

int parseKanjiRank(QChar ch)
{
    // 漢数字「一」〜「九」を 1-9 に変換
    static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
    const auto idx = kanjiRanks.indexOf(ch);
    if (idx >= 0) {
        return static_cast<int>(idx) + 1;
    }
    return 0;  // 無効
}

bool parseMoveLabel(const QString& moveLabel, int* outFile, int* outRank)
{
    if (!outFile || !outRank) return false;
    *outFile = 0;
    *outRank = 0;

    // 手番マーク（▲/△）を探す
    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark = QStringLiteral("△");

    qsizetype markPos = moveLabel.indexOf(senteMark);
    if (markPos < 0) {
        markPos = moveLabel.indexOf(goteMark);
    }
    if (markPos < 0 || moveLabel.length() <= markPos + 2) {
        return false;  // マークが見つからない、または文字列が短すぎる
    }

    // マーク後の文字列を取得
    const QString afterMark = moveLabel.mid(markPos + 1);

    // 「同」で始まる場合は前の手を参照する必要がある
    if (afterMark.startsWith(QStringLiteral("同"))) {
        return false;
    }

    // 「７六」のような漢字座標を解析
    const QChar fileChar = afterMark.at(0);  // 全角数字 '１'〜'９'
    const QChar rankChar = afterMark.at(1);  // 漢数字 '一'〜'九'

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

    // 指定行のラベルを取得
    const QModelIndex idx = model->index(row, 0);
    const QString moveLabel = model->data(idx, Qt::DisplayRole).toString();

    // まず直接座標を解析
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
