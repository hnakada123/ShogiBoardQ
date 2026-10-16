#ifndef USIINFO_H
#define USIINFO_H

/// @file usiinfo.h
/// @brief USI の info 行を表す値型

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include "usiscore.h"

/**
 * @brief USI エンジンの info 行を解析した結果
 *
 * 未指定の項目は負の値（depth など）か空（currmove・pv・string）。multipv は省略時 1。
 * 盤面や GUI には依存しないので、対局・解析・自動化・CLI のどこからでも使える。
 */
struct UsiInfo {
    int depth = -1;
    int seldepth = -1;
    int multipv = 1;
    qint64 timeMs = -1;
    qint64 nodes = -1;
    qint64 nps = -1;
    int hashfull = -1;
    UsiScore score;
    QString currmove;      ///< 探索中の手（USI 形式）
    QStringList pv;        ///< 読み筋（USI 形式の指し手。resign などの語もそのまま入る）
    QString string;        ///< info string の本文

    bool hasPv() const { return !pv.isEmpty(); }
    bool hasScore() const { return score.kind != UsiScore::Kind::None; }

    /// JSON 表現（snake_case。未指定の項目は省く。CLI・MCP サーバーの出力形式）
    QJsonObject toJson() const;
};

Q_DECLARE_METATYPE(UsiInfo)

#endif // USIINFO_H
