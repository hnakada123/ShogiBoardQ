/// @file csalexer.cpp
/// @brief CSA形式固有の字句解析・トークン化・盤面解析の実装

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

// --- P行（初期局面）解析ヘルパ ---

namespace {

// P行のトークン化（+XX/-XX/* 形式の9トークンに分割）
static QStringList tokenizePRow(const QString& raw)
{
    QString rest = raw.mid(2).trimmed();
    rest.replace(QLatin1Char('\t'), QLatin1Char(' '));

    // 正規化：+/-/* の前後にスペースを挿入
    QString norm; norm.reserve(rest.size() * 2);
    for (qsizetype i = 0; i < rest.size(); ++i) {
        const QChar c = rest.at(i);
        if (c == QLatin1Char('+') || c == QLatin1Char('-') || c == QLatin1Char('*')) {
            norm += QLatin1Char(' ');
            norm += c;
            if (c != QLatin1Char('*') && i + 2 < rest.size()) {
                norm += rest.at(i + 1);
                norm += rest.at(i + 2);
                i += 2;
            }
            norm += QLatin1Char(' ');
        } else {
            norm += c;
        }
    }
    QStringList toks = norm.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (toks.size() == 9) return toks;

    // フォールバック：スペースなし密着形式を直接解析
    QStringList ts; ts.reserve(9);
    for (qsizetype j = 0; j < rest.size(); ++j) {
        const QChar c = rest.at(j);
        if (c == QLatin1Char('*')) {
            ts.append(QStringLiteral("*"));
        } else if (c == QLatin1Char('+') || c == QLatin1Char('-')) {
            if (j + 2 < rest.size()) {
                const QString t = QString(c) + rest.at(j + 1) + rest.at(j + 2);
                ts.append(t);
                j += 2;
            }
        }
    }
    return ts;
}

} // anonymous namespace

// --- P行（初期局面）解析 ---

void setHirate(Board& board)
{
    // クリア
    for (int x = 1; x <= 9; ++x) {
        for (int y = 1; y <= 9; ++y) {
            board.sq[x][y] = Cell{};
        }
    }
    // 白（後手）陣（y=1: L N S G K G S N L）
    Piece back[9] = { KY, KE, GI, KI, OU, KI, GI, KE, KY };
    for (int x = 1; x <= 9; ++x) { board.sq[x][1].p = back[x-1]; board.sq[x][1].c = White; }
    // 白：y=2 角(2,2), 飛(8,2)
    board.sq[2][2] = { KA, White }; board.sq[8][2] = { HI, White };
    // 白：歩 y=3
    for (int x = 1; x <= 9; ++x) { board.sq[x][3] = { FU, White }; }

    // 黒：歩 y=7
    for (int x = 1; x <= 9; ++x) { board.sq[x][7] = { FU, Black }; }
    // 黒：y=8 飛(2,8), 角(8,8)
    board.sq[2][8] = { HI, Black }; board.sq[8][8] = { KA, Black };
    // 黒（先手）陣（y=9: L N S G K G S N L）
    for (int x = 1; x <= 9; ++x) { board.sq[x][9].p = back[x-1]; board.sq[x][9].c = Black; }
}

bool applyPRowLine(const QString& raw, Board& b)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const int row = raw.at(1).isDigit() ? raw.at(1).digitValue() : -1;
    if (row < 1 || row > 9) return false;

    const QStringList toks = tokenizePRow(raw);
    if (toks.size() != 9) return false;

    for (int i = 0; i < 9; ++i) {
        const QString t = toks.at(i);
        const int x = 9 - i;
        if (t == QLatin1String("*")) {
            b.sq[x][row].p = NO_P;
            b.sq[x][row].c = Black;
            continue;
        }
        if (t.size() != 3) return false;
        const Color side =
            (t.at(0) == QLatin1Char('+')) ? Black : White;
        const Piece pc = pieceFromCsa2(t.mid(1, 2));
        b.sq[x][row] = { pc, side };
    }
    return true;
}

