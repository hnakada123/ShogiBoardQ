/// @file csalexer.cpp
/// @brief CSA形式の字句解析・指し手解析・コメント/時間の実装（局面解析は csalexer_position.cpp）

#include "csalexer.h"
#include "notationutils.h"
#include "parsecommon.h"

namespace CsaLexer {

// --- 行の種類判定 ---

bool isMoveLine(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('+') || c == QLatin1Char('-'));
}

bool isResultLine(const QString& s)
{
    return s.startsWith(QLatin1Char('%'));
}

bool isMetaLine(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('V') || c == QLatin1Char('$') || c == QLatin1Char('N'));
}

bool isCommentLine(const QString& s)
{
    if (s.isEmpty()) return false;
    return s.startsWith(QLatin1Char('\''));
}

bool isTurnMarker(const QString& token)
{
    return token.size() == 1 &&
           (token.at(0) == QLatin1Char('+') || token.at(0) == QLatin1Char('-'));
}

// --- 駒種変換 ---

Piece pieceFromCsa2(const QString& two)
{
    if      (two == u"FU") return FU;
    else if (two == u"KY") return KY;
    else if (two == u"KE") return KE;
    else if (two == u"GI") return GI;
    else if (two == u"KI") return KI;
    else if (two == u"KA") return KA;
    else if (two == u"HI") return HI;
    else if (two == u"OU") return OU;
    else if (two == u"TO") return TO;
    else if (two == u"NY") return NY;
    else if (two == u"NK") return NK;
    else if (two == u"NG") return NG;
    else if (two == u"UM") return UM;
    else if (two == u"RY") return RY;
    return NO_P;
}

bool isPromotedPiece(Piece p)
{
    return (p == TO || p == NY || p == NK || p == NG || p == UM || p == RY);
}

Piece basePieceOf(Piece p)
{
    switch (p) {
    case TO: return FU; case NY: return KY; case NK: return KE;
    case NG: return GI; case UM: return KA; case RY: return HI;
    default: return p;
    }
}

QString pieceKanji(Piece p)
{
    switch (p) {
    case FU: return QStringLiteral("歩");
    case KY: return QStringLiteral("香");
    case KE: return QStringLiteral("桂");
    case GI: return QStringLiteral("銀");
    case KI: return QStringLiteral("金");
    case KA: return QStringLiteral("角");
    case HI: return QStringLiteral("飛");
    case OU: return QStringLiteral("玉");
    case TO: return QStringLiteral("と");
    case NY: return QStringLiteral("成香");
    case NK: return QStringLiteral("成桂");
    case NG: return QStringLiteral("成銀");
    case UM: return QStringLiteral("馬");
    case RY: return QStringLiteral("龍");
    default: return QStringLiteral("歩");
    }
}

bool inside(int v)
{
    return (v >= 1) && (v <= 9);
}

// --- 指し手解析 ---

bool parseCsaMoveToken(const QString& token,
                       int& fx, int& fy, int& tx, int& ty, Piece& afterPiece)
{
    if (token.size() < 7) return false;

    const QChar sign = token.at(0);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return false;

    auto d = [](QChar ch)->int { const int v = ch.digitValue(); return (v >= 0 && v <= 9) ? v : -1; };

    fx = d(token.at(1));
    fy = d(token.at(2));
    tx = d(token.at(3));
    ty = d(token.at(4));

    if (fx < 0 || fy < 0 || tx < 0 || ty < 0) return false;

    const QString p = token.mid(5, 2);
    if      (p == u"FU") afterPiece = FU;
    else if (p == u"KY") afterPiece = KY;
    else if (p == u"KE") afterPiece = KE;
    else if (p == u"GI") afterPiece = GI;
    else if (p == u"KI") afterPiece = KI;
    else if (p == u"KA") afterPiece = KA;
    else if (p == u"HI") afterPiece = HI;
    else if (p == u"OU") afterPiece = OU;
    else if (p == u"TO") afterPiece = TO;
    else if (p == u"NY") afterPiece = NY;
    else if (p == u"NK") afterPiece = NK;
    else if (p == u"NG") afterPiece = NG;
    else if (p == u"UM") afterPiece = UM;
    else if (p == u"RY") afterPiece = RY;
    else return false;

    return true;
}

