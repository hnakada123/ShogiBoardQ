/// @file usencodec.cpp
/// @brief USEN の符号の符号化と復号の実装

#include "usencodec.h"

#include "logcategories.h"
#include "sfenutils.h"
#include "terminalcodes.h"

namespace UsenCodec {

namespace {

constexpr QStringView kBase36Chars = u"0123456789abcdefghijklmnopqrstuvwxyz";
constexpr int kMaxCode = 36 * 36 * 36 - 1;
constexpr int kDropBase = 81;

struct DropPiece {
    int code;
    char usi;
};
constexpr DropPiece kDropPieces[] = {{9, 'G'}, {10, 'P'}, {11, 'L'}, {12, 'N'}, {13, 'S'}, {14, 'B'}, {15, 'R'}};

int dropCodeOf(QChar usiPiece)
{
    const char c = usiPiece.toUpper().toLatin1();
    for (const DropPiece& piece : kDropPieces)
        if (piece.usi == c) return piece.code;
    return -1;
}

QChar dropPieceOf(int code)
{
    for (const DropPiece& piece : kDropPieces)
        if (piece.code == code) return QLatin1Char(piece.usi);
    return {};
}

/// USI のマス（"7g"）を USEN のマス番号にする（読めなければ -1）
int squareOf(QChar file, QChar rank)
{
    const int f = file.toLatin1() - '0';
    const int r = rank.toLatin1() - 'a' + 1;
    if (f < 1 || f > 9 || r < 1 || r > 9) return -1;
    return (r - 1) * 9 + (f - 1);
}

QString squareText(int square)
{
    return QString::number(square % 9 + 1) + QLatin1Char(static_cast<char>('a' + square / 9));
}

} // namespace

QString encodeMove(const QString& usiMove)
{
    if (usiMove.size() < 4) return {};
    const bool promotion = usiMove.endsWith(QLatin1Char('+'));
    int from = -1;
    if (usiMove.at(1) == QLatin1Char('*')) {
        const int code = dropCodeOf(usiMove.at(0));
        if (code >= 0) from = kDropBase + code;
    } else {
        from = squareOf(usiMove.at(0), usiMove.at(1));
    }
    const int to = squareOf(usiMove.at(2), usiMove.at(3));
    if (from < 0 || to < 0) {
        qCWarning(lcKifu) << "USEN: cannot encode move" << usiMove;
        return {};
    }
    int code = (from * 81 + to) * 2 + (promotion ? 1 : 0);
    QString text(3, QLatin1Char('0'));
    for (int i = 2; i >= 0; --i) {
        text[i] = kBase36Chars.at(code % 36);
        code /= 36;
    }
    return text;
}

QString decodeMove(QStringView threeChars)
{
    if (threeChars.size() != 3) return {};
    int code = 0;
    for (const QChar c : threeChars) {
        const qsizetype digit = kBase36Chars.indexOf(c.toLower());
        if (digit < 0) return {};
        code = code * 36 + static_cast<int>(digit);
    }
    if (code > kMaxCode) return {};
    const bool promotion = (code % 2 == 1);
    const int from = (code / 2) / 81;
    const int to = (code / 2) % 81;
    if (from >= kDropBase) {
        const QChar piece = dropPieceOf(from - kDropBase);
        if (piece.isNull()) {
            qCWarning(lcKifu) << "USEN: unknown drop code" << threeChars;
            return {};
        }
        return piece + QStringLiteral("*") + squareText(to);
    }
    return squareText(from) + squareText(to) + (promotion ? QStringLiteral("+") : QString());
}

QString encodePosition(const QString& sfen)
{
    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty() || SfenUtils::isHirateStart(trimmed)) return {};
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

UsenDecodeResult decodeMoves(const QString& usen, QString* terminal)
{
    UsenDecodeResult result;
    QString moves = usen;
    // "~X." の前置きを除く
    if (moves.startsWith(QLatin1Char('~'))) {
        const qsizetype dot = moves.indexOf(QLatin1Char('.'));
        if (dot >= 0) moves = moves.mid(dot + 1);
    }
    // 終局コードを除く
    if (terminal) terminal->clear();
    const qsizetype lastDot = moves.lastIndexOf(QLatin1Char('.'));
    if (lastDot >= 0) {
        const QString code = moves.mid(lastDot + 1);
        if (code.isEmpty() || TerminalCodes::isUsenCode(code)) {
            if (terminal) *terminal = code;
            moves = moves.left(lastDot);
        }
    }

    qsizetype i = 0;
    int moveNumber = 0;
    for (; i + 2 < moves.size(); i += 3) {
        ++moveNumber;
        const QStringView threeChars = QStringView(moves).mid(i, 3);
        const QString usi = decodeMove(threeChars);
        if (!usi.isEmpty()) {
            result.moves.append(usi);
            continue;
        }
        // 無効な指し手を下流へ渡さず、棋譜全体を失敗として扱う
        ++result.invalidCount;
        if (result.firstError.isEmpty())
            result.firstError = QStringLiteral("Move %1 '%2' could not be decoded").arg(moveNumber).arg(threeChars);
    }
    if (i != moves.size()) {
        ++result.invalidCount;
        if (result.firstError.isEmpty())
            result.firstError = QStringLiteral("Incomplete USEN move at %1").arg(moveNumber + 1);
    }
    if (result.invalidCount > 0) result.moves.clear();
    return result;
}

} // namespace UsenCodec
