#include "csatosfenconverter.h"
#include "kifdisplayitem.h"
#include "shogimove.h"

#include <QStringDecoder>

// ====== NEW: CSA P 行（初期局面）と SFEN 補助 ======
namespace {

using Piece = CsaToSfenConverter::Piece;
using Color = CsaToSfenConverter::Color;

static Piece pieceFromCsa2_(const QString& two)
{
    using C = CsaToSfenConverter;
    if      (two == u"FU") return C::FU;
    else if (two == u"KY") return C::KY;
    else if (two == u"KE") return C::KE;
    else if (two == u"GI") return C::GI;
    else if (two == u"KI") return C::KI;
    else if (two == u"KA") return C::KA;
    else if (two == u"HI") return C::HI;
    else if (two == u"OU") return C::OU;
    else if (two == u"TO") return C::TO;
    else if (two == u"NY") return C::NY;
    else if (two == u"NK") return C::NK;
    else if (two == u"NG") return C::NG;
    else if (two == u"UM") return C::UM;
    else if (two == u"RY") return C::RY;
    return C::NO_P;
}

static Piece basePieceOfLocal_(Piece p)
{
    using C = CsaToSfenConverter;
    switch (p) {
    case C::TO: return C::FU;
    case C::NY: return C::KY;
    case C::NK: return C::KE;
    case C::NG: return C::GI;
    case C::UM: return C::KA;
    case C::RY: return C::HI;
    default:    return p;
    }
}

static bool isPromotedLocal_(Piece p)
{
    using C = CsaToSfenConverter;
    return (p == C::TO || p == C::NY || p == C::NK || p == C::NG || p == C::UM || p == C::RY);
}

// P1..P9 行
static bool applyPRowLine_(const QString& raw, CsaToSfenConverter::Board& b)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const int row = (raw.size() >= 2 && raw.at(1).isDigit()) ? raw.at(1).digitValue() : -1;
    if (row < 1 || row > 9) return false;

    QString rest = raw.mid(2).trimmed();
    rest.replace(QLatin1Char('\t'), QLatin1Char(' '));

