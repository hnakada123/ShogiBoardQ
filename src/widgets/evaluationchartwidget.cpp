/// @file evaluationchartwidget.cpp
/// @brief 評価値グラフの構築・軸更新・棋譜ナビゲーション
#include "evaluationchartwidget.h"
#include "evaluationchartconfigurator.h"
#include "evaluationchartview.h"
#include "evaluationseries.h"
#include "buttonstyles.h"

#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <limits>
#include <QLabel>
#include <cmath>

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
    m_ownSeries = new EvaluationSeries(this);
    setSeries(m_ownSeries);
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
    if (event->type() == QEvent::FontChange && m_flushTimer) {
        applyFontSize();
        updateNavigationPlacement();
    }
}

void EvaluationChartWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateNavigationPlacement();
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
    layout->addWidget(createToolbar());
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

QWidget* EvaluationChartWidget::createToolbar()
{
    auto* toolbar = new QWidget(this);
    m_toolbarLayout = new QGridLayout(toolbar);
    m_toolbarLayout->setContentsMargins(12, 4, 12, 4);
    m_toolbarLayout->setHorizontalSpacing(8);
    m_toolbarLayout->setVerticalSpacing(4);
    // 左右の列を同じ幅にして、ナビゲーションボタンを中央に置く。
    m_toolbarLayout->setColumnStretch(0, 1);
    m_toolbarLayout->setColumnStretch(2, 1);
    m_rangeSelector = m_configurator->createRangeSelector(toolbar);
    m_settingsButton = m_configurator->createSettingsButton(toolbar);
    m_toolbarLayout->addWidget(m_rangeSelector, 0, 0, Qt::AlignLeft);
    m_toolbarLayout->addWidget(createNavigationBar(toolbar), 0, 1);
    m_toolbarLayout->addWidget(m_settingsButton, 0, 2, Qt::AlignRight);
    return toolbar;
}

