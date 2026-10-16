/// @file jkfformatter.cpp
/// @brief JKF形式の変換ヘルパ関数の実装

#include "jkfformatter.h"
#include "jkfmoveparser.h"
#include "ki2movenotation.h"
#include "shogiposition.h"
#include "shoginotation.h"

#include <QJsonArray>
#include <QStringList>

namespace {

QJsonObject jkfSquare(BoardSquare square)
{
    return QJsonObject{{QStringLiteral("x"), square.file}, {QStringLiteral("y"), square.rank}};
}

int jkfColor(Turn side)
{
    return side == Turn::Black ? 0 : 1;
}

} // namespace

namespace JkfFormatter {

QJsonObject timeToJkf(const RecordTime& time)
{
    if (!time.valid) return {};
    const qint64 elapsed = time.elapsedMs / 1000;
    const qint64 total = time.totalMs / 1000;
    return {{QStringLiteral("now"), QJsonObject{{QStringLiteral("m"), elapsed / 60},
                                               {QStringLiteral("s"), elapsed % 60}}},
            {QStringLiteral("total"), QJsonObject{{QStringLiteral("h"), total / 3600},
                                                 {QStringLiteral("m"), total / 60 % 60},
                                                 {QStringLiteral("s"), total % 60}}}};
}

QJsonObject sfenToJkfData(const QString& sfen)
{
    const std::optional<Position> parsed = Position::fromSfen(sfen);
    if (!parsed) return {};
    const Position& board = *parsed;

    // board[筋 - 1][段 - 1]。空きマスは空のオブジェクト
    QJsonArray columns;
    for (int file = 1; file <= 9; ++file) {
        QJsonArray column;
        for (int rank = 1; rank <= 9; ++rank) {
            const Piece piece = board.pieceAt(BoardSquare{file, rank});
            if (piece == Piece::None) {
                column.append(QJsonObject());
            } else {
                column.append(QJsonObject{
                    {QStringLiteral("color"), jkfColor(isBlackPiece(piece) ? Turn::Black : Turn::White)},
                    {QStringLiteral("kind"), ShogiNotation::csaPieceCode(piece)}});
            }
        }
        columns.append(column);
    }

    QJsonObject hands[2];
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (int kind = 0; kind < kHandKindCount; ++kind) {
            const Piece piece = makePiece(side, static_cast<PieceKind>(kind));
            const int count = board.handCount(piece);
            if (count > 0) hands[side == Turn::Black ? 0 : 1][ShogiNotation::csaPieceCode(piece)] = count;
        }
    }

    QJsonObject data;
    data[QStringLiteral("color")] = jkfColor(board.sideToMove());
    data[QStringLiteral("board")] = columns;
    data[QStringLiteral("hands")] = QJsonArray{hands[0], hands[1]};
    return data;
}

QJsonObject moveToJkf(const Position& board, Turn mover, const UsiMove& move, BoardSquare previousTo)
{
    const Piece moving = move.isDrop() ? move.drop : board.pieceAt(move.from);

    QJsonObject result;
    result[QStringLiteral("color")] = jkfColor(mover);
    if (!move.isDrop()) result[QStringLiteral("from")] = jkfSquare(move.from);
    result[QStringLiteral("to")] = jkfSquare(move.to);
    result[QStringLiteral("piece")] = ShogiNotation::csaPieceCode(moving);
    if (previousTo.isValid() && previousTo == move.to) result[QStringLiteral("same")] = true;
    if (!move.isDrop()) {
        const Piece captured = board.pieceAt(move.to);
        if (captured != Piece::None) result[QStringLiteral("capture")] = ShogiNotation::csaPieceCode(captured);
    }

    const Ki2MoveNotation::Annotation annotation = Ki2MoveNotation::annotate(board, mover, move);
    if (move.promote) {
        result[QStringLiteral("promote")] = true;
    } else if (annotation.canPromote) {
        result[QStringLiteral("promote")] = false;
    }
    const QString relative = JkfMoveParser::modifierToRelative(annotation.modifiers);
    if (!relative.isEmpty()) result[QStringLiteral("relative")] = relative;
    return result;
}

} // namespace JkfFormatter
