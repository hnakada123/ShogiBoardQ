/// @file branchtreemanager.cpp
/// @brief 分岐ツリー管理クラスの実装（状態管理・ハイライト・イベント処理）

#include "branchtreemanager.h"
#include "logcategories.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QMouseEvent>
#include <QQueue>
#include <QSet>
#include <QTransform>

// ===================== コンストラクタ / デストラクタ =====================

BranchTreeManager::BranchTreeManager(QObject* parent)
    : QObject(parent)
{
}

BranchTreeManager::~BranchTreeManager()
{
    m_branchTreeViewport = nullptr;
}

// ===================== ビュー受け取り =====================

void BranchTreeManager::setView(QGraphicsView* view)
{
    m_branchTree = view;
    m_scene = new QGraphicsScene(m_branchTree);
    m_branchTree->setScene(m_scene);

    if (m_branchTree && m_branchTree->viewport()) {
        QWidget* vp = m_branchTree->viewport();
        if (!vp->property("branchFilterInstalled").toBool()) {
            m_branchTreeViewport = vp;
            vp->installEventFilter(this);
            vp->setProperty("branchFilterInstalled", true);
        }
    }

    rebuildBranchTree();
}

// ===================== 公開API =====================

void BranchTreeManager::setBranchTreeRows(const QList<ResolvedRowLite>& rows)
{
    m_rows = rows;
    rebuildBranchTree();
}

void BranchTreeManager::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    auto it = m_nodeIndex.find(qMakePair(row, ply));
    if (it != m_nodeIndex.end()) {
        highlightNodeId(it.value()->data(ROLE_NODE_ID).toInt(), centerOn);
        return;
    }

    const int nid = graphFallbackToPly(row, ply);
    if (nid > 0) {
        highlightNodeId(nid, centerOn);
    }
}

int BranchTreeManager::nodeIdFor(int row, int ply) const
{
    return m_nodeIdByRowPly.value(qMakePair(row, ply), -1);
}

// ===================== グラフAPI =====================

void BranchTreeManager::clearBranchGraph()
{
    m_nodeIdByRowPly.clear();
    m_nodesById.clear();
    m_prevIds.clear();
    m_nextIds.clear();
    m_rowEntryNode.clear();
    m_nextNodeId = 1;
    m_lastHighlightedRow = -1;
    m_lastHighlightedPly = -1;
}

int BranchTreeManager::registerNode(int vid, int row, int ply, QGraphicsPathItem* item)
{
    if (!item) return -1;
    const int id = m_nextNodeId++;

    BranchGraphNode n;
    n.id   = id;
    n.vid  = vid;
    n.row  = row;
    n.ply  = ply;
    n.item = item;

    m_nodesById.insert(id, n);
    m_nodeIdByRowPly.insert(qMakePair(row, ply), id);

    if (!m_rowEntryNode.contains(row))
        m_rowEntryNode.insert(row, id);

    return id;
}

void BranchTreeManager::linkEdge(int prevId, int nextId)
{
    if (prevId <= 0 || nextId <= 0) return;
    m_nextIds[prevId].push_back(nextId);
    m_prevIds[nextId].push_back(prevId);
}

// ===================== 親行解決 =====================

int BranchTreeManager::resolveParentRowForVariation(int row) const
{
    if (row < 1 || row >= m_rows.size()) {
        qCWarning(lcUi).noquote() << "[BranchTreeManager] resolveParentRowForVariation: row out of range"
                             << "row=" << row << "m_rows.size=" << m_rows.size();
        return 0;
    }

    const int p = m_rows.at(row).parent;
    if (p >= 0 && p < m_rows.size()) {
        return p;
    }

    return 0;
}

// ===================== ハイライト =====================

