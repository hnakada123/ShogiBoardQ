/// @file jkfformatter.cpp
/// @brief JKF形式の変換ヘルパ関数の実装

#include "jkfformatter.h"
#include "jkfmoveparser.h"
#include "ki2movenotation.h"
#include "shogiboard.h"
#include "shoginotation.h"

#include <QJsonArray>
#include <QStringList>

namespace {

Piece pieceAt(const ShogiBoard& board, BoardSquare square)
{
    if (!square.isValid()) return Piece::None;
    return board.boardData().at((square.rank - 1) * 9 + (square.file - 1));
}

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

QJsonObject parseTimeToJkf(const QString& timeText)
{
    QJsonObject result;

    if (timeText.isEmpty()) return result;

    QString text = timeText;
    text.remove(QLatin1Char('('));
    text.remove(QLatin1Char(')'));
    text = text.trimmed();

    const QStringList parts = text.split(QLatin1Char('/'));
    if (!parts.empty()) {
        const QStringList nowParts = parts[0].split(QLatin1Char(':'));
        if (nowParts.size() >= 2) {
            QJsonObject now;
            now[QStringLiteral("m")] = nowParts[0].trimmed().toInt();
            now[QStringLiteral("s")] = nowParts[1].section(QLatin1Char('.'), 0, 0).trimmed().toInt();
            result[QStringLiteral("now")] = now;
        }
    }
    if (parts.size() >= 2) {
        const QStringList totalParts = parts[1].split(QLatin1Char(':'));
        if (totalParts.size() >= 3) {
            QJsonObject total;
            total[QStringLiteral("h")] = totalParts[0].trimmed().toInt();
            total[QStringLiteral("m")] = totalParts[1].trimmed().toInt();
            total[QStringLiteral("s")] = totalParts[2].section(QLatin1Char('.'), 0, 0).trimmed().toInt();
            result[QStringLiteral("total")] = total;
        }
    }

    return result;
}

QJsonObject sfenToJkfData(const QString& sfen)
{
    ShogiBoard board;
    if (!board.trySetSfen(sfen)) return {};

    // board[筋 - 1][段 - 1]。空きマスは空のオブジェクト
    QJsonArray columns;
    for (int file = 1; file <= 9; ++file) {
        QJsonArray column;
        for (int rank = 1; rank <= 9; ++rank) {
            const Piece piece = pieceAt(board, BoardSquare{file, rank});
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
    const QMap<Piece, int>& stand = board.pieceStand();
    for (auto it = stand.cbegin(); it != stand.cend(); ++it) {
        if (it.value() <= 0) continue;
        hands[isBlackPiece(it.key()) ? 0 : 1][ShogiNotation::csaPieceCode(it.key())] = it.value();
    }

    QJsonObject data;
    data[QStringLiteral("color")] = jkfColor(board.currentPlayer());
    data[QStringLiteral("board")] = columns;
    data[QStringLiteral("hands")] = QJsonArray{hands[0], hands[1]};
    return data;
}

QJsonObject moveToJkf(const ShogiBoard& board, Turn mover, const UsiMove& move, BoardSquare previousTo)
{
    const Piece moving = move.isDrop() ? move.drop : pieceAt(board, move.from);

    QJsonObject result;
    result[QStringLiteral("color")] = jkfColor(mover);
    if (!move.isDrop()) result[QStringLiteral("from")] = jkfSquare(move.from);
    result[QStringLiteral("to")] = jkfSquare(move.to);
    result[QStringLiteral("piece")] = ShogiNotation::csaPieceCode(moving);
    if (previousTo.isValid() && previousTo == move.to) result[QStringLiteral("same")] = true;
    if (!move.isDrop()) {
        const Piece captured = pieceAt(board, move.to);
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

QString terminalToJkfSpecial(TerminalType type, Turn mover)
{
    switch (type) {
    case TerminalType::Resign:         return QStringLiteral("TORYO");
    case TerminalType::Checkmate:      return QStringLiteral("TSUMI");
    case TerminalType::Repetition:     return QStringLiteral("SENNICHITE");
    case TerminalType::PerpetualCheck: return QStringLiteral("OUTE_SENNICHITE");
    case TerminalType::Impasse:        return QStringLiteral("JISHOGI");
    case TerminalType::Timeout:        return QStringLiteral("TIME_UP");
    // 手番側の反則勝ちは、相手（手番でない側）の反則
    case TerminalType::IllegalWin:
        return mover == Turn::Black ? QStringLiteral("-ILLEGAL_ACTION") : QStringLiteral("+ILLEGAL_ACTION");
    case TerminalType::IllegalLoss:    return QStringLiteral("ILLEGAL_MOVE");
    case TerminalType::Interrupt:      return QStringLiteral("CHUDAN");
    case TerminalType::NoCheckmate:    return QStringLiteral("FUZUMI");
    case TerminalType::DeclarationWin: return QStringLiteral("KACHI");
    case TerminalType::Draw:           return QStringLiteral("HIKIWAKE");
    case TerminalType::MaxMoves:       return QStringLiteral("MAX_MOVES");
    case TerminalType::Error:          return QStringLiteral("ERROR");
    case TerminalType::Forfeit:
    case TerminalType::None:
        break;
    }
    return {};
}

} // namespace JkfFormatter
