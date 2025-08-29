#include "sfenpositiontracer.h"
#include <QtGlobal>

SfenPositionTracer::SfenPositionTracer() { resetToStartpos(); }

void SfenPositionTracer::resetToStartpos() {
    clearBoard();
    setStartposBoard();
    m_blackToMove = true;
    m_plyNext = 1;
    m_handB.fill(0);
    m_handW.fill(0);
}

QStringList SfenPositionTracer::generateSfensForMoves(const QStringList& usiMoves) {
    resetToStartpos();
    QStringList out;
    out.reserve(usiMoves.size());
    for (const QString& mv : usiMoves) {
        if (!applyUsiMove(mv)) break;
        out << toSfenString();
    }
    return out;
}

bool SfenPositionTracer::applyUsiMove(const QString& usiIn) {
    QString usi = usiIn.trimmed();

    // Drop: "P*5e"
    if (usi.contains(QLatin1Char('*'))) {
        const int star = usi.indexOf('*');
        if (star != 1 || usi.size() < 4) return false;
        const QChar up = usi.at(0).toUpper();
        const int col = fileToCol(usi.at(2).toLatin1() - '0');
        const int row = rankLetterToRow(usi.at(3));
        if (col<0 || row<0) return false;

        Kind k = letterToKind(up);
        if (k == KIND_N) return false;
        if (!subHand(m_blackToMove, k, 1)) {
            // 枚数チェックは緩めにしておく（0でも続行したい場合はtrueに）
            // return false;
        }
        m_board[row][col] = makeToken(up, m_blackToMove, false);

    } else {
        // Normal: "7g7f" or "2b3c+"
        if (usi.size() < 4) return false;
        const int colFrom = fileToCol(usi.at(0).toLatin1() - '0');
        const int rowFrom = rankLetterToRow(usi.at(1));
        const int colTo   = fileToCol(usi.at(2).toLatin1() - '0');
        const int rowTo   = rankLetterToRow(usi.at(3));
        const bool promote = (usi.size() >= 5 && usi.at(4) == QLatin1Char('+'));
        if (colFrom<0 || rowFrom<0 || colTo<0 || rowTo<0) return false;

        const QString fromTok = m_board[rowFrom][colFrom];
        if (fromTok.isEmpty()) return false;

        // 捕獲
        const QString toTok = m_board[rowTo][colTo];
        if (!toTok.isEmpty()) {
            const QChar capBase = baseUpperFromToken(toTok);
            Kind capK = letterToKind(capBase);
            if (capK != KIND_N) addHand(m_blackToMove, capK, 1);
        }

        // 駒を移動
        const QChar base = baseUpperFromToken(fromTok);
        const bool wasPromoted = isPromotedToken(fromTok);
        const bool nowPromoted = promote ? true : wasPromoted;
        m_board[rowTo][colTo] = makeToken(base, m_blackToMove, nowPromoted);
        m_board[rowFrom][colFrom].clear();
    }

    // 手番・手数更新
    m_blackToMove = !m_blackToMove;
    ++m_plyNext;
    return true;
}

QString SfenPositionTracer::toSfenString() const {
    const QString board = boardToSfenField();
    const QString stm = m_blackToMove ? QStringLiteral("b") : QStringLiteral("w");
    const QString hands = handsToSfenField();
    const QString ply = QString::number(m_plyNext);
    return QStringLiteral("%1 %2 %3 %4").arg(board, stm, hands, ply);
}

// ---------- ユーティリティ実装 ----------

int SfenPositionTracer::fileToCol(int file) { // 1..9 -> 0..8 (9筋が左端)
    if (file < 1 || file > 9) return -1;
    return 9 - file;
}

int SfenPositionTracer::rankLetterToRow(QChar r) { // a..i -> 0..8
    const ushort u = r.toLower().unicode();
    if (u < 'a' || u > 'i') return -1;
    return int(u - 'a');
}

QChar SfenPositionTracer::kindToLetter(Kind k) {
    switch (k) {
    case P: return QLatin1Char('P');
    case L: return QLatin1Char('L');
    case N: return QLatin1Char('N');
    case S: return QLatin1Char('S');
    case G: return QLatin1Char('G');
    case B: return QLatin1Char('B');
    case R: return QLatin1Char('R');
    default: return QChar();
    }
}
SfenPositionTracer::Kind SfenPositionTracer::letterToKind(QChar upper) {
    switch (upper.toUpper().unicode()) {
    case 'P': return P;
    case 'L': return L;
    case 'N': return N;
    case 'S': return S;
    case 'G': return G;
    case 'B': return B;
    case 'R': return R;
    default:  return KIND_N;
    }
}
QChar SfenPositionTracer::toSideCase(QChar upper, bool black) {
    return black ? upper.toUpper() : upper.toLower();
}
QString SfenPositionTracer::makeToken(QChar upper, bool black, bool promoted) {
    const QChar s = toSideCase(upper, black);
    return promoted ? (QString(QLatin1Char('+')) + s) : QString(s);
}
bool SfenPositionTracer::isPromotedToken(const QString& t) {
    return (!t.isEmpty() && t.at(0) == QLatin1Char('+'));
}
QChar SfenPositionTracer::baseUpperFromToken(const QString& t) {
    if (t.isEmpty()) return QChar();
    const QChar c = isPromotedToken(t) ? t.at(1) : t.at(0);
    return c.toUpper();
}

