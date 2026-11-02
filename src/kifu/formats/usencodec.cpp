/// @file usencodec.cpp
/// @brief USEN の符号の符号化と復号の実装

#include "usencodec.h"

#include "handicaps.h"
#include "logcategories.h"
#include "terminalcodes.h"

#include <QRegularExpression>

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

/// 終局コードの欄（空・符号の1文字・反則の i と反則手の3文字）を line に読む。読めなければ false と error
bool readTerminal(QStringView field, UsenLine& line, QString* error)
{
    if (field.isEmpty()) return true;
    const QString code = field.left(1).toString();
    if (!TerminalCodes::isUsenCode(code)) {
        if (error) *error = QStringLiteral("Unknown terminal code '%1'").arg(field);
        return false;
    }
    const QStringView rest = field.mid(1);
    if (!rest.isEmpty()) {
        // 反則の符号だけが、後ろに反則手の3文字を持つ
        if (TerminalCodes::fromUsen(code) != TerminalType::IllegalLoss) {
            if (error) *error = QStringLiteral("Unexpected characters after the terminal code '%1'").arg(field);
            return false;
        }
        line.illegalMove = decodeMove(rest);
        if (!line.illegalMove) {
            if (error) *error = QStringLiteral("The illegal move '%1' could not be decoded").arg(rest);
            return false;
        }
    }
    line.terminal = code;
    return true;
}

} // namespace

bool hasUsenShape(const QString& text)
{
    // 開始局面（SFEN の '/' ' ' '+' を '_' '.' 'z' にしたもの）の後に、"~" で始まる手順が1本以上続く。
    // 終局コードの欄は英数字の並び（中身は readTerminal が確かめる）。コメントや URL の中のチルダは含まない
    static const QRegularExpression re(
        QStringLiteral("^[a-zA-Z0-9_.-]*(?:~[0-9]*\\.[a-zA-Z0-9]*(?:\\.[a-zA-Z0-9]*)?)+$"));
    return re.match(text).hasMatch();
}

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

    // 指し手の列の後の ".<終局コード>"（未終局は空の欄。欄が無くてもよい）
    QStringView moves = part.mid(dot + 1);
    const qsizetype terminalDot = moves.indexOf(QLatin1Char('.'));
    if (terminalDot >= 0) {
        QString terminalError;
        if (!readTerminal(moves.mid(terminalDot + 1), line, &terminalError)) return fail(terminalError);
        moves = moves.left(terminalDot);
    }

    // 読めない手（3文字に足りない末尾を含む）の手前まで読む
    for (qsizetype i = 0; i < moves.size(); i += 3) {
        const QStringView code = moves.mid(i, 3);
        const std::optional<UsiMove> move = decodeMove(code);
        if (!move) {
            line.unreadableMove = code.toString();
            break;
        }
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
