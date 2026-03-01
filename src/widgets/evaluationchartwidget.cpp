/// @file evaluationchartwidget.cpp
/// @brief 評価値グラフウィジェットクラスの実装

#include "evaluationchartwidget.h"
#include "evaluationchartconfigurator.h"

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QBrush>
#include <QPen>
#include <QFont>
#include <QColor>
#include "logcategories.h"
#include <QtGlobal>
#include <QTimer>
#include <QLabel>
#include <QMouseEvent>

EvaluationChartWidget::EvaluationChartWidget(QWidget* parent)
    : QWidget(parent)
{
    // Configurator を生成して設定を読み込み
    m_configurator = new EvaluationChartConfigurator(this);
    m_configurator->loadSettings();

    // 軸
    m_axX = new QValueAxis(this);
    m_axY = new QValueAxis(this);

    QFont labelsFont("Noto Sans CJK JP", m_configurator->labelFontSize());

    // X軸設定
    m_axX->setRange(0, m_configurator->xAxisLimit());
    m_axX->setTickType(QValueAxis::TicksDynamic);
    m_axX->setTickInterval(m_configurator->xAxisInterval());
    m_axX->setLabelsFont(labelsFont);
    m_axX->setLabelFormat("%i");

    // Y軸設定
    m_axY->setRange(-m_configurator->yAxisLimit(), m_configurator->yAxisLimit());
    m_axY->setTickType(QValueAxis::TicksDynamic);
    m_axY->setTickInterval(m_configurator->yAxisInterval());
    m_axY->setLabelsFont(labelsFont);
    m_axY->setLabelFormat("%i");

    const QColor lightGray(192, 192, 192);
    m_axX->setLabelsColor(lightGray);
    m_axY->setLabelsColor(lightGray);

    // グリッド線の色を設定
    QPen gridPen(QColor(255, 255, 255, 80));
    gridPen.setWidth(1);
    m_axX->setGridLinePen(gridPen);
    m_axY->setGridLinePen(gridPen);

    // チャート
    m_chart = new QChart();
    m_chart->legend()->hide();
    m_chart->setBackgroundBrush(QBrush(QColor(0, 100, 0))); // ダークグリーン
    // 左マージンを広めに設定（Y軸ラベル用）
    m_chart->setMargins(QMargins(10, 5, 10, 5));

    m_chart->addAxis(m_axX, Qt::AlignBottom);
    m_chart->addAxis(m_axY, Qt::AlignLeft);

    // ゼロラインをセットアップ
    setupZeroLine();

    // カーソルライン（現在手数を示す縦線）をセットアップ
    setupCursorLine();

    // シリーズ
    m_s1 = new QLineSeries(this); // 先手側（黒線）
    m_s2 = new QLineSeries(this); // 後手側（白線）

    {
        QPen p = m_s1->pen();
        p.setWidth(2);
        m_s1->setPen(p);
        m_s1->setPointsVisible(true);
        m_s1->setColor(Qt::black);
    }
    {
        QPen p = m_s2->pen();
        p.setWidth(2);
        m_s2->setPen(p);
        m_s2->setPointsVisible(true);
        m_s2->setColor(Qt::white);
    }

    m_chart->addSeries(m_s1);
    m_chart->addSeries(m_s2);

    m_s1->attachAxis(m_axX);
    m_s1->attachAxis(m_axY);
    m_s2->attachAxis(m_axX);
    m_s2->attachAxis(m_axY);

    // ホバーシグナルの接続
    connect(m_s1, &QLineSeries::hovered, this, &EvaluationChartWidget::onSeriesHovered);
    connect(m_s2, &QLineSeries::hovered, this, &EvaluationChartWidget::onSeriesHovered);

    // ビュー
    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // ビューポートのクリックイベントをフィルタリング
    m_chartView->viewport()->installEventFilter(this);

    // ツールチップ（付箋）のセットアップ
    setupTooltip();

    // コントロールパネル（Configurator が生成）
    QWidget* controlPanel = m_configurator->createControlPanel(this);

    // Configurator の設定変更シグナルを接続
    connect(m_configurator, &EvaluationChartConfigurator::yAxisSettingsChanged,
            this, &EvaluationChartWidget::applyYAxisSettings);
    connect(m_configurator, &EvaluationChartConfigurator::xAxisSettingsChanged,
            this, &EvaluationChartWidget::applyXAxisSettings);
    connect(m_configurator, &EvaluationChartConfigurator::fontSizeChanged,
            this, &EvaluationChartWidget::applyFontSize);

    // レイアウト
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);
    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(m_chartView, 1);
    setLayout(mainLayout);

    setMinimumHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // 縦横とも伸縮可能

    initFlushTimer();
}

