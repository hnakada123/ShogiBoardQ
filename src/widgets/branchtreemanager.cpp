/// @file branchtreemanager.cpp
/// @brief 分岐ツリー管理クラスの実装（状態管理・ハイライト・イベント処理）

#include "branchtreemanager.h"
#include "analysissettings.h"
#include "logcategories.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QQueue>
#include <QSet>
#include <QTransform>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <climits>

namespace {
const QPen kEdgePen(QColor(90, 90, 90), 1.0);
const QPen kPathEdgePen(QColor(40, 110, 210), 2.6);
} // namespace

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
    // 矢印キーで手を移動できるようにする
    m_branchTree->setFocusPolicy(Qt::StrongFocus);

    if (m_branchTree && m_branchTree->viewport()) {
        QWidget* vp = m_branchTree->viewport();
        if (!vp->property("branchFilterInstalled").toBool()) {
            m_branchTreeViewport = vp;
            vp->installEventFilter(this);
            m_branchTree->installEventFilter(this);
            vp->setProperty("branchFilterInstalled", true);
        }
        connect(m_branchTree->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &BranchTreeManager::onVerticalScrolled);
    }

    m_compactLayout = AnalysisSettings::branchTreeCompactLayout();
    m_zoomPercent = qBound(kMinZoomPercent, AnalysisSettings::branchTreeZoomPercent(), kMaxZoomPercent);
    applyZoom();

    rebuildBranchTree();
}

// ===================== 公開API =====================

void BranchTreeManager::setBranchTreeRows(const QList<ResolvedRowLite>& rows)
{
    m_rows = rows;

    // 消えた変化の折りたたみ状態は捨てる
    QSet<int> heads;
    for (const auto& row : std::as_const(m_rows)) {
        if (row.headNodeId >= 0) heads.insert(row.headNodeId);
    }
    m_collapsedHeads.intersect(heads);

    rebuildBranchTree();
}

void BranchTreeManager::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    // 折りたたんだ変化の中の手を表示するときは、その変化を展開する
    if (revealNode(row, ply)) {
        rebuildBranchTree();
    }

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

// ===================== 表示設定 =====================

void BranchTreeManager::setCompactLayout(bool compact)
{
    if (m_compactLayout == compact) return;
    m_compactLayout = compact;
    AnalysisSettings::setBranchTreeCompactLayout(compact);
    const int row = m_lastHighlightedRow;
    const int ply = m_lastHighlightedPly;
    rebuildBranchTree();
    if (row >= 0 && ply >= 0) highlightBranchTreeAt(row, ply, true);
}

void BranchTreeManager::setZoomPercent(int percent)
{
    const int clamped = qBound(kMinZoomPercent, percent, kMaxZoomPercent);
    if (clamped == m_zoomPercent) return;
    m_zoomPercent = clamped;
    AnalysisSettings::setBranchTreeZoomPercent(clamped);
    applyZoom();
    scrollToCurrentNode();
}

void BranchTreeManager::applyZoom()
{
    if (!m_branchTree) return;
    const qreal scale = m_zoomPercent / 100.0;
    m_branchTree->setTransform(QTransform::fromScale(scale, scale));
    updateStickyHeader();
}

// ===================== グラフAPI =====================

void BranchTreeManager::clearBranchGraph()
{
    m_nodeIdByRowPly.clear();
    m_nodesById.clear();
    m_nextIds.clear();
    m_prevIdOf.clear();
    m_edgeInto.clear();
    m_highlightedEdges.clear();
    m_rowEntryNode.clear();
    m_nextNodeId = 1;
    m_lastHighlightedRow = -1;
    m_lastHighlightedPly = -1;
}

int BranchTreeManager::registerNode(int row, int ply, QGraphicsPathItem* item)
{
    if (!item) return -1;
    const int id = m_nextNodeId++;

    BranchGraphNode n;
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
    m_prevIdOf.insert(nextId, prevId);
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

    highlightPathTo(nodeId);
    scrollToNode(item, centerOn);
}

void BranchTreeManager::highlightPathTo(int nodeId)
{
    // 開始局面から現在の手までの経路の辺を目立たせる
    for (QGraphicsPathItem* edge : std::as_const(m_highlightedEdges)) {
        edge->setPen(kEdgePen);
        edge->setZValue(0);
    }
    m_highlightedEdges.clear();

    int current = nodeId;
    for (int guard = 0; current > 0 && guard <= m_nodesById.size(); ++guard) {
        QGraphicsPathItem* edge = m_edgeInto.value(current, nullptr);
        if (edge) {
            edge->setPen(kPathEdgePen);
            edge->setZValue(1);
            m_highlightedEdges.append(edge);
        }
        current = m_prevIdOf.value(current, -1);
    }
}

void BranchTreeManager::scrollToNode(QGraphicsPathItem* item, bool centerOn)
{
    // 非表示（タブの裏）の間はビューポートの寸法が定まらず、スクロール位置がずれたまま残るため
    // 何もしない。表示・リサイズ時に eventFilter から合わせ直す
    if (!m_branchTree || !item || !m_branchTree->isVisible()) return;

    // ノードの上にある「n手目」の見出しも見える範囲に入れる。上端の見出しの行と一緒に
    // 収まるときは、上端から表示する（手数が読めるように）
    QRectF area = item->mapRectToScene(item->boundingRect() | item->childrenBoundingRect());
    const qreal scale = m_zoomPercent / 100.0;
    if (m_scene && (area.bottom() + 8) * scale <= m_branchTree->viewport()->height()) {
        area.setTop(m_scene->sceneRect().top());
    }
    // 見えているときは動かさず、画面外にあるときは中央に寄せる（前後の手も見えるように）
    const QRectF visible = m_branchTree->mapToScene(m_branchTree->viewport()->rect()).boundingRect();
    if (centerOn || !visible.contains(area)) {
        m_branchTree->centerOn(area.center());
        m_branchTree->ensureVisible(area, 40, 8);
    }
    updateStickyHeader();
}

void BranchTreeManager::scrollToCurrentNode()
{
    scrollToNode(m_prevSelected, false);
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

