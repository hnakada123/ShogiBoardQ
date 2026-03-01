/// @file jkfformatter.cpp
/// @brief JKF形式の変換ヘルパ関数の実装

#include "jkfformatter.h"
#include "kifdisplayitem.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

static const QRegularExpression& newlineRe()
{
    static const QRegularExpression re(QStringLiteral("\r?\n"));
    return re;
}

namespace JkfFormatter {

QString kanjiToCsaPiece(const QString& kanji)
{
    if (kanji.contains(QStringLiteral("歩"))) return QStringLiteral("FU");
    if (kanji.contains(QStringLiteral("香"))) return QStringLiteral("KY");
    if (kanji.contains(QStringLiteral("桂"))) return QStringLiteral("KE");
    if (kanji.contains(QStringLiteral("銀"))) return QStringLiteral("GI");
    if (kanji.contains(QStringLiteral("金"))) return QStringLiteral("KI");
    if (kanji.contains(QStringLiteral("角"))) return QStringLiteral("KA");
    if (kanji.contains(QStringLiteral("飛"))) return QStringLiteral("HI");
    if (kanji.contains(QStringLiteral("玉")) || kanji.contains(QStringLiteral("王"))) return QStringLiteral("OU");
    if (kanji.contains(QStringLiteral("と"))) return QStringLiteral("TO");
    if (kanji.contains(QStringLiteral("成香"))) return QStringLiteral("NY");
    if (kanji.contains(QStringLiteral("成桂"))) return QStringLiteral("NK");
    if (kanji.contains(QStringLiteral("成銀"))) return QStringLiteral("NG");
    if (kanji.contains(QStringLiteral("馬"))) return QStringLiteral("UM");
    if (kanji.contains(QStringLiteral("龍")) || kanji.contains(QStringLiteral("竜"))) return QStringLiteral("RY");
    return QString();
}

int zenkakuToNumber(QChar c)
{
    static const QString zenkaku = QStringLiteral("０１２３４５６７８９");
    qsizetype idx = zenkaku.indexOf(c);
    if (idx >= 0) return static_cast<int>(idx);
    if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) return c.toLatin1() - '0';
    return -1;
}

int kanjiToNumber(QChar c)
{
    static const QString kanji = QStringLiteral("〇一二三四五六七八九");
    qsizetype idx = kanji.indexOf(c);
    if (idx >= 0) return static_cast<int>(idx);
    if (c >= QLatin1Char('1') && c <= QLatin1Char('9')) return c.toLatin1() - '0';
    return -1;
}

QString japaneseToJkfSpecial(const QString& japanese)
{
    if (japanese.contains(QStringLiteral("投了"))) return QStringLiteral("TORYO");
    if (japanese.contains(QStringLiteral("中断"))) return QStringLiteral("CHUDAN");
    if (japanese.contains(QStringLiteral("王手千日手"))) return QStringLiteral("OUTE_SENNICHITE");
    if (japanese.contains(QStringLiteral("千日手"))) return QStringLiteral("SENNICHITE");
    if (japanese.contains(QStringLiteral("持将棋"))) return QStringLiteral("JISHOGI");
    if (japanese.contains(QStringLiteral("切れ負け"))) return QStringLiteral("TIME_UP");
    if (japanese.contains(QStringLiteral("反則負け")) || japanese.contains(QStringLiteral("反則勝ち"))) return QStringLiteral("ILLEGAL_ACTION");
    if (japanese.contains(QStringLiteral("入玉勝ち"))) return QStringLiteral("KACHI");
    if (japanese.contains(QStringLiteral("引き分け"))) return QStringLiteral("HIKIWAKE");
    if (japanese.contains(QStringLiteral("詰み"))) return QStringLiteral("TSUMI");
    if (japanese.contains(QStringLiteral("不詰"))) return QStringLiteral("FUZUMI");
    return QString();
}

QJsonObject parseTimeToJkf(const QString& timeText)
{
    QJsonObject result;

    if (timeText.isEmpty()) return result;

    QString text = timeText;
    text.remove(QLatin1Char('('));
    text.remove(QLatin1Char(')'));
    text = text.trimmed();

    const QStringList parts = text.split(QLatin1Char('/'));
    if (parts.size() >= 1) {
        const QStringList nowParts = parts[0].split(QLatin1Char(':'));
        if (nowParts.size() >= 2) {
            QJsonObject now;
            now[QStringLiteral("m")] = nowParts[0].trimmed().toInt();
            now[QStringLiteral("s")] = nowParts[1].trimmed().toInt();
            result[QStringLiteral("now")] = now;
        }
    }
    if (parts.size() >= 2) {
        const QStringList totalParts = parts[1].split(QLatin1Char(':'));
        if (totalParts.size() >= 3) {
            QJsonObject total;
            total[QStringLiteral("h")] = totalParts[0].trimmed().toInt();
            total[QStringLiteral("m")] = totalParts[1].trimmed().toInt();
            total[QStringLiteral("s")] = totalParts[2].trimmed().toInt();
            result[QStringLiteral("total")] = total;
        }
    }

    return result;
}

