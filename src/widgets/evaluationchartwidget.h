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
#include "evaluationchartrange.h"
#include "usiscore.h"

class QComboBox;
class QPushButton;
class EvaluationSeries;
class StepButtons;

class EvaluationChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvaluationChartWidget(QWidget* parent = nullptr);
    ~EvaluationChartWidget() override;
    QWidget* chartViewWidget() const;

    /// 描く評価値の列（非所有。設定するまでは自分の空の列を描く）。グラフは列を描くだけで書き換えない
    void setSeries(const EvaluationSeries* series);
    const EvaluationSeries* series() const { return m_evaluation; }
    /// 足された点をすぐ描く（点は少し待ってからまとめて描く。棋譜解析で1局面ずつ届くとき）
    void flushPendingScores();

    /// 表示の設定（範囲・目盛り・文字の大きさ。変えると保存する）
    const EvaluationChartSettings& chartSettings() const { return m_settings; }
    void setChartSettings(const EvaluationChartSettings& settings);
    /// 今の軸（設定・点の範囲・描く場所の大きさから決めた範囲と目盛り）
    const EvaluationChartAxes& axes() const { return m_axes; }
    void setCurrentPly(int ply);
    void setRecordLength(int plies);
    int currentPly() const { return m_currentPly; }

    /// 点のクリックと矢印ボタンによる局面移動を切り替える（表示設定・評価値の更新は常に可能）
    void setNavigationEnabled(bool enabled);

    /// 棋譜欄と同じ6つの局面送りのボタン（KifuNavigationController::bindButtons で QAction に結び付ける）
    StepButtons* stepButtons() const { return m_navBar; }

signals:
    /// 「表示設定…」を押した（表示設定のダイアログは上の層が開き、setChartSettings で戻す）
    void settingsRequested();
    void plyClicked(int ply);
    /// 棋譜解析の点をクリックした（lineEndNodeId は解析した手順の末尾の手。EvaluationSeries::analysisLineEnd）
    void analysisPlyClicked(int lineEndNodeId, int ply);

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onRangeModeChanged(int index);
    void applyFontSize();
    void onPlotAreaChanged();
    void onScoreAdded(int series, int ply);
    void onScoresTrimmed(int maxPly);
    void onLabelsChanged();

private:
    using Score = UsiScore;
    int yAxisLimit() const { return m_axes.yLimit; }
    int xAxisLimit() const { return m_axes.xLimit; }
    /// 軸を決め直し、変わった軸を描き直す
    void updateAxes();
    void applyYAxis();
    void applyXAxis();
    void setupAxes();
    void setupChart();
    void setupSeries();
    void setupChartViewAndLayout();
    QWidget* createToolbar();
    void updateNavigationPlacement();
    void updateReferenceLines();
    void rebuildSeries();
    void refreshData();
    void showCurrentPly();
    void updatePresentation();
    qreal displayedValue(const Score& score) const;
    QString scoreText(const Score& score) const;
    QString seriesName(int side) const;
    const QMap<int, Score>& scores(int side) const;
    void setupTooltip();
    void hoverAt(const QPoint& position);
    void clearHover();

    QChart* m_chart = nullptr;
    QLineSeries* m_series[2] = {};
    QLineSeries* m_zeroLine = nullptr;
    QLineSeries* m_cursorLine = nullptr;
    QValueAxis* m_axX = nullptr;
    QValueAxis* m_axY = nullptr;
    EvaluationChartView* m_chartView = nullptr;
    QLabel* m_tooltip = nullptr;
    EvaluationChartSettings m_settings;   ///< 表示の設定（AnalysisSettings::evalChart に保存する）
    EvaluationChartAxes m_axes;
    int m_maxCp = 0;                      ///< 点の評価値の絶対値の最大（詰みを除く）
    int m_maxPly = 0;
    QSizeF m_plotSize{800, 240};          ///< 描く場所の大きさ（目盛りの間引きに使う）
    QGridLayout* m_toolbarLayout = nullptr;
    QWidget* m_rangeSelector = nullptr;
    QComboBox* m_rangeMode = nullptr;
    QPushButton* m_settingsButton = nullptr;
    StepButtons* m_navBar = nullptr;   ///< 局面送りのボタン（横）
    bool m_navigationEnabled = true;
    bool m_navBarWrapped = false; // 幅が足りず、ナビゲーションボタンを2行目に置いている
    int m_currentPly = 0;
    int m_maxVisitedPly = 0;
    bool m_scoresChanged = true;  // refreshData の後に点を足したか（足した後は表示範囲と点を作り直す）
    int m_hoverSide = -1;
    int m_hoverPly = -1;
    QTimer* m_flushTimer = nullptr;
    EvaluationSeries* m_ownSeries = nullptr;          ///< 列が設定されるまで描く空の列
    const EvaluationSeries* m_evaluation = nullptr;   ///< 描く評価値の列（非所有）
    int m_pendingMaxPly = 0;   ///< まだ描いていない点の最大の手数
};

#endif