void BranchTreeManager::highlightNodeId(int nodeId, bool centerOn)
{
    if (nodeId <= 0) return;
    const auto node = m_nodesById.value(nodeId);
    QGraphicsPathItem* item = node.item;
    if (!item) return;

    if (m_prevSelected) {
        const auto argb = m_prevSelected->data(ROLE_ORIGINAL_BRUSH).toUInt();
        m_prevSelected->setBrush(QColor::fromRgba(argb));
        m_prevSelected->setPen(QPen(Qt::black, 1.2));
        m_prevSelected->setZValue(10);
        m_prevSelected = nullptr;
    }

    item->setBrush(QColor(255, 235, 80));
    item->setPen(QPen(Qt::black, 1.8));
    item->setZValue(20);
    m_prevSelected = item;
    m_lastHighlightedRow = node.row;
    m_lastHighlightedPly = node.ply;

    if (centerOn && m_branchTree) m_branchTree->centerOn(item);
}

// ===================== フォールバック探索 =====================

int BranchTreeManager::graphFallbackToPly(int row, int targetPly) const
{
    const int direct = nodeIdFor(row, targetPly);
    if (direct > 0) return direct;

    if (row >= 1 && row < m_rows.size()) {
        const int startPly = qMax(1, m_rows.at(row).startPly);
        if (targetPly < startPly) {
            const int parentRow = resolveParentRowForVariation(row);
            return graphFallbackToPly(parentRow, targetPly);
        }
    }

    int seedId = -1;
    for (int p = targetPly; p >= 0; --p) {
        seedId = nodeIdFor(row, p);
        if (seedId > 0) break;
    }
    if (seedId <= 0) {
        seedId = m_rowEntryNode.value(row, -1);
    }

    if (seedId > 0) {
        QQueue<int> q;
        QSet<int> seen;
        q.enqueue(seedId);
        seen.insert(seedId);

        while (!q.isEmpty()) {
            const int cur = q.dequeue();
            const auto node = m_nodesById.value(cur);
            if (node.ply == targetPly) return cur;

            const auto nexts = m_nextIds.value(cur);
            for (int nx : nexts) {
                if (!seen.contains(nx)) {
                    seen.insert(nx);
                    q.enqueue(nx);
                }
            }
        }
    }

    if (row >= 1 && row < m_rows.size()) {
        const int parentRow = resolveParentRowForVariation(row);
        if (parentRow != row) {
            const int viaParent = graphFallbackToPly(parentRow, targetPly);
            if (viaParent > 0) return viaParent;
        }
    }

    {
        int seed0 = nodeIdFor(0, targetPly);
        if (seed0 <= 0) seed0 = m_rowEntryNode.value(0, -1);
        if (seed0 > 0) {
            QQueue<int> q;
            QSet<int> seen;
            q.enqueue(seed0);
            seen.insert(seed0);
            while (!q.isEmpty()) {
                const int cur = q.dequeue();
                const auto node = m_nodesById.value(cur);
                if (node.ply == targetPly) return cur;
                const auto nexts = m_nextIds.value(cur);
                for (int nx : nexts) if (!seen.contains(nx)) { seen.insert(nx); q.enqueue(nx); }
            }
        }
    }

    return -1;
}

// ===================== クリック検出 =====================

bool BranchTreeManager::eventFilter(QObject* obj, QEvent* ev)
{
    if (!obj || ev->type() == QEvent::Destroy) {
        return QObject::eventFilter(obj, ev);
    }

    if (m_branchTreeViewport && obj == m_branchTreeViewport
        && ev->type() == QEvent::MouseButtonRelease)
    {
        if (!m_branchTreeClickEnabled) {
            return false;
        }

        auto* me = static_cast<QMouseEvent*>(ev);
        if (!(me->button() & Qt::LeftButton)) return QObject::eventFilter(obj, ev);

        const QPointF scenePt = m_branchTree->mapToScene(me->pos());
        QGraphicsItem* hit =
            m_branchTree->scene() ? m_branchTree->scene()->itemAt(scenePt, QTransform()) : nullptr;

        while (hit && !hit->data(BR_ROLE_KIND).isValid())
            hit = hit->parentItem();
        if (!hit) return false;

        const int row = hit->data(ROLE_ROW).toInt();
        const int ply = hit->data(ROLE_PLY).toInt();

        highlightBranchTreeAt(row, ply, /*centerOn=*/false);

        emit branchNodeActivated(row, ply);
        return true;
    }
    return QObject::eventFilter(obj, ev);
}
