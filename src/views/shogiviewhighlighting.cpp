/// @file shogiviewhighlighting.cpp
/// @brief 将棋盤面のハイライト・矢印の実装

#include "shogiviewhighlighting.h"
#include "shogiviewlayout.h"

#include <QPainter>

ShogiViewHighlighting::ShogiViewHighlighting(ShogiView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// ハイライト管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::addHighlight(ShogiView::Highlight* hl)
{
    m_highlights.append(hl);
    updateHighlightArea(hl);
}

void ShogiViewHighlighting::removeHighlight(ShogiView::Highlight* hl)
{
    m_highlights.removeOne(hl);
    updateHighlightArea(hl);
}

// ハイライトのマスだけを描き直す。駒を選ぶたびに合法な移動先を数十マス足し引きするため、
// そのたびに盤全体を描き直さない。
void ShogiViewHighlighting::updateHighlightArea(const ShogiView::Highlight* hl)
{
    QRect rect;
    if (hl && hl->type() == ShogiView::FieldHighlight::Type) {
        const auto* fhl = static_cast<const ShogiView::FieldHighlight*>(hl);
        rect = m_view->fieldRect(QPoint(fhl->file(), fhl->rank()));
    }
    if (rect.isEmpty()) m_view->update();
    else m_view->update(rect);
}

void ShogiViewHighlighting::removeHighlightAllData()
{
    // ハイライトの寿命は呼び出し側が管理するため、ここでは参照だけ破棄する。
    m_highlights.clear();
    emit highlightsCleared();
    m_view->update();
}

// ─────────────────────────────────────────────────────────────────────────────
// 矢印管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::setArrows(const QList<ShogiView::Arrow>& arrows)
{
    // エンジンの候補手が届くたびに呼ばれるため、矢印が変わらなければ描き直さない。
    // 変わったときも、前の矢印と新しい矢印が掛かる範囲だけを描き直す（盤全体の描き直しは
    // 1倍で約0.5ms、2倍で約3ms。検討で候補手を表示していると候補が替わるたびに起きる）。
    if (m_arrows == arrows) return;
    const QRect dirty = arrowsPaintRect(m_arrows) | arrowsPaintRect(arrows);
    m_arrows = arrows;
    if (dirty.isEmpty()) m_view->update();
    else m_view->update(dirty);
}

void ShogiViewHighlighting::clearArrows()
{
    if (m_arrows.isEmpty()) return;
    const QRect dirty = arrowsPaintRect(m_arrows);
    m_arrows.clear();
    if (dirty.isEmpty()) m_view->update();
    else m_view->update(dirty);
}

QRect ShogiViewHighlighting::arrowsPaintRect(const QList<ShogiView::Arrow>& arrows) const
{
    if (!m_view->board()) return {};
    const int squareSize = m_view->squareSize();
    // drawArrows と同じ太さ・矢じり・順位の丸印の大きさに、アンチエイリアスの分を足す
    const int arrowWidth = qMax(3, squareSize / 12);
    const int arrowHeadSize = qMax(10, squareSize / 4);
    const int circleRadius = static_cast<int>(qMax(10, squareSize / 4) * 0.8);
    const int margin = qMax(arrowHeadSize, circleRadius) + arrowWidth + 2;
    QRect rect;
    for (const ShogiView::Arrow& arrow : arrows) {
        const QRect toRect = m_view->fieldRect(QPoint(arrow.toFile, arrow.toRank));
        const QRect fromRect = (arrow.fromFile == 0 || arrow.fromRank == 0)
            ? m_view->standPieceRect(arrow.dropPiece)
            : m_view->fieldRect(QPoint(arrow.fromFile, arrow.fromRank));
        if (fromRect.isEmpty() || toRect.isEmpty()) continue;
        rect |= (fromRect | toRect).adjusted(-margin, -margin, margin, margin);
    }
    return rect;
}



void ShogiViewHighlighting::drawHighlights(QPainter& painter, const QRegion& dirty)
{
    if (!m_view->board()) return;

    painter.save();
    for (int i = 0; i < highlightCount(); ++i) {
        ShogiView::Highlight* hl = highlight(i);
        if (!hl || hl->type() != ShogiView::FieldHighlight::Type) continue;

        const auto* fhl = static_cast<ShogiView::FieldHighlight*>(hl);
        const QRect rect = m_view->fieldRect(QPoint(fhl->file(), fhl->rank()));
        if (rect.isNull() || !dirty.intersects(rect)) continue;

        const QColor originalColor = fhl->color();

        // 移動元は木の盤になじむ黄緑系、移動先・選択表示は橙系に揃える。
        // マスの内側だけを塗り、盤の罫線と駒の視認性を保つ。
        const bool source = originalColor.red() > 200 && originalColor.green() < 100
            && originalColor.blue() < 150;
        const bool legalDestination = fhl->purpose() == ShogiView::FieldHighlight::Purpose::LegalDestination;
        const QColor fill = legalDestination ? originalColor
            : source ? QColor(145, 163, 96, 155) : QColor(238, 147, 68, 165);
        const QRect inner = rect.adjusted(1, 1, -1, -1);
        painter.fillRect(inner, fill);
        if (legalDestination) {
            painter.setPen(QPen(QColor(35, 105, 165, 200), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(inner.adjusted(1, 1, -1, -1));
        }
    }
    painter.restore();
}

void ShogiViewHighlighting::drawArrows(QPainter& painter, const ShogiViewLayout& layout)
{
    if (m_arrows.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const ShogiView::Arrow& arrow : std::as_const(m_arrows)) {
        const QPoint offset(layout.offsetX(), layout.offsetY());
        const QRect toRect = m_view->cachedFieldRect(arrow.toFile, arrow.toRank).translated(offset);
        // 駒打ちは該当する駒台セルを始点とし、盤上の移動と同じ矢印で描く。
        const QRect fromRect = (arrow.fromFile == 0 || arrow.fromRank == 0)
            ? m_view->standPieceRect(arrow.dropPiece)
            : m_view->cachedFieldRect(arrow.fromFile, arrow.fromRank).translated(offset);
        if (fromRect.isEmpty() || toRect.isEmpty()) continue;

        const QPointF from(fromRect.center());
        const QPointF to(toRect.center());

        const int arrowWidth = qMax(3, layout.squareSize() / 12);
        const int arrowHeadSize = qMax(10, layout.squareSize() / 4);

        QColor color = arrow.color;

        QPen pen(color);
        pen.setWidth(arrowWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setStyle(arrow.penStyle);
        painter.setPen(pen);

        QPointF dir = to - from;
        double length = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
        if (length < 1.0) continue;

        QPointF unit = dir / length;

        QPointF lineEnd = to - unit * (arrowHeadSize * 0.7);
        painter.drawLine(from, lineEnd);

        QPointF perpendicular(-unit.y(), unit.x());
        QPointF arrowPoint1 = to - unit * arrowHeadSize + perpendicular * (arrowHeadSize * 0.5);
        QPointF arrowPoint2 = to - unit * arrowHeadSize - perpendicular * (arrowHeadSize * 0.5);

        QPolygonF arrowHead;
        arrowHead << to << arrowPoint1 << arrowPoint2;

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(arrowHead);

        if (arrow.priority >= 1 && m_arrows.size() >= 2) {
            QPointF center((from.x() + to.x()) / 2.0, (from.y() + to.y()) / 2.0);

            const int fontSize = qMax(10, layout.squareSize() / 4);
            QFont font = painter.font();
            font.setPointSize(fontSize);
            font.setBold(true);
            painter.setFont(font);

            const int circleRadius = static_cast<int>(fontSize * 0.8);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 230));
            painter.drawEllipse(center, circleRadius, circleRadius);

            painter.setPen(arrow.color);
            QString priorityText = QString::number(arrow.priority);
            QRectF textRect(center.x() - circleRadius, center.y() - circleRadius,
                           circleRadius * 2, circleRadius * 2);
            painter.drawText(textRect, Qt::AlignCenter, priorityText);
        }
    }

    painter.restore();
}
