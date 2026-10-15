/// @file branchtreelayout.cpp
/// @brief 分岐ツリーの行（変化）の折りたたみと段の割り当ての実装

#include "branchtreelayout.h"

#include <QPair>
#include <QtGlobal>

bool BranchTreeLayout::isCollapsed(int row) const
{
    if (row < 1 || row >= rows.size()) return false;
    const int head = rows.at(row).headNodeId;
    return head >= 0 && collapsedHeads.contains(head);
}

bool BranchTreeLayout::isCollapsible(int row) const
{
    if (row < 1 || row >= rows.size()) return false;
    const Row& rv = rows.at(row);
    if (rv.headNodeId < 0) return false;
    if (rv.length - 1 > qMax(1, rv.startPly)) return true;
    for (qsizetype r = 1; r < rows.size(); ++r) {
        if (rows.at(r).parent == row) return true;
    }
    return false;
}

bool BranchTreeLayout::setCollapsed(int row, bool collapsed)
{
    if (row < 1 || row >= rows.size()) return false;
    const int head = rows.at(row).headNodeId;
    if (head < 0 || collapsed == collapsedHeads.contains(head)) return false;
    if (collapsed) collapsedHeads.insert(head);
    else collapsedHeads.remove(head);
    return true;
}

bool BranchTreeLayout::expandAll()
{
    if (collapsedHeads.isEmpty()) return false;
    collapsedHeads.clear();
    return true;
}

bool BranchTreeLayout::isHidden(int row) const
{
    // 祖先の変化が折りたたまれていれば隠す（循環に備えて回数を制限する）
    int current = (row >= 0 && row < rows.size()) ? rows.at(row).parent : -1;
    for (int guard = 0; current >= 1 && current < rows.size() && guard < rows.size(); ++guard) {
        if (isCollapsed(current)) return true;
        current = rows.at(current).parent;
    }
    return false;
}

int BranchTreeLayout::lastVisiblePly(int row) const
{
    if (row < 0 || row >= rows.size()) return -1;
    const Row& rv = rows.at(row);
    const int last = rv.length - 1;
    if (row >= 1 && isCollapsed(row)) return qMin(last, qMax(1, rv.startPly));
    return last;
}

int BranchTreeLayout::hiddenNodeCount(int row) const
{
    if (!isCollapsed(row)) return 0;
    const auto ownMoves = [this](int r) {
        const Row& rv = rows.at(r);
        return qMax(0, rv.length - qMax(1, rv.startPly));
    };
    int count = qMax(0, ownMoves(row) - 1);
    // 先の変化（この行を祖先に持つ行）の手も数える
    for (qsizetype r = 1; r < rows.size(); ++r) {
        int p = rows.at(r).parent;
        for (int guard = 0; p >= 1 && guard < rows.size(); ++guard) {
            if (p == row) {
                count += ownMoves(static_cast<int>(r));
                break;
            }
            p = rows.at(p).parent;
        }
    }
    return count;
}

bool BranchTreeLayout::reveal(int row, int ply)
{
    if (row < 1 || row >= rows.size()) return false;
    bool changed = false;
    // 自身が折りたたまれていて先頭より後の手を表示する場合
    if (isCollapsed(row) && ply > qMax(1, rows.at(row).startPly)) {
        collapsedHeads.remove(rows.at(row).headNodeId);
        changed = true;
    }
    // 祖先の変化の折りたたみ
    int current = rows.at(row).parent;
    for (int guard = 0; current >= 1 && current < rows.size() && guard < rows.size(); ++guard) {
        if (isCollapsed(current)) {
            collapsedHeads.remove(rows.at(current).headNodeId);
            changed = true;
        }
        current = rows.at(current).parent;
    }
    return changed;
}

int BranchTreeLayout::parentRow(int row) const
{
    if (row < 1 || row >= rows.size()) return 0;
    const int p = rows.at(row).parent;
    return (p >= 0 && p < rows.size()) ? p : 0;
}

int BranchTreeLayout::laneForRow(int row) const
{
    return (row >= 0 && row < m_rowLane.size()) ? m_rowLane.at(row) : -1;
}

void BranchTreeLayout::assignLanes(bool compact)
{
    m_rowLane = QList<int>(rows.size(), -1);
    m_laneCount = 1;
    if (rows.isEmpty()) return;

    m_rowLane[0] = 0;
    // 段ごとに使用中の手数の区間 [開始, 終了]
    QList<QList<QPair<int, int>>> occupied;
    occupied.append({qMakePair(0, qMax(0, lastVisiblePly(0)))});

    int sequentialLane = 0;
    for (qsizetype row = 1; row < rows.size(); ++row) {
        const int r = static_cast<int>(row);
        if (isHidden(r)) continue;
        const int startPly = qMax(1, rows.at(r).startPly);
        const int endPly = lastVisiblePly(r);
        if (endPly < startPly) continue;

        int lane = 0;
        if (!compact) {
            lane = ++sequentialLane;
        } else {
            // 親の手順より下で、手数の区間が他の変化と重ならない最初の段に置く。
            // 隣り合う手数に別の変化があると続いた手順に見えるため、1列空ける
            const int parentLane = qMax(0, laneForRow(parentRow(r)));
            lane = parentLane + 1;
            for (;; ++lane) {
                if (lane >= occupied.size()) break;
                bool overlaps = false;
                for (const auto& interval : std::as_const(occupied.at(lane))) {
                    if (startPly <= interval.second + 1 && interval.first <= endPly + 1) {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps) break;
            }
        }
        while (occupied.size() <= lane) occupied.append(QList<QPair<int, int>>());
        occupied[lane].append(qMakePair(startPly, endPly));
        m_rowLane[r] = lane;
        m_laneCount = qMax(m_laneCount, lane + 1);
    }
}
