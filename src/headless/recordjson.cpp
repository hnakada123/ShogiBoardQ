/// @file recordjson.cpp
/// @brief 棋譜の行の JSON の実装

#include "recordjson.h"

QJsonObject RecordJson::entry(int ply, const RecordEntry& entry)
{
    QJsonObject obj;
    obj[QStringLiteral("ply")] = ply;
    obj[QStringLiteral("text")] = entry.displayText;
    if (!entry.isTerminal() && entry.move.move.isValid()) obj[QStringLiteral("usi")] = entry.move.move.toString();
    if (const QString time = entry.time.toText(); !time.isEmpty()) obj[QStringLiteral("time")] = time;
    if (!entry.comment.isEmpty()) obj[QStringLiteral("comment")] = entry.comment;
    if (!entry.bookmark.isEmpty()) obj[QStringLiteral("bookmark")] = entry.bookmark;
    return obj;
}

QJsonArray RecordJson::moves(const RecordLine& line)
{
    QJsonArray array;
    for (qsizetype i = 0; i < line.entries.size(); ++i)
        array.append(entry(line.startPly + static_cast<int>(i), line.entries.at(i)));
    return array;
}
