/// @file branchtreemanager.cpp
/// @brief 分岐ツリー管理クラスの実装（状態管理・ハイライト・イベント処理）

#include "branchtreemanager.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
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
#include <QSet>
#include <QTransform>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <climits>
#include <QGraphicsItem>


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

void BranchTreeManager::setBranchTree(KifuBranchTree* tree)
{
    const bool sameTree = tree != nullptr && tree == m_tree;
    m_tree = tree;
    // 盤上で手を指したときなど、1つの手順の末尾に1手増えただけならその手だけを描き足す。
    // 全体の作り直しは手数に比例して重く、指すたびに行うと着手が遅くなる。
    if (sameTree && appendSingleMove()) return;

    syncRowsFromTree();
    // 消えた変化の折りたたみ状態は捨てる
    QSet<int> heads;
    for (const BranchTreeLayout::Row& row : std::as_const(m_layout.rows)) {
        if (row.headNodeId >= 0) heads.insert(row.headNodeId);
    }
    m_layout.collapsedHeads.intersect(heads);

    rebuildBranchTree();
}

void BranchTreeManager::syncRowsFromTree()
{
    m_layout.rows.clear();
    if (!m_tree) return;
    const QList<BranchLine> lines = m_tree->allLines();
    // ノードを最初に含むライン（そのノードを描く行）
    QHash<const KifuBranchNode*, int> ownerRow;
    m_layout.rows.reserve(lines.size());
    for (qsizetype lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const BranchLine& line = lines.at(lineIdx);
        BranchTreeLayout::Row row;
        row.startPly = line.branchPly > 0 ? line.branchPly : 1;
        row.length = static_cast<int>(line.nodes.size());
        // 変化の先頭の手（折りたたみ状態をツリーの再構築後も保つためのキー）
        if (lineIdx > 0 && row.startPly < line.nodes.size()) {
            row.headNodeId = line.nodes.at(row.startPly)->nodeId();
        }
        if (line.branchPoint != nullptr) row.parent = ownerRow.value(line.branchPoint, -1);
        for (const KifuBranchNode* node : line.nodes) {
            if (!ownerRow.contains(node)) ownerRow.insert(node, static_cast<int>(lineIdx));
        }
        m_layout.rows.append(row);
    }
}

void BranchTreeManager::rememberTreeState()
{
    m_syncedRevision = m_tree ? m_tree->revision() : 0;
    m_syncedNodeCount = m_tree ? m_tree->nodeCount() : -1;
}

const KifuBranchNode* BranchTreeManager::nodeAt(int row, int ply) const
{
    if (!m_tree || row < 0 || ply < 0) return nullptr;
    const QList<BranchLine> lines = m_tree->allLines();
    if (row >= lines.size()) return nullptr;
    const QList<KifuBranchNode*>& nodes = lines.at(row).nodes;
    return ply < nodes.size() ? nodes.at(ply) : nullptr;
}

QGraphicsPathItem* BranchTreeManager::itemForNode(const KifuBranchNode* node) const
{
    return node ? m_itemByNodeId.value(node->nodeId(), nullptr) : nullptr;
}

void BranchTreeManager::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    // 折りたたんだ変化の中の手を表示するときは、その変化を展開する
    if (m_layout.reveal(row, ply)) {
        rebuildBranchTree();
    }

    // 分岐より前の手は親の行に描いてあるので、ツリーのノードから図形を引く
    QGraphicsPathItem* item = itemForNode(nodeAt(row, ply));
    if (!item) item = m_nodeIndex.value(qMakePair(row, ply), nullptr);
    if (item) highlightItem(item, centerOn);
}

bool BranchTreeManager::appendSingleMove()
{
    if (!m_scene || !m_tree || m_layout.rows.isEmpty() || m_syncedNodeCount < 0) return false;
    // 構造の変更が「1ノードの追加」1回だけで、ラインの数が変わっていないこと
    if (m_tree->nodeCount() != m_syncedNodeCount + 1 || m_tree->revision() != m_syncedRevision + 1
        || m_tree->lineCount() != m_layout.rows.size()) {
        return false;
    }
    // 増えた葉は、どれかのラインの末尾で、まだ描いていないノード
    const QList<BranchLine> lines = m_tree->allLines();
    for (const BranchLine& line : lines) {
        const KifuBranchNode* last = line.nodes.isEmpty() ? nullptr : line.nodes.constLast();
        if (last && !itemForNode(last)) return appendNode(last);
    }
    return false;
}