QWidget* EvaluationChartWidget::createNavigationBar(QWidget* parentWidget)
{
    struct Spec {
        const char* objectName;
        QString text;
        QString toolTip;
    };
    // 横軸が手数なので ◀▶ で向きを表す。⏮⏭ は多くの日本語フォントに無いため、
    // 読み筋の盤面と同じく ◀▶ と縦棒を組み合わせて記号の大きさを揃える。
    const Spec specs[NavButtonCount] = {
        {"evalNavFirst", QStringLiteral("|◀"), tr("最初に戻る")},
        {"evalNavBack10", QStringLiteral("◀◀"), tr("10手戻る")},
        {"evalNavPrev", QStringLiteral("◀"), tr("1手戻る")},
        {"evalNavNext", QStringLiteral("▶"), tr("1手進む")},
        {"evalNavFwd10", QStringLiteral("▶▶"), tr("10手進む")},
        {"evalNavLast", QStringLiteral("▶|"), tr("最後に進む")},
    };
    m_navBar = new QWidget(parentWidget);
    m_navBar->setObjectName(QStringLiteral("evalNavigation"));
    auto* layout = new QHBoxLayout(m_navBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    const QString style = ButtonStyles::panelToolButton();
    for (int i = 0; i < NavButtonCount; ++i) {
        auto* button = new QPushButton(specs[i].text, m_navBar);
        button->setObjectName(QString::fromLatin1(specs[i].objectName));
        button->setToolTip(specs[i].toolTip);
        button->setAccessibleName(specs[i].toolTip);
        button->setStyleSheet(style);
        button->setFixedSize(36, 24);
        layout->addWidget(button);
        m_navButtons[i] = button;
    }
    return m_navBar;
}

void EvaluationChartWidget::updateNavigationPlacement()
{
    if (!m_toolbarLayout) return;
    // 表示範囲・表示設定と同じ行に収まらない幅では、ボタンを2行目の中央に回す。
    const QMargins margins = m_toolbarLayout->contentsMargins();
    const int inlineWidth = margins.left() + margins.right() + 2 * m_toolbarLayout->horizontalSpacing()
        + m_rangeSelector->sizeHint().width() + m_navBar->sizeHint().width()
        + m_settingsButton->sizeHint().width();
    const bool wrapped = width() < inlineWidth;
    if (wrapped == m_navBarWrapped) return;
    m_navBarWrapped = wrapped;
    m_toolbarLayout->removeWidget(m_navBar);
    if (wrapped) m_toolbarLayout->addWidget(m_navBar, 1, 0, 1, 3, Qt::AlignHCenter);
    else m_toolbarLayout->addWidget(m_navBar, 0, 1);
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

// 棋譜の現在行が動くたびに呼ばれる（対局中は1手ごとに setRecordLength と続けて呼ばれる）。
// 点と横軸の長さが前の refreshData と同じなら、表示範囲と点は変わらないので、現在の手の線と表示だけを更新する
void EvaluationChartWidget::setCurrentPly(int ply)
{
    m_currentPly = qMax(0, ply);
    const int maxVisitedPly = qMax(m_maxVisitedPly, m_currentPly);
    if (!m_scoresChanged && maxVisitedPly == m_maxVisitedPly) {
        showCurrentPly();
        return;
    }
    m_maxVisitedPly = maxVisitedPly;
    refreshData();
}

void EvaluationChartWidget::setRecordLength(int plies)
{
    const int length = qMax(0, plies);
    if (!m_scoresChanged && length == m_maxVisitedPly) {
        showCurrentPly();
        return;
    }
    m_maxVisitedPly = length;
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
            if (m_evaluation->analysisLineIndex() >= 0) emit analysisPlyClicked(m_evaluation->analysisLineIndex(), ply);
            else emit plyClicked(ply);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EvaluationChartWidget::setFloating(bool floating) { Q_UNUSED(floating) }

// ============================================================
// 生の評価データ保持・詰みと範囲外表示・バッチ更新
// ============================================================

void EvaluationChartWidget::setSeries(const EvaluationSeries* series)
{
    if (!series) series = m_ownSeries;
    if (m_evaluation == series) return;
    if (m_evaluation) disconnect(m_evaluation, nullptr, this, nullptr);
    m_evaluation = series;
    connect(m_evaluation, &EvaluationSeries::scoreAdded, this, &EvaluationChartWidget::onScoreAdded);
    connect(m_evaluation, &EvaluationSeries::scoresTrimmed, this, &EvaluationChartWidget::onScoresTrimmed);
    connect(m_evaluation, &EvaluationSeries::labelsChanged, this, &EvaluationChartWidget::onLabelsChanged);
    onScoresTrimmed(-1);
}

const QMap<int, EvaluationChartWidget::Score>& EvaluationChartWidget::scores(int side) const
{
    return m_evaluation->scores(side);
}

void EvaluationChartWidget::onScoreAdded(int /*series*/, int ply)
{
    // 点は少し待ってからまとめて描く。対局中は表示中の手の移動（setCurrentPly）ですぐ描く
    m_scoresChanged = true;
    m_pendingMaxPly = qMax(m_pendingMaxPly, ply);
    if (!m_flushTimer->isActive()) m_flushTimer->start(100);
}

void EvaluationChartWidget::onScoresTrimmed(int maxPly)
{
    m_flushTimer->stop();
    m_pendingMaxPly = 0;
    if (maxPly < 0) {
        m_currentPly = 0;
        m_maxVisitedPly = 0;
    } else {
        m_currentPly = qMin(m_currentPly, maxPly);
        m_maxVisitedPly = maxPly;
    }
    m_scoresChanged = true;
    clearHover();
    refreshData();
}

void EvaluationChartWidget::onLabelsChanged()
{
    updatePresentation();
}

qreal EvaluationChartWidget::displayedValue(const Score& score) const
{
    // 詰みは勝ち側の端に描く（mate 0 は手番側が既に詰んでいる）
    if (score.isMate()) return score.mateSign() > 0 ? yAxisLimit() : -yAxisLimit();
    return qBound(-yAxisLimit(), score.value, yAxisLimit());
}

QString EvaluationChartWidget::scoreText(const Score& score) const
{
    if (score.isMate()) {
        const QString side = score.mateSign() > 0 ? tr("先手") : tr("後手");
        return score.mateCountKnown ? tr("%1勝ち・詰みまで%2手").arg(side).arg(qAbs(score.value))
                                    : tr("%1勝ち・詰み").arg(side);
    }
    const QString value = score.value > 0 ? QStringLiteral("+%1").arg(score.value) : QString::number(score.value);
    return qAbs(qint64(score.value)) > yAxisLimit() ? tr("%1（表示範囲外）").arg(value) : value;
}

QString EvaluationChartWidget::seriesName(int side) const
{
    const QString name = m_evaluation->name(side);
    if (!name.isEmpty()) return name;
    return side == 0 ? tr("先手エンジン") : tr("後手エンジン");
}

void EvaluationChartWidget::rebuildSeries()
{
    for (int side = 0; side < 2; ++side) {
        QList<QPointF> points;
        points.reserve(scores(side).size());
        for (auto it = scores(side).cbegin(); it != scores(side).cend(); ++it)
            points.append(QPointF(it.key(), displayedValue(it.value())));
        m_series[side]->replace(points);
    }
}

void EvaluationChartWidget::refreshData()
{
    clearHover();
    int maxCp = 0;
    int maxPly = m_maxVisitedPly;
    for (int side = 0; side < EvaluationSeries::kSeriesCount; ++side) {
        const QMap<int, Score>& points = scores(side);
        for (auto it = points.cbegin(); it != points.cend(); ++it) {
            maxPly = qMax(maxPly, it.key());
            if (!it->isMate()) maxCp = qMax(maxCp, qAbs(it->value));
        }
    }
    const int yLimitBefore = yAxisLimit();
    m_configurator->updateDataRange(maxCp, maxPly);
    // 対局中は棋譜欄の行追加で記録の長さだけが伸びて呼ばれる。点が変わっていなければ、
    // 点を収める縦の表示範囲が変わったときだけ系列を作り直す。
    if (m_scoresChanged || yAxisLimit() != yLimitBefore) rebuildSeries();
    m_scoresChanged = false;
    updateReferenceLines();
    updatePresentation();
}

// refreshData のうち、現在の手で変わる部分（点と表示範囲が変わらないときに使う）
void EvaluationChartWidget::showCurrentPly()
{
    clearHover();
    updateReferenceLines();
    updatePresentation();
}

void EvaluationChartWidget::updatePresentation()
{
    if (!m_chartView) return;
    QStringList values;
    m_chartView->markers.clear();
    for (int side = 0; side < 2; ++side) {
        m_chartView->legends[side] = scores(side).isEmpty() ? QString() : seriesName(side);
        const auto selected = scores(side).constFind(m_currentPly);
        if (selected != scores(side).cend()) values.append(tr("%1：%2").arg(seriesName(side), scoreText(*selected)));
        for (auto it = scores(side).cbegin(); it != scores(side).cend(); ++it) {
            const bool edge = it->isMate() || qAbs(qint64(it->value)) > yAxisLimit();
            const bool highlight = it.key() == m_currentPly || (m_hoverSide == side && m_hoverPly == it.key());
            if (edge || highlight || scores(side).size() <= 40)
                m_chartView->markers.append({QPointF(it.key(), displayedValue(*it)),
                    EvaluationChartView::seriesColor(side), edge, highlight, side == 1});
        }
    }
    m_chartView->summary = tr("%1手目  |  %2").arg(m_currentPly)
        .arg(values.isEmpty() ? tr("評価値なし") : values.join(QStringLiteral("   /   ")));
    const int analysisLine = m_evaluation->analysisLineIndex();
    if (analysisLine >= 0) {
        const QString lineName = analysisLine == 0 ? tr("本譜") : tr("分岐 %1").arg(analysisLine);
        m_chartView->summary.prepend(lineName + QStringLiteral("  |  "));
    }
    m_chartView->setAccessibleName(m_chartView->summary);
    m_chartView->currentPly = m_currentPly;
    m_chartView->xLimit = xAxisLimit();
    m_chartView->yLimit = yAxisLimit();
    m_chartView->viewport()->update();
}

void EvaluationChartWidget::flushPendingScores()
{
    m_flushTimer->stop();
    // 足された点の手数まで横軸を伸ばす。閲覧・解析中の現在手数はナビゲーション側が管理し、過去の手数へ戻さない
    const int maxVisitedPly = qMax(m_maxVisitedPly, m_pendingMaxPly);
    m_pendingMaxPly = 0;
    if (!m_scoresChanged && maxVisitedPly == m_maxVisitedPly) return;
    m_maxVisitedPly = maxVisitedPly;
    refreshData();
}

// ============================================================
// 近傍点のホバー・元の評価値を表示するツールチップ
// ============================================================

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
        for (auto it = scores(side).cbegin(); it != scores(side).cend(); ++it) {
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
    // マウスが動くたびに呼ばれる。同じ点の上にいる間は、吹き出しを動かすだけにする
    // （文字の設定し直しとグラフ全体の描き直しを省く）
    const bool samePoint = (sideFound == m_hoverSide && plyFound == m_hoverPly && m_tooltip->isVisible());
    m_hoverSide = sideFound;
    m_hoverPly = plyFound;
    if (!samePoint) {
        const Score score = scores(sideFound).value(plyFound);
        m_tooltip->setFont(m_chartView->font());
        m_tooltip->setText(tr("%1\n%2手目：%3\n先手視点の評価値")
                           .arg(seriesName(sideFound)).arg(plyFound).arg(scoreText(score)));
        m_tooltip->adjustSize();
    }
    const int x = qBound(4, position.x() + 12, qMax(4, m_chartView->width() - m_tooltip->width() - 4));
    const int y = qBound(4, position.y() - m_tooltip->height() - 12,
                        qMax(4, m_chartView->height() - m_tooltip->height() - 4));
    m_tooltip->move(x, y);
    if (samePoint) return;
    m_tooltip->show();
    m_tooltip->raise();
    updatePresentation();
}
