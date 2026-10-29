/// @file sfenvalidator.cpp
/// @brief SFEN 文字列の妥当性検査の実装

#include "sfenvalidator.h"

#include "enginemovevalidator.h"
#include "piecerules.h"
#include "positionvalidator.h"
#include "sfenutils.h"

namespace {

/// 持ち駒の枚数（"R","B","G","S","N","L","P" → 枚数）
QMap<QString, int> handCounts(const Position& position, Turn side)
{
    QMap<QString, int> counts;
    for (const PieceKind kind : PieceRules::kHandOrder) {
        const int count = position.handCount(makePiece(side, kind));
        if (count > 0) counts.insert(pieceSfen(makePiece(Turn::Black, kind)), count);
    }
    return counts;
}

QString problemText(PositionValidator::Problem problem)
{
    using Problem = PositionValidator::Problem;
    switch (problem) {
    case Problem::PieceCountOver: return QStringLiteral("Piece count is over the limit (board and hands combined)");
    case Problem::KingInHand: return QStringLiteral("A king cannot be in hand");
    case Problem::DuplicatedKing: return QStringLiteral("A side has more than one king");
    case Problem::NoKing: return QStringLiteral("Both kings are missing");
    case Problem::OpponentInCheck: return QStringLiteral("The side not to move is in check; the position cannot arise in a game");
    case Problem::None: break;
    }
    return {};
}

} // namespace

SfenValidation SfenValidator::validate(const QString& sfenLike)
{
    SfenValidation result;
    const QString sfen = SfenUtils::normalizePositionLikeSfen(sfenLike);
    if (sfen.isEmpty()) {
        result.errors.append(QStringLiteral("SFEN is empty"));
        return result;
    }

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3 || parts.size() > 4) {
        result.errors.append(QStringLiteral("SFEN must have 3 or 4 fields: <board> <turn b|w> <hands or -> [move number]"));
        return result;
    }
    // 手数は省略可（既定 1）
    const QString full = parts.size() == 4 ? parts.join(QLatin1Char(' '))
                                           : parts.join(QLatin1Char(' ')) + QStringLiteral(" 1");

    bool numberOk = false;
    const int moveNumber = full.section(QLatin1Char(' '), 3, 3).toInt(&numberOk);
    const QString turn = full.section(QLatin1Char(' '), 1, 1);
    if ((turn != QLatin1String("b") && turn != QLatin1String("w")) || !numberOk || moveNumber <= 0) {
        result.errors.append(QStringLiteral("Turn must be 'b' or 'w' and the move number a positive integer"));
        return result;
    }
    result.turn = turn;
    result.moveNumber = moveNumber;

    const std::optional<Position> position = Position::fromSfen(full);
    if (!position) {
        result.errors.append(QStringLiteral("Board or hand is invalid (unknown piece letter, wrong rank length, "
                                            "or a piece that can never move again)"));
        return result;
    }
    const PositionValidator::Report report = PositionValidator::check(*position);
    result.blackKing = report.blackKing;
    result.whiteKing = report.whiteKing;
    if (!report.isWellFormed()) {
        result.errors.append(problemText(report.problem));
        return result;
    }
    result.blackHand = handCounts(*position, Turn::Black);
    result.whiteHand = handCounts(*position, Turn::White);
    result.inCheck = report.inCheck;
    result.opponentInCheck = report.opponentInCheck;
    result.legalMoveCount = report.legalMoveCount;
    result.normalizedSfen = position->toSfen();
    result.position = *position;
    if (!report.isValid()) {
        result.errors.append(problemText(report.problem));
        return result;
    }
    result.valid = true;
    return result;
}

QString SfenValidator::applyUsiMoves(const QString& sfenLike, const QStringList& moves, QString* error)
{
    const SfenValidation base = validate(sfenLike);
    if (!base.valid || !base.position) {
        if (error) *error = base.errors.join(QStringLiteral("; "));
        return {};
    }
    Position position = *base.position;
    for (const QString& text : moves) {
        const std::optional<UsiMove> move = UsiMove::parse(text);
        if (!move || !EngineMoveValidator::applyIfLegal(position, *move)) {
            if (error) *error = QStringLiteral("Illegal or malformed USI move: %1").arg(text);
            return {};
        }
    }
    return position.toSfen();
}