QString sfenToJkfPreset(const QString& sfen)
{
    const QString defaultSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString hiratePos = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    if (sfen.isEmpty() || sfen == defaultSfen || sfen.startsWith(hiratePos)) {
        return QStringLiteral("HIRATE");
    }

    struct PresetDef { const char* preset; const char* pos; };
    static const PresetDef presets[] = {
        {"KY",    "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"KY_R",  "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"KA",    "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"HI",    "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"HIKY",  "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"2",     "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"3",     "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"4",     "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"5",     "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"5_L",   "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"6",     "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"7_L",   "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"7_R",   "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"8",     "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
        {"10",    "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"},
    };

    const QString boardPart = sfen.section(QLatin1Char(' '), 0, 0);
    for (const auto& p : presets) {
        if (boardPart == QString::fromUtf8(p.pos)) {
            return QString::fromUtf8(p.preset);
        }
    }

    return QString();
}

QString sfenPieceToJkfKind(QChar c, bool promoted)
{
    QChar upper = c.toUpper();
    QString base;
    if (upper == QLatin1Char('P')) base = QStringLiteral("FU");
    else if (upper == QLatin1Char('L')) base = QStringLiteral("KY");
    else if (upper == QLatin1Char('N')) base = QStringLiteral("KE");
    else if (upper == QLatin1Char('S')) base = QStringLiteral("GI");
    else if (upper == QLatin1Char('G')) base = QStringLiteral("KI");
    else if (upper == QLatin1Char('B')) base = QStringLiteral("KA");
    else if (upper == QLatin1Char('R')) base = QStringLiteral("HI");
    else if (upper == QLatin1Char('K')) base = QStringLiteral("OU");
    else return QString();

    if (promoted) {
        if (base == QStringLiteral("FU")) return QStringLiteral("TO");
        if (base == QStringLiteral("KY")) return QStringLiteral("NY");
        if (base == QStringLiteral("KE")) return QStringLiteral("NK");
        if (base == QStringLiteral("GI")) return QStringLiteral("NG");
        if (base == QStringLiteral("KA")) return QStringLiteral("UM");
        if (base == QStringLiteral("HI")) return QStringLiteral("RY");
    }
    return base;
}

QJsonObject sfenToJkfData(const QString& sfen)
{
    QJsonObject data;

    if (sfen.isEmpty()) return data;

    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() < 3) return data;

    const QString boardStr = parts[0];
    const QString turnStr = parts[1];
    const QString handsStr = parts[2];

    data[QStringLiteral("color")] = (turnStr == QStringLiteral("b")) ? 0 : 1;

    QJsonArray board;
    for (int x = 0; x < 9; ++x) {
        QJsonArray col;
        for (int y = 0; y < 9; ++y) {
            col.append(QJsonObject());
        }
        board.append(col);
    }

    const QStringList rows = boardStr.split(QLatin1Char('/'));
    for (qsizetype y = 0; y < qMin(qsizetype(9), rows.size()); ++y) {
        const QString& rowStr = rows[y];
        int x = 8;
        bool isPromoted = false;

        for (qsizetype i = 0; i < rowStr.size() && x >= 0; ++i) {
            QChar c = rowStr.at(i);

            if (c == QLatin1Char('+')) {
                isPromoted = true;
                continue;
            }

            if (c.isDigit()) {
                x -= c.digitValue();
                isPromoted = false;
            } else {
                QJsonObject piece;
                piece[QStringLiteral("color")] = c.isUpper() ? 0 : 1;
                piece[QStringLiteral("kind")] = sfenPieceToJkfKind(c, isPromoted);

                QJsonArray col = board[x].toArray();
                col[y] = piece;
                board[x] = col;

                --x;
                isPromoted = false;
            }
        }
    }
    data[QStringLiteral("board")] = board;

    QJsonObject blackHands;
    QJsonObject whiteHands;

    if (handsStr != QStringLiteral("-")) {
        int count = 0;
        for (qsizetype i = 0; i < handsStr.size(); ++i) {
            QChar c = handsStr.at(i);
            if (c.isDigit()) {
                count = count * 10 + c.digitValue();
            } else {
                if (count == 0) count = 1;
                QString kind = sfenPieceToJkfKind(c, false);
                if (!kind.isEmpty()) {
                    if (c.isUpper()) {
                        blackHands[kind] = blackHands[kind].toInt() + count;
                    } else {
                        whiteHands[kind] = whiteHands[kind].toInt() + count;
                    }
                }
                count = 0;
            }
        }
    }

    QJsonArray hands;
    hands.append(blackHands);
    hands.append(whiteHands);
    data[QStringLiteral("hands")] = hands;

    return data;
}

