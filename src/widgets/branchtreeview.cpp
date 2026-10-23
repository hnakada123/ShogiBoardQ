/// @file branchtreeview.cpp
/// @brief 分岐ツリーの図の実装（ツリーの変更・描画・強調・操作）

#include "branchtreeview.h"
#include "branchnodeitem.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifupresentation.h"
#include "analysissettings.h"
#include "applicationfonts.h"
#include "logcategories.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainterPath>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QTransform>
#include <QWheelEvent>

#include <climits>

namespace {
// レイアウト定数（ノード配置・シーン範囲・見出しの位置で共有）
constexpr qreal kBaseX  = 40.0;
constexpr qreal kShiftX = 40.0;
constexpr qreal kBaseY  = 40.0;
constexpr qreal kStepY  = 56.0;
constexpr qreal kHeaderHeight = 22.0;   ///< 手数見出しの帯の高さ
constexpr int kZoomStep = 10;

QFont baseFont(int pointSize)
{
    return QFont(QApplication::font().family(), pointSize);
}

/// 手の表記に使う書体（日本語表記の指し手は日本語の書体）
QFont labelFont(const KifuBranchNode* node)
{
    QFont font = baseFont(10);
    if (node->isActualMove() && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese)
        font = ApplicationFonts::japaneseFont(font);
    return font;
}

/// 分岐ツリーの手の表記（KifuPresentation の表記設定に従う）
QString labelOf(const KifuBranchNode* node, bool fullOrigin = false)
{
    return KifuPresentation::entryLabel(node->record(), node->parent() ? node->parent()->sfen() : QString(), fullOrigin);
}

/// 列の間隔に必要な幅（表記の幅＋余白）
qreal requiredColumnSpacing(const KifuBranchNode* node)
{
    return QFontMetrics(labelFont(node)).horizontalAdvance(labelOf(node)) + 48;
}

/// 手の中身（先手の手は青、後手の手は橙。終局の行は終局を記録した側）
BranchNodeItem::Content moveContent(const KifuBranchNode* node)
{
    BranchNodeItem::Content content;
    content.label = labelOf(node);
    // 手の表記（読み上げ形式）
    content.tooltip = labelOf(node, true).toHtmlEscaped();
    content.font = labelFont(node);
    content.fill = node->record().mover == Turn::Black ? QColor(196, 230, 255) : QColor(255, 223, 196);
    return content;
}

QPen edgePen()
{
    return QPen(QColor(90, 90, 90), 1.0);
}

/// 枝の線（選んでいる手順）
QPen pathEdgePen()
{
    return QPen(QColor(40, 110, 210), 2.6);
}
} // namespace

// ============================================================
// 生成・ツリー
// ============================================================

BranchTreeView::BranchTreeView(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setObjectName(QStringLiteral("branchTreeView"));
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setScene(m_scene);
    // 矢印キーで手を移動できるようにする
    setFocusPolicy(Qt::StrongFocus);

    m_compactLayout = AnalysisSettings::branchTreeCompactLayout();
    m_zoomPercent = qBound(kMinZoomPercent, AnalysisSettings::branchTreeZoomPercent(), kMaxZoomPercent);
    applyZoom();

    rebuild();
}

BranchTreeView::~BranchTreeView() = default;

void BranchTreeView::setTree(KifuBranchTree* tree)
{
    if (m_tree != tree) {
        if (m_tree) disconnect(m_tree, nullptr, this, nullptr);
        m_tree = tree;
        if (tree) {
            connect(tree, &KifuBranchTree::nodeAppended, this, &BranchTreeView::onNodeAppended);
            connect(tree, &KifuBranchTree::childrenReordered, this, &BranchTreeView::onStructureChanged);
            connect(tree, &KifuBranchTree::subtreeRemoved, this, &BranchTreeView::onStructureChanged);
            connect(tree, &KifuBranchTree::reset, this, &BranchTreeView::onStructureChanged);
        }
    }
    rebuild();
}

KifuBranchTree* BranchTreeView::tree() const
{
    return m_tree;
}

void BranchTreeView::onNodeAppended(int parentId, int nodeId)
{
    Q_UNUSED(parentId)
    const KifuBranchNode* node = m_tree ? m_tree->nodeAt(nodeId) : nullptr;
    // 盤上で手を指したときなど、1つの手順の末尾に1手増えただけならその手だけを描き足す。
    // 新しい変化ができたとき（行の並びが変わり得る）は作り直す
    if (!node || !appendNode(node)) {
        rebuild();
        return;
    }
    applyCurrent(false);
}