bool applyPPlusMinusLine(const QString& raw, Board& b,
                          int bH[7], int wH[7], bool& alBlack, bool& alWhite)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const QChar sign = raw.at(1);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return false;

    const Color side = (sign == QLatin1Char('+')) ? Black : White;

    QString rest = raw.mid(2).trimmed();
    QString noSpace = rest; noSpace.remove(QLatin1Char(' '));

    int matched = 0;
    for (qsizetype pos = 0; pos + 3 < noSpace.size(); pos += 4) {
        const int file = noSpace.at(pos + 0).digitValue();
        const int rank = noSpace.at(pos + 1).digitValue();
        const QString pc2 = noSpace.mid(pos + 2, 2);

        if (file == 0 && rank == 0 && pc2 == QLatin1String("AL")) {
            if (side == Black) alBlack = true;
            else               alWhite = true;
            continue;
        }

        const Piece pc = pieceFromCsa2(pc2);
        if (file == 0 && rank == 0) {
            const Piece base = basePieceOf(pc);
            int idx = -1;
            switch (base) {
            case HI: idx = 0; break; case KA: idx = 1; break;
            case KI: idx = 2; break; case GI: idx = 3; break;
            case KE: idx = 4; break; case KY: idx = 5; break;
            case FU: idx = 6; break; default: idx = -1; break;
            }
            if (idx >= 0) {
                if (side == Black) ++bH[idx];
                else ++wH[idx];
            }
        } else {
            if (file < 1 || file > 9 || rank < 1 || rank > 9) continue;
            b.sq[file][rank] = { pc, side };
        }
        ++matched;
    }
    return matched > 0;
}

void processAlRemainder(Board& board, int bH[7], int wH[7], bool alBlack, bool alWhite)
{
    const int TOTAL[7] = {2, 2, 4, 4, 4, 4, 18}; // R,B,G,S,N,L,P
    int used[7] = {0, 0, 0, 0, 0, 0, 0};
    for (int x = 1; x <= 9; ++x) {
        for (int y = 1; y <= 9; ++y) {
            switch (basePieceOf(board.sq[x][y].p)) {
            case HI: ++used[0]; break; case KA: ++used[1]; break;
            case KI: ++used[2]; break; case GI: ++used[3]; break;
            case KE: ++used[4]; break; case KY: ++used[5]; break;
            case FU: ++used[6]; break; default: break;
            }
        }
    }
    for (int k = 0; k < 7; ++k) used[k] += bH[k] + wH[k];
    for (int k = 0; k < 7; ++k) {
        const int rem = TOTAL[k] - used[k];
        if (rem <= 0) continue;
        if (alBlack) bH[k] += rem;
        if (alWhite) wH[k] += rem;
    }
}

// --- 盤面→SFEN変換 ---

QString toSfenBoard(const Board& b)
{
    QString s;
    s.reserve(81);
    for (int y = 1; y <= 9; ++y) {
        int empty = 0;
        for (int x = 9; x >= 1; --x) {
            const auto& cell = b.sq[x][y];
            if (cell.p == NO_P) {
                ++empty;
            } else {
                if (empty > 0) { s += QString::number(empty); empty = 0; }
                const Piece base = basePieceOf(cell.p);
                QChar ch;
                switch (base) {
                case FU: ch = QLatin1Char('P'); break;
                case KY: ch = QLatin1Char('L'); break;
                case KE: ch = QLatin1Char('N'); break;
                case GI: ch = QLatin1Char('S'); break;
                case KI: ch = QLatin1Char('G'); break;
                case KA: ch = QLatin1Char('B'); break;
                case HI: ch = QLatin1Char('R'); break;
                case OU: ch = QLatin1Char('K'); break;
                default: ch = QLatin1Char('?'); break;
                }
                if (cell.c == White) ch = ch.toLower();
                if (isPromotedPiece(cell.p)) s += QLatin1Char('+');
                s += ch;
            }
        }
        if (empty > 0) s += QString::number(empty);
        if (y != 9) s += QLatin1Char('/');
    }
    return s;
}