EvaluationChartWidget::~EvaluationChartWidget()
{
    if (m_flushTimer && m_flushTimer->isActive()) {
        m_flushTimer->stop();
    }
    m_configurator->saveSettings();
}

QWidget* EvaluationChartWidget::chartViewWidget() const
{
    return m_chartView;
}

// --- Configurator からの設定変更通知 ---

void EvaluationChartWidget::applyYAxisSettings()
{
    if (!m_axY) return;

    const int yLimit = m_configurator->yAxisLimit();
    const int yInterval = m_configurator->yAxisInterval();
    m_axY->setRange(-yLimit, yLimit);
    m_axY->setTickInterval(yInterval);

    // カーソルラインもY軸範囲に合わせて更新
    updateCursorLine();

    if (m_chartView) {
        m_chartView->update();
    }

    emit yAxisSettingsChanged(yLimit, yInterval);
}

void EvaluationChartWidget::applyXAxisSettings()
{
    if (!m_axX) return;

    const int xLimit = m_configurator->xAxisLimit();
    const int xInterval = m_configurator->xAxisInterval();
    m_axX->setRange(0, xLimit);
    m_axX->setTickInterval(xInterval);

    // ゼロラインも更新
    updateZeroLine();

    if (m_chartView) {
        m_chartView->update();
    }

    emit xAxisSettingsChanged(xLimit, xInterval);
}

void EvaluationChartWidget::applyFontSize()
{
    updateLabelFonts();
}

// --- 公開 getter / setter ---

int EvaluationChartWidget::yAxisLimit() const { return m_configurator->yAxisLimit(); }
int EvaluationChartWidget::yAxisInterval() const { return m_configurator->yAxisInterval(); }
int EvaluationChartWidget::xAxisLimit() const { return m_configurator->xAxisLimit(); }
int EvaluationChartWidget::xAxisInterval() const { return m_configurator->xAxisInterval(); }
int EvaluationChartWidget::labelFontSize() const { return m_configurator->labelFontSize(); }

void EvaluationChartWidget::setYAxisLimit(int limit)
{
    m_configurator->setYAxisLimit(limit);
    // Configurator の signal 経由で applyYAxisSettings が呼ばれる
}

void EvaluationChartWidget::setYAxisInterval(int interval)
{
    m_configurator->setYAxisInterval(interval);
}

void EvaluationChartWidget::setXAxisLimit(int limit)
{
    m_configurator->setXAxisLimit(limit);
}

void EvaluationChartWidget::setXAxisInterval(int interval)
{
    m_configurator->setXAxisInterval(interval);
}

void EvaluationChartWidget::setLabelFontSize(int size)
{
    m_configurator->setLabelFontSize(size);
}

// --- ゼロライン / カーソルライン ---

void EvaluationChartWidget::setupZeroLine()
{
    m_zeroLine = new QLineSeries(this);

    // ゼロラインのスタイル（太い黄色の線）
    QPen zeroPen(QColor(255, 255, 0, 200));  // 黄色、やや透明
    zeroPen.setWidth(2);
    m_zeroLine->setPen(zeroPen);
    m_zeroLine->setPointsVisible(false);

    // X軸の範囲全体にわたる水平線
    m_zeroLine->append(0, 0);
    m_zeroLine->append(m_configurator->xAxisLimit(), 0);

    m_chart->addSeries(m_zeroLine);
    m_zeroLine->attachAxis(m_axX);
    m_zeroLine->attachAxis(m_axY);
}

void EvaluationChartWidget::updateZeroLine()
{
    if (!m_zeroLine) return;

    m_zeroLine->clear();
    m_zeroLine->append(0, 0);
    m_zeroLine->append(m_configurator->xAxisLimit(), 0);
}

void EvaluationChartWidget::setupCursorLine()
{
    qCDebug(lcUi) << "setupCursorLine called: m_currentPly=" << m_currentPly
                   << "yLimit=" << m_configurator->yAxisLimit();

    m_cursorLine = new QLineSeries(this);

    // カーソルラインのスタイル（オレンジ色の縦線）
    QPen cursorPen(QColor(255, 165, 0, 220));  // オレンジ、やや透明
    cursorPen.setWidth(2);
    m_cursorLine->setPen(cursorPen);
    m_cursorLine->setPointsVisible(false);

    // 初期状態では手数0の位置に縦線を描画
    const int yLimit = m_configurator->yAxisLimit();
    m_cursorLine->append(m_currentPly, -yLimit);
    m_cursorLine->append(m_currentPly, yLimit);

    m_chart->addSeries(m_cursorLine);
    m_cursorLine->attachAxis(m_axX);
    m_cursorLine->attachAxis(m_axY);

    qCDebug(lcUi) << "setupCursorLine done";
}