void BranchTreeView::onStructureChanged()
{
    rebuild();
}

void BranchTreeView::refreshLabels()
{
    rebuild();
}

void BranchTreeView::refreshNode(int nodeId)
{
    BranchNodeItem* item = m_items.value(nodeId, nullptr);
    const KifuBranchNode* node = m_tree ? m_tree->nodeAt(nodeId) : nullptr;
    if (item && node) item->setComment(node->comment());
}

void BranchTreeView::syncRowsFromTree()
{
    m_layout.rows.clear();
    m_rowOf.clear();
    if (!m_tree) return;
    const QList<BranchLine> lines = m_tree->allLines();
    m_layout.rows.reserve(lines.size());
    QSet<int> heads;
    for (qsizetype lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const BranchLine& line = lines.at(lineIdx);
        BranchTreeLayout::Row row;
        row.startPly = line.branchPly > 0 ? line.branchPly : 1;
        row.length = static_cast<int>(line.nodes.size());
        // 変化の先頭の手（折りたたみ状態をツリーの再構築後も保つためのキー）
        if (lineIdx > 0 && row.startPly < line.nodes.size()) {
            row.headNodeId = line.nodes.at(row.startPly)->nodeId();
            heads.insert(row.headNodeId);
        }
        if (line.branchPoint != nullptr) row.parent = rowOf(line.branchPoint->nodeId());
        // 手は、その手を初めて含むラインの行に描く
        for (const KifuBranchNode* node : line.nodes) {
            if (!m_rowOf.contains(node->nodeId())) m_rowOf.insert(node->nodeId(), static_cast<int>(lineIdx));
        }
        m_layout.rows.append(row);
    }
    // 消えた変化の折りたたみ状態は捨てる
    m_layout.collapsedHeads.intersect(heads);
}

// ============================================================
// 描画
// ============================================================

void BranchTreeView::rebuild()
{
    ++m_rebuildCount;
    m_scene->clear();
    m_items.clear();
    m_edgeInto.clear();
    m_highlightedEdges.clear();
    m_plyLabels.clear();   // scene->clear() で削除済み
    m_headerBand = nullptr;
    m_currentItem = nullptr;

    syncRowsFromTree();
    m_layout.assignLanes(m_compactLayout);
    const QList<BranchLine> lines = m_tree ? m_tree->allLines() : QList<BranchLine>();

    m_columnSpacing = 110.0;
    for (qsizetype row = 0; row < lines.size(); ++row) {
        const QList<KifuBranchNode*>& nodes = lines.at(row).nodes;
        for (qsizetype ply = qMax(1, m_layout.rows.at(row).startPly); ply < nodes.size(); ++ply)
            m_columnSpacing = qMax(m_columnSpacing, requiredColumnSpacing(nodes.at(ply)));
    }

    // 手数見出しの帯（上端に固定して表示する。下に隠れたノードが透けないよう不透明にする）
    m_headerBand = m_scene->addRect(QRectF(0, 0, 1, kHeaderHeight), Qt::NoPen, palette().color(QPalette::Base));
    m_headerBand->setZValue(40);

    // 開始局面
    const KifuBranchNode* root = m_tree ? m_tree->root() : nullptr;
    BranchNodeItem::Content start;
    start.label = tr("開始局面");
    start.font = baseFont(10);
    start.fill = QColor(235, 235, 235);
    start.start = true;
    auto* startItem = new BranchNodeItem(root ? root->nodeId() : -1, 0, 0, start, QPointF(kBaseX + kShiftX, laneY(0)));
    m_scene->addItem(startItem);
    if (root) {
        startItem->setComment(root->comment());
        m_items.insert(root->nodeId(), startItem);
    }

    // 各行（0=本譜、1..=変化）：変化の先頭から描き、分岐元の手とつなぐ
    for (qsizetype row = 0; row < lines.size(); ++row) {
        const int r = static_cast<int>(row);
        if (m_layout.laneForRow(r) < 0) continue;   // 折りたたみで隠れている
        const QList<KifuBranchNode*>& nodes = lines.at(row).nodes;
        const int lastPly = m_layout.lastVisiblePly(r);
        for (int ply = qMax(1, m_layout.rows.at(row).startPly); ply <= lastPly && ply < nodes.size(); ++ply) {
            const KifuBranchNode* node = nodes.at(ply);
            const BranchNodeItem* prev = m_items.value(node->parent()->nodeId(), startItem);
            addEdge(prev, addNode(r, node));
        }
    }
    qCDebug(lcUi).noquote() << "[BranchTreeView] rebuild: rows=" << m_layout.rows.size()
                            << "lanes=" << m_layout.laneCount() << "compact=" << m_compactLayout;

    const int maxPly = maxDrawnPly();
    for (int ply = 1; ply <= maxPly; ++ply) addMoveNumberLabel(ply);
    updateSceneRect();
    applyCurrent(false);
}

