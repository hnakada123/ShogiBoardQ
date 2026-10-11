/// @file shogiview_events.cpp
/// @brief ShogiView のイベント処理・座標変換・駒画像管理

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "globaltooltip.h"
#include "pieceimageprovider.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QHelpEvent>
#include <QCursor>

// ─────────────────────────────────────────────────────────────────────────────
// イベントフィルタ
// ─────────────────────────────────────────────────────────────────────────────

bool ShogiView::eventFilter(QObject* obj, QEvent* ev)
{
    if (!m_tooltip) {
        m_tooltip = new GlobalToolTip(this);
        m_tooltip->setCompact(true);
        m_tooltip->setPointSizeF(12.0);
    }

    if (obj == m_blackNameLabel || obj == m_whiteNameLabel) {
        if (ev->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(ev);
            const QString text = (obj == m_blackNameLabel) ? m_blackNameBase : m_whiteNameBase;
            m_tooltip->showText(he->globalPos(), text);
            return true;
        }
        else if (ev->type() == QEvent::Leave) {
            m_tooltip->hideTip();
        }
    }

    return QWidget::eventFilter(obj, ev);
}

// ─────────────────────────────────────────────────────────────────────────────
// 入力座標変換
// ─────────────────────────────────────────────────────────────────────────────

QPoint ShogiView::clickedSquare(const QPoint &clickPosition) const
{
    return m_interaction.clickedSquare(clickPosition, m_layout, m_board, pieceBoxRect());
}

QPoint ShogiView::getClickedSquareInDefaultState(const QPoint& pos) const
{
    return m_interaction.getClickedSquareInDefaultState(pos, m_layout, m_board);
}

QPoint ShogiView::getClickedSquareInFlippedState(const QPoint& pos) const
{
    return m_interaction.getClickedSquareInFlippedState(pos, m_layout, m_board);
}

// ─────────────────────────────────────────────────────────────────────────────
// マウス・キーイベント
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_interaction.dragging() && event->button() == Qt::RightButton) {
        endDrag();
        QPoint pt = clickedSquare(event->pos());
        emit rightClicked(pt);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QPoint pt = clickedSquare(event->pos());

        if (pt.isNull()) return;
        if (m_errorOccurred) return;
        emit clicked(pt);
    }
    else if (event->button() == Qt::RightButton) {
        QPoint pt = clickedSquare(event->pos());
        if (pt.isNull()) return;
        emit rightClicked(pt);
    }
}

void ShogiView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_interaction.dragging()) {
        // 盤全体ではなく、駒の移動前と移動後の範囲だけを描き直す。
        const QRect before = m_interaction.draggingPieceRect(m_layout, m_boardVisuals);
        m_interaction.updateDragPos(event->pos());
        update(before);
        update(m_interaction.draggingPieceRect(m_layout, m_boardVisuals));
    }
    QWidget::mouseMoveEvent(event);
}

void ShogiView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    relayoutTurnLabels();
}

void ShogiView::fitBoardToWidget()
{
    // Fixed SizePolicy方式では使用しない
}

void ShogiView::wheelEvent(QWheelEvent* e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        const int delta = e->angleDelta().y();
        if (delta > 0) {
            enlargeBoard();
        } else if (delta < 0) {
            reduceBoard();
        }
        e->accept();
        return;
    }

    QWidget::wheelEvent(e);
}

// ─────────────────────────────────────────────────────────────────────────────
// ドラッグ操作
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::startDrag(const QPoint &from)
{
    m_interaction.startDrag(from, m_board, mapFromGlobal(QCursor::pos()));
    // つまんだマス（駒が消える・持駒の枚数が減る）と持ち上げた駒だけを描き直す。
    update(fieldPaintRect(from));
    update(m_interaction.draggingPieceRect(m_layout, m_boardVisuals));
}

void ShogiView::endDrag()
{
    if (!m_interaction.dragging()) return;
    // 持ち上げた駒とつまんだマス（駒台・駒箱の枚数を含む）だけを描き直す。
    // 着手による盤・駒台の変化は ShogiBoard の通知（dataChanged / standChanged）で描き直す。
    const QRect lifted = m_interaction.draggingPieceRect(m_layout, m_boardVisuals);
    const QPoint from = m_interaction.dragFrom();
    m_interaction.endDrag();
    update(lifted);
    update(fieldPaintRect(from));
}

// ─────────────────────────────────────────────────────────────────────────────
// 駒画像管理
// ─────────────────────────────────────────────────────────────────────────────

QIcon ShogiView::piece(QChar type) const
{
    return m_pieces.value(type, QIcon());
}

void ShogiView::setPieces()
{
    loadPieceImages(false);
}

void ShogiView::setPiecesFlip()
{
    loadPieceImages(true);
}

void ShogiView::refreshPieceImages()
{
    loadPieceImages(flipMode());
}

void ShogiView::loadPieceImages(bool flipped)
{
    auto& provider = PieceImageProvider::instance();
    const QString types = QStringLiteral("PLNSGBRKQMOTCUplnsgbrkqmotcu");
    bool changed = false;
    for (const QChar type : types) {
        const QIcon icon = provider.icon(type, flipped);
        const auto current = m_pieces.constFind(type);
        if (current != m_pieces.cend() && current->cacheKey() == icon.cacheKey()) continue;
        m_pieces.insert(type, icon);
        changed = true;
    }
    // 画像が同じなら描き直さない（盤を読み込み直すたびに呼ばれるため）
    if (changed) update();
}
