#ifndef RECORDJSON_H
#define RECORDJSON_H

/// @file recordjson.h
/// @brief 棋譜の行の JSON（CLI の convert-kifu と自動化 API の kifu.get で共通）

#include <QJsonArray>
#include <QJsonObject>

#include "recordentry.h"
#include "recordline.h"

namespace RecordJson {

/// 棋譜の1行（指し手・終局）。ply・text、指し手なら usi、あれば time・comment・bookmark
QJsonObject entry(int ply, const RecordEntry& entry);
/// 手順の各行（ply は line.startPly から）
QJsonArray moves(const RecordLine& line);

} // namespace RecordJson

#endif // RECORDJSON_H
