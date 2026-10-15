/// @file sfenvalidationservice.cpp
/// @brief SFEN 文字列の妥当性検査の実装

#include "sfenvalidationservice.h"

#include "sfenutils.h"
#include "shogiboard.h"

#include <position.h>

#include <QJsonArray>

namespace {

const char* const kHandLetters = " PLNSGBR";

QMap<QString, int> handCounts(const shogi::Position& position, shogi::Color color)
{
    QMap<QString, int> counts;
    for (int kind = 1; kind <= 7; ++kind) {
        const int count = position.hand_count(color, static_cast<shogi::PieceType>(kind));
        if (count > 0) counts.insert(QString(QLatin1Char(kHandLetters[kind])), count);
    }
    return counts;
}

QJsonObject handToJson(const QMap<QString, int>& hand)
{
    QJsonObject obj;
    for (auto it = hand.cbegin(); it != hand.cend(); ++it) obj[it.key()] = it.value();
    return obj;
}

} // namespace

QJsonObject SfenValidation::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("valid")] = valid;
    obj[QStringLiteral("errors")] = QJsonArray::fromStringList(errors);
    if (!normalizedSfen.isEmpty()) obj[QStringLiteral("normalized_sfen")] = normalizedSfen;
    if (!turn.isEmpty()) obj[QStringLiteral("turn")] = turn;
    if (moveNumber > 0) obj[QStringLiteral("move_number")] = moveNumber;
    QJsonObject kings;
    kings[QStringLiteral("black")] = blackKing;
    kings[QStringLiteral("white")] = whiteKing;
    obj[QStringLiteral("kings")] = kings;
    obj[QStringLiteral("in_check")] = inCheck;
    obj[QStringLiteral("opponent_in_check")] = opponentInCheck;
    if (legalMoveCount >= 0) obj[QStringLiteral("legal_move_count")] = legalMoveCount;
    QJsonObject hands;
    hands[QStringLiteral("black")] = handToJson(blackHand);
    hands[QStringLiteral("white")] = handToJson(whiteHand);
    obj[QStringLiteral("hands")] = hands;
    return obj;
}

SfenValidation SfenValidationService::validate(const QString& sfenLike)
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

    const auto components = ShogiBoard::parseSfen(full);
    if (!components) {
        result.errors.append(QStringLiteral("Turn must be 'b' or 'w' and the move number a positive integer"));
        return result;
    }
    result.turn = turnToSfen(components->turn);
    result.moveNumber = components->moveNumber;

    shogi::Position position;
    if (!position.set_sfen(full.toStdString(), true)) {
        result.errors.append(QStringLiteral("Board or hand is invalid (unknown piece letter, wrong rank length, "
                                            "piece count over the limit, duplicated king, or both kings missing)"));
        return result;
    }

    result.blackKing = position.find_king(shogi::Color::Black) >= 0;
    result.whiteKing = position.find_king(shogi::Color::White) >= 0;
    result.blackHand = handCounts(position, shogi::Color::Black);
    result.whiteHand = handCounts(position, shogi::Color::White);

    const shogi::Color mover = position.side_to_move();
    const shogi::Color opponent = shogi::opposite(mover);
    result.inCheck = position.find_king(mover) >= 0 && position.is_in_check(mover);
    result.opponentInCheck = position.find_king(opponent) >= 0 && position.is_in_check(opponent);
    result.legalMoveCount = static_cast<int>(position.generate_legal_moves().size());
    result.normalizedSfen = QString::fromStdString(position.to_sfen());

    if (result.opponentInCheck) {
        result.errors.append(QStringLiteral("The side not to move is in check; the position cannot arise in a game"));
        return result;
    }
    result.valid = true;
    return result;
}

QString SfenValidationService::applyUsiMoves(const QString& sfenLike, const QStringList& moves, QString* error)
{
    const SfenValidation base = validate(sfenLike);
    if (!base.valid) {
        if (error) *error = base.errors.join(QStringLiteral("; "));
        return {};
    }
    shogi::Position position;
    position.set_sfen(base.normalizedSfen.toStdString(), true);
    for (const QString& move : moves) {
        if (!position.apply_usi_move(move.toStdString())) {
            if (error) *error = QStringLiteral("Illegal or malformed USI move: %1").arg(move);
            return {};
        }
    }
    return QString::fromStdString(position.to_sfen());
}