void SfenPositionTracer::clearBoard() {
    for (int r=0; r<9; ++r)
        for (int c=0; c<9; ++c)
            m_board[r][c].clear();
}

void SfenPositionTracer::setStartposBoard() {
    // 平手初期配置（SFEN: lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1）
    // 上段(a)は後手の駒（小文字）、下段(i)は先手の駒（大文字）
    const char* row0 = "lnsgkgsnl";
    const char* row1 = "1r5b1";
    const char* row2 = "ppppppppp";
    const char* row6 = "PPPPPPPPP";
    const char* row7 = "1B5R1";
    const char* row8 = "LNSGKGSNL";

    auto fillFromPattern = [&](int row, const char* pat) {
        int c = 0;
        for (int i=0; pat[i] != '\0' && c<9; ++i) {
            const char ch = pat[i];
            if (ch >= '1' && ch <= '9') {
                int n = ch - '0';
                while (n-- > 0 && c < 9) { m_board[row][c++].clear(); }
            } else {
                m_board[row][c++] = QString(QChar(ch));
            }
        }
        while (c < 9) m_board[row][c++].clear();
    };

    fillFromPattern(0, row0);
    fillFromPattern(1, row1);
    fillFromPattern(2, row2);
    for (int r=3; r<=5; ++r) for (int c=0; c<9; ++c) m_board[r][c].clear();
    fillFromPattern(6, row6);
    fillFromPattern(7, row7);
    fillFromPattern(8, row8);
}

void SfenPositionTracer::addHand(bool black, Kind k, int n) {
    (black ? m_handB : m_handW)[k] += n;
}
bool SfenPositionTracer::subHand(bool black, Kind k, int n) {
    auto& v = (black ? m_handB : m_handW)[k];
    if (v < n) { v = 0; return false; }
    v -= n; return true;
}

QString SfenPositionTracer::boardToSfenField() const {
    QString s;
    s.reserve(90);
    for (int r=0; r<9; ++r) {
        int empties = 0;
        for (int c=0; c<9; ++c) {
            const QString& t = m_board[r][c];
            if (t.isEmpty()) {
                ++empties;
            } else {
                if (empties > 0) { s += QString::number(empties); empties = 0; }
                s += t;
            }
        }
        if (empties > 0) s += QString::number(empties);
        if (r != 8) s += QLatin1Char('/');
    }
    return s;
}

QString SfenPositionTracer::handsToSfenField() const {
    auto appendSide = [](QString& out, const std::array<int,KIND_N>& h, bool black) {
        // 推奨順: R B G S N L P
        const Kind order[] = { R, B, G, S, N, L, P };
        for (Kind k : order) {
            int n = h[k];
            if (n <= 0) continue;
            if (n > 1) out += QString::number(n);
            const QChar up = kindToLetter(k);
            out += black ? up : up.toLower();
        }
    };
    QString out;
    appendSide(out, m_handB, true);
    appendSide(out, m_handW, false);
    return out.isEmpty() ? QStringLiteral("-") : out;
}

bool SfenPositionTracer::setFromSfen(const QString& sfen) {
    m_handB.fill(0);
    m_handW.fill(0);
    clearBoard();

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 4) return false;

    // --- board ---
    const QString board = parts[0];
    const QStringList ranks = board.split(QLatin1Char('/'));
    if (ranks.size() != 9) return false;

    for (int r = 0; r < 9; ++r) {
        const QString& row = ranks[r];
        int c = 0;
        for (int i = 0; i < row.size() && c < 9; ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                int n = ch.digitValue();
                while (n-- > 0 && c < 9) { m_board[r][c++].clear(); }
            } else if (ch == QLatin1Char('+')) {
                if (i + 1 >= row.size()) return false;
                const QChar p = row.at(++i);
                m_board[r][c++] = QString(QLatin1Char('+')) + p;
            } else {
                m_board[r][c++] = QString(ch);
            }
        }
        while (c < 9) m_board[r][c++].clear();
    }

    // --- stm ---
    m_blackToMove = (parts[1] == QStringLiteral("b"));

    // --- hands ---
    const QString hands = parts[2];
    if (hands != QStringLiteral("-")) {
        int num = 0;
        for (QChar ch : hands) {
            if (ch.isDigit()) {
                num = num * 10 + ch.digitValue();
            } else {
                const bool black = ch.isUpper();
                const int n = (num > 0) ? num : 1;
                num = 0;
                Kind k = letterToKind(ch.toUpper());
                if (k != KIND_N) addHand(black, k, n);
            }
        }
    }

    // --- ply ---
    bool ok = false;
    int ply = parts[3].toInt(&ok);
    m_plyNext = ok && ply > 0 ? ply : 1;

    return true;
}
