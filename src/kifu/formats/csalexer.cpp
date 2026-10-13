/// @file csalexer.cpp
/// @brief CSA形式の字句解析・指し手解析・コメント/時間の実装（局面解析は csalexer_position.cpp）

#include "csalexer.h"
#include "kifmovetext.h"
#include "parsecommon.h"
#include "shogiboard.h"

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

QString csaResultToLabel(const QString& token)
{
    const QString result = KifuParseCommon::csaSpecialToJapanese(token);
    return (result == token) ? QString() : result;
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
