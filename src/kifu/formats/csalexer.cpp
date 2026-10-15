/// @file csalexer.cpp
/// @brief CSA形式の字句解析・指し手解析・コメント/時間の実装・局面解析

#include "csalexer.h"
#include "kifmovetext.h"
#include "parsecommon.h"
#include "shogiboard.h"
#include "sfenutils.h"
#include "shoginotation.h"
#include <QMap>
#include <QString>
#include <QStringList>

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

// --- 指し手解析 ---

namespace {

bool belongsTo(Piece piece, Turn side)
{
    return side == Turn::Black ? isBlackPiece(piece) : isWhitePiece(piece);
}

} // namespace

std::optional<CsaMoveToken> parseCsaMoveToken(const QString& token)
{
    if (token.size() < 7) return std::nullopt;

    const QChar sign = token.at(0);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return std::nullopt;

    auto d = [](QChar ch)->int { const int v = ch.digitValue(); return (v >= 0 && v <= 9) ? v : -1; };

    const int fx = d(token.at(1));
    const int fy = d(token.at(2));
    const int tx = d(token.at(3));
    const int ty = d(token.at(4));

    if (fx < 0 || fy < 0 || tx < 0 || ty < 0) return std::nullopt;

    const Piece piece = ShogiNotation::pieceFromCsaCode(QStringView(token).mid(5, 2));
    if (piece == Piece::None) return std::nullopt;

    return CsaMoveToken{BoardSquare{fx, fy}, BoardSquare{tx, ty}, piece};
}

bool parseMoveLine(const QString& line, Turn mover, ShogiBoard& board, BoardSquare& previousTo,
                   QString& usiMoveOut, QString& prettyOut, QString* warn)
{
    const qsizetype comma = line.indexOf(QLatin1Char(','));
    const QString token = (comma >= 0) ? line.left(comma) : line;

    const auto csa = parseCsaMoveToken(token);
    if (!csa) {
        if (warn) *warn += QStringLiteral("Malformed move token: %1\n").arg(token);
        return false;
    }

    const bool isDrop = (csa->from == BoardSquare{0, 0});
    if (!csa->to.isValid()
        || (isDrop && (isPromoted(csa->piece) || csa->piece == Piece::BlackKing))) {
        if (warn) *warn += QStringLiteral("Invalid destination or drop piece: %1\n").arg(token);
        return false;
    }
    if (!isDrop && !csa->from.isValid()) {
        if (warn) *warn += QStringLiteral("Source out of range: %1\n").arg(token);
        return false;
    }

    // 指し手と動かす駒（駒打ちは打つ駒、盤上の移動は成る前の駒）。成りは移動元の駒と指し手の駒種から決まる
    UsiMove move;
    move.to = csa->to;
    Piece moving = Piece::None;
    if (isDrop) {
        move.drop = csa->piece;
        moving = move.dropPieceFor(mover);
    } else {
        move.from = csa->from;
        moving = board.pieceAt(csa->from);
        const Piece before = toBlack(moving);
        move.promote = (before != csa->piece && promote(before) == csa->piece);
        if (!belongsTo(moving, mover) || (before != csa->piece && !move.promote)) moving = Piece::None;
    }
    if (moving == Piece::None || belongsTo(board.pieceAt(move.to), mover) || !board.applyUsiMove(move, mover)) {
        if (warn) *warn += QStringLiteral("Move does not match the position: %1\n").arg(token);
        return false;
    }

    usiMoveOut = move.toString();
    prettyOut = KifMoveText::format(mover, moving, move, previousTo);
    previousTo = move.to;
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

    if (trimmed.startsWith(QLatin1String("** "))) {
        const QString body = trimmed.mid(2).trimmed();
        return QStringLiteral("評価/読み筋: ") + body;
    }

    if (t.startsWith(QLatin1Char('*'))) {
        return t.mid(1);
    }
    return t;
}

std::optional<qint64> parseTimeTokenMs(const QString& token)
{
    if (!token.startsWith(QLatin1Char('T'))) return std::nullopt;
    const QString t = token.mid(1).trimmed();
    if (t.isEmpty()) return std::nullopt;

    const qsizetype dot = t.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        bool ok = false; const qint64 sec = t.toLongLong(&ok); if (!ok) return std::nullopt;
        return sec * 1000;
    }
    const QString secPart = t.left(dot);
    const QString msPart  = t.mid(dot + 1);
    bool okS = false, okM = false;
    const qint64 sec = secPart.toLongLong(&okS); if (!okS) return std::nullopt;
    QString ms3 = msPart.left(3); while (ms3.size() < 3) ms3.append(QLatin1Char('0'));
    const qint64 milli = ms3.toLongLong(&okM); if (!okM) return std::nullopt;
    return sec * 1000 + milli;
}

QString composeTimeText(qint64 moveMs, qint64 cumMs)
{
    auto fraction = [](qint64 ms) {
        return ms % 1000 == 0 ? QString()
            : QStringLiteral(".%1").arg(ms % 1000, 3, 10, QLatin1Char('0'));
    };
    return KifuParseCommon::formatTimeMS(moveMs) + fraction(moveMs) + QLatin1Char('/')
        + KifuParseCommon::formatTimeHMS(cumMs) + fraction(cumMs);
}

} // namespace CsaLexer

// ============================================================
// CSA形式の初期局面（PI・P行・00AL）を盤（ShogiBoard）に組み立てる実装
// ============================================================