BranchNodeItem* BranchTreeView::addNode(int row, const KifuBranchNode* node)
{
    const QPointF center(kBaseX + kShiftX + node->ply() * m_columnSpacing, laneY(m_layout.laneForRow(row)));
    auto* item = new BranchNodeItem(node->nodeId(), row, node->ply(), moveContent(node), center);
    m_scene->addItem(item);
    item->setComment(node->comment());
    // 折りたたんだ変化の先頭には、隠れている手の数を表示する
    if (row >= 1 && m_layout.isCollapsed(row) && node->ply() == qMax(1, m_layout.rows.at(row).startPly))
        item->setCollapsedBadge(m_layout.hiddenNodeCount(row), palette().color(QPalette::Text));
    m_items.insert(node->nodeId(), item);
    return item;
}

void BranchTreeView::addEdge(const BranchNodeItem* from, const BranchNodeItem* to)
{
    if (!from || !to) return;
    const QRectF fromRect = from->frame();
    const QRectF toRect = to->frame();
    const qreal ay = fromRect.center().y();
    const qreal by = toRect.center().y();

    QPainterPath path;
    if (qFuzzyCompare(ay, by)) {
        // 同じ段：中心どうしを結ぶ
        const QPointF a = fromRect.center();
        const QPointF b = toRect.center();
        path.moveTo(a);
        path.cubicTo(QPointF(a.x() + 8, a.y()), QPointF(b.x() - 8, b.y()), b);
    } else {
        // 別の段：列の間のすき間で縦に下ろし、間の段のノードに線が重ならないようにする。
        // 縦線は列の中心どうしの中間に置き、同じ手から出る変化が1本の縦線を共有するようにする
        // （ノードの幅は表記で変わるため、ノードの端を基準にすると縦線が並んでしまう）
        const qreal xm = (fromRect.center().x() + toRect.center().x()) / 2.0;
        const qreal r = qMin<qreal>(8.0, qMax<qreal>(1.0, (toRect.left() - fromRect.right()) / 4.0));
        const qreal dir = (by > ay) ? 1.0 : -1.0;
        path.moveTo(fromRect.right(), ay);
        path.lineTo(xm - r, ay);
        path.quadTo(QPointF(xm, ay), QPointF(xm, ay + dir * r));
        path.lineTo(xm, by - dir * r);
        path.quadTo(QPointF(xm, by), QPointF(xm + r, by));
        path.lineTo(toRect.left(), by);
    }
    auto* edge = m_scene->addPath(path, edgePen());
    edge->setZValue(0);
    m_edgeInto.insert(to->nodeId(), edge);
}

bool BranchTreeView::appendNode(const KifuBranchNode* node)
{
    const KifuBranchNode* parent = node->parent();
    // 直前の手が、ある行の末尾に描いてあり、足した手がその唯一の子であること（新しい変化でない）
    if (!parent || node->childCount() != 0 || parent->childCount() != 1) return false;
    const BranchNodeItem* prev = m_items.value(parent->nodeId(), nullptr);
    if (!prev) return false;
    const int row = prev->row();
    const int ply = node->ply();
    if (row < 0 || row >= m_layout.rows.size()) return false;
    BranchTreeLayout::Row& rv = m_layout.rows[row];
    if (rv.length != ply) return false;
    // 折りたたみで隠れている行は作り直しに任せる（表示中の手を見せるため展開される）
    const int lane = m_layout.laneForRow(row);
    if (lane < 0 || m_layout.isCollapsed(row)) return false;

    // 詰めた配置では、伸びた先に同じ段の別の変化があれば配置し直す
    if (m_compactLayout) {
        for (qsizetype other = 0; other < m_layout.rows.size(); ++other) {
            if (other == row || m_layout.laneForRow(static_cast<int>(other)) != lane) continue;
            const int otherStart = qMax(1, m_layout.rows.at(other).startPly);
            if (otherStart > rv.startPly && otherStart <= ply + 1) return false;
        }
    }
    // 表記が列の幅に収まらなければ、列の幅を広げて全体を作り直す
    if (requiredColumnSpacing(node) > m_columnSpacing) return false;

    ++rv.length;
    m_rowOf.insert(node->nodeId(), row);
    addEdge(prev, addNode(row, node));
    addMoveNumberLabel(ply);
    updateSceneRect();
    return true;
}

