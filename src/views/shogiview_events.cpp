/// @file shogiview_events.cpp
/// @brief ShogiView のイベント処理・座標変換・駒画像管理

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "globaltooltip.h"

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

QPoint ShogiView::getClickedSquare(const QPoint &clickPosition) const
{
    return m_interaction.getClickedSquare(clickPosition, m_layout, m_board);
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
        QPoint pt = getClickedSquare(event->pos());
        emit rightClicked(pt);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QPoint pt = getClickedSquare(event->pos());

        if (pt.isNull()) return;
        if (m_errorOccurred) return;
        emit clicked(pt);
    }
    else if (event->button() == Qt::RightButton) {
        QPoint pt = getClickedSquare(event->pos());
        if (pt.isNull()) return;
        emit rightClicked(pt);
    }
}

void ShogiView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_interaction.dragging()) {
        m_interaction.updateDragPos(event->pos());
        update();
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
    update();
}

void ShogiView::endDrag()
{
    m_interaction.endDrag();
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// 駒画像管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setPiece(char type, const QIcon &icon)
{
    m_pieces.insert(type, icon);
    m_standPiecePixmapCache.clear();
    m_highlighting->clearDropPieceCache();
    update();
}

QIcon ShogiView::piece(QChar type) const
{
    return m_pieces.value(type, QIcon());
}

void ShogiView::setPieces()
{
    // ── 先手（大文字） ──
    setPiece('P', QIcon(":/pieces/Sente_fu45.svg"));
    setPiece('L', QIcon(":/pieces/Sente_kyou45.svg"));
    setPiece('N', QIcon(":/pieces/Sente_kei45.svg"));
    setPiece('S', QIcon(":/pieces/Sente_gin45.svg"));
    setPiece('G', QIcon(":/pieces/Sente_kin45.svg"));
    setPiece('B', QIcon(":/pieces/Sente_kaku45.svg"));
    setPiece('R', QIcon(":/pieces/Sente_hi45.svg"));
    setPiece('K', QIcon(":/pieces/Sente_ou45.svg"));

    // 昇格駒（先手）
    setPiece('Q', QIcon(":/pieces/Sente_to45.svg"));
    setPiece('M', QIcon(":/pieces/Sente_narikyou45.svg"));
    setPiece('O', QIcon(":/pieces/Sente_narikei45.svg"));
    setPiece('T', QIcon(":/pieces/Sente_narigin45.svg"));
    setPiece('C', QIcon(":/pieces/Sente_uma45.svg"));
    setPiece('U', QIcon(":/pieces/Sente_ryuu45.svg"));

    // ── 後手（小文字） ──
    setPiece('p', QIcon(":/pieces/Gote_fu45.svg"));
    setPiece('l', QIcon(":/pieces/Gote_kyou45.svg"));
    setPiece('n', QIcon(":/pieces/Gote_kei45.svg"));
    setPiece('s', QIcon(":/pieces/Gote_gin45.svg"));
    setPiece('g', QIcon(":/pieces/Gote_kin45.svg"));
    setPiece('b', QIcon(":/pieces/Gote_kaku45.svg"));
    setPiece('r', QIcon(":/pieces/Gote_hi45.svg"));
    setPiece('k', QIcon(":/pieces/Gote_gyoku45.svg"));

    // 昇格駒（後手）
    setPiece('q', QIcon(":/pieces/Gote_to45.svg"));
    setPiece('m', QIcon(":/pieces/Gote_narikyou45.svg"));
    setPiece('o', QIcon(":/pieces/Gote_narikei45.svg"));
    setPiece('t', QIcon(":/pieces/Gote_narigin45.svg"));
    setPiece('c', QIcon(":/pieces/Gote_uma45.svg"));
    setPiece('u', QIcon(":/pieces/Gote_ryuu45.svg"));
}

void ShogiView::setPiecesFlip()
{
    // ── flip 時：小文字（元・後手）に先手の画像を割り当て ──
    setPiece('p', QIcon(":/pieces/Sente_fu45.svg"));
    setPiece('l', QIcon(":/pieces/Sente_kyou45.svg"));
    setPiece('n', QIcon(":/pieces/Sente_kei45.svg"));
    setPiece('s', QIcon(":/pieces/Sente_gin45.svg"));
    setPiece('g', QIcon(":/pieces/Sente_kin45.svg"));
    setPiece('b', QIcon(":/pieces/Sente_kaku45.svg"));
    setPiece('r', QIcon(":/pieces/Sente_hi45.svg"));
    setPiece('k', QIcon(":/pieces/Sente_gyoku45.svg"));

    // 昇格駒（flip：小文字→先手画像）
    setPiece('q', QIcon(":/pieces/Sente_to45.svg"));
    setPiece('m', QIcon(":/pieces/Sente_narikyou45.svg"));
    setPiece('o', QIcon(":/pieces/Sente_narikei45.svg"));
    setPiece('t', QIcon(":/pieces/Sente_narigin45.svg"));
    setPiece('c', QIcon(":/pieces/Sente_uma45.svg"));
    setPiece('u', QIcon(":/pieces/Sente_ryuu45.svg"));

    // ── flip 時：大文字（元・先手）に後手の画像を割り当て ──
    setPiece('P', QIcon(":/pieces/Gote_fu45.svg"));
    setPiece('L', QIcon(":/pieces/Gote_kyou45.svg"));
    setPiece('N', QIcon(":/pieces/Gote_kei45.svg"));
    setPiece('S', QIcon(":/pieces/Gote_gin45.svg"));
    setPiece('G', QIcon(":/pieces/Gote_kin45.svg"));
    setPiece('B', QIcon(":/pieces/Gote_kaku45.svg"));
    setPiece('R', QIcon(":/pieces/Gote_hi45.svg"));
    setPiece('K', QIcon(":/pieces/Gote_ou45.svg"));

    // 昇格駒（flip：大文字→後手画像）
    setPiece('Q', QIcon(":/pieces/Gote_to45.svg"));
    setPiece('M', QIcon(":/pieces/Gote_narikyou45.svg"));
    setPiece('O', QIcon(":/pieces/Gote_narikei45.svg"));
    setPiece('T', QIcon(":/pieces/Gote_narigin45.svg"));
    setPiece('C', QIcon(":/pieces/Gote_uma45.svg"));
    setPiece('U', QIcon(":/pieces/Gote_ryuu45.svg"));
}
