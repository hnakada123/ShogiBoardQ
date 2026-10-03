/// @file evaluationchartwidget.cpp
/// @brief 評価値グラフの構築・軸更新・棋譜ナビゲーション
#include "evaluationchartwidget.h"
#include "evaluationchartconfigurator.h"
#include "evaluationchartview.h"

#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QTimer>

EvaluationChartWidget::EvaluationChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_chart(new QChart())
    , m_axX(new QValueAxis())
    , m_axY(new QValueAxis())
    , m_chartView(new EvaluationChartView(m_chart, this))
    , m_configurator(new EvaluationChartConfigurator(this))
{
    m_configurator->loadSettings();
    setupAxes();
    setupChart();
    setupSeries();
    setupChartViewAndLayout();
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout, this, &EvaluationChartWidget::flushPendingScores);
    applyFontSize();
    refreshData();
}

EvaluationChartWidget::~EvaluationChartWidget()
{
    m_configurator->saveSettings();
}

void EvaluationChartWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange && m_flushTimer) applyFontSize();
}

void EvaluationChartWidget::setupAxes()
{
    m_axX->setObjectName(QStringLiteral("evalAxisX"));
    m_axY->setObjectName(QStringLiteral("evalAxisY"));
    for (auto* axis : {m_axX, m_axY}) {
        axis->setTickType(QValueAxis::TicksDynamic);
        axis->setTickAnchor(0);
        axis->setLabelFormat("%i");
        axis->setLabelsColor(QColor(QStringLiteral("#475569")));
        axis->setGridLinePen(QPen(QColor(QStringLiteral("#E2E8F0")), 1));
        axis->setLineVisible(false);
    }
    m_axX->setRange(0, xAxisLimit());
    m_axX->setTickInterval(xAxisInterval());
    m_axY->setRange(-yAxisLimit(), yAxisLimit());
    m_axY->setTickInterval(yAxisInterval());
}

void EvaluationChartWidget::setupChart()
{
    m_chart->legend()->hide();
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->setBackgroundBrush(QColor(QStringLiteral("#FAFBFC")));
    m_chart->setBackgroundRoundness(6);
    m_chart->setDropShadowEnabled(false);
    m_chart->addAxis(m_axX, Qt::AlignBottom);
    m_chart->addAxis(m_axY, Qt::AlignLeft);
}

void EvaluationChartWidget::setupSeries()
{
    m_zeroLine = new QLineSeries;
    m_zeroLine->setPen(QPen(QColor(QStringLiteral("#94A3B8")), 1));
    m_cursorLine = new QLineSeries;
    m_cursorLine->setPen(QPen(QColor(QStringLiteral("#64748B")), 1, Qt::DashLine));
    for (auto* line : {m_zeroLine, m_cursorLine}) {
        m_chart->addSeries(line);
        line->attachAxis(m_axX);
        line->attachAxis(m_axY);
    }
    for (int side = 0; side < 2; ++side) {
        auto* series = new QLineSeries;
        series->setObjectName(side == 0 ? QStringLiteral("evalSeries1") : QStringLiteral("evalSeries2"));
        series->setPen(QPen(EvaluationChartView::seriesColor(side), 2,
                            side == 0 ? Qt::SolidLine : Qt::DashLine));
        m_chart->addSeries(series);
        series->attachAxis(m_axX);
        series->attachAxis(m_axY);
        m_series[side] = series;
    }
    updateReferenceLines();
}

void EvaluationChartWidget::setupChartViewAndLayout()
{
    m_chartView->viewport()->installEventFilter(this);
    setupTooltip();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_configurator->createControlPanel(this));
    layout->addWidget(m_chartView, 1);
    setMinimumSize(320, 230);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_configurator, &EvaluationChartConfigurator::yAxisSettingsChanged,
            this, &EvaluationChartWidget::applyYAxisSettings);
    connect(m_configurator, &EvaluationChartConfigurator::xAxisSettingsChanged,
            this, &EvaluationChartWidget::applyXAxisSettings);
    connect(m_configurator, &EvaluationChartConfigurator::fontSizeChanged,
            this, &EvaluationChartWidget::applyFontSize);
    connect(m_chart, &QChart::plotAreaChanged, this, &EvaluationChartWidget::onPlotAreaChanged);
}

