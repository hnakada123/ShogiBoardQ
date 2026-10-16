#ifndef USISCORE_H
#define USISCORE_H

/// @file usiscore.h
/// @brief USI の評価値（score cp / score mate とその境界）

#include <QMetaType>
#include <QString>

/**
 * @brief 評価値（score cp / score mate とその境界）
 *
 * エンジンの info 行の値は手番側から見た値。先手から見た値にするときは negated() を使う。
 * 表示用の文字列はここでは作らない（表示する側のモデルが作る）。
 */
struct UsiScore {
    enum class Kind { None, Cp, Mate };
    enum class Bound { None, Lower, Upper };

    /// 詰みをグラフ・並べ替えで数として扱うときの値（やねうら王系の優等局面の値 28000 を
    /// 歩 90 点で centi-pawn に直した約 31111）
    static constexpr int kMateValue = 31111;

    Kind kind = Kind::None;
    Bound bound = Bound::None;
    /// Cp なら評価値（centipawn）。Mate なら詰みまでの手数（正が見ている側の勝ち、負が負け）。
    /// 手数を省く "mate +" / "mate -" は 0
    int value = 0;
    /// Mate で手数が示されたか（"mate +" / "mate -" は false）
    bool mateCountKnown = false;
    /// Mate の勝ち負け（見ている側の勝ちが true）。手数が 0 でなければ value の符号と同じ。
    /// "mate 0"（見ている側が詰んでいる）は false、それを相手側から見た値は true
    bool mateWin = false;

    static UsiScore cp(int centipawns, Bound bound = Bound::None)
    {
        UsiScore s;
        s.kind = Kind::Cp;
        s.value = centipawns;
        s.bound = bound;
        return s;
    }
    static UsiScore mate(int plies)
    {
        UsiScore s;
        s.kind = Kind::Mate;
        s.value = plies;
        s.mateCountKnown = true;
        s.mateWin = plies > 0;
        return s;
    }

    bool isNone() const { return kind == Kind::None; }
    bool isCp() const { return kind == Kind::Cp; }
    bool isMate() const { return kind == Kind::Mate; }

    /// 詰みの表記。"3" / "-3" / "0" / "+0" / "+" / "-"。Mate 以外は空
    QString mateText() const;

    /// 詰みの勝ち負け。+1 が見ている側の勝ち、-1 が負け。Mate 以外は 0
    int mateSign() const;

    /// 相手側から見た値（評価値と詰みの勝ち負けを反転し、上限・下限を入れ替える）
    UsiScore negated() const;

    /// グラフ・並べ替えに使う数（Cp は評価値、Mate は ±kMateValue、None は 0）
    int chartValue() const;

    bool operator==(const UsiScore& other) const
    {
        return kind == other.kind && bound == other.bound && value == other.value
            && mateCountKnown == other.mateCountKnown && mateWin == other.mateWin;
    }
    bool operator!=(const UsiScore& other) const { return !(*this == other); }
};

Q_DECLARE_METATYPE(UsiScore)

#endif // USISCORE_H