    QString norm; norm.reserve(rest.size() * 2);
    for (int i = 0; i < rest.size(); ++i) {
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

    if (toks.size() != 9) {
        QStringList ts; ts.reserve(9);
        for (int j = 0; j < rest.size(); ++j) {
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
        if (ts.size() != 9) return false;
        toks = ts;
    }

    for (int i = 0; i < 9; ++i) {
        const QString t = toks.at(i);
        const int x = 9 - i;
        if (t == QLatin1String("*")) {
            b.sq[x][row].p = CsaToSfenConverter::NO_P;
            b.sq[x][row].c = CsaToSfenConverter::Black;
            continue;
        }
        if (t.size() != 3) return false;
        const CsaToSfenConverter::Color side =
            (t.at(0) == QLatin1Char('+')) ? CsaToSfenConverter::Black
                                          : CsaToSfenConverter::White;
        const CsaToSfenConverter::Piece pc = pieceFromCsa2_(t.mid(1, 2));
        b.sq[x][row] = { pc, side };
    }
    return true;
}

// P+ / P-（手駒 00XY、盤上配置、00AL）
static bool applyPPlusMinusLine_(const QString& raw, CsaToSfenConverter::Board& b,
                                 int bH[7], int wH[7], bool& alBlack, bool& alWhite)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const QChar sign = raw.at(1);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return false;

    const CsaToSfenConverter::Color side =
        (sign == QLatin1Char('+')) ? CsaToSfenConverter::Black : CsaToSfenConverter::White;

    QString rest = raw.mid(2).trimmed();
    QString noSpace = rest; noSpace.remove(QLatin1Char(' '));

    int matched = 0;
    for (int pos = 0; pos + 3 < noSpace.size(); pos += 4) {
        const int file = noSpace.at(pos + 0).digitValue();
        const int rank = noSpace.at(pos + 1).digitValue();
        const QString pc2 = noSpace.mid(pos + 2, 2);

        if (file == 0 && rank == 0 && pc2 == QLatin1String("AL")) {
            if (side == CsaToSfenConverter::Black) alBlack = true;
            else                                    alWhite = true;
            continue;
        }

        const CsaToSfenConverter::Piece pc = pieceFromCsa2_(pc2);
        if (file == 0 && rank == 0) {
            const CsaToSfenConverter::Piece base = basePieceOfLocal_(pc);
            int idx = -1;
            switch (base) {
            case CsaToSfenConverter::HI: idx = 0; break;
            case CsaToSfenConverter::KA: idx = 1; break;
            case CsaToSfenConverter::KI: idx = 2; break;
            case CsaToSfenConverter::GI: idx = 3; break;
            case CsaToSfenConverter::KE: idx = 4; break;
            case CsaToSfenConverter::KY: idx = 5; break;
            case CsaToSfenConverter::FU: idx = 6; break;
            default: idx = -1; break;
            }
            if (idx >= 0) {
                if (side == CsaToSfenConverter::Black) ++bH[idx];
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

// 盤→SFEN（面フィールドのみ）
static QString toSfenBoard_(const CsaToSfenConverter::Board& b)
{
    QString s;
    s.reserve(81);
    for (int y = 1; y <= 9; ++y) {
        int empty = 0;
        for (int x = 9; x >= 1; --x) {
            const auto& cell = b.sq[x][y];
            if (cell.p == CsaToSfenConverter::NO_P) {
                ++empty;
            } else {
                if (empty > 0) { s += QString::number(empty); empty = 0; }
                const Piece base = basePieceOfLocal_(cell.p);
                QChar ch;
                switch (base) {
                case CsaToSfenConverter::FU: ch = QLatin1Char('P'); break;
                case CsaToSfenConverter::KY: ch = QLatin1Char('L'); break;
                case CsaToSfenConverter::KE: ch = QLatin1Char('N'); break;
                case CsaToSfenConverter::GI: ch = QLatin1Char('S'); break;
                case CsaToSfenConverter::KI: ch = QLatin1Char('G'); break;
                case CsaToSfenConverter::KA: ch = QLatin1Char('B'); break;
                case CsaToSfenConverter::HI: ch = QLatin1Char('R'); break;
                case CsaToSfenConverter::OU: ch = QLatin1Char('K'); break;
                default: ch = QLatin1Char('?'); break;
                }
                if (cell.c == CsaToSfenConverter::White) ch = ch.toLower();
                if (isPromotedLocal_(cell.p)) s += QLatin1Char('+');
                s += ch;
            }
        }
        if (empty > 0) s += QString::number(empty);
        if (y != 9) s += QLatin1Char('/');
    }
    return s;
}

// 持駒→SFEN
static QString handsToSfen_(const int bH[7], const int wH[7])
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

} // namespace

// コメント正規化（'、'*'、'** 読み筋/Floodgate）
static QString normalizeCsaCommentLine_(const QString& line)
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
        return QStringLiteral("• ") + trimmed.mid(1).trimmed();
    }
    return t;
}

// --- file-scope helper: 初手の直前に連続して並ぶ "'*" コメント群を pending へ取り込む ---
// lines       : ファイル全行
// firstMoveIx : 1手目（最初に現れる + / - で始まる指し手行）の行インデックス
// outPending  : 正規化済みコメント行（順序は元ファイルどおり）
static void collectPreMoveCommentBlock_(const QStringList& lines, int firstMoveIx, QStringList& outPending)
{
    outPending.clear();
    if (firstMoveIx <= 0) return;

    // 1手目の直前から上方向へ "'..." が連続するかぎり拾う（空行はスキップし、最初の非コメント行で停止）
    QStringList buf;  // 逆順で溜めて最後に反転
    for (int j = firstMoveIx - 1; j >= 0; --j) {
        QString s = lines.at(j);
        if (s.isEmpty()) continue;
        s.replace("\r\n", "\n");
        s = s.trimmed();
        if (s.isEmpty()) continue;

        if (s.startsWith(QLatin1Char('\''))) {
            const QString norm = normalizeCsaCommentLine_(s);
            if (!norm.isNull()) buf.append(norm); // いまは逆順で push
            continue;
        }
        // 直近のコメント塊だけ欲しいので非コメントが出たら終了
        break;
    }

    // 元の順序に戻す
    for (int k = buf.size() - 1; k >= 0; --k) {
        outPending.append(buf.at(k));
    }
}

// ---- Board: 平手初期配置 ----
void CsaToSfenConverter::Board::setHirate()
{
    // クリア
    for (int x = 1; x <= 9; ++x) {
        for (int y = 1; y <= 9; ++y) {
            sq[x][y] = Cell{};
        }
    }
    // 白（後手）陣（y=1: L N S G K G S N L）
    Piece back[9] = { KY, KE, GI, KI, OU, KI, GI, KE, KY };
    for (int x = 1; x <= 9; ++x) { sq[x][1].p = back[x-1]; sq[x][1].c = White; }
    // 白：y=2 角(2,2), 飛(8,2)
    sq[2][2] = { KA, White }; sq[8][2] = { HI, White };
    // 白：歩 y=3
    for (int x = 1; x <= 9; ++x) { sq[x][3] = { FU, White }; }

    // 黒：歩 y=7
    for (int x = 1; x <= 9; ++x) { sq[x][7] = { FU, Black }; }
    // 黒：y=8 飛(2,8), 角(8,8)
    sq[2][8] = { HI, Black }; sq[8][8] = { KA, Black };
    // 黒（先手）陣（y=9: L N S G K G S N L）
    for (int x = 1; x <= 9; ++x) { sq[x][9].p = back[x-1]; sq[x][9].c = Black; }
}

// ---- ファイル読込 ----
bool CsaToSfenConverter::readAllLinesDetectEncoding_(const QString& path, QStringList& outLines, QString* warn)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (warn) *warn += QStringLiteral("Failed to open: %1\n").arg(path);
        return false;
    }

