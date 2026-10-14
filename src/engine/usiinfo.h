#ifndef USIINFO_H
#define USIINFO_H

/// @file usiinfo.h
/// @brief USI の info 行を表す値型

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>

/**
 * @brief info 行の評価値（score cp / score mate とその境界）
 */
struct UsiScore {
    enum class Kind { None, Cp, Mate };
    enum class Bound { None, Lower, Upper };

    Kind kind = Kind::None;
    Bound bound = Bound::None;
    /// Cp なら評価値（centipawn）。Mate なら詰みまでの手数（正が手番側の勝ち、負が負け）。
    /// 手数を省く "mate +" / "mate -" は 0
    int value = 0;
    /// Mate で手数が示されたか（"mate +" / "mate -" は false）
    bool mateCountKnown = false;
    /// Mate で手数のないときの勝ち負け（"mate +" が true）。手数があるときは value の符号と同じ
    bool mateWin = false;

    bool isCp() const { return kind == Kind::Cp; }
    bool isMate() const { return kind == Kind::Mate; }

    /// 詰みの表記。"3" / "-3" / "+" / "-"。Mate 以外は空
    QString mateText() const;

    /// 詰みの勝ち負け。+1 が手番側の勝ち、-1 が負け。"mate 0" と Mate 以外は 0
    int mateSign() const;
};

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
