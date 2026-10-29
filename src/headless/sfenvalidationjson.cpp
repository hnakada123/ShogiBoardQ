/// @file sfenvalidationjson.cpp
/// @brief SFEN の検査結果の JSON 表現の実装

#include "sfenvalidationjson.h"

#include "sfenvalidator.h"

#include <QJsonArray>

namespace {

QJsonObject handToJson(const QMap<QString, int>& hand)
{
    QJsonObject obj;
    for (auto it = hand.cbegin(); it != hand.cend(); ++it) obj[it.key()] = it.value();
    return obj;
}

} // namespace

QJsonObject SfenValidationJson::toJson(const SfenValidation& validation)
{
    QJsonObject obj;
    obj[QStringLiteral("valid")] = validation.valid;
    obj[QStringLiteral("errors")] = QJsonArray::fromStringList(validation.errors);
    if (!validation.normalizedSfen.isEmpty()) obj[QStringLiteral("normalized_sfen")] = validation.normalizedSfen;
    if (!validation.turn.isEmpty()) obj[QStringLiteral("turn")] = validation.turn;
    if (validation.moveNumber > 0) obj[QStringLiteral("move_number")] = validation.moveNumber;
    QJsonObject kings;
    kings[QStringLiteral("black")] = validation.blackKing;
    kings[QStringLiteral("white")] = validation.whiteKing;
    obj[QStringLiteral("kings")] = kings;
    obj[QStringLiteral("in_check")] = validation.inCheck;
    obj[QStringLiteral("opponent_in_check")] = validation.opponentInCheck;
    if (validation.legalMoveCount >= 0) obj[QStringLiteral("legal_move_count")] = validation.legalMoveCount;
    QJsonObject hands;
    hands[QStringLiteral("black")] = handToJson(validation.blackHand);
    hands[QStringLiteral("white")] = handToJson(validation.whiteHand);
    obj[QStringLiteral("hands")] = hands;
    return obj;
}