    const QByteArray raw = f.readAll();
    const QByteArray head = raw.left(128);
    const bool utf8Header  = head.contains("CSA encoding=UTF-8");
    const bool sjisHeader  = head.contains("CSA encoding=SHIFT_JIS") || head.contains("CSA encoding=Shift_JIS");

    QString text;

    if (utf8Header) {
        QStringDecoder dec(QStringDecoder::Utf8);
        text = dec(raw);
    } else if (sjisHeader) {
        QStringDecoder dec(QStringDecoder::System);
        QString t = dec(raw);
        if (t.isEmpty()) {
            QStringDecoder decSjis("Shift-JIS");
            if (decSjis.isValid()) t = decSjis(raw);
        }
        text = t;
    } else {
        // ヘッダなし：UTF-8優先→Shift_JIS→System の順に試す
        QStringDecoder decUtf8(QStringDecoder::Utf8);
        QString t = decUtf8(raw);
        if (!t.isEmpty()) {
            text = t;
        } else {
            QStringDecoder decSjis("Shift-JIS");
            if (decSjis.isValid()) {
                text = decSjis(raw);
            } else {
                QStringDecoder decSys(QStringDecoder::System);
                text = decSys(raw);
            }
        }
    }

    text.replace("\r\n", "\n");
    text.replace("\r", "\n");
    outLines = text.split('\n', Qt::KeepEmptyParts);
    return true;
}

bool CsaToSfenConverter::isMoveLine_(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('+') || c == QLatin1Char('-'));
}

bool CsaToSfenConverter::isResultLine_(const QString& s)
{
    return s.startsWith(QLatin1Char('%'));
}

bool CsaToSfenConverter::isMetaLine_(const QString& s)
{
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('V') || c == QLatin1Char('$') || c == QLatin1Char('N'));
}

bool CsaToSfenConverter::isCommentLine_(const QString& s)
{
    if (s.isEmpty()) return false;
    return s.startsWith(QLatin1Char('\''));
}