bool parseMoveLine(const QString& line, Color mover, Board& b,
                   int& prevTx, int& prevTy,
                   QString& usiMoveOut, QString& prettyOut, QString* warn)
{
    const qsizetype comma = line.indexOf(QLatin1Char(','));
    const QString token = (comma >= 0) ? line.left(comma) : line;

    int fx=0, fy=0, tx=0, ty=0;
    Piece after = NO_P;
    if (!parseCsaMoveToken(token, fx, fy, tx, ty, after)) {
        if (warn) *warn += QStringLiteral("Malformed move token: %1\n").arg(token);
        return false;
    }

    const bool isDrop = (fx == 0 && fy == 0);

    Piece beforePiece = NO_P;
    const bool srcInside = (!isDrop && inside(fx) && inside(fy));
    bool  beforeProm  = false;
    if (!isDrop) {
        if (!srcInside) {
            if (warn) *warn += QStringLiteral("Source out of range: %1\n").arg(token);
            return false;
        }
        const Cell from = b.sq[fx][fy];
        beforePiece = from.p;
        beforeProm  = isPromotedPiece(from.p);
    }

    bool promote = false;
    if (!isDrop) {
        if (isPromotedPiece(after)) {
            const Piece base = basePieceOf(after);
            if (!beforeProm && base == basePieceOf(beforePiece)) {
                promote = true;
            }
        }
    }

    // USI
    QString usi;
    if (isDrop) {
        QChar usiPieceChar;
        switch (after) {
        case FU: usiPieceChar = QLatin1Char('P'); break;
        case KY: usiPieceChar = QLatin1Char('L'); break;
        case KE: usiPieceChar = QLatin1Char('N'); break;
        case GI: usiPieceChar = QLatin1Char('S'); break;
        case KI: usiPieceChar = QLatin1Char('G'); break;
        case KA: usiPieceChar = QLatin1Char('B'); break;
        case HI: usiPieceChar = QLatin1Char('R'); break;
        default: usiPieceChar = QLatin1Char('P'); break;
        }
        usi = NotationUtils::formatSfenDrop(usiPieceChar, tx, ty);
    } else {
        usi = NotationUtils::formatSfenMove(fx, fy, tx, ty, promote);
    }
    usiMoveOut = usi;

    // 表示（「同」「(44)」「馬/龍」「打」）
    const QString sideMark = (mover == Black) ? QStringLiteral("▲") : QStringLiteral("△");

    if (isDrop) {
        const QString pj = pieceKanji(after);
        const QString dest = NotationUtils::intToZenkakuDigit(tx) + NotationUtils::intToKanjiDigit(ty);
        prettyOut = sideMark + dest + pj + QStringLiteral("打");
    } else {
        QString pj;
        if (promote)           pj = pieceKanji(after);
        else if (beforeProm)   pj = pieceKanji(beforePiece);
        else                   pj = pieceKanji(beforePiece);

        QString dest;
        if (tx == prevTx && ty == prevTy) dest = QStringLiteral("同　");
        else dest = NotationUtils::intToZenkakuDigit(tx) + NotationUtils::intToKanjiDigit(ty);

        const QString origin = QStringLiteral("(") + QString::number(fx) + QString::number(fy) + QStringLiteral(")");
        prettyOut = sideMark + dest + pj + origin;
    }

    // 盤面更新
    if (!isDrop) { b.sq[tx][ty] = { after, mover }; b.sq[fx][fy] = Cell{}; }
    else         { b.sq[tx][ty] = { after, mover }; }

    prevTx = tx; prevTy = ty;
    return true;
}

// --- コメント・時間 ---

QString normalizeCsaCommentLine(const QString& line)
{
    if (line.isEmpty() || line.at(0) != QLatin1Char('\'')) return QString();

    QString t = line.mid(1);
    if (!t.isEmpty() && t.at(0).isSpace()) t.remove(0, 1);

    if (t.startsWith(QLatin1String("---- Kifu for Windows"))) return QString();

    const QString trimmed = t.trimmed();

    if (trimmed.startsWith(QLatin1String("**"))) {
        const QString body = trimmed.mid(2).trimmed();
        return QStringLiteral("評価/読み筋: ") + body;
    }

    if (trimmed == QLatin1String("*")) return QString("");

    if (trimmed.startsWith(QLatin1Char('*'))) {
        return trimmed.mid(1).trimmed();
    }
    return t;
}

QString csaResultToLabel(const QString& token)
{
    const QString result = KifuParseCommon::csaSpecialToJapanese(token);
    return (result == token) ? QString() : result;
}

bool parseTimeTokenMs(const QString& token, qint64& msOut)
{
    msOut = 0;
    if (!token.startsWith(QLatin1Char('T'))) return false;
    const QString t = token.mid(1).trimmed();
    if (t.isEmpty()) return false;

    const qsizetype dot = t.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        bool ok = false; const qint64 sec = t.toLongLong(&ok); if (!ok) return false;
        msOut = sec * 1000; return true;
    }
    const QString secPart = t.left(dot);
    const QString msPart  = t.mid(dot + 1);
    bool okS = false, okM = false;
    const qint64 sec = secPart.toLongLong(&okS); if (!okS) return false;
    QString ms3 = msPart.left(3); while (ms3.size() < 3) ms3.append(QLatin1Char('0'));
    const qint64 milli = ms3.toLongLong(&okM); if (!okM) return false;
    msOut = sec * 1000 + milli; return true;
}

QString composeTimeText(qint64 moveMs, qint64 cumMs)
{
    return KifuParseCommon::formatTimeText(moveMs, cumMs);
}

} // namespace CsaLexer
