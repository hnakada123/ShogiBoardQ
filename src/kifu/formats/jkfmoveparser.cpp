/// @file jkfmoveparser.cpp
/// @brief JKF形式固有の指し手変換・初期局面構築・時間解析の実装

#include "jkfmoveparser.h"
#include "notationutils.h"
#include "parsecommon.h"

namespace JkfMoveParser {

// --- プリセット・初期局面 ---

QString presetToSfen(const QString& preset)
{
    static const char* kHirate = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

    struct Pair { const char* key; const char* sfen; };
    static const Pair tbl[] = {
        {"HIRATE",  kHirate},
        {"KY",      "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"KY_R",    "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"KA",      "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"HI",      "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"HIKY",    "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"2",       "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"3",       "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"4",       "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"5",       "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"5_L",     "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"6",       "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"7_L",     "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"7_R",     "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"8",       "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"10",      "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    };

    for (const auto& p : tbl) {
        if (preset == QString::fromUtf8(p.key)) {
            return QString::fromUtf8(p.sfen);
        }
    }
    return QString::fromUtf8(kHirate);
}

QString buildSfenFromInitialData(const QJsonObject& data, int moveNumber)
{
    const int color = data[QStringLiteral("color")].toInt(0);

    QString boardStr;
    if (data.contains(QStringLiteral("board"))) {
        const QJsonArray board = data[QStringLiteral("board")].toArray();

        for (int y = 0; y < 9; ++y) {
            int emptyCount = 0;
            QString rowStr;
            for (int x = 8; x >= 0; --x) {
                const QJsonArray col = board[x].toArray();
                const QJsonObject cell = col[y].toObject();

                if (cell.isEmpty() || !cell.contains(QStringLiteral("kind"))) {
                    ++emptyCount;
                } else {
                    if (emptyCount > 0) {
                        rowStr += QString::number(emptyCount);
                        emptyCount = 0;
                    }
                    const QString kind = cell[QStringLiteral("kind")].toString();
                    const int cellColor = cell[QStringLiteral("color")].toInt();
                    const int pieceCode = pieceKindFromCsa(kind);
                    const bool promoted = (pieceCode >= 9 && pieceCode <= 14);

                    if (promoted) rowStr += QLatin1Char('+');
                    rowStr += pieceToSfenChar(pieceCode, cellColor == 0);
                }
            }
            if (emptyCount > 0) {
                rowStr += QString::number(emptyCount);
            }
            if (!boardStr.isEmpty()) boardStr += QLatin1Char('/');
            boardStr += rowStr;
        }
    }

    QString handsStr;
    if (data.contains(QStringLiteral("hands"))) {
        const QJsonArray hands = data[QStringLiteral("hands")].toArray();
        if (hands.size() >= 2) {
            const QJsonObject blackHands = hands[0].toObject();
            const QJsonObject whiteHands = hands[1].toObject();

            auto emitHands = [](const QJsonObject& h, bool isBlack) -> QString {
                QString s;
                const char* order[] = {"HI", "KA", "KI", "GI", "KE", "KY", "FU"};
                const char* sfenChar[] = {"R", "B", "G", "S", "N", "L", "P"};
                for (int i = 0; i < 7; ++i) {
                    const QString key = QString::fromUtf8(order[i]);
                    if (h.contains(key)) {
                        int count = h[key].toInt();
                        if (count > 0) {
                            if (count > 1) s += QString::number(count);
                            QString ch = QString::fromUtf8(sfenChar[i]);
                            if (!isBlack) ch = ch.toLower();
                            s += ch;
                        }
                    }
                }
                return s;
            };

            handsStr = emitHands(blackHands, true) + emitHands(whiteHands, false);
        }
    }
    if (handsStr.isEmpty()) handsStr = QStringLiteral("-");

    const QString turnStr = (color == 0) ? QStringLiteral("b") : QStringLiteral("w");

    return QStringLiteral("%1 %2 %3 %4")
        .arg(boardStr, turnStr, handsStr, QString::number(moveNumber));
}

// --- 駒種変換 ---

int pieceKindFromCsa(const QString& kind)
{
    if (kind == QStringLiteral("FU")) return 1;
    if (kind == QStringLiteral("KY")) return 2;
    if (kind == QStringLiteral("KE")) return 3;
    if (kind == QStringLiteral("GI")) return 4;
    if (kind == QStringLiteral("KI")) return 5;
    if (kind == QStringLiteral("KA")) return 6;
    if (kind == QStringLiteral("HI")) return 7;
    if (kind == QStringLiteral("OU")) return 8;
    if (kind == QStringLiteral("TO")) return 9;
    if (kind == QStringLiteral("NY")) return 10;
    if (kind == QStringLiteral("NK")) return 11;
    if (kind == QStringLiteral("NG")) return 12;
    if (kind == QStringLiteral("UM")) return 13;
    if (kind == QStringLiteral("RY")) return 14;
    return 0;
}

QChar pieceToSfenChar(int kind, bool isBlack)
{
    QChar c;
    switch (kind) {
    case 1:  c = QLatin1Char('P'); break;
    case 2:  c = QLatin1Char('L'); break;
    case 3:  c = QLatin1Char('N'); break;
    case 4:  c = QLatin1Char('S'); break;
    case 5:  c = QLatin1Char('G'); break;
    case 6:  c = QLatin1Char('B'); break;
    case 7:  c = QLatin1Char('R'); break;
    case 8:  c = QLatin1Char('K'); break;
    case 9:  c = QLatin1Char('P'); break;
    case 10: c = QLatin1Char('L'); break;
    case 11: c = QLatin1Char('N'); break;
    case 12: c = QLatin1Char('S'); break;
    case 13: c = QLatin1Char('B'); break;
    case 14: c = QLatin1Char('R'); break;
    default: return QChar();
    }
    if (!isBlack) c = c.toLower();
    return c;
}

QString pieceKindToKanji(const QString& kind)
{
    if (kind == QStringLiteral("FU")) return QStringLiteral("歩");
    if (kind == QStringLiteral("KY")) return QStringLiteral("香");
    if (kind == QStringLiteral("KE")) return QStringLiteral("桂");
    if (kind == QStringLiteral("GI")) return QStringLiteral("銀");
    if (kind == QStringLiteral("KI")) return QStringLiteral("金");
    if (kind == QStringLiteral("KA")) return QStringLiteral("角");
    if (kind == QStringLiteral("HI")) return QStringLiteral("飛");
    if (kind == QStringLiteral("OU")) return QStringLiteral("玉");
    if (kind == QStringLiteral("TO")) return QStringLiteral("と");
    if (kind == QStringLiteral("NY")) return QStringLiteral("成香");
    if (kind == QStringLiteral("NK")) return QStringLiteral("成桂");
    if (kind == QStringLiteral("NG")) return QStringLiteral("成銀");
    if (kind == QStringLiteral("UM")) return QStringLiteral("馬");
    if (kind == QStringLiteral("RY")) return QStringLiteral("龍");
    return kind;
}

// --- 指し手変換 ---

QString convertMoveToUsi(const QJsonObject& move, int& prevToX, int& prevToY)
{
    if (!move.contains(QStringLiteral("to"))) {
        return QString();
    }

    const QJsonObject to = move[QStringLiteral("to")].toObject();
    const int toX = to[QStringLiteral("x")].toInt();
    const int toY = to[QStringLiteral("y")].toInt();

    QString usi;

    if (!move.contains(QStringLiteral("from"))) {
        const QString piece = move[QStringLiteral("piece")].toString();
        QChar pieceChar;
        if (piece == QStringLiteral("FU")) pieceChar = QLatin1Char('P');
        else if (piece == QStringLiteral("KY")) pieceChar = QLatin1Char('L');
        else if (piece == QStringLiteral("KE")) pieceChar = QLatin1Char('N');
        else if (piece == QStringLiteral("GI")) pieceChar = QLatin1Char('S');
        else if (piece == QStringLiteral("KI")) pieceChar = QLatin1Char('G');
        else if (piece == QStringLiteral("KA")) pieceChar = QLatin1Char('B');
        else if (piece == QStringLiteral("HI")) pieceChar = QLatin1Char('R');
        else return QString();

        usi = NotationUtils::formatSfenDrop(pieceChar, toX, toY);
    } else {
        const QJsonObject from = move[QStringLiteral("from")].toObject();
        const int fromX = from[QStringLiteral("x")].toInt();
        const int fromY = from[QStringLiteral("y")].toInt();

        const bool promotes = move.contains(QStringLiteral("promote")) && move[QStringLiteral("promote")].toBool();
        usi = NotationUtils::formatSfenMove(fromX, fromY, toX, toY, promotes);
    }

    prevToX = toX;
    prevToY = toY;

    return usi;
}

QString convertMoveToPretty(const QJsonObject& move, int plyNumber,
                            int& prevToX, int& prevToY)
{
    const QString teban = KifuParseCommon::tebanMark(plyNumber);

    if (!move.contains(QStringLiteral("to"))) {
        return teban + QStringLiteral("???");
    }

    const QJsonObject to = move[QStringLiteral("to")].toObject();
    const int toX = to[QStringLiteral("x")].toInt();
    const int toY = to[QStringLiteral("y")].toInt();

    QString result = teban;

    const bool isSame = move.contains(QStringLiteral("same")) && move[QStringLiteral("same")].toBool();
    if (isSame) {
        result += QStringLiteral("同　");
    } else {
        result += NotationUtils::intToZenkakuDigit(toX) + NotationUtils::intToKanjiDigit(toY);
    }

    const QString piece = move[QStringLiteral("piece")].toString();
    result += pieceKindToKanji(piece);

    bool isDrop = false;
    if (move.contains(QStringLiteral("relative"))) {
        const QString relative = move[QStringLiteral("relative")].toString();
        if (relative.contains(QLatin1Char('H'))) {
            isDrop = true;
        }
        result += relativeToModifier(relative);
    }

    if (!move.contains(QStringLiteral("from")) && !isDrop) {
        result += QStringLiteral("打");
    }

    if (move.contains(QStringLiteral("promote"))) {
        if (move[QStringLiteral("promote")].toBool()) {
            result += QStringLiteral("成");
        } else {
            result += QStringLiteral("不成");
        }
    }

    if (move.contains(QStringLiteral("from"))) {
        const QJsonObject from = move[QStringLiteral("from")].toObject();
        const int fromX = from[QStringLiteral("x")].toInt();
        const int fromY = from[QStringLiteral("y")].toInt();
        result += QStringLiteral("(%1%2)").arg(fromX).arg(fromY);
    }

    prevToX = toX;
    prevToY = toY;

    return result;
}

// --- 時間解析 ---

QString formatTimeText(const QJsonObject& timeObj, qint64& cumSec)
{
    qint64 nowSec = 0;
    if (timeObj.contains(QStringLiteral("now"))) {
        const QJsonObject now = timeObj[QStringLiteral("now")].toObject();
        int h = now[QStringLiteral("h")].toInt(0);
        int m = now[QStringLiteral("m")].toInt(0);
        int s = now[QStringLiteral("s")].toInt(0);
        nowSec = h * 3600 + m * 60 + s;
    }

    qint64 totalSec = 0;
    if (timeObj.contains(QStringLiteral("total"))) {
        const QJsonObject total = timeObj[QStringLiteral("total")].toObject();
        int h = total[QStringLiteral("h")].toInt(0);
        int m = total[QStringLiteral("m")].toInt(0);
        int s = total[QStringLiteral("s")].toInt(0);
        totalSec = h * 3600 + m * 60 + s;
        cumSec = totalSec;
    } else {
        cumSec += nowSec;
        totalSec = cumSec;
    }

    return KifuParseCommon::formatTimeMS(nowSec * 1000)
           + QLatin1Char('/')
           + KifuParseCommon::formatTimeHMS(totalSec * 1000);
}

// --- コメント・修飾語 ---

QString extractCommentsFromMoveObj(const QJsonObject& moveObj)
{
    if (!moveObj.contains(QStringLiteral("comments"))) {
        return QString();
    }
    const QJsonArray comments = moveObj[QStringLiteral("comments")].toArray();
    QStringList cmtList;
    for (const QJsonValueConstRef c : comments) {
        cmtList.append(c.toString());
    }
    return cmtList.join(QLatin1Char('\n'));
}

QString relativeToModifier(const QString& relative)
{
    QString mod;
    if (relative.contains(QLatin1Char('L'))) mod += QStringLiteral("左");
    if (relative.contains(QLatin1Char('C'))) mod += QStringLiteral("直");
    if (relative.contains(QLatin1Char('R'))) mod += QStringLiteral("右");
    if (relative.contains(QLatin1Char('U'))) mod += QStringLiteral("上");
    if (relative.contains(QLatin1Char('M'))) mod += QStringLiteral("寄");
    if (relative.contains(QLatin1Char('D'))) mod += QStringLiteral("引");
    if (relative.contains(QLatin1Char('H'))) mod += QStringLiteral("打");
    return mod;
}

QString specialToJapanese(const QString& special)
{
    return KifuParseCommon::csaSpecialToJapanese(special);
}

} // namespace JkfMoveParser