// ---- 開始局面解析（CSA v3: PI / P1..P9 / P+ / P- / + or - 対応）----
bool CsaToSfenConverter::parseStartPos_(const QStringList& lines, int& idx,
                                        QString& baseSfen, Color& stm, Board& board)
{
    board.setHirate();
    stm = Black;

    // 手駒（R,B,G,S,N,L,P）
    int bH[7] = {0,0,0,0,0,0,0};
    int wH[7] = {0,0,0,0,0,0,0};

    bool alBlack = false;
    bool alWhite = false;

    // 盤クリア（ラムダ未使用）
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
        if (isMetaLine_(raw) || isCommentLine_(raw)) continue;

        if (raw == QLatin1String("PI")) {
            board.setHirate();
            for (int k = 0; k < 7; ++k) { bH[k] = 0; wH[k] = 0; }
            sawPI = true; sawAnyP = true;
            continue;
        }

        if (raw.size() >= 2 && raw.at(0) == QLatin1Char('P')) {
            if (!sawPI && !sawAnyP) Clear::all(board);

            if (raw.at(1).isDigit()) {
                if (applyPRowLine_(raw, board)) sawAnyP = true;
                continue;
            } else if (raw.at(1) == QLatin1Char('+') || raw.at(1) == QLatin1Char('-')) {
                if (applyPPlusMinusLine_(raw, board, bH, wH, alBlack, alWhite)) sawAnyP = true;
                continue;
            }
        }

        if (raw == QLatin1String("+")) { stm = Black; ++i; break; }
        if (raw == QLatin1String("-")) { stm = White; ++i; break; }

        if (isMoveLine_(raw) || isResultLine_(raw)) {
            if (raw.startsWith(QLatin1Char('+'))) stm = Black;
            else if (raw.startsWith(QLatin1Char('-'))) stm = White;
            break;
        }
    }
    idx = i;

    // 00AL の後処理：未使用駒を指定側の持駒へ
    if (alBlack || alWhite) {
        const int TOTAL[7] = {2,2,4,4,4,4,18}; // R,B,G,S,N,L,P
        int used[7] = {0,0,0,0,0,0,0};
        for (int x = 1; x <= 9; ++x)
            for (int y = 1; y <= 9; ++y) {
                const Piece p = board.sq[x][y].p;
                switch (basePieceOf_(p)) {
                case HI: ++used[0]; break; case KA: ++used[1]; break; case KI: ++used[2]; break;
                case GI: ++used[3]; break; case KE: ++used[4]; break; case KY: ++used[5]; break;
                case FU: ++used[6]; break; default: break;
                }
            }
        for (int k = 0; k < 7; ++k) { used[k] += bH[k] + wH[k]; }
        int rem[7] = {0,0,0,0,0,0,0};
        for (int k = 0; k < 7; ++k) { rem[k] = TOTAL[k] - used[k]; if (rem[k] < 0) rem[k] = 0; }
        if (alBlack) for (int k = 0; k < 7; ++k) bH[k] += rem[k];
        if (alWhite) for (int k = 0; k < 7; ++k) wH[k] += rem[k];
    }

    // base SFEN を構築
    QString boardField = sawAnyP || sawPI
                             ? toSfenBoard_(board)
                             : QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
    const QString handsField = handsToSfen_(bH, wH);

    baseSfen = boardField
               + (stm == Black ? QStringLiteral(" b ") : QStringLiteral(" w "))
               + handsField + QStringLiteral(" 1");
    return true;
}

// ---- CSAトークンを USI 1手 にして盤も更新 ----
bool CsaToSfenConverter::parseMoveLine_(const QString& line, Color mover, Board& b,
                                        int& prevTx, int& prevTy,
                                        QString& usiMoveOut, QString& prettyOut, QString* warn)
{
    const int comma = line.indexOf(QLatin1Char(','));
    const QString token = (comma >= 0) ? line.left(comma) : line;

    int fx=0, fy=0, tx=0, ty=0;
    Piece after = NO_P;
    if (!parseCsaMoveToken_(token, fx, fy, tx, ty, after)) {
        if (warn) *warn += QStringLiteral("Malformed move token: %1\n").arg(token);
        return false;
    }

    const bool isDrop = (fx == 0 && fy == 0);

    // from 情報
    Piece beforePiece = NO_P;
    const bool srcInside = (!isDrop && inside_(fx) && inside_(fy));
    bool  beforeProm  = false;
    if (!isDrop) {
        if (!srcInside) {
            if (warn) *warn += QStringLiteral("Source out of range: %1\n").arg(token);
            return false;
        }
        const Cell from = b.sq[fx][fy];
        beforePiece = from.p;
        beforeProm  = isPromotedPiece_(from.p);
    }

    // 成り（USIの '+'）
    bool promote = false;
    if (!isDrop) {
        if (isPromotedPiece_(after)) {
            const Piece base = basePieceOf_(after);
            if (!beforeProm && base == basePieceOf_(beforePiece)) {
                promote = true;
            }
        }
    }

    // USI
    const QString toSq = toUsiSquare_(tx, ty);
    QString usi;
    if (isDrop) {
        QString usiPiece;
        switch (after) {
        case FU: usiPiece = QStringLiteral("P"); break;
        case KY: usiPiece = QStringLiteral("L"); break;
        case KE: usiPiece = QStringLiteral("N"); break;
        case GI: usiPiece = QStringLiteral("S"); break;
        case KI: usiPiece = QStringLiteral("G"); break;
        case KA: usiPiece = QStringLiteral("B"); break;
        case HI: usiPiece = QStringLiteral("R"); break;
        default: usiPiece = QStringLiteral("P"); break;
        }
        usi = usiPiece + QStringLiteral("*") + toSq;
    } else {
        const QString fromSq = toUsiSquare_(fx, fy);
        usi = fromSq + toSq + (promote ? QStringLiteral("+") : QString());
    }
    usiMoveOut = usi;

    // 表示（「同」「(44)」「馬/龍」「打」）
    const QString sideMark = (mover == Black) ? QStringLiteral("▲") : QStringLiteral("△");

    if (isDrop) {
        const QString pj = pieceKanji_(after);
        const QString dest = zenkakuDigit_(tx) + kanjiRank_(ty);
        prettyOut = sideMark + dest + pj + QStringLiteral("打");
    } else {
        QString pj;
        if (promote)           pj = pieceKanji_(after);       // 角→馬、飛→龍 等
        else if (beforeProm)   pj = pieceKanji_(beforePiece); // 既成成駒(UM/RY)の移動
        else                   pj = pieceKanji_(beforePiece);

        QString dest;
        if (tx == prevTx && ty == prevTy) dest = QStringLiteral("同　");
        else dest = zenkakuDigit_(tx) + kanjiRank_(ty);

        const QString origin = QStringLiteral("(") + QString::number(fx) + QString::number(fy) + QStringLiteral(")");
        prettyOut = sideMark + dest + pj + origin;
    }

    // 盤面更新
    if (!isDrop) { b.sq[tx][ty] = { after, mover }; b.sq[fx][fy] = Cell{}; }
    else         { b.sq[tx][ty] = { after, mover }; }

    prevTx = tx; prevTy = ty;
    return true;
}