QJsonObject convertMoveToJkf(const KifDisplayItem& disp, int& prevToX, int& prevToY, int /*ply*/)
{
    QJsonObject result;

    QString moveText = disp.prettyMove.trimmed();
    if (moveText.isEmpty()) return result;

    const bool isSente = moveText.startsWith(QStringLiteral("▲"));
    if (moveText.startsWith(QStringLiteral("▲")) || moveText.startsWith(QStringLiteral("△"))) {
        moveText = moveText.mid(1);
    }

    const QString special = japaneseToJkfSpecial(moveText);
    if (!special.isEmpty()) {
        result[QStringLiteral("special")] = special;
        return result;
    }

    QJsonObject move;
    move[QStringLiteral("color")] = isSente ? 0 : 1;

    int toX = 0, toY = 0;
    bool isSame = false;
    int parsePos = 0;

    if (moveText.startsWith(QStringLiteral("同"))) {
        isSame = true;
        toX = prevToX;
        toY = prevToY;
        parsePos = 1;
        while (parsePos < moveText.size() && (moveText.at(parsePos).isSpace() || moveText.at(parsePos) == QChar(0x3000))) {
            ++parsePos;
        }
    } else {
        if (moveText.size() >= 2) {
            toX = zenkakuToNumber(moveText.at(0));
            toY = kanjiToNumber(moveText.at(1));
            parsePos = 2;
        }
    }

    if (toX > 0 && toY > 0) {
        QJsonObject to;
        to[QStringLiteral("x")] = toX;
        to[QStringLiteral("y")] = toY;
        move[QStringLiteral("to")] = to;

        if (isSame) {
            move[QStringLiteral("same")] = true;
        }

        prevToX = toX;
        prevToY = toY;
    }

    QString remaining = moveText.mid(parsePos);
    QString pieceStr;

    if (remaining.startsWith(QStringLiteral("成香")) ||
        remaining.startsWith(QStringLiteral("成桂")) ||
        remaining.startsWith(QStringLiteral("成銀"))) {
        pieceStr = remaining.left(2);
        remaining = remaining.mid(2);
    } else if (!remaining.isEmpty()) {
        pieceStr = remaining.left(1);
        remaining = remaining.mid(1);
    }

    const QString csaPiece = kanjiToCsaPiece(pieceStr);
    if (!csaPiece.isEmpty()) {
        move[QStringLiteral("piece")] = csaPiece;
    }

    QString relative;
    bool isDrop = false;
    bool isPromote = false;
    bool isNonPromote = false;
    int fromX = 0, fromY = 0;

    while (!remaining.isEmpty()) {
        QChar c = remaining.at(0);
        if (c == QChar(0x5DE6)) {
            relative += QLatin1Char('L');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x53F3)) {
            relative += QLatin1Char('R');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x4E0A)) {
            relative += QLatin1Char('U');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x5F15)) {
            relative += QLatin1Char('D');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x5BC4)) {
            relative += QLatin1Char('M');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x76F4)) {
            relative += QLatin1Char('C');
            remaining = remaining.mid(1);
        } else if (c == QChar(0x6253)) {
            isDrop = true;
            relative += QLatin1Char('H');
            remaining = remaining.mid(1);
        } else if (remaining.startsWith(QStringLiteral("成"))) {
            isPromote = true;
            remaining = remaining.mid(1);
        } else if (remaining.startsWith(QStringLiteral("不成"))) {
            isNonPromote = true;
            remaining = remaining.mid(2);
        } else if (c == QLatin1Char('(')) {
            const qsizetype closePos = remaining.indexOf(QLatin1Char(')'));
            if (closePos > 1) {
                const QString fromStr = remaining.mid(1, closePos - 1);
                if (fromStr.size() >= 2) {
                    fromX = fromStr.at(0).digitValue();
                    if (fromX < 0) fromX = zenkakuToNumber(fromStr.at(0));
                    fromY = fromStr.at(1).digitValue();
                    if (fromY < 0) fromY = zenkakuToNumber(fromStr.at(1));
                }
            }
            break;
        } else {
            break;
        }
    }

    if (!relative.isEmpty()) {
        move[QStringLiteral("relative")] = relative;
    }

    if (fromX > 0 && fromY > 0 && !isDrop) {
        QJsonObject from;
        from[QStringLiteral("x")] = fromX;
        from[QStringLiteral("y")] = fromY;
        move[QStringLiteral("from")] = from;
    }

    if (isPromote) {
        move[QStringLiteral("promote")] = true;
    } else if (isNonPromote) {
        move[QStringLiteral("promote")] = false;
    }

    result[QStringLiteral("move")] = move;

    const QJsonObject timeObj = parseTimeToJkf(disp.timeText);
    if (!timeObj.isEmpty()) {
        result[QStringLiteral("time")] = timeObj;
    }

    if (!disp.comment.isEmpty()) {
        QJsonArray comments;
        const QStringList lines = disp.comment.split(newlineRe());
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.startsWith(QLatin1Char('*'))) {
                comments.append(trimmed.mid(1));
            } else if (!trimmed.isEmpty()) {
                comments.append(trimmed);
            }
        }
        if (!comments.isEmpty()) {
            result[QStringLiteral("comments")] = comments;
        }
    }

    return result;
}

} // namespace JkfFormatter
