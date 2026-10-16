#include "sfencsapositionconverter.h"
#include "csalexer.h"
#include "shogiboard.h"
#include "shoginotation.h"
#include "sfenutils.h"

#include <QChar>
#include <algorithm>
#include <optional>
#include <utility>

namespace SfenCsaPositionConverter {
namespace {

QString csaCodeFromSfenPiece(QChar piece, bool promoted)
{
    const Piece base = charToPiece(piece.toUpper());
    return ShogiNotation::csaPieceCode(promoted ? promote(base) : base);
}

} // namespace

std::optional<QString> fromCsaPositionLines(const QStringList& csaLines, QString* outError)
{
    if (outError) outError->clear();
    if (csaLines.isEmpty()) {
        if (outError) *outError = QStringLiteral("empty csa lines; using default start position");
        return SfenUtils::hirateSfen();
    }
    // 盤の行（PI か P1〜P9）がなければ局面を決められない
    const bool hasBoardRows = std::any_of(csaLines.cbegin(), csaLines.cend(), [](const QString& raw) {
        const QString line = raw.trimmed();
        return line.startsWith(QLatin1String("PI"))
            || (line.size() >= 2 && line.at(0) == QLatin1Char('P') && line.at(1) >= QLatin1Char('1')
                && line.at(1) <= QLatin1Char('9'));
    });
    if (!hasBoardRows) {
        if (outError) *outError = QStringLiteral("no board rows found in csa position lines");
        return std::nullopt;
    }
    // 棋譜ファイルの開始局面と同じ読み方（CsaLexer）にする
    ShogiBoard board;
    QString sfen;
    int index = 0;
    if (!CsaLexer::parseStartPos(csaLines, index, sfen, board)) {
        if (outError) *outError = QStringLiteral("invalid csa position lines");
        return std::nullopt;
    }
    return sfen;
}

std::optional<QStringList> toCsaPositionLines(const QString& sfen, QString* outError)
{
    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        if (outError) *outError = QStringLiteral("sfen is empty");
        return std::nullopt;
    }

    const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        if (outError) *outError = QStringLiteral("invalid sfen format");
        return std::nullopt;
    }

    const QStringList ranks = parts[0].split(QLatin1Char('/'));
    if (ranks.size() != 9) {
        if (outError) *outError = QStringLiteral("invalid rank count");
        return std::nullopt;
    }

    QStringList out;
    for (int rank = 0; rank < 9; ++rank) {
        QString line = QStringLiteral("P%1").arg(rank + 1);
        const QString row = ranks.at(rank);
        int file = 9;
        for (int i = 0; i < row.size() && file >= 1; ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                const int empties = ch.digitValue();
                for (int k = 0; k < empties; ++k) {
                    line += QStringLiteral(" * ");
                    --file;
                }
                continue;
            }

            bool promoted = false;
            QChar piece = ch;
            if (ch == QLatin1Char('+')) {
                if (i + 1 >= row.size()) {
                    if (outError) *outError = QStringLiteral("invalid promoted piece in sfen board");
                    return std::nullopt;
                }
                promoted = true;
                piece = row.at(++i);
            }

            const bool sente = piece.isUpper();
            const QString code = csaCodeFromSfenPiece(piece, promoted);
            if (code.isEmpty()) {
                if (outError) *outError = QStringLiteral("unsupported piece in sfen board");
                return std::nullopt;
            }
            line += sente ? QLatin1Char('+') : QLatin1Char('-');
            line += code;
            --file;
        }

        while (file >= 1) {
            line += QStringLiteral(" * ");
            --file;
        }
        out << line;
    }

    const QString hands = parts.at(2);
    if (hands != QStringLiteral("-")) {
        QString pPlus = QStringLiteral("P+");
        QString pMinus = QStringLiteral("P-");

        int count = 0;
        for (int i = 0; i < hands.size(); ++i) {
            const QChar ch = hands.at(i);
            if (ch.isDigit()) {
                count = count * 10 + ch.digitValue();
                continue;
            }

            int n = (count > 0) ? count : 1;
            count = 0;
            const bool sente = ch.isUpper();
            const QString code = csaCodeFromSfenPiece(ch, false);
            if (code.isEmpty()) {
                if (outError) *outError = QStringLiteral("unsupported hand piece in sfen");
                return std::nullopt;
            }
            QString& dst = sente ? pPlus : pMinus;
            for (int k = 0; k < n; ++k) {
                dst += QStringLiteral("00") + code;
            }
        }
        if (pPlus.size() > 2) out << pPlus;
        if (pMinus.size() > 2) out << pMinus;
    }

    out << ((parts.at(1) == QStringLiteral("w")) ? QStringLiteral("-") : QStringLiteral("+"));
    return out;
}

} // namespace SfenCsaPositionConverter
