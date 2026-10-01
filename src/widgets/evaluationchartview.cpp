#include "evaluationchartview.h"

#include <QChart>
#include <QPainter>
#include <QFontMetrics>
#include <QCoreApplication>

EvaluationChartView::EvaluationChartView(QChart* chart, QWidget* parent)
    : QChartView(chart, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor(QStringLiteral("#FAFBFC")));
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}

QColor EvaluationChartView::seriesColor(int side)
{
    return QColor(side == 0 ? QStringLiteral("#2563EB") : QStringLiteral("#C26719"));
}

void EvaluationChartView::drawHeader(QPainter* painter, const QRectF& area)
{
    const QFontMetrics fm(font());
    const qreal row = fm.height() + 7;
    const QRectF header(16, 10, chart()->size().width() - 32, row);
    const bool compact = width() >= 700;
    const qreal summaryWidth = compact ? header.width() * 0.55 : header.width();
    painter->setPen(QColor(QStringLiteral("#334155")));
    painter->drawText(QRectF(header.topLeft(), QSizeF(summaryWidth, row)), Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(summary, Qt::ElideRight, static_cast<int>(summaryWidth)));
    const int count = (!legends[0].isEmpty() ? 1 : 0) + (!legends[1].isEmpty() ? 1 : 0);
    const qreal legendWidth = (compact ? header.width() - summaryWidth - 16 : header.width()) / qMax(1, count);
    const qreal legendTop = compact ? header.top() : header.bottom();
    qreal x = compact ? header.left() + summaryWidth + 16 : header.left();
    for (int side = 0; side < 2; ++side) {
        if (legends[side].isEmpty()) continue;
        const qreal y = legendTop + row / 2;
        QPen pen(seriesColor(side), 2);
        if (side == 1) pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->drawLine(QPointF(x, y), QPointF(x + 20, y));
        painter->setPen(QColor(QStringLiteral("#475569")));
        painter->drawText(QRectF(x + 28, legendTop, legendWidth - 36, row), Qt::AlignVCenter,
                          fm.elidedText(legends[side], Qt::ElideRight, static_cast<int>(legendWidth - 36)));
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

    if (currentPly > xLimit) return;
    const QString text = QCoreApplication::translate("EvaluationChartWidget", "%1手目").arg(currentPly);
    const qreal labelWidth = fm.horizontalAdvance(text) + 16;
    const qreal cursorX = chart()->mapToPosition(QPointF(currentPly, 0)).x();
    const qreal labelX = qBound(area.left(), cursorX - labelWidth / 2, qMax(area.left(), area.right() - labelWidth));
    const QRectF label(labelX, area.top() - row - 3, labelWidth, row);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(QStringLiteral("#E2E8F0")));
    painter->drawRoundedRect(label, 4, 4);
    painter->setPen(QColor(QStringLiteral("#334155")));
    painter->drawText(label, Qt::AlignCenter, text);
}

void EvaluationChartView::drawMarkers(QPainter* painter, const QRectF& area)
{
    painter->save();
    painter->setClipRect(area.adjusted(-6, -6, 6, 6));
    for (const auto& marker : std::as_const(markers)) {
        if (marker.value.x() > xLimit) continue;
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
