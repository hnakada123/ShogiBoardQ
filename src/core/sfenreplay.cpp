/// @file sfenreplay.cpp
/// @brief 開始局面から USI の手を順に指して局面列を作る

#include "sfenreplay.h"

#include "sfenutils.h"
#include "shogiboard.h"
#include "usimove.h"

namespace SfenReplay {

bool applyLenient(ShogiBoard& board, const QString& usiMove)
{
    const auto move = UsiMove::parse(usiMove);
    if (!move) return false;
    const Turn mover = board.currentPlayer();
    if (!move->isDrop()) return board.applyUsiMove(*move, mover);

    // 棋譜の読み込みでは、持っていない駒を打つ手・駒のあるマスへ打つ手も読み進める
    // （持ち駒があれば1枚減らし、マスの駒は打った駒で置き換える）
    const Piece piece = move->dropPieceFor(mover);
    if (board.pieceStandCount(piece) > 0) board.consumeStandPiece(piece);
    board.movePieceToSquare(piece, 0, 0, move->to.file, move->to.rank, false);
    board.setCurrentPlayer(opponent(mover));
    board.setMoveNumber(board.moveNumber() + 1);
    return true;
}

QString positionAfter(const QString& sfen, const QString& usiMove)
{
    ShogiBoard board;
    if (!board.trySetSfen(sfen) || !applyLenient(board, usiMove)) return {};
    return board.toSfen();
}

Result replay(const QString& initialSfen, const QStringList& usiMoves)
{
    Result result;
    ShogiBoard board;
    if (!board.trySetSfen(initialSfen)) {
        result.failedPly = 0;
        result.error = QStringLiteral("Invalid initial SFEN: %1").arg(initialSfen);
        return result;
    }
    result.initialSfen = board.toSfen();
    for (const QString& text : usiMoves) {
        const auto usi = UsiMove::parse(text);
        const Turn mover = board.currentPlayer();
        Step step;
        step.beforeSfen = board.toSfen();
        if (usi) {
            const Piece moving = usi->isDrop() ? usi->dropPieceFor(mover) : board.pieceAt(usi->from);
            step.move = ShogiMove(*usi, moving, board.pieceAt(usi->to));
        }
        const bool emptySource = usi && !usi->isDrop() && step.move.movingPiece == Piece::None;
        if (!usi || emptySource || !applyLenient(board, text)) {
            result.failedPly = static_cast<int>(result.moves.size()) + 1;
            result.error = !usi ? QStringLiteral("Invalid USI move: %1").arg(text)
                : emptySource ? QStringLiteral("Source square is empty: %1").arg(text)
                              : QStringLiteral("Move cannot be applied: %1").arg(text);
            return result;
        }
        step.sfen = board.toSfen();
        result.moves.append(step);
    }
    return result;
}

QString sfenFromPositionCommand(const QString& positionCommand)
{
    const QStringList tokens = positionCommand.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.size() < 2 || tokens.at(0) != QStringLiteral("position")) return {};
    ShogiBoard board;
    qsizetype movesIndex = -1;
    if (tokens.at(1) == QStringLiteral("startpos")) {
        (void)board.trySetSfen(SfenUtils::hirateSfen());
        movesIndex = tokens.indexOf(QStringLiteral("moves"), 2);
    } else if (tokens.at(1) == QStringLiteral("sfen")) {
        if (tokens.size() < 6 || !board.trySetSfen(tokens.mid(2, 4).join(QLatin1Char(' ')))) return {};
        movesIndex = tokens.indexOf(QStringLiteral("moves"), 6);
    } else {
        return {};
    }
    if (movesIndex >= 0) {
        for (qsizetype i = movesIndex + 1; i < tokens.size(); ++i) {
            if (!applyLenient(board, tokens.at(i))) return {};
        }
    }
    return board.toSfen();
}

} // namespace SfenReplay
