#ifndef RECORDJSON_H
#define RECORDJSON_H

/// @file recordjson.h
/// @brief 棋譜の行の JSON（CLI の convert-kifu と自動化 API の kifu.get で共通）

#include <QJsonArray>
#include <QJsonObject>

#include "kifuexport.h"
#include "recordentry.h"
#include "recordline.h"

namespace RecordJson {

/// 棋譜の1行（指し手・終局）。ply・text、指し手なら usi、あれば time・comment・bookmark
QJsonObject entry(int ply, const RecordEntry& entry);
/// 手順の各行（ply は line.startPly から）。開始局面のコメント・しおりは行にせず addOpening で書く
QJsonArray moves(const RecordLine& line);
/// 開始局面のコメント・しおりを obj の opening_comment・opening_bookmark に書く（空なら書かない）
void addOpening(QJsonObject& obj, const QString& comment, const QString& bookmark);
/// 形式に書けずに失ったものの語の列（variations・times・comments・bookmarks・terminals）。CLI の convert-kifu と kifu.save の lost
QJsonArray lost(const KifuExport::Loss& loss);

} // namespace RecordJson

#endif // RECORDJSON_H
