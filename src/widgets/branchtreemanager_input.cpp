/// @file branchtreemanager_input.cpp
/// @brief BranchTreeManager の入力処理（クリック・キー操作・右クリックメニュー・拡大縮小）

#include "branchtreemanager.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <climits>

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
        targetPly = qMax(0, lastVisiblePly(row));
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
    if (!m_collapsedHeads.isEmpty()) {
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