bool CsaToSfenConverter::parseCsaMoveToken_(const QString& token,
                                            int& fx, int& fy, int& tx, int& ty,
                                            Piece& afterPiece)
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

// ---- 文字変換 ----
QChar CsaToSfenConverter::usiRankLetter_(int y)
{
    static const char table[10] = {0,'a','b','c','d','e','f','g','h','i'};
    if (y < 1 || y > 9) return QLatin1Char('a');
    return QLatin1Char(table[y]);
}

QString CsaToSfenConverter::toUsiSquare_(int x, int y)
{
    return QString::number(x) + usiRankLetter_(y);
}

bool CsaToSfenConverter::isPromotedPiece_(Piece p)
{
    return (p == TO || p == NY || p == NK || p == NG || p == UM || p == RY);
}

CsaToSfenConverter::Piece CsaToSfenConverter::basePieceOf_(Piece p)
{
    switch (p) {
    case TO: return FU; case NY: return KY; case NK: return KE;
    case NG: return GI; case UM: return KA; case RY: return HI;
    default: return p;
    }
}

QString CsaToSfenConverter::pieceKanji_(Piece p)
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

QString CsaToSfenConverter::zenkakuDigit_(int d)
{
    static const QChar map[] = { QChar(u'０'),QChar(u'１'),QChar(u'２'),QChar(u'３'),QChar(u'４'),
                                QChar(u'５'),QChar(u'６'),QChar(u'７'),QChar(u'８'),QChar(u'９') };
    if (d >= 1 && d <= 9) return QString(map[d]);
    return QString(QChar(u'？'));
}

QString CsaToSfenConverter::kanjiRank_(int y)
{
    static const QChar map[10] = { QChar(), QChar(u'一'), QChar(u'二'), QChar(u'三'), QChar(u'四'),
                                  QChar(u'五'), QChar(u'六'), QChar(u'七'), QChar(u'八'), QChar(u'九') };
    if (y >= 1 && y <= 9) return QString(map[y]);
    return QString(QChar(u'？'));
}

bool CsaToSfenConverter::inside_(int v)
{
    return (v >= 1) && (v <= 9);
}

// CSAファイルからヘッダー情報を抽出する
QList<KifGameInfoItem> CsaToSfenConverter::extractGameInfo(const QString& filePath)
{
    QList<KifGameInfoItem> items;
    QStringList lines;
    QString warn;

    if (!readAllLinesDetectEncoding_(filePath, lines, &warn)) return items;

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;

        // 盤定義/指し手が始まったら終了
        if (isMoveLine_(line)) break;
        if (line.startsWith(QLatin1Char('P')) || line == QLatin1String("PI")) continue;

        if (line.startsWith(QLatin1String("N+"))) {
            items.append({ QStringLiteral("先手"), line.mid(2).trimmed() });
        } else if (line.startsWith(QLatin1String("N-"))) {
            items.append({ QStringLiteral("後手"), line.mid(2).trimmed() });
        } else if (line.startsWith(QLatin1Char('$'))) {
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon > 0) {
                QString key = line.mid(1, colon - 1).trimmed();
                const QString val = line.mid(colon + 1).trimmed();

                if (key == QLatin1String("EVENT"))          key = QStringLiteral("棋戦");
                else if (key == QLatin1String("SITE"))       key = QStringLiteral("場所");
                else if (key == QLatin1String("START_TIME")) key = QStringLiteral("開始日時");
                else if (key == QLatin1String("END_TIME"))   key = QStringLiteral("終了日時");
                else if (key == QLatin1String("TIME_LIMIT")) key = QStringLiteral("持ち時間");
                else if (key == QLatin1String("TIME"))       key = QStringLiteral("持ち時間(秒/加算)");
                else if (key == QLatin1String("TIME+"))      key = QStringLiteral("先手:持ち時間(秒/加算)");
                else if (key == QLatin1String("TIME-"))      key = QStringLiteral("後手:持ち時間(秒/加算)");
                else if (key == QLatin1String("MAX_MOVES") || key == QLatin1String("SMAX_MOVES"))
                    key = QStringLiteral("最大手数");
                else if (key == QLatin1String("JISHOGI_POINTS")
                         || key == QLatin1String("JISHOGI_POINT")
                         || key == QLatin1String("JISHOGI"))
                    key = QStringLiteral("持将棋点数");
                else if (key == QLatin1String("OPENING"))    key = QStringLiteral("戦型");

                items.append({ key, val });
            }
        } else if (line.startsWith(QLatin1Char('V'))) {
            items.append({ QStringLiteral("バージョン"), line });
        }
    }
    return items;
}

