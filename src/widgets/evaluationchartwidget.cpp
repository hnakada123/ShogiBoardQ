#include "evaluationchartwidget.h"

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QBrush>
#include <QPen>
#include <QFont>
#include <QColor>

EvaluationChartWidget::EvaluationChartWidget(QWidget* parent)
    : QWidget(parent)
{
    // 軸
    m_axX = new QValueAxis(this);
    m_axY = new QValueAxis(this);

    QFont labelsFont("Noto Sans CJK JP", 6);
    m_axX->setRange(0, 1000);
    m_axX->setTickCount(201);
    m_axX->setLabelsFont(labelsFont);
    m_axX->setLabelFormat("%i");

    m_axY->setRange(-2000, 2000);
    m_axY->setTickCount(5);
    m_axY->setLabelsFont(labelsFont);
    m_axY->setLabelFormat("%i");

    const QColor lightGray(192, 192, 192);
    m_axX->setLabelsColor(lightGray);
    m_axY->setLabelsColor(lightGray);

    // チャート
    m_chart = new QChart();
    m_chart->legend()->hide();
    m_chart->setBackgroundBrush(QBrush(QColor(0, 100, 0))); // ダークグリーン

    m_chart->addAxis(m_axX, Qt::AlignBottom);
    m_chart->addAxis(m_axY, Qt::AlignLeft);

    // シリーズ
    m_s1 = new QLineSeries(this); // 先手側
    m_s2 = new QLineSeries(this); // 後手側

    {
        QPen p = m_s1->pen();
        p.setWidth(2);
        m_s1->setPen(p);
        m_s1->setPointsVisible(true);
        m_s1->setColor(Qt::white);
    }
    {
        QPen p = m_s2->pen();
        p.setWidth(2);
        m_s2->setPen(p);
        m_s2->setPointsVisible(true);
        m_s2->setColor(Qt::black);
    }

    m_chart->addSeries(m_s1);
    m_chart->addSeries(m_s2);

    m_s1->attachAxis(m_axX);
    m_s1->attachAxis(m_axY);
    m_s2->attachAxis(m_axX);
    m_s2->attachAxis(m_axY);

    // ビュー
    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_chartView);
    setLayout(lay);

    setMinimumHeight(170);
}

void EvaluationChartWidget::appendScoreP1(int ply, int cp, bool invert)
{
    const qreal y = invert ? -cp : cp;
    m_s1->append(ply, y);
    if (ply > m_axX->max()) m_axX->setRange(0, ply + 50);
    if (m_chartView) m_chartView->update();
}

void EvaluationChartWidget::appendScoreP2(int ply, int cp, bool invert)
{
    const qreal y = invert ? -cp : cp;
    m_s2->append(ply, y);
    if (ply > m_axX->max()) m_axX->setRange(0, ply + 50);
    if (m_chartView) m_chartView->update();
}

void EvaluationChartWidget::clearAll()
{
    if (m_s1) m_s1->clear();
    if (m_s2) m_s2->clear();
    if (m_axX) m_axX->setRange(0, 1000);
    if (m_axY) m_axY->setRange(-2000, 2000);
    if (m_chartView) m_chartView->update();
}

void EvaluationChartWidget::removeLastP1()
{
    if (!m_s1) return;
    const int n = m_s1->count();
    if (n > 0) m_s1->removePoints(n - 1, 1);
    if (m_chartView) m_chartView->update();
}

void EvaluationChartWidget::removeLastP2()
{
    if (!m_s2) return;
    const int n = m_s2->count();
    if (n > 0) m_s2->removePoints(n - 1, 1);
    if (m_chartView) m_chartView->update();
}
