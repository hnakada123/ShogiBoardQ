#ifndef SFENVALIDATIONJSON_H
#define SFENVALIDATIONJSON_H

/// @file sfenvalidationjson.h
/// @brief SFEN の検査結果の JSON 表現（CLI の validate-sfen）

#include <QJsonObject>

struct SfenValidation;

namespace SfenValidationJson {

/// 検査結果を snake_case の JSON にする（valid・errors・normalized_sfen・turn・move_number・kings・
/// in_check・opponent_in_check・legal_move_count・hands）
QJsonObject toJson(const SfenValidation& validation);

} // namespace SfenValidationJson

#endif // SFENVALIDATIONJSON_H