int BranchTreeView::maxDrawnPly() const
{
    int maxPly = 0;
    for (const BranchNodeItem* item : std::as_const(m_items)) maxPly = qMax(maxPly, item->ply());
    return maxPly;
}

void BranchTreeView::addMoveNumberLabel(int ply)
{
    if (!m_headerBand || ply < 1 || m_plyLabels.contains(ply)) return;
    auto* label = new QGraphicsSimpleTextItem(tr("%1手目").arg(ply), m_headerBand);
    label->setFont(baseFont(9));
    label->setBrush(palette().color(QPalette::Text));
    const QRectF rect = label->boundingRect();
    const qreal x = kBaseX + kShiftX + ply * m_columnSpacing;
    label->setPos(x - rect.width() / 2.0, kHeaderHeight - rect.height() - 1.0);
    m_plyLabels.insert(ply, label);
}

void BranchTreeView::updateSceneRect()
{
    const int mainLen = m_layout.rows.isEmpty() ? 0 : qMax(0, m_layout.rows.at(0).length - 1);
    const int spanLen = qMax(mainLen, maxDrawnPly());
    const qreal width  = (kBaseX + kShiftX) + m_columnSpacing * qMax(40, spanLen + 6) + 40.0;
    const qreal height = 30 + kStepY * static_cast<qreal>(qMax(2, m_layout.laneCount() + 1));
    m_scene->setSceneRect(QRectF(0, 0, width, height));
    if (m_headerBand) m_headerBand->setRect(QRectF(0, 0, width, kHeaderHeight));
    updateStickyHeader();
}

void BranchTreeView::updateStickyHeader()
{
    if (!m_headerBand) return;
    // 見えている範囲の上端に帯を置く（スクロールしても手数が読める）
    const qreal visibleTop = mapToScene(QPoint(0, 0)).y();
    m_headerBand->setY(qMax<qreal>(0.0, visibleTop));
}

qreal BranchTreeView::laneY(int lane) const
{
    return kBaseY + qMax(0, lane) * kStepY;
}

void BranchTreeView::applyZoom()
{
    const qreal scale = m_zoomPercent / 100.0;
    setTransform(QTransform::fromScale(scale, scale));
    updateStickyHeader();
}

// ============================================================
// 表示中の手
// ============================================================

void BranchTreeView::setCurrentNode(int nodeId, bool center)
{
    m_currentNodeId = nodeId;
    // 折りたたんだ変化の中の手を表示するときは、その変化を展開する
    const KifuBranchNode* node = m_tree ? m_tree->nodeAt(nodeId) : nullptr;
    const int row = rowOf(nodeId);
    if (node && row >= 0 && m_layout.reveal(row, node->ply())) {
        rebuild();
        if (center) applyCurrent(true);
        return;
    }
    applyCurrent(center);
}

int BranchTreeView::currentNodeId() const
{
    return m_currentItem ? m_currentItem->nodeId() : -1;
}

void BranchTreeView::applyCurrent(bool center)
{
    BranchNodeItem* item = m_items.value(m_currentNodeId, nullptr);
    if (m_currentItem && m_currentItem != item) m_currentItem->setCurrent(false);
    m_currentItem = item;
    if (!item) {
        highlightPathTo(nullptr);
        return;
    }
    item->setCurrent(true);
    highlightPathTo(m_tree ? m_tree->nodeAt(item->nodeId()) : nullptr);
    scrollToNode(item, center);
}

void BranchTreeView::highlightPathTo(const KifuBranchNode* node)
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

