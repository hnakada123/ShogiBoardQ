#ifndef BRANCHTREELAYOUT_H
#define BRANCHTREELAYOUT_H

/// @file branchtreelayout.h
/// @brief 分岐ツリーの行（変化）の折りたたみと段の割り当て（描画に依存しない値型）

#include <QList>
#include <QSet>

/**
 * @brief 分岐ツリーの行（変化）ごとの配置
 *
 * 行 0 は本譜、行 1 以降は変化。折りたたんだ変化は先頭の手だけを残し、続きの手と
 * その先の変化を隠す。段（画面の縦位置）は、手数の区間が重ならない変化を同じ段に詰めて割り当てる。
 * 描画（BranchTreeView）はこの結果を読むだけにする。
 */
struct BranchTreeLayout {
    /// 行（ライン）の配置情報。手順そのものはツリーから引く
    struct Row {
        int startPly = 1;      ///< この行に描く最初の手数（変化の先頭）
        int parent   = -1;     ///< 分岐元の行
        int headNodeId = -1;   ///< 変化の先頭の手の KifuBranchNode ID（折りたたみ状態の保持に使う）
        int length = 0;        ///< 開始局面を含む手順の長さ
    };

    QList<Row> rows;
    QSet<int> collapsedHeads;  ///< 折りたたんだ変化の先頭ノードID（行を作り直しても状態を保つ）

    // --- 折りたたみ ---
    bool isCollapsed(int row) const;
    /// 折りたためる行か（本譜以外で2手以上あるか、先の変化を持つ）
    bool isCollapsible(int row) const;
    /// 折りたたみ状態を変える。変わったら true
    bool setCollapsed(int row, bool collapsed);
    /// すべて展開する。変わったら true
    bool expandAll();
    /// 祖先の変化が折りたたまれていて描画しない行か
    bool isHidden(int row) const;
    /// 行の描画する最後の手数（行が無ければ -1）
    int lastVisiblePly(int row) const;
    /// 折りたたみで隠れている手の数（先の変化の手を含む）
    int hiddenNodeCount(int row) const;
    /// 指定の手が隠れていれば折りたたみを解除する。解除したら true
    bool reveal(int row, int ply);

    // --- 段 ---
    /// 分岐元の行（範囲外なら本譜の 0）
    int parentRow(int row) const;
    /// 段を割り当てる。compact なら手数の区間が重ならない変化を空いた段に詰め、そうでなければ1変化1段
    void assignLanes(bool compact);
    /// 行の段（隠れている・割り当てていない行は -1）
    int laneForRow(int row) const;
    int laneCount() const { return m_laneCount; }

private:
    QList<int> m_rowLane;      ///< 行 → 段
    int m_laneCount = 1;
};

#endif // BRANCHTREELAYOUT_H