// 秒 or 秒.ミリ(最大3桁) → ms
static bool parseTimeTokenMs_(const QString& token, qint64& msOut)
{
    msOut = 0;
    if (!token.startsWith(QLatin1Char('T'))) return false;
    const QString t = token.mid(1).trimmed();
    if (t.isEmpty()) return false;

    const int dot = t.indexOf(QLatin1Char('.'));
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

static QString mmssFromMs_(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 tot = ms / 1000;
    const qint64 mm = tot / 60;
    const qint64 ss = tot % 60;
    return QStringLiteral("%1:%2").arg(mm, 2, 10, QLatin1Char('0')).arg(ss, 2, 10, QLatin1Char('0'));
}

static QString hhmmssFromMs_(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 tot = ms / 1000;
    const qint64 hh = tot / 3600;
    const qint64 mm = (tot % 3600) / 60;
    const qint64 ss = tot % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hh, 2, 10, QLatin1Char('0'))
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
}

static QString composeTimeText_(qint64 moveMs, qint64 cumMs)
{
    return mmssFromMs_(moveMs) + QStringLiteral("/") + hhmmssFromMs_(cumMs);
}

bool CsaToSfenConverter::parse(const QString& filePath, KifParseResult& out, QString* warn)
{
    QStringList lines;
    if (!readAllLinesDetectEncoding_(filePath, lines, warn)) return false;

    out.mainline.baseSfen.clear();
    out.mainline.usiMoves.clear();
    out.mainline.disp.clear();
    out.variations.clear();

    int   idx  = 0;
    Color stm  = Black;
    Board board; board.setHirate();
    if (!parseStartPos_(lines, idx, out.mainline.baseSfen, stm, board)) {
        if (warn) *warn += QStringLiteral("Failed to parse start position.\n");
        return false;
    }

    QStringList pendingComments;
    // ★ 追加：初手直前の "'*" コメント群を pending へ先取り（v3仕様）
    collectPreMoveCommentBlock_(lines, idx, pendingComments);

    int prevTx = -1, prevTy = -1;

    qint64 cumMs[2] = {0, 0};
    int    lastMover = -1;

    bool lastDispIsResult = false;
    int  lastResultDispIndex = -1;
    int  lastResultSideIdx   = -1;

    // 次に指す側
    Color turn = stm;

    for (int i = idx; i < lines.size(); ++i) {
        QString s = lines.at(i);
        if (s.isEmpty()) continue;
        s.replace("\r\n", "\n");
        s = s.trimmed();
        if (s.isEmpty()) continue;

        // 1 行に複数トークン（, 区切り）
        if (s.contains(QLatin1Char(','))) {
            const QStringList tokens = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString& tokenRaw : tokens) {
                const QString token = tokenRaw.trimmed();
                if (token.isEmpty()) continue;

                // コメント
                if (token.startsWith(QLatin1Char('\''))) {
                    const QString norm = normalizeCsaCommentLine_(token);
                    if (!norm.isNull()) {
                        if (lastDispIsResult && lastResultDispIndex >= 0 &&
                            lastResultDispIndex < out.mainline.disp.size()) {
                            QString& dst = out.mainline.disp[lastResultDispIndex].comment;
                            if (!dst.isEmpty()) dst += QLatin1Char('\n');
                            dst += norm;
                        } else {
                            pendingComments.append(norm);
                        }
                    }
                    continue;
                }

                // 手番明示
                if (token.size() == 1 && (token[0] == QLatin1Char('+') || token[0] == QLatin1Char('-'))) {
                    turn = (token[0] == QLatin1Char('+')) ? Black : White;
                    lastDispIsResult = false; lastResultDispIndex = -1; lastResultSideIdx = -1;
                    continue;
                }

                // 結果コード（%...）
                if (token.startsWith(QLatin1Char('%'))) {
                    const QString sideMark = (turn == Black) ? QStringLiteral("▲") : QStringLiteral("△");
                    QString label;
                    if      (token.startsWith(QLatin1String("%TORYO")))            label = QStringLiteral("投了");
                    else if (token.startsWith(QLatin1String("%CHUDAN")))           label = QStringLiteral("中断");
                    else if (token.startsWith(QLatin1String("%SENNICHITE")))       label = QStringLiteral("千日手");
                    else if (token.startsWith(QLatin1String("%OUTE_SENNICHITE")))  label = QStringLiteral("王手千日手");
                    else if (token.startsWith(QLatin1String("%JISHOGI")))          label = QStringLiteral("持将棋");
                    else if (token.startsWith(QLatin1String("%TIME_UP")))          label = QStringLiteral("切れ負け");
                    else if (token.startsWith(QLatin1String("%ILLEGAL_MOVE")))     label = QStringLiteral("反則負け");
                    else if (token.startsWith(QLatin1String("%+ILLEGAL_ACTION")))  label = QStringLiteral("反則負け");
                    else if (token.startsWith(QLatin1String("%-ILLEGAL_ACTION")))  label = QStringLiteral("反則負け");
                    else if (token.startsWith(QLatin1String("%KACHI")))            label = QStringLiteral("入玉勝ち");
                    else if (token.startsWith(QLatin1String("%HIKIWAKE")))         label = QStringLiteral("引き分け");
                    else if (token.startsWith(QLatin1String("%MAX_MOVES")))        label = QStringLiteral("最大手数到達");
                    else if (token.startsWith(QLatin1String("%TSUMI")))            label = QStringLiteral("詰み");
                    else if (token.startsWith(QLatin1String("%FUZUMI")))           label = QStringLiteral("不詰");
                    else if (token.startsWith(QLatin1String("%ERROR")))            label = QStringLiteral("エラー");
                    else                                                           label = token;

                    KifDisplayItem di;
                    di.prettyMove = sideMark + label;
                    // 直前まで溜めたコメントを結果行に移す
                    if (!pendingComments.isEmpty()) {
                        const QString joined = pendingComments.join(QStringLiteral("\n"));
                        di.comment = joined;
                        pendingComments.clear();
                    }
                    out.mainline.disp.append(di);

                    lastDispIsResult   = true;
                    lastResultDispIndex = out.mainline.disp.size() - 1;
                    lastResultSideIdx   = (turn == Black) ? 0 : 1;
                    continue;
                }

                // 時間（T...）
                if (token.startsWith(QLatin1Char('T'))) {
                    qint64 moveMs = 0;
                    if (parseTimeTokenMs_(token, moveMs)) {
                        if (lastDispIsResult && lastResultDispIndex >= 0 &&
                            lastResultDispIndex < out.mainline.disp.size())
                        {
                            if (lastResultSideIdx >= 0) {
                                cumMs[lastResultSideIdx] += moveMs;
                                out.mainline.disp[lastResultDispIndex].timeText =
                                    composeTimeText_(moveMs, cumMs[lastResultSideIdx]);
                            } else {
                                out.mainline.disp[lastResultDispIndex].timeText = composeTimeText_(moveMs, 0);
                            }
                        } else if (!out.mainline.disp.isEmpty() && lastMover >= 0) {
                            cumMs[lastMover] += moveMs;
                            out.mainline.disp.last().timeText = composeTimeText_(moveMs, cumMs[lastMover]);
                        }
                    } else if (warn) {
                        *warn += QStringLiteral("Failed to parse time token: %1\n").arg(token);
                    }
                    continue;
                }

                // 通常の指し手
                if (isMoveLine_(token)) {
                    const QChar head  = token.at(0);
                    const Color mover = (head == QLatin1Char('+')) ? Black : White;

                    QString usi, pretty;
                    if (!parseMoveLine_(token, mover, board, prevTx, prevTy, usi, pretty, warn)) {
                        if (warn) *warn += QStringLiteral("Failed to parse move: %1\n").arg(token);
                        continue;
                    }

                    out.mainline.usiMoves.append(usi);
                    KifDisplayItem di;
                    di.prettyMove = pretty;
                    // ここで pending コメントを移す（1手目のコメント落ち対策）
                    if (!pendingComments.isEmpty()) {
                        const QString joined = pendingComments.join(QStringLiteral("\n"));
                        di.comment = joined;
                        pendingComments.clear();
                    }
                    out.mainline.disp.append(di);

                    lastMover = (mover == Black) ? 0 : 1;
                    lastDispIsResult = false; lastResultDispIndex = -1; lastResultSideIdx = -1;

                    turn = (mover == Black) ? White : Black;
                    continue;
                }
            }
            continue;
        }

        // 単独行・コメント
        if (isCommentLine_(s)) {
            const QString norm = normalizeCsaCommentLine_(s);
            if (!norm.isNull()) pendingComments.append(norm);
            continue;
        }

        // 単独行・手番明示
        if (s == QLatin1String("+") || s == QLatin1String("-")) {
            turn = (s == QLatin1String("+")) ? Black : White;
            lastDispIsResult = false; lastResultDispIndex = -1; lastResultSideIdx = -1;
            continue;
        }

        // 単独行・結果コード
        if (isResultLine_(s)) {
            const QString sideMark = (turn == Black) ? QStringLiteral("▲") : QStringLiteral("△");
            QString label;
            if      (s.startsWith(QLatin1String("%TORYO")))            label = QStringLiteral("投了");
            else if (s.startsWith(QLatin1String("%CHUDAN")))           label = QStringLiteral("中断");
            else if (s.startsWith(QLatin1String("%SENNICHITE")))       label = QStringLiteral("千日手");
            else if (s.startsWith(QLatin1String("%OUTE_SENNICHITE")))  label = QStringLiteral("王手千日手");
            else if (s.startsWith(QLatin1String("%JISHOGI")))          label = QStringLiteral("持将棋");
            else if (s.startsWith(QLatin1String("%TIME_UP")))          label = QStringLiteral("切れ負け");
            else if (s.startsWith(QLatin1String("%ILLEGAL_MOVE")))     label = QStringLiteral("反則負け");
            else if (s.startsWith(QLatin1String("%+ILLEGAL_ACTION")))  label = QStringLiteral("反則負け");
            else if (s.startsWith(QLatin1String("%-ILLEGAL_ACTION")))  label = QStringLiteral("反則負け");
            else if (s.startsWith(QLatin1String("%KACHI")))            label = QStringLiteral("入玉勝ち");
            else if (s.startsWith(QLatin1String("%HIKIWAKE")))         label = QStringLiteral("引き分け");
            else if (s.startsWith(QLatin1String("%MAX_MOVES")))        label = QStringLiteral("最大手数到達");
            else if (s.startsWith(QLatin1String("%TSUMI")))            label = QStringLiteral("詰み");
            else if (s.startsWith(QLatin1String("%FUZUMI")))           label = QStringLiteral("不詰");
            else if (s.startsWith(QLatin1String("%ERROR")))            label = QStringLiteral("エラー");
            else                                                       label = s;

            KifDisplayItem di;
            di.prettyMove = sideMark + label;
            if (!pendingComments.isEmpty()) {
                const QString joined = pendingComments.join(QStringLiteral("\n"));
                di.comment = joined;
                pendingComments.clear();
            }
            out.mainline.disp.append(di);

            lastDispIsResult   = true;
            lastResultDispIndex = out.mainline.disp.size() - 1;
            lastResultSideIdx   = (turn == Black) ? 0 : 1;
            continue;
        }

        // 単独行・指し手
        if (isMoveLine_(s)) {
            const QChar head  = s.at(0);
            const Color mover = (head == QLatin1Char('+')) ? Black : White;

            QString usi, pretty;
            if (!parseMoveLine_(s, mover, board, prevTx, prevTy, usi, pretty, warn)) {
                if (warn) *warn += QStringLiteral("Failed to parse move: %1\n").arg(s);
                continue;
            }

            out.mainline.usiMoves.append(usi);
            KifDisplayItem di;
            di.prettyMove = pretty;
            if (!pendingComments.isEmpty()) {
                const QString joined = pendingComments.join(QStringLiteral("\n"));
                di.comment = joined;
                pendingComments.clear();
            }
            out.mainline.disp.append(di);

            lastMover = (mover == Black) ? 0 : 1;
            lastDispIsResult = false; lastResultDispIndex = -1; lastResultSideIdx = -1;

            turn = (mover == Black) ? White : Black;
            continue;
        }

        // その他は無視
    }

    return true;
}