QWidget* EvaluationChartWidget::chartViewWidget() const { return m_chartView; }
int EvaluationChartWidget::yAxisLimit() const { return m_configurator->yAxisLimit(); }
int EvaluationChartWidget::yAxisInterval() const { return m_configurator->yAxisInterval(); }
int EvaluationChartWidget::xAxisLimit() const { return m_configurator->xAxisLimit(); }
int EvaluationChartWidget::xAxisInterval() const { return m_configurator->xAxisInterval(); }
int EvaluationChartWidget::labelFontSize() const { return m_configurator->labelFontSize(); }
void EvaluationChartWidget::setYAxisLimit(int limit) { m_configurator->setYAxisLimit(limit); }
void EvaluationChartWidget::setXAxisLimit(int limit) { m_configurator->setXAxisLimit(limit); }
void EvaluationChartWidget::setYAxisInterval(int interval) { m_configurator->setYAxisInterval(interval); }
void EvaluationChartWidget::setXAxisInterval(int interval) { m_configurator->setXAxisInterval(interval); }
void EvaluationChartWidget::setLabelFontSize(int size) { m_configurator->setLabelFontSize(size); }
void EvaluationChartWidget::setAutomaticRange(bool automatic) { m_configurator->setAutomaticRange(automatic); }

void EvaluationChartWidget::applyYAxisSettings()
{
    m_axY->setRange(-yAxisLimit(), yAxisLimit());
    m_axY->setTickInterval(yAxisInterval());
    rebuildSeries();
    updateReferenceLines();
    updatePresentation();
    clearHover();
    emit yAxisSettingsChanged(yAxisLimit(), yAxisInterval());
}

void EvaluationChartWidget::applyXAxisSettings()
{
    m_axX->setRange(0, xAxisLimit());
    m_axX->setTickInterval(xAxisInterval());
    updateReferenceLines();
    updatePresentation();
    clearHover();
    emit xAxisSettingsChanged(xAxisLimit(), xAxisInterval());
}

void EvaluationChartWidget::applyFontSize()
{
    QFont labels = font();
    labels.setPointSize(labelFontSize());
    m_axX->setLabelsFont(labels);
    m_axY->setLabelsFont(labels);
    m_chartView->setFont(labels);
    onPlotAreaChanged();
}

void EvaluationChartWidget::onPlotAreaChanged()
{
    // 横長表示では評価値と凡例を同じ行に置き、描画領域を広く取る。
    const QFontMetrics fm(m_chartView->font());
    const int hintWidth = qMax(fm.horizontalAdvance(tr("先手有利")), fm.horizontalAdvance(tr("後手有利")));
    const int headerRows = m_chartView->width() >= 700 ? 2 : 3;
    const QMargins margins(8, (fm.height() + 7) * headerRows + 8, hintWidth + 8, 8);
    if (m_chart->margins() != margins) m_chart->setMargins(margins);
    m_configurator->updatePlotSize(m_chart->plotArea().size());
    updatePresentation();
    clearHover();
}

void EvaluationChartWidget::updateReferenceLines()
{
    m_zeroLine->replace({QPointF(0, 0), QPointF(xAxisLimit(), 0)});
    m_cursorLine->replace({QPointF(m_currentPly, -yAxisLimit()), QPointF(m_currentPly, yAxisLimit())});
    m_cursorLine->setVisible(m_currentPly <= xAxisLimit());
}

void EvaluationChartWidget::setCurrentPly(int ply)
{
    m_currentPly = qMax(0, ply);
    m_maxVisitedPly = qMax(m_maxVisitedPly, m_currentPly);
    refreshData();
}

void EvaluationChartWidget::setAnalysisLineIndex(int lineIndex)
{
    m_analysisLineIndex = lineIndex;
    updatePresentation();
}

void EvaluationChartWidget::setRecordLength(int plies)
{
    m_maxVisitedPly = qMax(0, plies);
    refreshData();
}

bool EvaluationChartWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != m_chartView->viewport()) return QWidget::eventFilter(obj, event);
    if (event->type() == QEvent::MouseMove) {
        hoverAt(static_cast<QMouseEvent*>(event)->pos());
    } else if (event->type() == QEvent::Leave) {
        clearHover();
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPointF chartPos = m_chart->mapFromScene(m_chartView->mapToScene(mouse->pos()));
        if (mouse->button() == Qt::LeftButton && m_chart->plotArea().contains(chartPos)) {
            const int ply = qMax(0, qRound(m_chart->mapToValue(chartPos, m_series[0]).x()));
            if (m_analysisLineIndex >= 0) emit analysisPlyClicked(m_analysisLineIndex, ply);
            else emit plyClicked(ply);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EvaluationChartWidget::setFloating(bool floating) { Q_UNUSED(floating) }
