#ifndef EVALUATIONCHARTVIEW_H
#define EVALUATIONCHARTVIEW_H

#include <QChartView>
#include <QColor>
#include <QList>
#include <QString>

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
    /// 前景に描くもの（見出し・凡例・点の印・表示中の手数の線）
    struct Overlay {
        QString summary;                 ///< 見出し（表示中の手数と評価値）。読み上げ名にもする
        QString legends[2];              ///< 系列の名前（空なら凡例を描かない）
        QList<Marker> markers;           ///< 点の印
        int currentPly = 0;              ///< 表示中の手数（線を引く）
        int xLimit = 20;                 ///< 横軸の最大の手数（これより右は描かない）
    };

    explicit EvaluationChartView(QChart* chart, QWidget* parent = nullptr);

    /// 前景に描くものを置き換えて描き直す
    void setOverlay(Overlay overlay);
    const Overlay& overlay() const { return m_overlay; }

    static QColor seriesColor(int side);

protected:
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    void drawHeader(QPainter* painter, const QRectF& area);
    void drawMarkers(QPainter* painter, const QRectF& area);

    Overlay m_overlay;
};

#endif
