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
class QGridLayout;
#include "usiscore.h"

class QPushButton;
class EvaluationChartConfigurator;

class EvaluationChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvaluationChartWidget(QWidget* parent = nullptr);
    ~EvaluationChartWidget() override;
    QWidget* chartViewWidget() const;

    /// 先手から見た評価値を ply 手目に描く（P1 は 1つ目の系列、P2 は 2つ目の系列）。評価値なしは描かない
    void appendScoreP1(int ply, const UsiScore& blackScore);
    void appendScoreP2(int ply, const UsiScore& blackScore);
    /// 少し待ってからまとめて描く（棋譜解析で1局面ずつ届くとき）
    void appendScoreP1Buffered(int ply, const UsiScore& blackScore);
    void appendScoreP2Buffered(int ply, const UsiScore& blackScore);
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

    // 棋譜欄と同じ6つのナビゲーションボタン（KifuNavigationController に接続する）
    QPushButton* firstButton() const { return m_navButtons[NavFirst]; }
    QPushButton* back10Button() const { return m_navButtons[NavBack10]; }
    QPushButton* prevButton() const { return m_navButtons[NavPrev]; }
    QPushButton* nextButton() const { return m_navButtons[NavNext]; }
    QPushButton* fwd10Button() const { return m_navButtons[NavFwd10]; }
    QPushButton* lastButton() const { return m_navButtons[NavLast]; }

signals:
    void yAxisSettingsChanged(int limit, int interval);
    void xAxisSettingsChanged(int limit, int interval);
    void plyClicked(int ply);
    void analysisPlyClicked(int lineIndex, int ply);

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

public slots:
    void setFloating(bool floating);
    /// ナビゲーションボタンの有効/無効を切り替える（棋譜欄の矢印ボタンに合わせる）
    void setNavigationEnabled(bool on);

private slots:
    void applyYAxisSettings();
    void applyXAxisSettings();
    void applyFontSize();
    void onPlotAreaChanged();

private:
    enum NavButton { NavFirst, NavBack10, NavPrev, NavNext, NavFwd10, NavLast, NavButtonCount };
    /// 先手から見た評価値。描くときに表示範囲へ丸めても、元の値を持つ
    using Score = UsiScore;
    struct PendingScore {
        int side;
        int ply;
        UsiScore score;
    };
    void setupAxes();
    void setupChart();
    void setupSeries();
    void setupChartViewAndLayout();
    QWidget* createToolbar();
    QWidget* createNavigationBar(QWidget* parentWidget);
    void updateNavigationPlacement();
    void updateReferenceLines();
    void rebuildSeries();
    void refreshData();
    void showCurrentPly();
    void updatePresentation();
    void appendScore(int side, int ply, const UsiScore& blackScore);
    void bufferScore(int side, int ply, const UsiScore& blackScore);
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
    QGridLayout* m_toolbarLayout = nullptr;
    QWidget* m_rangeSelector = nullptr;
    QWidget* m_settingsButton = nullptr;
    QWidget* m_navBar = nullptr;
    QPushButton* m_navButtons[NavButtonCount] = {};
    bool m_navBarWrapped = false; // 幅が足りず、ナビゲーションボタンを2行目に置いている
    QString m_engineNames[2];
    QMap<int, Score> m_scores[2];
    int m_currentPly = 0;
    int m_maxVisitedPly = 0;
    bool m_scoresChanged = true;  // refreshData の後に点を足したか（足した後は表示範囲と点を作り直す）
    int m_hoverSide = -1;
    int m_hoverPly = -1;
    QList<PendingScore> m_pending;
    QTimer* m_flushTimer = nullptr;
};

#endif
