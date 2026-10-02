#ifndef EVALUATIONCHARTWIDGET_H
#define EVALUATIONCHARTWIDGET_H

#include <QWidget>
#include <QMap>
#include <QPointF>

class QChart;
class QLineSeries;
class QValueAxis;
class EvaluationChartView;
class QLabel;
class QTimer;
class EvaluationChartConfigurator;

class EvaluationChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvaluationChartWidget(QWidget* parent = nullptr);
    ~EvaluationChartWidget() override;
    QWidget* chartViewWidget() const;

    // mate は USI の score mate（数値または +/-）。空文字列は通常評価値。
    void appendScoreP1(int ply, int cp, bool invert = false, const QString& mate = {});
    void appendScoreP2(int ply, int cp, bool invert = false, const QString& mate = {});
    void appendScoreP1Buffered(int ply, int cp, bool invert = false, const QString& mate = {});
    void appendScoreP2Buffered(int ply, int cp, bool invert = false, const QString& mate = {});
    void flushPendingScores();
    void clearAll();
    void removeLastP1();
    void removeLastP2();
    void trimToPly(int maxPly);
    int countP1() const;
    int countP2() const;

    int yAxisLimit() const;
    int yAxisInterval() const;
    int xAxisLimit() const;
    int xAxisInterval() const;
    int labelFontSize() const;
    void setYAxisLimit(int limit);
    void setYAxisInterval(int interval);
    void setXAxisLimit(int limit);
    void setXAxisInterval(int interval);
    void setLabelFontSize(int size);
    void setAutomaticRange(bool automatic);
    void setEngine1Name(const QString& name);
    void setEngine2Name(const QString& name);
    void setCurrentPly(int ply);
    void setRecordLength(int plies);
    void setAnalysisLineIndex(int lineIndex);
    int currentPly() const { return m_currentPly; }

signals:
    void yAxisSettingsChanged(int limit, int interval);
    void xAxisSettingsChanged(int limit, int interval);
    void plyClicked(int ply);
    void analysisPlyClicked(int lineIndex, int ply);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

public slots:
    void setFloating(bool floating);

private slots:
    void applyYAxisSettings();
    void applyXAxisSettings();
    void applyFontSize();
    void onPlotAreaChanged();

private:
    struct Score {
        int cp = 0;                 // 先手視点。描画時に丸めても元の値を保持する。
        QString mate;              // 先手視点の符号、手数不明は +/-。
    };
    struct PendingScore {
        int side;
        int ply;
        int cp;
        bool invert;
        QString mate;
    };
    void setupAxes();
    void setupChart();
    void setupSeries();
    void setupChartViewAndLayout();
    void updateReferenceLines();
    void rebuildSeries();
    void refreshData();
    void updatePresentation();
    void appendScore(int side, int ply, int cp, bool invert, const QString& mate);
    void bufferScore(int side, int ply, int cp, bool invert, const QString& mate);
    void removeLast(int side);
    qreal displayedValue(const Score& score) const;
    QString scoreText(const Score& score) const;
    QString seriesName(int side) const;
    void setupTooltip();
    void hoverAt(const QPoint& position);
    void clearHover();

    int m_analysisLineIndex = -1; // -1: 対局中のグラフ、0以上: 解析対象のライン
    QChart* m_chart = nullptr;
    QLineSeries* m_series[2] = {};
    QLineSeries* m_zeroLine = nullptr;
    QLineSeries* m_cursorLine = nullptr;
    QValueAxis* m_axX = nullptr;
    QValueAxis* m_axY = nullptr;
    EvaluationChartView* m_chartView = nullptr;
    QLabel* m_tooltip = nullptr;
    EvaluationChartConfigurator* m_configurator = nullptr;
    QString m_engineNames[2];
    QMap<int, Score> m_scores[2];
    int m_currentPly = 0;
    int m_maxVisitedPly = 0;
    int m_hoverSide = -1;
    int m_hoverPly = -1;
    QList<PendingScore> m_pending;
    QTimer* m_flushTimer = nullptr;
};

#endif
