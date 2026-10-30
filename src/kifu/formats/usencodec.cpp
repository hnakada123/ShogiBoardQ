/// @file usencodec.cpp
/// @brief USEN の符号の符号化と復号の実装

#include "usencodec.h"

#include "handicaps.h"
#include "logcategories.h"
#include "terminalcodes.h"

namespace UsenCodec {

namespace {

constexpr QStringView kBase36Chars = u"0123456789abcdefghijklmnopqrstuvwxyz";
constexpr int kMaxCode = 36 * 36 * 36 - 1;
constexpr int kDropBase = 81;

/// 駒打ちの駒（先手の生駒）と USEN の駒のコード
struct DropPiece {
    int code;
    Piece piece;
};
constexpr DropPiece kDropPieces[] = {{9, Piece::BlackGold},    {10, Piece::BlackPawn},   {11, Piece::BlackLance},
                                     {12, Piece::BlackKnight}, {13, Piece::BlackSilver}, {14, Piece::BlackBishop},
                                     {15, Piece::BlackRook}};

int dropCodeOf(Piece piece)
{
    for (const DropPiece& drop : kDropPieces)
        if (drop.piece == piece) return drop.code;
    return -1;
}

Piece dropPieceOf(int code)
{
    for (const DropPiece& drop : kDropPieces)
        if (drop.code == code) return drop.piece;
    return Piece::None;
}

/// 盤のマスを USEN のマス番号にする（盤の外なら -1）
int squareOf(BoardSquare square)
{
    return square.isValid() ? (square.rank - 1) * 9 + (square.file - 1) : -1;
}

BoardSquare squareAt(int index)
{
    return BoardSquare{index % 9 + 1, index / 9 + 1};
}

} // namespace

QString encodeMove(const UsiMove& move)
{
    const int from = move.isDrop() ? (dropCodeOf(move.drop) >= 0 ? kDropBase + dropCodeOf(move.drop) : -1)
                                   : squareOf(move.from);
    const int to = squareOf(move.to);
    if (from < 0 || to < 0) {
        qCWarning(lcKifu) << "USEN: cannot encode move" << move.toString();
        return {};
    }
    int code = (from * 81 + to) * 2 + (move.promote ? 1 : 0);
    QString text(3, QLatin1Char('0'));
    for (int i = 2; i >= 0; --i) {
        text[i] = kBase36Chars.at(code % 36);
        code /= 36;
    }
    return text;
}

std::optional<UsiMove> decodeMove(QStringView threeChars)
{
    if (threeChars.size() != 3) return std::nullopt;
    int code = 0;
    for (const QChar c : threeChars) {
        const qsizetype digit = kBase36Chars.indexOf(c.toLower());
        if (digit < 0) return std::nullopt;
        code = code * 36 + static_cast<int>(digit);
    }
    if (code > kMaxCode) return std::nullopt;
    UsiMove move;
    move.promote = (code % 2 == 1);
    const int from = (code / 2) / 81;
    move.to = squareAt((code / 2) % 81);
    if (from >= kDropBase) {
        move.drop = dropPieceOf(from - kDropBase);
        if (move.drop == Piece::None || move.promote) {
            qCWarning(lcKifu) << "USEN: unknown drop code" << threeChars;
            return std::nullopt;
        }
    } else {
        move.from = squareAt(from);
    }
    return move;
}

QString encodePosition(const QString& sfen)
{
    const QString trimmed = sfen.trimmed();
    if (Handicaps::isHirate(trimmed)) return {};
    // 手数（4番目の欄）は書かない
    QString usen = trimmed.section(QLatin1Char(' '), 0, 2);
    usen.replace(QLatin1Char('/'), QLatin1Char('_'));
    usen.replace(QLatin1Char(' '), QLatin1Char('.'));
    usen.replace(QLatin1Char('+'), QLatin1Char('z'));
    return usen;
}

QString decodePosition(QStringView part)
{
    if (part.isEmpty()) return {};
    QString sfen = part.toString();
    sfen.replace(QLatin1Char('_'), QLatin1Char('/'));
    sfen.replace(QLatin1Char('.'), QLatin1Char(' '));
    sfen.replace(QLatin1Char('z'), QLatin1Char('+'));
    // 手数が省略されている場合は補う（盤の SFEN は4つの欄が必須）
    if (sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() == 3) sfen += QStringLiteral(" 1");
    return sfen;
}

std::optional<UsenLine> decodeLine(QStringView part, QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<UsenLine> {
        if (error) *error = message;
        return std::nullopt;
    };
    UsenLine line;
    const qsizetype dot = part.indexOf(QLatin1Char('.'));
    if (dot < 0) return fail(QStringLiteral("No '.' after the line offset in '%1'").arg(part));
    bool ok = false;
    line.offset = part.left(dot).toInt(&ok);
    if (!ok || line.offset < 0) return fail(QStringLiteral("Invalid line offset '%1'").arg(part.left(dot)));

    // 末尾の ".<終局コード>"（未終局は空のコード）
    QStringView moves = part.mid(dot + 1);
    const qsizetype lastDot = moves.lastIndexOf(QLatin1Char('.'));
    if (lastDot >= 0) {
        const QString code = moves.mid(lastDot + 1).toString();
        if (code.isEmpty() || TerminalCodes::isUsenCode(code)) {
            line.terminal = code;
            moves = moves.left(lastDot);
        }
    }

    for (qsizetype i = 0; i < moves.size(); i += 3) {
        const int moveNumber = static_cast<int>(i / 3) + 1;
        if (i + 3 > moves.size()) return fail(QStringLiteral("Incomplete USEN move at %1").arg(moveNumber));
        const std::optional<UsiMove> move = decodeMove(moves.mid(i, 3));
        if (!move)
            return fail(QStringLiteral("Move %1 '%2' could not be decoded").arg(moveNumber).arg(moves.mid(i, 3)));
        line.moves.append(*move);
    }
    return line;
}

std::optional<QList<UsenLine>> decodeLines(QStringView usen, QString* error)
{
    const qsizetype tilde = usen.indexOf(QLatin1Char('~'));
    if (tilde < 0) {
        if (error) *error = QStringLiteral("No '~' before the moves");
        return std::nullopt;
    }
    QList<UsenLine> lines;
    const QList<QStringView> parts = usen.mid(tilde + 1).split(QLatin1Char('~'), Qt::SkipEmptyParts);
    for (const QStringView part : parts) {
        std::optional<UsenLine> line = decodeLine(part, error);
        if (!line) return std::nullopt;
        lines.append(std::move(*line));
    }
    if (lines.isEmpty()) {
        if (error) *error = QStringLiteral("No moves");
        return std::nullopt;
    }
    return lines;
}

} // namespace UsenCodec