void EvaluationChartWidget::updateCursorLine()
{
    if (!m_cursorLine) return;

    const int yLimit = m_configurator->yAxisLimit();
    m_cursorLine->clear();
    m_cursorLine->append(m_currentPly, -yLimit);
    m_cursorLine->append(m_currentPly, yLimit);
}

void EvaluationChartWidget::setCurrentPly(int ply)
{
    qCDebug(lcUi) << "setCurrentPly called: ply=" << ply << "m_currentPly(before)=" << m_currentPly;

    if (m_currentPly == ply) {
        qCDebug(lcUi) << "setCurrentPly: same value, skipping";
        return;
    }

    m_currentPly = ply;
    updateCursorLine();

    if (m_chartView) {
        m_chartView->update();
    }

    qCDebug(lcUi) << "setCurrentPly done: m_currentPly(after)=" << m_currentPly;
}

// --- フォントサイズ ---

void EvaluationChartWidget::updateLabelFonts()
{
    const int fontSize = m_configurator->labelFontSize();
    QFont labelsFont("Noto Sans CJK JP", fontSize);

    if (m_axX) {
        m_axX->setLabelsFont(labelsFont);
    }
    if (m_axY) {
        m_axY->setLabelsFont(labelsFont);
    }

    // フォントサイズに応じて左マージンを調整（Y軸ラベルが切れないように）
    if (m_chart) {
        // Y軸の数値幅（最大5桁: -20000）とフォントサイズに基づいてマージンを計算
        int leftMargin = 10 + (fontSize - 5) * 4;
        m_chart->setMargins(QMargins(leftMargin, 5, 10, 5));
    }

    if (m_chartView) {
        m_chartView->update();
    }
}

// --- データ操作 ---

void EvaluationChartWidget::appendScoreP1(int ply, int cp, bool invert)
{
    if (!m_s1) { qCWarning(lcUi) << "P1 series null"; return; }

    qCDebug(lcUi) << "P1 append ply=" << ply << "cp=" << cp << "invert=" << invert;

    appendScoreToSeries(m_s1, ply, cp, invert);

    // エンジン情報を更新
    m_engine1Ply = ply;
    m_engine1Cp = invert ? -cp : cp;

    // 対局中は最新のプロット位置に縦線を更新
    setCurrentPly(ply);

    if (m_chartView) {
        m_chartView->update();
    }
}

void EvaluationChartWidget::appendScoreP2(int ply, int cp, bool invert)
{
    if (!m_s2) { qCWarning(lcUi) << "P2 series null"; return; }

    qCDebug(lcUi) << "P2 append ply=" << ply << "cp=" << cp << "invert=" << invert;

    appendScoreToSeries(m_s2, ply, cp, invert);

    // エンジン情報を更新
    m_engine2Ply = ply;
    m_engine2Cp = invert ? -cp : cp;

    // 対局中は最新のプロット位置に縦線を更新
    setCurrentPly(ply);

    if (m_chartView) {
        m_chartView->update();
    }
}

