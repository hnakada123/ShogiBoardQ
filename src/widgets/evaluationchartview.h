#ifndef EVALUATIONCHARTVIEW_H
#define EVALUATIONCHARTVIEW_H

#include <QChartView>
#include <QColor>
#include <QFont>
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

        bool operator==(const Marker& other) const
        {
            return value == other.value && color == other.color && edge == other.edge
                && selected == other.selected && second == other.second;
        }
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

    /// 前景に描くものを置き換え、変わった所（見出しの文・凡例・「N手目」の札・変わった印）だけを描き直す。
    /// グラフの位置や目盛りが前に置き換えたときと違えば、全体を描き直す
    void setOverlay(Overlay overlay);
    const Overlay& overlay() const { return m_overlay; }

    static QColor seriesColor(int side);

protected:
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    /// 前景の配置を決める値（グラフの大きさ・描画領域・値から位置への対応・書体・1行に収める幅か）。
    /// 同じなら、同じ内容は同じ位置に描かれる
    struct Placement {
        QPointF chartPos;
        QSizeF chartSize;
        QRectF plotArea;
        QPointF origin;   ///< 値 (0, 0) の位置
        QPointF unit;     ///< 値 (1, 1) の位置
        QFont font;
        bool compact = false;

        bool operator==(const Placement& other) const
        {
            return chartPos == other.chartPos && chartSize == other.chartSize && plotArea == other.plotArea
                && origin == other.origin && unit == other.unit && font == other.font && compact == other.compact;
        }
    };
    /// 見出しの行の配置（描くときと描き直す範囲で同じ値を使う）
    struct HeaderLayout {
        qreal row = 0;        ///< 1行の高さ
        QRectF summary;       ///< 見出しの文
        QRectF legends;       ///< 凡例の並び
        qreal legendWidth = 0;
    };

    Placement placement() const;
    HeaderLayout headerLayout() const;
    /// 「N手目」の札の範囲（描かなければ空）
    QRectF plyLabelRect(const Overlay& overlay) const;
    /// 印を描く範囲（選んだ印・端の三角も収まる大きさ）
    QRectF markerRect(const Marker& marker) const;
    /// グラフの座標の範囲を描き直す
    void updateChartRect(const QRectF& rect);
    void drawHeader(QPainter* painter, const QRectF& area);
    void drawMarkers(QPainter* painter, const QRectF& area);

    Overlay m_overlay;
    Placement m_placement;   ///< 前に置き換えたときの配置
};

#endif
