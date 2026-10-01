#ifndef EVALUATIONCHARTVIEW_H
#define EVALUATIONCHARTVIEW_H

#include <QChartView>
#include <QColor>

/// 凡例・選択位置・範囲外マーカーを画像出力にも含めて描くチャートビュー。
class EvaluationChartView : public QChartView
{
public:
    struct Marker {
        QPointF value;
        QColor color;
        bool edge = false;
        bool selected = false;
        bool second = false;
    };
    explicit EvaluationChartView(QChart* chart, QWidget* parent = nullptr);
    QString summary;
    QString legends[2];
    QList<Marker> markers;
    int currentPly = 0;
    int xLimit = 20;
    int yLimit = 1000;

    static QColor seriesColor(int side);

protected:
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    void drawHeader(QPainter* painter, const QRectF& area);
    void drawMarkers(QPainter* painter, const QRectF& area);
};

#endif
