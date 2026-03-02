/// @file sfenpositiontracer.cpp
/// @brief USI手列を順に適用して各手後のSFEN局面を生成する簡易トレーサの実装

#include "sfenpositiontracer.h"
#include "shogitypes.h"
#include <QtGlobal>
#include <QHash>
#include <QPoint>

// ======================================================================
// コンストラクタ・初期化
// ======================================================================

SfenPositionTracer::SfenPositionTracer() { resetToStartpos(); }

void SfenPositionTracer::resetToStartpos() {
    clearBoard();
    setStartposBoard();
    m_blackToMove = true;
    m_plyNext = 1;
    m_handB.fill(0);
    m_handW.fill(0);
}

// ======================================================================
// 手の適用・SFEN生成
// ======================================================================

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

    // 駒打ち: "P*5e"
    if (usi.contains(QLatin1Char('*'))) {
        const qsizetype star = usi.indexOf('*');
        if (star != 1 || usi.size() < 4) return false;
        const QChar up = usi.at(0).toUpper();
        const int col = fileToCol(usi.at(2).toLatin1() - '0');
        const int row = rankLetterToRow(usi.at(3));
        if (col<0 || row<0) return false;

        Kind k = letterToKind(up);
        if (k == KIND_N) return false;
        if (!subHand(m_blackToMove, k, 1)) {
            // 枚数チェックは緩めにしておく（0でも続行したい場合がある）
        }
        m_board[row][col] = makeToken(up, m_blackToMove, false);

    } else {
        // 通常手: "7g7f" or "2b3c+"
        if (usi.size() < 4) return false;
        const int colFrom = fileToCol(usi.at(0).toLatin1() - '0');
        const int rowFrom = rankLetterToRow(usi.at(1));
        const int colTo   = fileToCol(usi.at(2).toLatin1() - '0');
        const int rowTo   = rankLetterToRow(usi.at(3));
        const bool promote = (usi.size() >= 5 && usi.at(4) == QLatin1Char('+'));
        if (colFrom<0 || rowFrom<0 || colTo<0 || rowTo<0) return false;

        const QString fromTok = m_board[rowFrom][colFrom];
        if (fromTok.isEmpty()) return false;

        // 捕獲処理
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

// ======================================================================
// 座標変換・トークン操作ユーティリティ
// ======================================================================

int SfenPositionTracer::fileToCol(int file) { // 1..9 -> 0..8 (9筋が左端)
    if (file < 1 || file > 9) return -1;
    return 9 - file;
}

int SfenPositionTracer::rankLetterToRow(QChar r) { // a..i -> 0..8
    const ushort u = r.toLower().unicode();
    if (u < 'a' || u > 'i') return -1;
    return u - 'a';
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

// ======================================================================
// 盤面操作（内部）
// ======================================================================

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

// ======================================================================
// SFEN文字列生成
// ======================================================================

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
        // SFEN仕様の推奨順: R B G S N L P
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

// ======================================================================
// SFEN読み込み
// ======================================================================

bool SfenPositionTracer::setFromSfen(const QString& sfen) {
    m_handB.fill(0);
    m_handW.fill(0);
    clearBoard();

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 4) return false;

    // 盤面パース
    const QString board = parts[0];
    const QStringList ranks = board.split(QLatin1Char('/'));
    if (ranks.size() != 9) return false;

    for (int r = 0; r < 9; ++r) {
        const QString& row = ranks[r];
        int c = 0;
        for (qsizetype i = 0; i < row.size() && c < 9; ++i) {
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

    // 手番
    m_blackToMove = (parts[1] == QStringLiteral("b"));

    // 持ち駒
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

    // 手数
    bool ok = false;
    int ply = parts[3].toInt(&ok);
    m_plyNext = ok && ply > 0 ? ply : 1;

    return true;
}

QString SfenPositionTracer::tokenAtFileRank(int file, QChar rankLetter) const {
    int col = fileToCol(file);
    int row = rankLetterToRow(rankLetter);
    if (col < 0 || row < 0) return QString();
    return m_board[row][col];
}

// ======================================================================
// 局面列構築ユーティリティ
// ======================================================================

QStringList SfenPositionTracer::buildSfenRecord(const QString& initialSfen,
                                                const QStringList& usiMoves,
                                                bool hasTerminal)
{
    SfenPositionTracer tracer;
    // 初期SFENのセットに失敗したら平手初期局面でフォールバック
    if (!tracer.setFromSfen(initialSfen)) {
        (void)tracer.setFromSfen(QStringLiteral(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
    }

    QStringList list;
    list << tracer.toSfenString(); // 開始局面

    for (const QString& mv : usiMoves) {
        (void)tracer.applyUsiMove(mv);
        list << tracer.toSfenString(); // 各手後の局面
    }
    if (hasTerminal) {
        // 終局/中断の表示を楽にするため、末尾に同一局面をもう1つ追加
        list << tracer.toSfenString();
    }
    return list;
}

QList<ShogiMove> SfenPositionTracer::buildGameMoves(const QString& initialSfen,
                                                      const QStringList& usiMoves)
{
    QList<ShogiMove> out;
    out.reserve(usiMoves.size());

    SfenPositionTracer tracer;
    (void)tracer.setFromSfen(initialSfen);

    for (const QString& usi : usiMoves) {
        // 駒打ち: "P*5e"
        if (usi.size() >= 4 && usi.at(1) == QLatin1Char('*')) {
            const bool black     = tracer.blackToMove();
            const QChar dropUp   = usi.at(0).toUpper();
            const int  file      = usi.at(2).toLatin1() - '0';
            const int  rank      = rankLetterToNum(usi.at(3));
            if (file < 1 || file > 9 || rank < 1 || rank > 9) { (void)tracer.applyUsiMove(usi); continue; }

            const QPoint from    = dropFromSquare(dropUp, black);
            const QPoint to      = QPoint(file - 1, rank - 1);
            const QChar  moving  = dropLetterWithSide(dropUp, black);
            const QChar  captured= QLatin1Char(' ');
            const bool   promo   = false;

            out.push_back(ShogiMove(from, to, charToPiece(moving), charToPiece(captured), promo));
            (void)tracer.applyUsiMove(usi);
            continue;
        }

        // 通常手: "7g7f" or "2b3c+" など
        if (usi.size() < 4) { (void)tracer.applyUsiMove(usi); continue; }

        const int ff = usi.at(0).toLatin1() - '0';
        const int rf = rankLetterToNum(usi.at(1));
        const int ft = usi.at(2).toLatin1() - '0';
        const int rt = rankLetterToNum(usi.at(3));
        const bool isProm = (usi.size() >= 5 && usi.at(4) == QLatin1Char('+'));
        if (ff<1||ff>9||rf<1||rf>9||ft<1||ft>9||rt<1||rt>9) { (void)tracer.applyUsiMove(usi); continue; }

        // 動く前の盤面から移動駒・取った駒を決定
        const QString fromTok = tracer.tokenAtFileRank(ff, usi.at(1));
        const QString toTok   = tracer.tokenAtFileRank(ft, usi.at(3));

        const QPoint from(ff - 1, rf - 1);
        const QPoint to  (ft - 1, rt - 1);

        const QChar moving   = tokenToOneChar(fromTok);
        const QChar captured = tokenToOneChar(toTok);

        out.push_back(ShogiMove(from, to, charToPiece(moving), charToPiece(captured), isProm));
        (void)tracer.applyUsiMove(usi);
    }

    return out;
}

// ======================================================================
// USI座標ヘルパ
// ======================================================================

int SfenPositionTracer::rankLetterToNum(QChar r) { // 'a'..'i' -> 1..9
    const ushort u = r.toLower().unicode();
    return (u < 'a' || u > 'i') ? -1 : (u - 'a') + 1;
}

QPoint SfenPositionTracer::dropFromSquare(QChar dropUpper, bool black) {
    const int x = black ? 9 : 10; // 先手=9, 後手=10（UI内の「駒台」側レーン）
    int y = -1;
    switch (dropUpper.toUpper().unicode()) {
    case 'P': y = black ? 0 : 8; break;
    case 'L': y = black ? 1 : 7; break;
    case 'N': y = black ? 2 : 6; break;
    case 'S': y = black ? 3 : 5; break;
    case 'G': y = 4;            break; // 金だけ共通
    case 'B': y = black ? 5 : 3; break;
    case 'R': y = black ? 6 : 2; break;
    default:  y = -1;           break;
    }
    return QPoint(x, y);
}

QChar SfenPositionTracer::dropLetterWithSide(QChar upper, bool black) {
    return black ? upper.toUpper() : upper.toLower();
}

QChar SfenPositionTracer::tokenToOneChar(const QString& tok) {
    if (tok.isEmpty()) return QLatin1Char(' ');
    if (tok.size() == 1) return tok.at(0);
    static const QHash<QString,QChar> map = {
                                              {"+P",'Q'},{"+L",'M'},{"+N",'O'},{"+S",'T'},{"+B",'C'},{"+R",'U'},
                                              {"+p",'q'},{"+l",'m'},{"+n",'o'},{"+s",'t'},{"+b",'c'},{"+r",'u'},
                                              };
    const auto it = map.find(tok);
    return it == map.end() ? QLatin1Char(' ') : *it;
}
