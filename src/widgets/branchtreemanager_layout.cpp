/// @file branchtreemanager_layout.cpp
/// @brief BranchTreeManager の段の割り当て（詰めた配置）と変化の折りたたみ

#include "branchtreemanager.h"

#include <QGraphicsPathItem>

namespace {
constexpr qreal kBaseY  = 40.0;
constexpr qreal kStepY  = 56.0;
} // namespace

bool BranchTreeManager::isRowCollapsed(int row) const
{
    if (row < 1 || row >= m_rows.size()) return false;
    const int head = m_rows.at(row).headNodeId;
    return head >= 0 && m_collapsedHeads.contains(head);
}

bool BranchTreeManager::isRowCollapsible(int row) const
{
    if (row < 1 || row >= m_rows.size()) return false;
    const ResolvedRowLite& rv = m_rows.at(row);
    if (rv.headNodeId < 0) return false;
    if (rv.disp.size() - 1 > qMax(1, rv.startPly)) return true;
    for (qsizetype r = 1; r < m_rows.size(); ++r) {
        if (m_rows.at(r).parent == row) return true;
    }
    return false;
}

void BranchTreeManager::setRowCollapsed(int row, bool collapsed)
{
    if (row < 1 || row >= m_rows.size()) return;
    const int head = m_rows.at(row).headNodeId;
    if (head < 0) return;
    if (collapsed == m_collapsedHeads.contains(head)) return;
    if (collapsed) {
        m_collapsedHeads.insert(head);
    } else {
        m_collapsedHeads.remove(head);
    }

    // 現在の手が隠れる場合は、折りたたんだ変化の先頭の手を強調する
    int hlRow = m_lastHighlightedRow;
    int hlPly = m_lastHighlightedPly;
    rebuildBranchTree();
    if (hlRow >= 0 && hlPly >= 0) {
        if (!m_nodeIndex.contains(qMakePair(hlRow, hlPly))) {
            hlRow = row;
            hlPly = qMax(1, m_rows.at(row).startPly);
        }
        const auto it = m_nodeIndex.constFind(qMakePair(hlRow, hlPly));
        if (it != m_nodeIndex.cend()) highlightNodeId(it.value()->data(ROLE_NODE_ID).toInt(), false);
    }
}

void BranchTreeManager::expandAll()
{
    if (m_collapsedHeads.isEmpty()) return;
    m_collapsedHeads.clear();
    const int row = m_lastHighlightedRow;
    const int ply = m_lastHighlightedPly;
    rebuildBranchTree();
    if (row >= 0 && ply >= 0) highlightBranchTreeAt(row, ply, false);
}

bool BranchTreeManager::isRowHidden(int row) const
{
    // 祖先の変化が折りたたまれていれば隠す（循環に備えて回数を制限する）
    int current = (row >= 0 && row < m_rows.size()) ? m_rows.at(row).parent : -1;
    for (int guard = 0; current >= 1 && current < m_rows.size() && guard < m_rows.size(); ++guard) {
        if (isRowCollapsed(current)) return true;
        current = m_rows.at(current).parent;
    }
    return false;
}

int BranchTreeManager::lastVisiblePly(int row) const
{
    if (row < 0 || row >= m_rows.size()) return -1;
    const ResolvedRowLite& rv = m_rows.at(row);
    const int last = static_cast<int>(rv.disp.size()) - 1;
    if (row >= 1 && isRowCollapsed(row)) return qMin(last, qMax(1, rv.startPly));
    return last;
}

int BranchTreeManager::hiddenNodeCount(int row) const
{
    if (!isRowCollapsed(row)) return 0;
    const auto ownMoves = [this](int r) {
        const ResolvedRowLite& rv = m_rows.at(r);
        return qMax(0, static_cast<int>(rv.disp.size()) - qMax(1, rv.startPly));
    };
    int count = qMax(0, ownMoves(row) - 1);
    // 先の変化（この行を祖先に持つ行）の手も数える
    for (qsizetype r = 1; r < m_rows.size(); ++r) {
        int p = m_rows.at(r).parent;
        for (int guard = 0; p >= 1 && guard < m_rows.size(); ++guard) {
            if (p == row) {
                count += ownMoves(static_cast<int>(r));
                break;
            }
            p = m_rows.at(p).parent;
        }
    }
    return count;
}

bool BranchTreeManager::revealNode(int row, int ply)
{
    if (row < 1 || row >= m_rows.size()) return false;
    bool changed = false;
    // 自身が折りたたまれていて先頭より後の手を表示する場合
    if (isRowCollapsed(row) && ply > qMax(1, m_rows.at(row).startPly)) {
        m_collapsedHeads.remove(m_rows.at(row).headNodeId);
        changed = true;
    }
    // 祖先の変化の折りたたみ
    int current = m_rows.at(row).parent;
    for (int guard = 0; current >= 1 && current < m_rows.size() && guard < m_rows.size(); ++guard) {
        if (isRowCollapsed(current)) {
            m_collapsedHeads.remove(m_rows.at(current).headNodeId);
            changed = true;
        }
        current = m_rows.at(current).parent;
    }
    return changed;
}

int BranchTreeManager::laneForRow(int row) const
{
    return (row >= 0 && row < m_rowLane.size()) ? m_rowLane.at(row) : -1;
}

qreal BranchTreeManager::laneY(int lane) const
{
    return kBaseY + qMax(0, lane) * kStepY;
}

void BranchTreeManager::computeLayout()
{
    m_rowLane = QList<int>(m_rows.size(), -1);
    m_laneCount = 1;
    if (m_rows.isEmpty()) return;

    m_rowLane[0] = 0;
    // 段ごとに使用中の手数の区間 [開始, 終了]
    QList<QList<QPair<int, int>>> occupied;
    occupied.append({qMakePair(0, qMax(0, lastVisiblePly(0)))});

    int sequentialLane = 0;
    for (qsizetype row = 1; row < m_rows.size(); ++row) {
        const int r = static_cast<int>(row);
        if (isRowHidden(r)) continue;
        const int startPly = qMax(1, m_rows.at(r).startPly);
        const int endPly = lastVisiblePly(r);
        if (endPly < startPly) continue;

        int lane = 0;
        if (!m_compactLayout) {
            lane = ++sequentialLane;
        } else {
            // 親の手順より下で、手数の区間が他の変化と重ならない最初の段に置く。
            // 隣り合う手数に別の変化があると続いた手順に見えるため、1列空ける
            const int parentLane = qMax(0, laneForRow(resolveParentRowForVariation(r)));
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
