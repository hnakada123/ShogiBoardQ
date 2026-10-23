#include "sfencsapositionconverter.h"
#include "csalexer.h"
#include "piecerules.h"
#include "shogiposition.h"
#include "shoginotation.h"
#include "sfenutils.h"

#include <QChar>
#include <algorithm>
#include <optional>
#include <utility>

namespace SfenCsaPositionConverter {

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
    Position board;
    int index = 0;
    if (!CsaLexer::parseStartPos(csaLines, index, board)) {
        if (outError) *outError = QStringLiteral("invalid csa position lines");
        return std::nullopt;
    }
    return board.toSfen();
}

QStringList toCsaPositionLines(const Position& position)
{
    // 盤（P1〜P9。各段は9筋から1筋へ）
    QStringList out;
    for (int rank = 1; rank <= 9; ++rank) {
        QString line = QStringLiteral("P%1").arg(rank);
        for (int file = 9; file >= 1; --file) {
            const Piece piece = position.pieceAt(file, rank);
            if (piece == Piece::None) {
                line += QStringLiteral(" * ");
            } else {
                line += isBlackPiece(piece) ? QLatin1Char('+') : QLatin1Char('-');
                line += ShogiNotation::csaPieceCode(piece);
            }
        }
        out << line;
    }

    // 持ち駒（P+00HI00KA... の形。飛・角・金・銀・桂・香・歩の順）
    for (const Turn side : {Turn::Black, Turn::White}) {
        QString line = side == Turn::Black ? QStringLiteral("P+") : QStringLiteral("P-");
        for (const PieceKind kind : PieceRules::kHandOrder) {
            const Piece piece = makePiece(side, kind);
            for (int k = 0; k < position.handCount(piece); ++k) line += QStringLiteral("00") + ShogiNotation::csaPieceCode(piece);
        }
        if (line.size() > 2) out << line;
    }

    out << (position.sideToMove() == Turn::White ? QStringLiteral("-") : QStringLiteral("+"));
    return out;
}

} // namespace SfenCsaPositionConverter