void BranchTreeView::scrollToNode(const BranchNodeItem* item, bool center)
{
    // 非表示（タブの裏）の間はビューポートの寸法が定まらず、スクロール位置がずれたまま残るため
    // 何もしない。表示・リサイズ時に合わせ直す
    if (!item || !isVisible()) return;

    // ノードの上にある「n手目」の見出しも見える範囲に入れる。上端の見出しの行と一緒に
    // 収まるときは、上端から表示する（手数が読めるように）
    QRectF area = item->mapRectToScene(item->boundingRect() | item->childrenBoundingRect());
    const qreal scale = m_zoomPercent / 100.0;
    if ((area.bottom() + 8) * scale <= viewport()->height()) area.setTop(m_scene->sceneRect().top());
    // 見えているときは動かさず、画面外にあるときは中央に寄せる（前後の手も見えるように）
    const QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    if (center || !visible.contains(area)) {
        centerOn(area.center());
        ensureVisible(area, 40, 8);
    }
    updateStickyHeader();
}

void BranchTreeView::scrollToCurrentNode()
{
    scrollToNode(m_currentItem, false);
}

// ============================================================
// 表示設定・折りたたみ
// ============================================================

void BranchTreeView::setCompactLayout(bool compact)
{
    if (m_compactLayout == compact) return;
    m_compactLayout = compact;
    AnalysisSettings::branchTreeCompactLayout.set(compact);
    rebuild();
    applyCurrent(true);
}

void BranchTreeView::setZoomPercent(int percent)
{
    const int clamped = qBound(kMinZoomPercent, percent, kMaxZoomPercent);
    if (clamped == m_zoomPercent) return;
    m_zoomPercent = clamped;
    AnalysisSettings::branchTreeZoomPercent.set(clamped);
    applyZoom();
    scrollToCurrentNode();
}

int BranchTreeView::laneOf(int nodeId) const
{
    const BranchNodeItem* item = m_items.value(nodeId, nullptr);
    return item ? m_layout.laneForRow(item->row()) : -1;
}

bool BranchTreeView::isCollapsed(int nodeId) const
{
    return m_layout.isCollapsed(rowOf(nodeId));
}

bool BranchTreeView::isCollapsible(int nodeId) const
{
    return m_layout.isCollapsible(rowOf(nodeId));
}

void BranchTreeView::setCollapsed(int nodeId, bool collapsed)
{
    const int row = rowOf(nodeId);
    if (!m_layout.setCollapsed(row, collapsed)) return;
    rebuild();
    // 表示中の手が隠れたときは、折りたたんだ変化の先頭の手を目立たせる
    if (!m_currentItem) {
        if (BranchNodeItem* head = m_items.value(m_layout.rows.at(row).headNodeId, nullptr)) {
            head->setCurrent(true);
            m_currentItem = head;
        }
    }
}

void BranchTreeView::expandAll()
{
    if (!m_layout.expandAll()) return;
    rebuild();
}

// ============================================================
// キー・マウス・右クリックメニュー
// ============================================================

BranchNodeItem* BranchTreeView::nodeItemAt(const QPoint& viewportPos) const
{
    const QPointF scenePt = mapToScene(viewportPos);
    // 見出しの帯は手数の表示なので、その下のノードを選ばない
    if (m_headerBand && m_headerBand->sceneBoundingRect().contains(scenePt)) return nullptr;
    const QList<QGraphicsItem*> hits = m_scene->items(scenePt);
    for (QGraphicsItem* hit : hits) {
        for (; hit; hit = hit->parentItem()) {
            if (auto* item = qgraphicsitem_cast<BranchNodeItem*>(hit)) return item;
        }
    }
    return nullptr;
}

const BranchNodeItem* BranchTreeView::keyTarget(int key) const
{
    const BranchNodeItem* current = m_currentItem;
    const KifuBranchNode* node = (current && m_tree) ? m_tree->nodeAt(current->nodeId()) : nullptr;
    if (!node) return nullptr;
    const auto itemOf = [this](const KifuBranchNode* n) { return n ? m_items.value(n->nodeId(), nullptr) : nullptr; };

    switch (key) {
    case Qt::Key_Right:
        // 同じ行の次の手（先頭の子はその手を描いた行に続く）
        return itemOf(node->childAt(0));
    case Qt::Key_Left:
        return itemOf(node->parent());
    case Qt::Key_Up:
    case Qt::Key_Down: {
        // 同じ手数の列で、画面上の上（下）にある最も近い手へ移る
        const int lane = m_layout.laneForRow(current->row());
        const bool up = key == Qt::Key_Up;
        const BranchNodeItem* best = nullptr;
        int bestLane = up ? -1 : INT_MAX;
        for (const BranchNodeItem* item : std::as_const(m_items)) {
            if (item->ply() != current->ply()) continue;
            const int candidate = m_layout.laneForRow(item->row());
            if (candidate < 0) continue;
            if ((up && candidate < lane && candidate > bestLane) || (!up && candidate > lane && candidate < bestLane)) {
                bestLane = candidate;
                best = item;
            }
        }
        return best;
    }
    case Qt::Key_Home:
        return itemOf(m_tree->root());
    case Qt::Key_End: {
        // この行の描いてある最後の手
        const BranchNodeItem* last = current;
        for (const BranchNodeItem* next = itemOf(node->childAt(0)); next; ) {
            last = next;
            node = node->childAt(0);
            next = itemOf(node->childAt(0));
        }
        return last;
    }
    default:
        return nullptr;
    }
}

