#include "evaluationchartview.h"

#include <QChart>
#include <QPainter>
#include <QFontMetrics>
#include <QCoreApplication>

#include <utility>

EvaluationChartView::EvaluationChartView(QChart* chart, QWidget* parent)
    : QChartView(chart, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(QStringLiteral("#FAFBFC")));
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}

EvaluationChartView::Placement EvaluationChartView::placement() const
{
    Placement result;
    result.chartPos = chart()->pos();
    result.chartSize = chart()->size();
    result.plotArea = chart()->plotArea();
    result.origin = chart()->mapToPosition(QPointF(0, 0));
    result.unit = chart()->mapToPosition(QPointF(1, 1));
    result.font = font();
    result.compact = width() >= 700;
    return result;
}

void EvaluationChartView::setOverlay(Overlay overlay)
{
    // 棋譜を1手たどるたび・解析の1局面ごと・マウスが別の点に近づくたびに呼ばれる。変わるのは見出しの文・
    // 「N手目」の札・選んだ点の印くらいなので、全体（1倍で約26万画素）ではなく、変わった所だけを描き直す
    const Overlay previous = std::exchange(m_overlay, std::move(overlay));
    if (m_overlay.summary != previous.summary) setAccessibleName(m_overlay.summary);
    const Placement now = placement();
    if (!(now == m_placement) || m_overlay.xLimit != previous.xLimit) {
        m_placement = now;
        viewport()->update();
        return;
    }
    const HeaderLayout header = headerLayout();
    if (m_overlay.summary != previous.summary) updateChartRect(header.summary);
    if (m_overlay.legends[0] != previous.legends[0] || m_overlay.legends[1] != previous.legends[1])
        updateChartRect(header.legends);
    if (m_overlay.currentPly != previous.currentPly) {
        updateChartRect(plyLabelRect(previous));
        updateChartRect(plyLabelRect(m_overlay));
    }
    if (m_overlay.markers != previous.markers) {
        // 前後のどちらかにしか無い印（選んだ印は、選ぶ前の印とは別の印として数える）だけを描き直す
        for (const Marker& marker : previous.markers)
            if (!m_overlay.markers.contains(marker)) updateChartRect(markerRect(marker));
        for (const Marker& marker : std::as_const(m_overlay.markers))
            if (!previous.markers.contains(marker)) updateChartRect(markerRect(marker));
    }
}

void EvaluationChartView::updateChartRect(const QRectF& rect)
{
    if (rect.isEmpty()) return;
    // グラフの座標 → シーンの座標 → 表示領域の座標。アンチエイリアスの分を足す
    const QRectF scene = rect.translated(chart()->pos());
    viewport()->update(mapFromScene(scene).boundingRect().adjusted(-2, -2, 2, 2));
}

EvaluationChartView::HeaderLayout EvaluationChartView::headerLayout() const
{
    HeaderLayout layout;
    const QFontMetrics fm(font());
    layout.row = fm.height() + 7;
    const QRectF header(16, 10, chart()->size().width() - 32, layout.row);
    const bool compact = width() >= 700;
    const qreal summaryWidth = compact ? header.width() * 0.55 : header.width();
    layout.summary = QRectF(header.topLeft(), QSizeF(summaryWidth, layout.row));
    const qreal legendsWidth = compact ? header.width() - summaryWidth - 16 : header.width();
    layout.legends = QRectF(compact ? header.left() + summaryWidth + 16 : header.left(),
                            compact ? header.top() : header.bottom(), legendsWidth, layout.row);
    return layout;
}

QRectF EvaluationChartView::plyLabelRect(const Overlay& overlay) const
{
    if (overlay.currentPly > overlay.xLimit) return {};
    const QFontMetrics fm(font());
    const QRectF area = chart()->plotArea();
    const qreal row = fm.height() + 7;
    const QString text = QCoreApplication::translate("EvaluationChartWidget", "%1手目").arg(overlay.currentPly);
    const qreal labelWidth = fm.horizontalAdvance(text) + 16;
    const qreal cursorX = chart()->mapToPosition(QPointF(overlay.currentPly, 0)).x();
    const qreal labelX = qBound(area.left(), cursorX - labelWidth / 2, qMax(area.left(), area.right() - labelWidth));
    return QRectF(labelX, area.top() - row - 3, labelWidth, row);
}