QString handsToSfen(const int bH[7], const int wH[7])
{
    auto append = [](QString& out, int n, QChar ch) {
        if (n <= 0) return;
        if (n >= 2) out += QString::number(n);
        out += ch;
    };
    QString s;
    append(s, bH[0], QLatin1Char('R')); append(s, bH[1], QLatin1Char('B'));
    append(s, bH[2], QLatin1Char('G')); append(s, bH[3], QLatin1Char('S'));
    append(s, bH[4], QLatin1Char('N')); append(s, bH[5], QLatin1Char('L'));
    append(s, bH[6], QLatin1Char('P'));
    append(s, wH[0], QLatin1Char('r')); append(s, wH[1], QLatin1Char('b'));
    append(s, wH[2], QLatin1Char('g')); append(s, wH[3], QLatin1Char('s'));
    append(s, wH[4], QLatin1Char('n')); append(s, wH[5], QLatin1Char('l'));
    append(s, wH[6], QLatin1Char('p'));
    if (s.isEmpty()) return QStringLiteral("-");
    return s;
}

// --- 開始局面解析 ---

bool parseStartPos(const QStringList& lines, int& idx,
                   QString& baseSfen, Color& stm, Board& board)
{
    setHirate(board);
    stm = Black;

    int bH[7] = {0,0,0,0,0,0,0};
    int wH[7] = {0,0,0,0,0,0,0};

    bool alBlack = false;
    bool alWhite = false;

    // 盤クリア
    struct Clear {
        static void all(Board& b) {
            for (int x = 1; x <= 9; ++x)
                for (int y = 1; y <= 9; ++y) { b.sq[x][y].p = NO_P; b.sq[x][y].c = Black; }
        }
    };

    bool sawPI = false;
    bool sawAnyP = false;

    int i = idx;
    for (; i < lines.size(); ++i) {
        QString raw = lines.at(i).trimmed();
        if (raw.isEmpty()) continue;
        if (isMetaLine(raw) || isCommentLine(raw)) continue;

        if (raw == QLatin1String("PI")) {
            setHirate(board);
            for (int k = 0; k < 7; ++k) { bH[k] = 0; wH[k] = 0; }
            sawPI = true; sawAnyP = true;
            continue;
        }

        if (raw.size() >= 2 && raw.at(0) == QLatin1Char('P')) {
            if (!sawPI && !sawAnyP) Clear::all(board);

            if (raw.at(1).isDigit()) {
                if (applyPRowLine(raw, board)) sawAnyP = true;
                continue;
            } else if (raw.at(1) == QLatin1Char('+') || raw.at(1) == QLatin1Char('-')) {
                if (applyPPlusMinusLine(raw, board, bH, wH, alBlack, alWhite)) sawAnyP = true;
                continue;
            }
        }

        if (raw == QLatin1String("+")) { stm = Black; ++i; break; }
        if (raw == QLatin1String("-")) { stm = White; ++i; break; }

        if (isMoveLine(raw) || isResultLine(raw)) {
            if (raw.startsWith(QLatin1Char('+'))) stm = Black;
            else if (raw.startsWith(QLatin1Char('-'))) stm = White;
            break;
        }
    }
    idx = i;

    if (alBlack || alWhite)
        processAlRemainder(board, bH, wH, alBlack, alWhite);

    QString boardField = sawAnyP || sawPI
                             ? toSfenBoard(board)
                             : QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    const QString handsField = handsToSfen(bH, wH);

    baseSfen = boardField
               + (stm == Black ? QStringLiteral(" b ") : QStringLiteral(" w "))
               + handsField + QStringLiteral(" 1");
    return true;
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