void BranchTreeView::keyPressEvent(QKeyEvent* event)
{
    static const QList<int> kNavigationKeys{Qt::Key_Right, Qt::Key_Left, Qt::Key_Up, Qt::Key_Down,
                                           Qt::Key_Home, Qt::Key_End};
    if (!m_clickEnabled || !m_currentItem || !kNavigationKeys.contains(event->key())) {
        QGraphicsView::keyPressEvent(event);
        return;
    }
    // 移る先が無くても、スクロールには使わない
    event->accept();
    const BranchNodeItem* target = keyTarget(event->key());
    if (target && target != m_currentItem) activate(target->nodeId());
}

void BranchTreeView::activate(int nodeId)
{
    setCurrentNode(nodeId);
    emit nodeActivated(nodeId);
}

void BranchTreeView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_clickEnabled && (event->button() & Qt::LeftButton)) {
        if (const BranchNodeItem* hit = nodeItemAt(event->pos())) {
            activate(hit->nodeId());
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void BranchTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // 折りたたんだ変化の先頭をダブルクリックすると展開する
    if (event->button() & Qt::LeftButton) {
        const BranchNodeItem* hit = nodeItemAt(event->pos());
        if (hit && isCollapsed(hit->nodeId())) {
            setCollapsed(hit->nodeId(), false);
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void BranchTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->pos(), event->globalPos());
    event->accept();
}

void BranchTreeView::showContextMenu(const QPoint& viewportPos, const QPoint& globalPos)
{
    const BranchNodeItem* hit = nodeItemAt(viewportPos);
    const int nodeId = hit ? hit->nodeId() : -1;

    QMenu menu(this);
    // 分岐の編集など、受信側が操作を追加する
    if (hit && m_clickEnabled) emit nodeContextMenuAboutToShow(nodeId, &menu);
    if (!menu.isEmpty()) menu.addSeparator();

    QAction* collapseAction = nullptr;
    if (hit && hit->row() >= 1 && isCollapsible(nodeId)) {
        collapseAction = menu.addAction(isCollapsed(nodeId) ? tr("この変化を展開する") : tr("この変化を折りたたむ"));
    }
    QAction* expandAllAction = nullptr;
    if (!m_layout.collapsedHeads.isEmpty()) expandAllAction = menu.addAction(tr("すべての変化を展開する"));
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
        setCollapsed(nodeId, !isCollapsed(nodeId));
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

void BranchTreeView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int delta = event->angleDelta().y();
        if (delta != 0) setZoomPercent(m_zoomPercent + (delta > 0 ? kZoomStep : -kZoomStep));
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

bool BranchTreeView::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            setZoomPercent(m_zoomPercent + qRound(gesture->value() * 100.0));
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

void BranchTreeView::showEvent(QShowEvent* event)
{
    QGraphicsView::showEvent(event);
    // 表示された直後は寸法が確定していないため、レイアウト後に現在の手が見える位置までスクロールする
    QTimer::singleShot(0, this, &BranchTreeView::scrollToCurrentNode);
}

void BranchTreeView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    QTimer::singleShot(0, this, &BranchTreeView::scrollToCurrentNode);
}

void BranchTreeView::changeEvent(QEvent* event)
{
    QGraphicsView::changeEvent(event);
    if (event->type() != QEvent::FontChange || !m_scene) return;
    // 文字の大きさが変わったら描き直す（スクロール位置は保つ）
    const int x = horizontalScrollBar()->value();
    const int y = verticalScrollBar()->value();
    rebuild();
    horizontalScrollBar()->setValue(x);
    verticalScrollBar()->setValue(y);
}

void BranchTreeView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    // 縦にスクロールしても手数の見出しを上端に表示する
    updateStickyHeader();
}