QRectF EvaluationChartView::markerRect(const Marker& marker) const
{
    if (marker.value.x() > m_overlay.xLimit) return {};
    // 選んだ端の三角は、点から横に 6、縦に 9.6 まで広がる。線の太さの分を足す
    const QPointF p = chart()->mapToPosition(marker.value);
    return QRectF(p.x() - 8, p.y() - 12, 16, 24);
}

QColor EvaluationChartView::seriesColor(int side)
{
    return QColor(side == 0 ? QStringLiteral("#2563EB") : QStringLiteral("#C26719"));
}

void EvaluationChartView::drawHeader(QPainter* painter, const QRectF& area)
{
    const QFontMetrics fm(font());
    const HeaderLayout header = headerLayout();
    const qreal row = header.row;
    painter->setPen(QColor(QStringLiteral("#334155")));
    painter->drawText(header.summary, Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(m_overlay.summary, Qt::ElideRight, static_cast<int>(header.summary.width())));
    const int count = (!m_overlay.legends[0].isEmpty() ? 1 : 0) + (!m_overlay.legends[1].isEmpty() ? 1 : 0);
    const qreal legendWidth = header.legends.width() / qMax(1, count);
    const qreal legendTop = header.legends.top();
    qreal x = header.legends.left();
    for (int side = 0; side < 2; ++side) {
        if (m_overlay.legends[side].isEmpty()) continue;
        const qreal y = legendTop + row / 2;
        QPen pen(seriesColor(side), 2);
        if (side == 1) pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->drawLine(QPointF(x, y), QPointF(x + 20, y));
        painter->setPen(QColor(QStringLiteral("#475569")));
        painter->drawText(QRectF(x + 28, legendTop, legendWidth - 36, row), Qt::AlignVCenter,
                          fm.elidedText(m_overlay.legends[side], Qt::ElideRight, static_cast<int>(legendWidth - 36)));
        x += legendWidth;
    }

    painter->setPen(QColor(QStringLiteral("#64748B")));
    const qreal hintWidth = chart()->size().width() - area.right() - 12;
    painter->drawText(QRectF(area.right() + 8, area.top() - fm.height() / 2.0, hintWidth, fm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QCoreApplication::translate("EvaluationChartWidget", "先手有利"));
    painter->drawText(QRectF(area.right() + 8, area.bottom() - fm.height() / 2.0, hintWidth, fm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QCoreApplication::translate("EvaluationChartWidget", "後手有利"));

    const QRectF label = plyLabelRect(m_overlay);
    if (label.isEmpty()) return;
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(QStringLiteral("#E2E8F0")));
    painter->drawRoundedRect(label, 4, 4);
    painter->setPen(QColor(QStringLiteral("#334155")));
    painter->drawText(label, Qt::AlignCenter,
                      QCoreApplication::translate("EvaluationChartWidget", "%1手目").arg(m_overlay.currentPly));
}

void EvaluationChartView::drawMarkers(QPainter* painter, const QRectF& area)
{
    painter->save();
    painter->setClipRect(area.adjusted(-6, -6, 6, 6));
    for (const auto& marker : std::as_const(m_overlay.markers)) {
        if (marker.value.x() > m_overlay.xLimit) continue;
        const QPointF p = chart()->mapToPosition(marker.value);
        painter->setPen(QPen(marker.color, marker.selected ? 2 : 1));
        painter->setBrush(marker.selected ? QColor(Qt::white) : marker.color);
        if (marker.edge) {
            const qreal direction = marker.value.y() >= 0 ? 1 : -1;
            const qreal size = marker.selected ? 6 : 4;
            painter->drawPolygon(QPolygonF{p, p + QPointF(-size, direction * size * 1.6),
                                            p + QPointF(size, direction * size * 1.6)});
        } else {
            const qreal radius = marker.selected ? 4.5 : 2;
            if (marker.second) painter->drawRect(QRectF(p.x() - radius, p.y() - radius, radius * 2, radius * 2));
            else painter->drawEllipse(p, radius, radius);
        }
    }
    painter->restore();
}

void EvaluationChartView::drawForeground(QPainter* painter, const QRectF& rect)
{
    QChartView::drawForeground(painter, rect);
    painter->save();
    painter->translate(chart()->pos());
    painter->setFont(font());
    drawHeader(painter, chart()->plotArea());
    drawMarkers(painter, chart()->plotArea());
    painter->restore();
}
