/// @file bodtextgenerator.cpp
/// @brief BOD形式テキスト生成クラスの実装

#include "bodtextgenerator.h"

#include <QChar>
#include <QMap>
#include <QStringList>
#include <QVector>

QString BodTextGenerator::generate(const QString& sfenStr, int moveIndex, const QString& lastMoveStr)
{
    if (sfenStr.isEmpty()) return QString();

    const QStringList parts = sfenStr.split(QLatin1Char(' '));
    if (parts.size() < 4) return QString();

    const QString& boardSfen = parts[0];
    const QString& turnSfen = parts[1];
    const QString& handSfen = parts[2];
    const int sfenMoveNum = parts[3].toInt();

    // 持ち駒を解析
    QMap<QChar, int> senteHand;
    QMap<QChar, int> goteHand;

    if (handSfen != QStringLiteral("-")) {
        int count = 0;
        for (qsizetype i = 0; i < handSfen.size(); ++i) {
            const QChar c = handSfen.at(i);
            if (c.isDigit()) {
                count = count * 10 + c.digitValue();
            } else {
                if (count == 0) count = 1;
                if (c.isUpper()) {
                    senteHand[c] += count;
                } else {
                    goteHand[c.toUpper()] += count;
                }
                count = 0;
            }
        }
    }

    // 持ち駒を日本語文字列に変換
    auto handToString = [](const QMap<QChar, int>& hand) -> QString {
        if (hand.isEmpty()) return QStringLiteral("なし");

        const QString order = QStringLiteral("RBGSNLP");
        const QMap<QChar, QString> pieceNames = {
            {QLatin1Char('R'), QStringLiteral("飛")},
            {QLatin1Char('B'), QStringLiteral("角")},
            {QLatin1Char('G'), QStringLiteral("金")},
            {QLatin1Char('S'), QStringLiteral("銀")},
            {QLatin1Char('N'), QStringLiteral("桂")},
            {QLatin1Char('L'), QStringLiteral("香")},
            {QLatin1Char('P'), QStringLiteral("歩")}
        };
        const QMap<int, QString> kanjiNumbers = {
            {2, QStringLiteral("二")}, {3, QStringLiteral("三")}, {4, QStringLiteral("四")},
            {5, QStringLiteral("五")}, {6, QStringLiteral("六")}, {7, QStringLiteral("七")},
            {8, QStringLiteral("八")}, {9, QStringLiteral("九")}, {10, QStringLiteral("十")},
            {11, QStringLiteral("十一")}, {12, QStringLiteral("十二")}, {13, QStringLiteral("十三")},
            {14, QStringLiteral("十四")}, {15, QStringLiteral("十五")}, {16, QStringLiteral("十六")},
            {17, QStringLiteral("十七")}, {18, QStringLiteral("十八")}
        };

        QString result;
        for (qsizetype i = 0; i < order.size(); ++i) {
            const QChar piece = order.at(i);
            if (hand.contains(piece) && hand[piece] > 0) {
                result += pieceNames[piece];
                if (hand[piece] > 1) {
                    result += kanjiNumbers.value(hand[piece], QString::number(hand[piece]));
                }
                result += QStringLiteral("　");
            }
        }

        return result.isEmpty() ? QStringLiteral("なし") : result.trimmed();
    };

    // 盤面を9x9配列に展開
    QVector<QVector<QString>> board(9, QVector<QString>(9));
    const QStringList ranks = boardSfen.split(QLatin1Char('/'));
    for (qsizetype rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
        const QString& rankStr = ranks[rank];
        int file = 0;
        bool promoted = false;
        for (qsizetype k = 0; k < rankStr.size() && file < 9; ++k) {
            const QChar c = rankStr.at(k);
            if (c == QLatin1Char('+')) {
                promoted = true;
            } else if (c.isDigit()) {
                const int skip = c.toLatin1() - '0';
                for (int s = 0; s < skip && file < 9; ++s) {
                    board[rank][file++] = QStringLiteral(" ・");
                }
                promoted = false;
            } else {
                QString piece = c.isUpper() ? QStringLiteral(" ") : QStringLiteral("v");
                const QChar upperC = c.toUpper();
                const QMap<QChar, QString> pieceChars = {
                    {QLatin1Char('P'), promoted ? QStringLiteral("と") : QStringLiteral("歩")},
                    {QLatin1Char('L'), promoted ? QStringLiteral("杏") : QStringLiteral("香")},
                    {QLatin1Char('N'), promoted ? QStringLiteral("圭") : QStringLiteral("桂")},
                    {QLatin1Char('S'), promoted ? QStringLiteral("全") : QStringLiteral("銀")},
                    {QLatin1Char('G'), QStringLiteral("金")},
                    {QLatin1Char('B'), promoted ? QStringLiteral("馬") : QStringLiteral("角")},
                    {QLatin1Char('R'), promoted ? QStringLiteral("龍") : QStringLiteral("飛")},
                    {QLatin1Char('K'), QStringLiteral("玉")}
                };
                piece += pieceChars.value(upperC, QStringLiteral("？"));
                board[rank][file++] = piece;
                promoted = false;
            }
        }
        while (file < 9) {
            board[rank][file++] = QStringLiteral(" ・");
        }
    }

    // BOD形式の文字列を生成
    QStringList bodLines;
    bodLines << QStringLiteral("後手の持駒：%1").arg(handToString(goteHand));
    bodLines << QStringLiteral("  ９ ８ ７ ６ ５ ４ ３ ２ １");
    bodLines << QStringLiteral("+---------------------------+");

    const QStringList rankNames = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
    };

    for (int rank = 0; rank < 9; ++rank) {
        QString line = QStringLiteral("|");
        for (int file = 0; file < 9; ++file) {
            line += board[rank][file];
        }
        line += QStringLiteral("|") + rankNames[rank];
        bodLines << line;
    }

    bodLines << QStringLiteral("+---------------------------+");
    bodLines << QStringLiteral("先手の持駒：%1").arg(handToString(senteHand));
    bodLines << (turnSfen == QStringLiteral("b") ? QStringLiteral("先手番") : QStringLiteral("後手番"));

    if (moveIndex > 0) {
        const int displayMoveNum = sfenMoveNum - 1;
        if (!lastMoveStr.isEmpty()) {
            bodLines << QStringLiteral("手数＝%1  %2  まで").arg(displayMoveNum).arg(lastMoveStr);
        } else {
            bodLines << QStringLiteral("手数＝%1  まで").arg(displayMoveNum);
        }
    }

    return bodLines.join(QStringLiteral("\n"));
}