void EvaluationChartWidget::clearAll()
{
    // ペンディングバッファをクリア
    m_pendingP1.clear();
    m_pendingP2.clear();
    if (m_flushTimer && m_flushTimer->isActive()) {
        m_flushTimer->stop();
    }

    if (m_s1) m_s1->clear();
    if (m_s2) m_s2->clear();

    // 縦線を初期位置（0手目）にリセット
    m_currentPly = 0;
    updateCursorLine();

    // 設定はリセットしない（ユーザーの好みを維持）
    applyXAxisSettings();
    applyYAxisSettings();

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

void EvaluationChartWidget::trimToPly(int maxPly)
{
    // maxPly以降の手数のデータポイントを削除（replace()で一括置換）
    auto trimSeries = [maxPly](QLineSeries* series) {
        if (!series) return;
        const QList<QPointF> points = series->points();
        QList<QPointF> kept;
        for (const QPointF& p : std::as_const(points)) {
            if (p.x() <= maxPly) {
                kept.append(p);
            }
        }
        series->replace(kept);
    };

    m_chartView->setUpdatesEnabled(false);
    trimSeries(m_s1);
    trimSeries(m_s2);
    m_chartView->setUpdatesEnabled(true);
}

int EvaluationChartWidget::countP1() const { return m_s1 ? m_s1->count() : 0; }
int EvaluationChartWidget::countP2() const { return m_s2 ? m_s2->count() : 0; }

// --- エンジン情報 ---

void EvaluationChartWidget::setEngine1Info(const QString& name, int ply, int cp)
{
    qCDebug(lcUi) << "setEngine1Info called: name=" << name << "ply=" << ply << "cp=" << cp;
    m_engine1Name = name;
    m_engine1Ply = ply;
    m_engine1Cp = cp;
}

void EvaluationChartWidget::setEngine2Info(const QString& name, int ply, int cp)
{
    qCDebug(lcUi) << "setEngine2Info called: name=" << name << "ply=" << ply << "cp=" << cp;
    m_engine2Name = name;
    m_engine2Ply = ply;
    m_engine2Cp = cp;
}

void EvaluationChartWidget::setEngine1Name(const QString& name)
{
    qCDebug(lcUi) << "setEngine1Name called: name=" << name;
    m_engine1Name = name;
}

void EvaluationChartWidget::setEngine2Name(const QString& name)
{
    qCDebug(lcUi) << "setEngine2Name called: name=" << name;
    m_engine2Name = name;
}

bool EvaluationChartWidget::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_chartView->viewport() && ev->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() == Qt::LeftButton) {
            const QPointF scenePos = m_chartView->mapToScene(me->pos());
            const QPointF dataPos = m_chart->mapToValue(scenePos, m_s1);
            const int ply = qMax(0, qRound(dataPos.x()));
            emit plyClicked(ply);
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void EvaluationChartWidget::setFloating(bool floating)
{
    Q_UNUSED(floating)
    // 現在は特別な処理なし（将来の拡張用に残す）
}

// --- バッチ更新 ---

void EvaluationChartWidget::initFlushTimer()
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout,
            this, &EvaluationChartWidget::flushPendingScores);
}

void EvaluationChartWidget::appendScoreToSeries(QLineSeries* series, int ply, int cp, bool invert)
{
    if (!series) return;

    const qreal y = invert ? -cp : cp;
    m_configurator->autoExpandYAxisIfNeeded(cp);
    m_configurator->autoExpandXAxisIfNeeded(ply);
    series->append(ply, y);
}

void EvaluationChartWidget::appendScoreP1Buffered(int ply, int cp, bool invert)
{
    m_pendingP1.append({ply, cp, invert});

    if (!m_flushTimer->isActive()) {
        m_flushTimer->start(100);
    }
}

void EvaluationChartWidget::appendScoreP2Buffered(int ply, int cp, bool invert)
{
    m_pendingP2.append({ply, cp, invert});

    if (!m_flushTimer->isActive()) {
        m_flushTimer->start(100);
    }
}

void EvaluationChartWidget::flushPendingScores()
{
    if (m_pendingP1.isEmpty() && m_pendingP2.isEmpty())
        return;

    m_chartView->setUpdatesEnabled(false);

    for (const auto& item : std::as_const(m_pendingP1)) {
        appendScoreToSeries(m_s1, item.ply, item.cp, item.invert);
    }

    for (const auto& item : std::as_const(m_pendingP2)) {
        appendScoreToSeries(m_s2, item.ply, item.cp, item.invert);
    }

    // カーソルラインを最後の手数に更新
    int lastPly = m_currentPly;
    if (!m_pendingP1.isEmpty()) {
        lastPly = qMax(lastPly, m_pendingP1.last().ply);
    }
    if (!m_pendingP2.isEmpty()) {
        lastPly = qMax(lastPly, m_pendingP2.last().ply);
    }
    m_currentPly = lastPly;
    updateCursorLine();

    m_pendingP1.clear();
    m_pendingP2.clear();

    m_chartView->setUpdatesEnabled(true);
}

// --- 一括置換 ---

void EvaluationChartWidget::replaceAllScoresP1(const QList<QPointF>& points)
{
    if (!m_s1) return;

    m_chartView->setUpdatesEnabled(false);
    m_s1->replace(points);
    m_chartView->setUpdatesEnabled(true);
}

void EvaluationChartWidget::replaceAllScoresP2(const QList<QPointF>& points)
{
    if (!m_s2) return;

    m_chartView->setUpdatesEnabled(false);
    m_s2->replace(points);
    m_chartView->setUpdatesEnabled(true);
}
