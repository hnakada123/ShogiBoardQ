/// @file shogiviewhighlighting.cpp
/// @brief 将棋盤面のハイライト・矢印の実装

#include "shogiviewhighlighting.h"

#include <QPainter>

ShogiViewHighlighting::ShogiViewHighlighting(ShogiView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// ハイライト
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::setHighlights(BoardHighlightLayer layer, const QList<QPoint>& squares)
{
    QList<QPoint>& current = m_layers[static_cast<std::size_t>(layer)];
    if (current == squares) return;
    // 駒を選ぶたびに合法な移動先を数十マス足し引きするので、前と後のマスだけを描き直す
    updateSquares(current);
    current = squares;
    updateSquares(current);
}

void ShogiViewHighlighting::clearHighlights()
{
    for (std::size_t layer = 0; layer < m_layers.size(); ++layer) {
        updateSquares(m_layers[layer]);
        m_layers[layer].clear();
    }
}

const QList<QPoint>& ShogiViewHighlighting::highlights(BoardHighlightLayer layer) const
{
    return m_layers[static_cast<std::size_t>(layer)];
}

void ShogiViewHighlighting::updateSquares(const QList<QPoint>& squares)
{
    for (const QPoint& square : squares) {
        const QRect rect = m_view->fieldRect(square);
        if (rect.isEmpty()) {
            m_view->update();
            return;
        }
        m_view->update(rect);
    }
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

    // 層ごとの色。移動元は木の盤になじむ黄緑系、移動先・選択は橙系、合法な移動先は青で縁取る。
    // マスの内側だけを塗り、盤の罫線と駒の視認性を保つ
    struct LayerStyle {
        QColor fill;
        QColor border;   ///< 無効なら縁取らない
    };
    static const std::array<LayerStyle, kLayerCount> kStyles{{
        {QColor(145, 163, 96, 155), QColor()},           // LastMoveFrom
        {QColor(238, 147, 68, 165), QColor()},           // LastMoveTo
        {QColor(238, 147, 68, 165), QColor()},           // Selection
        {QColor(65, 155, 225, 105), QColor(35, 105, 165, 200)},   // LegalDestinations
    }};

    painter.save();
    for (std::size_t layer = 0; layer < m_layers.size(); ++layer) {
        const LayerStyle& style = kStyles[layer];
        for (const QPoint& square : m_layers[layer]) {
            const QRect rect = m_view->fieldRect(square);
            if (rect.isNull() || !dirty.intersects(rect)) continue;
            const QRect inner = rect.adjusted(1, 1, -1, -1);
            painter.fillRect(inner, style.fill);
            if (style.border.isValid()) {
                painter.setPen(QPen(style.border, 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(inner.adjusted(1, 1, -1, -1));
            }
        }
    }
    painter.restore();
}

void ShogiViewHighlighting::drawArrows(QPainter& painter)
{
    if (m_arrows.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const ShogiView::Arrow& arrow : std::as_const(m_arrows)) {
        const BoardGeometry& geometry = m_view->boardGeometry();
        const QRect toRect = geometry.square(arrow.toFile, arrow.toRank);
        // 駒打ちは該当する駒台セルを始点とし、盤上の移動と同じ矢印で描く。
        const QRect fromRect = (arrow.fromFile == 0 || arrow.fromRank == 0)
            ? geometry.standCell(arrow.dropPiece)
            : geometry.square(arrow.fromFile, arrow.fromRank);
        if (fromRect.isEmpty() || toRect.isEmpty()) continue;

        const QPointF from(fromRect.center());
        const QPointF to(toRect.center());

        const int arrowWidth = qMax(3, geometry.squareSize() / 12);
        const int arrowHeadSize = qMax(10, geometry.squareSize() / 4);

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

            const int fontSize = qMax(10, geometry.squareSize() / 4);
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
