#ifndef EVALUATIONCHARTWIDGET_H
#define EVALUATIONCHARTWIDGET_H

#pragma once
#include <QWidget>

class QChart;
class QLineSeries;
class QValueAxis;
class QChartView;

class EvaluationChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvaluationChartWidget(QWidget* parent = nullptr);

    void appendScoreP1(int ply, int cp, bool invert = false);
    void appendScoreP2(int ply, int cp, bool invert = false);
    void clearAll();

    void removeLastP1();
    void removeLastP2();

private:
    QChart*      m_chart = nullptr;
    QLineSeries* m_s1    = nullptr;
    QLineSeries* m_s2    = nullptr;
    QValueAxis*  m_axX   = nullptr;
    QValueAxis*  m_axY   = nullptr;
    QChartView*  m_chartView = nullptr;
};

#endif // EVALUATIONCHARTWIDGET_H