int BranchTreeManager::nodeIdFor(int row, int ply) const
{
    const QGraphicsPathItem* item = m_nodeIndex.value(qMakePair(row, ply), nullptr);
    return item ? item->data(ROLE_NODE_ID).toInt() : -1;
}

// ===================== 表示設定 =====================

void BranchTreeManager::setCompactLayout(bool compact)
{
    if (m_compactLayout == compact) return;
    m_compactLayout = compact;
    AnalysisSettings::branchTreeCompactLayout.set(compact);
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
    AnalysisSettings::branchTreeZoomPercent.set(clamped);
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

// ===================== 親行解決 =====================

// ===================== ハイライト =====================

void BranchTreeManager::highlightItem(QGraphicsPathItem* item, bool centerOn)
{
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
    m_lastHighlightedRow = item->data(ROLE_ROW).toInt();
    m_lastHighlightedPly = item->data(ROLE_PLY).toInt();

    highlightPathTo(m_tree ? m_tree->nodeAt(item->data(ROLE_NODE_ID).toInt()) : nullptr);
    scrollToNode(item, centerOn);
}

void BranchTreeManager::highlightPathTo(const KifuBranchNode* node)
{
    // 開始局面から現在の手までの経路（ツリーの親をたどる）の辺を目立たせる。
    // 1手ごとに呼ばれ、経路の長さは手数に比例する。前回の経路と開始局面の側から比べ、
    // 違う部分の辺だけを戻す・付ける（続けて指すときは1本）。全部を戻してから付け直すと、
    // 共通の辺でもペンと重なり順が2回変わり、そのたびに Qt が辺の輪郭を計算し直していた。
    QList<QGraphicsPathItem*> path;  // 現在の手 → 開始局面 の順
    for (const KifuBranchNode* current = node; current; current = current->parent()) {
        if (QGraphicsPathItem* edge = m_edgeInto.value(current->nodeId(), nullptr)) path.append(edge);
    }

    // 開始局面の側（末尾）から共通している辺の数
    qsizetype common = 0;
    while (common < path.size() && common < m_highlightedEdges.size()
           && path.at(path.size() - 1 - common) == m_highlightedEdges.at(m_highlightedEdges.size() - 1 - common)) {
        ++common;
    }
    for (qsizetype i = 0; i < m_highlightedEdges.size() - common; ++i) {
        QGraphicsPathItem* edge = m_highlightedEdges.at(i);
        edge->setPen(edgePen());
        edge->setZValue(0);
    }
    for (qsizetype i = 0; i < path.size() - common; ++i) {
        QGraphicsPathItem* edge = path.at(i);
        edge->setPen(pathEdgePen());
        edge->setZValue(1);
    }
    m_highlightedEdges = path;
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

// ============================================================
// 変化の折りたたみと段（配置は BranchTreeLayout）
// ============================================================

bool BranchTreeManager::isRowCollapsed(int row) const
{
    return m_layout.isCollapsed(row);
}

bool BranchTreeManager::isRowCollapsible(int row) const
{
    return m_layout.isCollapsible(row);
}

int BranchTreeManager::laneForRow(int row) const
{
    return m_layout.laneForRow(row);
}

void BranchTreeManager::setRowCollapsed(int row, bool collapsed)
{
    if (!m_layout.setCollapsed(row, collapsed)) return;

    // 現在の手が隠れる場合は、折りたたんだ変化の先頭の手を強調する
    int hlRow = m_lastHighlightedRow;
    int hlPly = m_lastHighlightedPly;
    rebuildBranchTree();
    if (hlRow >= 0 && hlPly >= 0) {
        if (!m_nodeIndex.contains(qMakePair(hlRow, hlPly))) {
            hlRow = row;
            hlPly = qMax(1, m_layout.rows.at(row).startPly);
        }
        const auto it = m_nodeIndex.constFind(qMakePair(hlRow, hlPly));
        if (it != m_nodeIndex.cend()) highlightItem(it.value(), false);
    }
}

void BranchTreeManager::expandAll()
{
    if (!m_layout.expandAll()) return;
    const int row = m_lastHighlightedRow;
    const int ply = m_lastHighlightedPly;
    rebuildBranchTree();
    if (row >= 0 && ply >= 0) highlightBranchTreeAt(row, ply, false);
}


// ============================================================
// キー・マウス・右クリックメニュー
// ============================================================

namespace {
constexpr int kZoomStep = 10;
} // namespace

QGraphicsItem* BranchTreeManager::nodeItemAt(const QPoint& viewportPos) const
{
    if (!m_branchTree || !m_branchTree->scene()) return nullptr;
    const QPointF scenePt = m_branchTree->mapToScene(viewportPos);
    // 見出しの帯は手数の表示なので、その下のノードを選ばない
    if (m_headerBand && m_headerBand->sceneBoundingRect().contains(scenePt)) return nullptr;
    const QList<QGraphicsItem*> hits = m_branchTree->scene()->items(scenePt);
    for (QGraphicsItem* hit : hits) {
        while (hit && !hit->data(BR_ROLE_KIND).isValid())
            hit = hit->parentItem();
        if (hit) return hit;
    }
    return nullptr;
}

bool BranchTreeManager::handleKeyPress(QKeyEvent* event)
{
    if (!m_branchTreeClickEnabled || !hasHighlightedNode()) return false;

    const int row = m_lastHighlightedRow;
    const int ply = m_lastHighlightedPly;
    int targetRow = row;
    int targetPly = ply;

    switch (event->key()) {
    case Qt::Key_Right:
        targetPly = ply + 1;
        if (!m_nodeIndex.contains(qMakePair(row, targetPly))) {
            // 開始局面から右へは本譜の1手目
            if (ply == 0 && m_nodeIndex.contains(qMakePair(0, 1))) {
                targetRow = 0;
            } else {
                return true;
            }
        }
        break;
    case Qt::Key_Left: {
        if (ply <= 0) return true;
        // 同じ行（ライン）のまま1手戻す。分岐前の手は親の行のノードとして強調される
        targetPly = ply - 1;
        break;
    }
    case Qt::Key_Up:
    case Qt::Key_Down: {
        // 同じ手数の列で、画面上の上（下）にある最も近い手へ移る
        const int lane = laneForRow(row);
        const bool up = event->key() == Qt::Key_Up;
        int bestRow = -1;
        int bestLane = up ? -1 : INT_MAX;
        for (auto it = m_nodeIndex.cbegin(); it != m_nodeIndex.cend(); ++it) {
            if (it.key().second != ply) continue;
            const int candidateLane = laneForRow(it.key().first);
            if (candidateLane < 0) continue;
            if (up && candidateLane < lane && candidateLane > bestLane) {
                bestLane = candidateLane;
                bestRow = it.key().first;
            } else if (!up && candidateLane > lane && candidateLane < bestLane) {
                bestLane = candidateLane;
                bestRow = it.key().first;
            }
        }
        if (bestRow < 0) return true;
        targetRow = bestRow;
        break;
    }
    case Qt::Key_Home:
        targetPly = 0;
        break;
    case Qt::Key_End:
        targetPly = qMax(0, m_layout.lastVisiblePly(row));
        break;
    default:
        return false;
    }

    if (targetRow == row && targetPly == ply) return true;
    highlightBranchTreeAt(targetRow, targetPly, false);
    emit branchNodeActivated(targetRow, targetPly);
    return true;
}

void BranchTreeManager::showContextMenu(const QPoint& viewportPos, const QPoint& globalPos)
{
    if (!m_branchTree) return;
    QGraphicsItem* hit = nodeItemAt(viewportPos);
    const int row = hit ? hit->data(ROLE_ROW).toInt() : -1;
    const int ply = hit ? hit->data(ROLE_PLY).toInt() : -1;

    QMenu menu(m_branchTree);
    if (hit && m_branchTreeClickEnabled) {
        // 分岐の編集など、受信側が操作を追加する
        emit nodeContextMenuAboutToShow(row, ply, &menu);
    }

    if (!menu.isEmpty()) menu.addSeparator();

    QAction* collapseAction = nullptr;
    if (hit && row >= 1 && isRowCollapsible(row)) {
        collapseAction = menu.addAction(isRowCollapsed(row) ? tr("この変化を展開する")
                                                            : tr("この変化を折りたたむ"));
    }
    QAction* expandAllAction = nullptr;
    if (!m_layout.collapsedHeads.isEmpty()) {
        expandAllAction = menu.addAction(tr("すべての変化を展開する"));
    }
    QAction* compactAction = menu.addAction(tr("変化を詰めて表示する"));
    compactAction->setCheckable(true);
    compactAction->setChecked(m_compactLayout);
    menu.addSeparator();
    QAction* zoomInAction = menu.addAction(tr("拡大"));
    zoomInAction->setEnabled(m_zoomPercent < kMaxZoomPercent);
    QAction* zoomOutAction = menu.addAction(tr("縮小"));
    zoomOutAction->setEnabled(m_zoomPercent > kMinZoomPercent);
    QAction* zoomResetAction = menu.addAction(tr("標準の大きさ（100%）"));
    zoomResetAction->setEnabled(m_zoomPercent != 100);

    QAction* chosen = menu.exec(globalPos);
    if (chosen == nullptr) return;
    if (chosen == collapseAction) {
        setRowCollapsed(row, !isRowCollapsed(row));
    } else if (chosen == expandAllAction) {
        expandAll();
    } else if (chosen == compactAction) {
        setCompactLayout(compactAction->isChecked());
    } else if (chosen == zoomInAction) {
        setZoomPercent(m_zoomPercent + kZoomStep);
    } else if (chosen == zoomOutAction) {
        setZoomPercent(m_zoomPercent - kZoomStep);
    } else if (chosen == zoomResetAction) {
        setZoomPercent(100);
    }
}

void BranchTreeManager::onVerticalScrolled(int value)
{
    Q_UNUSED(value)
    updateStickyHeader();
}

bool BranchTreeManager::eventFilter(QObject* obj, QEvent* ev)
{
    if (!obj || ev->type() == QEvent::Destroy) {
        return QObject::eventFilter(obj, ev);
    }

    // キー操作はビュー本体に届く
    if (m_branchTree && obj == m_branchTree && ev->type() == QEvent::KeyPress) {
        if (handleKeyPress(static_cast<QKeyEvent*>(ev))) return true;
        return QObject::eventFilter(obj, ev);
    }

    if (obj != m_branchTreeViewport || !m_branchTreeViewport) {
        return QObject::eventFilter(obj, ev);
    }

    switch (ev->type()) {
    case QEvent::Show:
    case QEvent::Resize:
        // 表示されたとき・大きさが変わったときは、現在の手が見える位置までスクロールする
        // （表示直後は寸法が確定していないため、レイアウト後に合わせる）
        QTimer::singleShot(0, this, &BranchTreeManager::scrollToCurrentNode);
        break;
    case QEvent::FontChange: {
        const int row = m_lastHighlightedRow;
        const int ply = m_lastHighlightedPly;
        const int x = m_branchTree->horizontalScrollBar()->value();
        const int y = m_branchTree->verticalScrollBar()->value();
        rebuildBranchTree();
        if (row >= 0 && ply >= 0) highlightBranchTreeAt(row, ply, false);
        m_branchTree->horizontalScrollBar()->setValue(x);
        m_branchTree->verticalScrollBar()->setValue(y);
        break;
    }
    case QEvent::Wheel: {
        auto* we = static_cast<QWheelEvent*>(ev);
        if (we->modifiers() & Qt::ControlModifier) {
            const int delta = we->angleDelta().y();
            if (delta != 0) setZoomPercent(m_zoomPercent + (delta > 0 ? kZoomStep : -kZoomStep));
            return true;
        }
        break;
    }
    case QEvent::NativeGesture: {
        auto* ge = static_cast<QNativeGestureEvent*>(ev);
        if (ge->gestureType() == Qt::ZoomNativeGesture) {
            setZoomPercent(m_zoomPercent + qRound(ge->value() * 100.0));
            return true;
        }
        break;
    }
    case QEvent::ContextMenu: {
        auto* ce = static_cast<QContextMenuEvent*>(ev);
        showContextMenu(ce->pos(), ce->globalPos());
        return true;
    }
    case QEvent::MouseButtonDblClick: {
        // 折りたたんだ変化の先頭をダブルクリックすると展開する
        auto* me = static_cast<QMouseEvent*>(ev);
        if (!(me->button() & Qt::LeftButton)) break;
        QGraphicsItem* hit = nodeItemAt(me->pos());
        if (!hit) break;
        const int row = hit->data(ROLE_ROW).toInt();
        if (isRowCollapsed(row)) {
            setRowCollapsed(row, false);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (!m_branchTreeClickEnabled) {
            return false;
        }

        auto* me = static_cast<QMouseEvent*>(ev);
        if (!(me->button() & Qt::LeftButton)) return QObject::eventFilter(obj, ev);

        QGraphicsItem* hit = nodeItemAt(me->pos());
        if (!hit) return false;

        const int row = hit->data(ROLE_ROW).toInt();
        const int ply = hit->data(ROLE_PLY).toInt();

        highlightBranchTreeAt(row, ply, /*centerOn=*/false);

        emit branchNodeActivated(row, ply);
        return true;
    }
    default:
        break;
    }
    return QObject::eventFilter(obj, ev);
}