namespace CsaLexer {

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

bool applyPRowLine(const QString& raw, ShogiBoard& board)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const int row = raw.at(1).isDigit() ? raw.at(1).digitValue() : -1;
    if (row < 1 || row > 9) return false;

    const QStringList toks = tokenizePRow(raw);
    if (toks.size() != 9) return false;

    for (int i = 0; i < 9; ++i) {
        const QString& t = toks.at(i);
        const BoardSquare square{9 - i, row};
        if (t == QLatin1String("*")) {
            board.setPieceAt(square, Piece::None);
            continue;
        }
        if (t.size() != 3) return false;
        const Turn side = (t.at(0) == QLatin1Char('+')) ? Turn::Black : Turn::White;
        board.setPieceAt(square, ShogiNotation::pieceFromCsaCode(QStringView(t).mid(1, 2), side));
    }
    return true;
}

bool applyPPlusMinusLine(const QString& raw, ShogiBoard& board, std::optional<Turn>& allRemainingTo)
{
    if (raw.size() < 2 || raw.at(0) != QLatin1Char('P')) return false;
    const QChar sign = raw.at(1);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return false;

    const Turn side = (sign == QLatin1Char('+')) ? Turn::Black : Turn::White;

    QString noSpace = raw.mid(2).trimmed();
    noSpace.remove(QLatin1Char(' '));

    int matched = 0;
    for (qsizetype pos = 0; pos + 3 < noSpace.size(); pos += 4) {
        const int file = noSpace.at(pos + 0).digitValue();
        const int rank = noSpace.at(pos + 1).digitValue();
        const QStringView code = QStringView(noSpace).mid(pos + 2, 2);

        if (file == 0 && rank == 0 && code == u"AL") {
            if (!allRemainingTo) allRemainingTo = side;
            ++matched;
            continue;
        }

        const Piece piece = ShogiNotation::pieceFromCsaCode(code, side);
        if (file == 0 && rank == 0) {
            // 持駒は生駒で数える（玉は持駒にしない）
            const Piece inHand = demote(piece);
            if (inHand != Piece::None && toBlack(inHand) != Piece::BlackKing) board.addStandPiece(inHand);
        } else {
            const BoardSquare square{file, rank};
            if (!square.isValid()) continue;
            board.setPieceAt(square, piece);
        }
        ++matched;
    }
    return matched > 0;
}

void giveRemainingPiecesTo(ShogiBoard& board, Turn side)
{
    const QMap<Piece, int> remaining = board.pieceBox();
    for (auto it = remaining.cbegin(); it != remaining.cend(); ++it) {
        if (it.key() == Piece::BlackKing || it.value() <= 0) continue;
        board.addStandPiece(side == Turn::Black ? it.key() : toWhite(it.key()), it.value());
    }
}

// --- 開始局面解析 ---

bool parseStartPos(const QStringList& lines, int& idx, QString& baseSfen, ShogiBoard& board)
{
    // CSAでは配置が省略された駒は駒箱に残る。PIだけが平手配置を指定する。
    board.resetGameBoard();
    Turn stm = Turn::Black;
    std::optional<Turn> allRemainingTo;

    int i = idx;
    for (; i < lines.size(); ++i) {
        QString raw = lines.at(i).trimmed();
        if (raw.isEmpty()) continue;
        if (isMetaLine(raw) || isCommentLine(raw)) continue;

        if (raw.startsWith(QLatin1String("PI"))) {
            // 平手から、指定された駒（駒落ち）を除く
            (void)board.trySetSfen(SfenUtils::hirateSfen());
            const QString removed = raw.mid(2);
            if (removed.size() % 4 != 0) return false;
            for (qsizetype pos = 0; pos < removed.size(); pos += 4) {
                const BoardSquare square{removed.at(pos).digitValue(), removed.at(pos + 1).digitValue()};
                const Piece piece = ShogiNotation::pieceFromCsaCode(QStringView(removed).mid(pos + 2, 2));
                if (!square.isValid() || piece == Piece::None || toBlack(board.pieceAt(square)) != piece) return false;
                board.setPieceAt(square, Piece::None);
            }
            continue;
        }

        if (raw.size() >= 2 && raw.at(0) == QLatin1Char('P')) {
            if (raw.at(1).isDigit()) {
                applyPRowLine(raw, board);
                continue;
            } else if (raw.at(1) == QLatin1Char('+') || raw.at(1) == QLatin1Char('-')) {
                applyPPlusMinusLine(raw, board, allRemainingTo);
                continue;
            }
        }

        if (raw == QLatin1String("+")) { stm = Turn::Black; ++i; break; }
        if (raw == QLatin1String("-")) { stm = Turn::White; ++i; break; }

        if (isMoveLine(raw) || isResultLine(raw)) {
            if (raw.startsWith(QLatin1Char('+'))) stm = Turn::Black;
            else if (raw.startsWith(QLatin1Char('-'))) stm = Turn::White;
            break;
        }
    }
    idx = i;

    if (allRemainingTo) giveRemainingPiecesTo(board, *allRemainingTo);

    board.setCurrentPlayer(stm);
    baseSfen = board.convertBoardToSfen() + QLatin1Char(' ') + turnToSfen(stm) + QLatin1Char(' ')
               + board.convertStandToSfen() + QStringLiteral(" 1");
    return true;
}

} // namespace CsaLexer
