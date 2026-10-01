/// @file evaluationchartwidget_tooltip.cpp
/// @brief 近傍点のホバー・元の評価値を表示するツールチップ
#include "evaluationchartwidget.h"
#include "evaluationchartview.h"

#include <QChart>
#include <QLabel>
#include <QLineSeries>
#include <cmath>
#include <limits>

void EvaluationChartWidget::setupTooltip()
{
    m_tooltip = new QLabel(m_chartView);
    m_tooltip->setObjectName(QStringLiteral("evalTooltip"));
    m_tooltip->setTextFormat(Qt::PlainText);
    m_tooltip->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_tooltip->setStyleSheet(QStringLiteral(
        "QLabel { background: #FFFFFF; color: #334155; border: 1px solid #CBD5E1;"
        "border-radius: 5px; padding: 7px 10px; }"));
    m_tooltip->hide();
}

void EvaluationChartWidget::clearHover()
{
    if (m_tooltip) m_tooltip->hide();
    if (m_hoverSide < 0) return;
    m_hoverSide = -1;
    m_hoverPly = -1;
    updatePresentation();
}

void EvaluationChartWidget::hoverAt(const QPoint& position)
{
    const QPointF chartPos = m_chart->mapFromScene(m_chartView->mapToScene(position));
    if (!m_chart->plotArea().contains(chartPos)) { clearHover(); return; }
    double distance = std::numeric_limits<double>::max();
    int sideFound = -1;
    int plyFound = -1;
    for (int side = 0; side < 2; ++side) {
        for (auto it = m_scores[side].cbegin(); it != m_scores[side].cend(); ++it) {
            if (it.key() > xAxisLimit()) continue;
            const QPointF point = m_chart->mapToPosition(QPointF(it.key(), displayedValue(*it)), m_series[side]);
            const qreal dx = qAbs(point.x() - chartPos.x());
            // 手数の範囲によらず、画面上で近い点を拾う。
            if (dx > 14) continue;
            const double candidate = dx * dx + std::pow((point.y() - chartPos.y()) * 0.15, 2);
            if (candidate < distance) { distance = candidate; sideFound = side; plyFound = it.key(); }
        }
    }
    if (sideFound < 0) { clearHover(); return; }
    m_hoverSide = sideFound;
    m_hoverPly = plyFound;
    const Score score = m_scores[sideFound].value(plyFound);
    m_tooltip->setFont(m_chartView->font());
    m_tooltip->setText(tr("%1\n%2手目：%3\n先手視点の評価値")
                       .arg(seriesName(sideFound)).arg(plyFound).arg(scoreText(score)));
    m_tooltip->adjustSize();
    const int x = qBound(4, position.x() + 12, qMax(4, m_chartView->width() - m_tooltip->width() - 4));
    const int y = qBound(4, position.y() - m_tooltip->height() - 12,
                        qMax(4, m_chartView->height() - m_tooltip->height() - 4));
    m_tooltip->move(x, y);
    m_tooltip->show();
    m_tooltip->raise();
    updatePresentation();
}
