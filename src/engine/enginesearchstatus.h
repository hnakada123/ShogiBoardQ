#ifndef ENGINESEARCHSTATUS_H
#define ENGINESEARCHSTATUS_H

/// @file enginesearchstatus.h
/// @brief エンジンの探索状況（info 1行の探索中の手・深さ・ノード数・NPS・ハッシュ使用率）

#include <QMetaType>
#include <QString>
#include <QtGlobal>

/**
 * @brief エンジンの探索状況（情報欄に出す値）
 *
 * 数値は数値のまま持ち、表示の書式（3桁区切り・百分率）は表示する側が決める。
 * info 1行ぶんを1回で知らせる。info に含まれなかった項目は「無し」（-1・空）で、受け手は前の値を残す。
 */
struct EngineSearchStatus {
    QString searchedMove;   ///< 探索中の手（表記済み。無ければ空）
    int depth = -1;         ///< 探索の深さ
    int seldepth = -1;      ///< 選択的探索の深さ
    qint64 nodes = -1;      ///< 探索したノード数
    qint64 nps = -1;        ///< 1秒あたりのノード数
    int hashfull = -1;      ///< ハッシュの使用率（パーミル。info に無ければ -1）

    /// next の項目のうち info に含まれていたものだけを重ねる（ハッシュの使用率は毎回置き換える）
    void merge(const EngineSearchStatus& next)
    {
        if (!next.searchedMove.isEmpty()) searchedMove = next.searchedMove;
        if (next.depth >= 0) {
            depth = next.depth;
            seldepth = next.seldepth;
        }
        if (next.nodes >= 0) nodes = next.nodes;
        if (next.nps >= 0) nps = next.nps;
        hashfull = next.hashfull;
    }

    bool operator==(const EngineSearchStatus& other) const
    {
        return searchedMove == other.searchedMove && depth == other.depth && seldepth == other.seldepth
            && nodes == other.nodes && nps == other.nps && hashfull == other.hashfull;
    }
    bool operator!=(const EngineSearchStatus& other) const { return !(*this == other); }
};

Q_DECLARE_METATYPE(EngineSearchStatus)

#endif // ENGINESEARCHSTATUS_H
