#include "evaluationchartwidget.h"

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QBrush>
#include <QPen>
#include <QFont>
#include <QColor>
#include <QDebug>
#include <QtGlobal>
#include <QThread>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

// 利用可能な上限値のリスト（Y軸）
const QList<int> EvaluationChartWidget::s_availableYLimits = {
    500, 1000, 2000, 3000, 5000, 10000, 20000
};

// 利用可能な間隔値のリスト（Y軸）
const QList<int> EvaluationChartWidget::s_availableYIntervals = {
    100, 250, 500, 1000, 2000, 5000
};

// 利用可能な上限値のリスト（X軸：手数上限）
const QList<int> EvaluationChartWidget::s_availableXLimits = {
    500, 600, 700, 800, 900, 1000
};

// 利用可能な間隔値のリスト（X軸：手数間隔）
const QList<int> EvaluationChartWidget::s_availableXIntervals = {
    1, 5, 10, 20, 25, 50, 100
};

// 利用可能なフォントサイズのリスト
const QList<int> EvaluationChartWidget::s_availableFontSizes = {
    5, 6, 7, 8, 9, 10, 11, 12
};

EvaluationChartWidget::EvaluationChartWidget(QWidget* parent)
    : QWidget(parent)
{
    // 設定を読み込み
    loadSettings();

    // 軸
    m_axX = new QValueAxis(this);
    m_axY = new QValueAxis(this);

    QFont labelsFont("Noto Sans CJK JP", m_labelFontSize);

    // X軸設定
    m_axX->setRange(0, m_xLimit);
    m_axX->setTickType(QValueAxis::TicksDynamic);
    m_axX->setTickInterval(m_xInterval);
    m_axX->setLabelsFont(labelsFont);
    m_axX->setLabelFormat("%i");

    // Y軸設定
    m_axY->setRange(-m_yLimit, m_yLimit);
    m_axY->setTickType(QValueAxis::TicksDynamic);
    m_axY->setTickInterval(m_yInterval);
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

    // シリーズ
    m_s1 = new QLineSeries(this); // 先手側（白線）
    m_s2 = new QLineSeries(this); // 後手側（黒線）

    {
        QPen p = m_s1->pen();
        p.setWidth(2);
        m_s1->setPen(p);
        m_s1->setPointsVisible(true);
        m_s1->setColor(Qt::white);
    }
    {
        QPen p = m_s2->pen();
        p.setWidth(2);
        m_s2->setPen(p);
        m_s2->setPointsVisible(true);
        m_s2->setColor(Qt::black);
    }

    m_chart->addSeries(m_s1);
    m_chart->addSeries(m_s2);

    m_s1->attachAxis(m_axX);
    m_s1->attachAxis(m_axY);
    m_s2->attachAxis(m_axX);
    m_s2->attachAxis(m_axY);

    // ビュー
    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // コントロールパネルのセットアップ
    setupControlPanel();

    // レイアウト
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);
    mainLayout->addWidget(m_controlPanel);
    mainLayout->addWidget(m_chartView, 1);
    setLayout(mainLayout);

    setMinimumHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // 縦横とも伸縮可能
}

EvaluationChartWidget::~EvaluationChartWidget()
{
    saveSettings();
}

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
    m_zeroLine->append(m_xLimit, 0);

    m_chart->addSeries(m_zeroLine);
    m_zeroLine->attachAxis(m_axX);
    m_zeroLine->attachAxis(m_axY);
}

void EvaluationChartWidget::updateZeroLine()
{
    if (!m_zeroLine) return;

    m_zeroLine->clear();
    m_zeroLine->append(0, 0);
    m_zeroLine->append(m_xLimit, 0);
}

void EvaluationChartWidget::setupControlPanel()
{
    m_controlPanel = new QWidget(this);
    m_controlPanel->setFixedHeight(28);

    auto* layout = new QHBoxLayout(m_controlPanel);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    // ボタンスタイル（USI通信ログタブと同じサイズ）
    const QString btnStyle = QStringLiteral(
        "QPushButton { "
        "  background-color: #e0e0e0; "
        "  border: 1px solid #999; "
        "  border-radius: 2px; "
        "  font-size: 11px; "
        "  padding: 1px; "
        "} "
        "QPushButton:hover { background-color: #d0d0d0; } "
        "QPushButton:pressed { background-color: #c0c0c0; }"
    );

    // ラベルスタイル（GUIメニューと同じフォントサイズ）
    const QString labelStyle = QStringLiteral("QLabel { color: #333; font-size: 9pt; }");

    // 評価値上限ボタン
    auto* lblYLimit = new QLabel(QStringLiteral("評価値上限:"), m_controlPanel);
    lblYLimit->setStyleSheet(labelStyle);
    m_btnYLimitDown = new QPushButton(QStringLiteral("▼"), m_controlPanel);
    m_btnYLimitUp = new QPushButton(QStringLiteral("▲"), m_controlPanel);
    m_btnYLimitDown->setFixedSize(24, 24);
    m_btnYLimitUp->setFixedSize(24, 24);
    m_btnYLimitDown->setStyleSheet(btnStyle);
    m_btnYLimitUp->setStyleSheet(btnStyle);
    m_btnYLimitDown->setToolTip(tr("評価値上限を縮小"));
    m_btnYLimitUp->setToolTip(tr("評価値上限を拡大"));

    // 評価値間隔ボタン
    auto* lblYInterval = new QLabel(QStringLiteral("評価値間隔:"), m_controlPanel);
    lblYInterval->setStyleSheet(labelStyle);
    m_btnYIntervalDown = new QPushButton(QStringLiteral("－"), m_controlPanel);
    m_btnYIntervalUp = new QPushButton(QStringLiteral("＋"), m_controlPanel);
    m_btnYIntervalDown->setFixedSize(24, 24);
    m_btnYIntervalUp->setFixedSize(24, 24);
    m_btnYIntervalDown->setStyleSheet(btnStyle);
    m_btnYIntervalUp->setStyleSheet(btnStyle);
    m_btnYIntervalDown->setToolTip(tr("評価値目盛り間隔を縮小"));
    m_btnYIntervalUp->setToolTip(tr("評価値目盛り間隔を拡大"));

    // 手数上限ボタン
    auto* lblXLimit = new QLabel(QStringLiteral("手数上限:"), m_controlPanel);
    lblXLimit->setStyleSheet(labelStyle);
    m_btnXLimitDown = new QPushButton(QStringLiteral("▼"), m_controlPanel);
    m_btnXLimitUp = new QPushButton(QStringLiteral("▲"), m_controlPanel);
    m_btnXLimitDown->setFixedSize(24, 24);
    m_btnXLimitUp->setFixedSize(24, 24);
    m_btnXLimitDown->setStyleSheet(btnStyle);
    m_btnXLimitUp->setStyleSheet(btnStyle);
    m_btnXLimitDown->setToolTip(tr("手数上限を縮小"));
    m_btnXLimitUp->setToolTip(tr("手数上限を拡大"));

    // 手数間隔ボタン
    auto* lblXInterval = new QLabel(QStringLiteral("手数間隔:"), m_controlPanel);
    lblXInterval->setStyleSheet(labelStyle);
    m_btnXIntervalDown = new QPushButton(QStringLiteral("－"), m_controlPanel);
    m_btnXIntervalUp = new QPushButton(QStringLiteral("＋"), m_controlPanel);
    m_btnXIntervalDown->setFixedSize(24, 24);
    m_btnXIntervalUp->setFixedSize(24, 24);
    m_btnXIntervalDown->setStyleSheet(btnStyle);
    m_btnXIntervalUp->setStyleSheet(btnStyle);
    m_btnXIntervalDown->setToolTip(tr("手数目盛り間隔を縮小"));
    m_btnXIntervalUp->setToolTip(tr("手数目盛り間隔を拡大"));

    // フォントサイズボタン（USI通信ログと同じサイズ 28x24）
    m_btnFontDown = new QPushButton(QStringLiteral("A-"), m_controlPanel);
    m_btnFontUp = new QPushButton(QStringLiteral("A+"), m_controlPanel);
    m_btnFontDown->setFixedSize(28, 24);
    m_btnFontUp->setFixedSize(28, 24);
    m_btnFontDown->setStyleSheet(btnStyle);
    m_btnFontUp->setStyleSheet(btnStyle);
    m_btnFontDown->setToolTip(tr("目盛りの文字を小さく"));
    m_btnFontUp->setToolTip(tr("目盛りの文字を大きく"));

    // レイアウトに追加（左端から配置）
    layout->addWidget(lblYLimit);
    layout->addWidget(m_btnYLimitDown);
    layout->addWidget(m_btnYLimitUp);
    layout->addSpacing(8);

    layout->addWidget(lblYInterval);
    layout->addWidget(m_btnYIntervalDown);
    layout->addWidget(m_btnYIntervalUp);
    layout->addSpacing(8);

    layout->addWidget(lblXLimit);
    layout->addWidget(m_btnXLimitDown);
    layout->addWidget(m_btnXLimitUp);
    layout->addSpacing(8);

    layout->addWidget(lblXInterval);
    layout->addWidget(m_btnXIntervalDown);
    layout->addWidget(m_btnXIntervalUp);
    layout->addSpacing(8);

    layout->addWidget(m_btnFontDown);
    layout->addWidget(m_btnFontUp);

    layout->addStretch();  // 残りのスペースを右側に

    m_controlPanel->setLayout(layout);

    // シグナル接続（ラムダ不使用）
    connect(m_btnYLimitUp, &QPushButton::clicked,
            this, &EvaluationChartWidget::onIncreaseYLimitClicked);
    connect(m_btnYLimitDown, &QPushButton::clicked,
            this, &EvaluationChartWidget::onDecreaseYLimitClicked);
    connect(m_btnYIntervalUp, &QPushButton::clicked,
            this, &EvaluationChartWidget::onIncreaseYIntervalClicked);
    connect(m_btnYIntervalDown, &QPushButton::clicked,
            this, &EvaluationChartWidget::onDecreaseYIntervalClicked);

    connect(m_btnXLimitUp, &QPushButton::clicked,
            this, &EvaluationChartWidget::onIncreaseXLimitClicked);
    connect(m_btnXLimitDown, &QPushButton::clicked,
            this, &EvaluationChartWidget::onDecreaseXLimitClicked);
    connect(m_btnXIntervalUp, &QPushButton::clicked,
            this, &EvaluationChartWidget::onIncreaseXIntervalClicked);
    connect(m_btnXIntervalDown, &QPushButton::clicked,
            this, &EvaluationChartWidget::onDecreaseXIntervalClicked);

    connect(m_btnFontUp, &QPushButton::clicked,
            this, &EvaluationChartWidget::onIncreaseFontSizeClicked);
    connect(m_btnFontDown, &QPushButton::clicked,
            this, &EvaluationChartWidget::onDecreaseFontSizeClicked);
}

void EvaluationChartWidget::saveSettings()
{
    QSettings settings("ShogiBoardQ", "EvaluationChart");
    settings.setValue("yLimit", m_yLimit);
    settings.setValue("yInterval", m_yInterval);
    settings.setValue("xLimit", m_xLimit);
    settings.setValue("xInterval", m_xInterval);
    settings.setValue("labelFontSize", m_labelFontSize);
}

void EvaluationChartWidget::loadSettings()
{
    QSettings settings("ShogiBoardQ", "EvaluationChart");
    m_yLimit = settings.value("yLimit", 2000).toInt();
    m_yInterval = settings.value("yInterval", 1000).toInt();
    m_xLimit = settings.value("xLimit", 500).toInt();
    m_xInterval = settings.value("xInterval", 10).toInt();
    m_labelFontSize = settings.value("labelFontSize", 7).toInt();

    // 範囲チェック
    if (!s_availableYLimits.contains(m_yLimit)) m_yLimit = 2000;
    if (!s_availableYIntervals.contains(m_yInterval)) m_yInterval = 1000;
    if (!s_availableXLimits.contains(m_xLimit)) m_xLimit = 500;
    if (!s_availableXIntervals.contains(m_xInterval)) m_xInterval = 10;
    if (!s_availableFontSizes.contains(m_labelFontSize)) m_labelFontSize = 7;

    // 間隔が上限を超えないように調整
    if (m_yInterval > m_yLimit) m_yInterval = m_yLimit;
    if (m_xInterval > m_xLimit) m_xInterval = m_xLimit;
}

void EvaluationChartWidget::updateYAxis()
{
    if (!m_axY) return;

    m_axY->setRange(-m_yLimit, m_yLimit);
    m_axY->setTickInterval(m_yInterval);

    if (m_chartView) {
        m_chartView->update();
    }

    emit yAxisSettingsChanged(m_yLimit, m_yInterval);
}

void EvaluationChartWidget::updateXAxis()
{
    if (!m_axX) return;

    m_axX->setRange(0, m_xLimit);
    m_axX->setTickInterval(m_xInterval);

    // ゼロラインも更新
    updateZeroLine();

    if (m_chartView) {
        m_chartView->update();
    }

    emit xAxisSettingsChanged(m_xLimit, m_xInterval);
}

void EvaluationChartWidget::updateLabelFonts()
{
    QFont labelsFont("Noto Sans CJK JP", m_labelFontSize);

    if (m_axX) {
        m_axX->setLabelsFont(labelsFont);
    }
    if (m_axY) {
        m_axY->setLabelsFont(labelsFont);
    }

    // フォントサイズに応じて左マージンを調整（Y軸ラベルが切れないように）
    if (m_chart) {
        // Y軸の数値幅（最大5桁: -20000）とフォントサイズに基づいてマージンを計算
        int leftMargin = 10 + (m_labelFontSize - 5) * 4;
        m_chart->setMargins(QMargins(leftMargin, 5, 10, 5));
    }

    if (m_chartView) {
        m_chartView->update();
    }
}

void EvaluationChartWidget::setYAxisLimit(int limit)
{
    if (limit > 0 && limit != m_yLimit) {
        m_yLimit = limit;
        // 間隔が上限を超えないように調整
        if (m_yInterval > m_yLimit) {
            m_yInterval = m_yLimit;
        }
        updateYAxis();
    }
}

void EvaluationChartWidget::setYAxisInterval(int interval)
{
    if (interval > 0 && interval <= m_yLimit && interval != m_yInterval) {
        m_yInterval = interval;
        updateYAxis();
    }
}

void EvaluationChartWidget::setXAxisLimit(int limit)
{
    if (limit > 0 && limit != m_xLimit) {
        m_xLimit = limit;
        // 間隔が上限を超えないように調整
        if (m_xInterval > m_xLimit) {
            m_xInterval = m_xLimit;
        }
        updateXAxis();
    }
}

void EvaluationChartWidget::setXAxisInterval(int interval)
{
    if (interval > 0 && interval <= m_xLimit && interval != m_xInterval) {
        m_xInterval = interval;
        updateXAxis();
    }
}

void EvaluationChartWidget::setLabelFontSize(int size)
{
    if (size > 0 && size != m_labelFontSize) {
        m_labelFontSize = size;
        updateLabelFonts();
    }
}

void EvaluationChartWidget::onIncreaseYLimitClicked()
{
    for (int limit : s_availableYLimits) {
        if (limit > m_yLimit) {
            setYAxisLimit(limit);
            return;
        }
    }
}

void EvaluationChartWidget::onDecreaseYLimitClicked()
{
    int newLimit = s_availableYLimits.first();
    for (int limit : s_availableYLimits) {
        if (limit >= m_yLimit) {
            break;
        }
        newLimit = limit;
    }
    if (newLimit < m_yLimit) {
        setYAxisLimit(newLimit);
    }
}

void EvaluationChartWidget::onIncreaseYIntervalClicked()
{
    for (int interval : s_availableYIntervals) {
        if (interval > m_yInterval && interval <= m_yLimit) {
            setYAxisInterval(interval);
            return;
        }
    }
}

void EvaluationChartWidget::onDecreaseYIntervalClicked()
{
    int newInterval = s_availableYIntervals.first();
    for (int interval : s_availableYIntervals) {
        if (interval >= m_yInterval) {
            break;
        }
        newInterval = interval;
    }
    if (newInterval < m_yInterval) {
        setYAxisInterval(newInterval);
    }
}

void EvaluationChartWidget::onIncreaseXLimitClicked()
{
    for (int limit : s_availableXLimits) {
        if (limit > m_xLimit) {
            setXAxisLimit(limit);
            return;
        }
    }
}

void EvaluationChartWidget::onDecreaseXLimitClicked()
{
    int newLimit = s_availableXLimits.first();
    for (int limit : s_availableXLimits) {
        if (limit >= m_xLimit) {
            break;
        }
        newLimit = limit;
    }
    if (newLimit < m_xLimit) {
        setXAxisLimit(newLimit);
    }
}

void EvaluationChartWidget::onIncreaseXIntervalClicked()
{
    for (int interval : s_availableXIntervals) {
        if (interval > m_xInterval && interval <= m_xLimit) {
            setXAxisInterval(interval);
            return;
        }
    }
}

void EvaluationChartWidget::onDecreaseXIntervalClicked()
{
    int newInterval = s_availableXIntervals.first();
    for (int interval : s_availableXIntervals) {
        if (interval >= m_xInterval) {
            break;
        }
        newInterval = interval;
    }
    if (newInterval < m_xInterval) {
        setXAxisInterval(newInterval);
    }
}

void EvaluationChartWidget::onIncreaseFontSizeClicked()
{
    for (int size : s_availableFontSizes) {
        if (size > m_labelFontSize) {
            setLabelFontSize(size);
            return;
        }
    }
}

void EvaluationChartWidget::onDecreaseFontSizeClicked()
{
    int newSize = s_availableFontSizes.first();
    for (int size : s_availableFontSizes) {
        if (size >= m_labelFontSize) {
            break;
        }
        newSize = size;
    }
    if (newSize < m_labelFontSize) {
        setLabelFontSize(newSize);
    }
}

void EvaluationChartWidget::autoExpandYAxisIfNeeded(int cp)
{
    const int absCp = qAbs(cp);
    if (absCp > m_yLimit) {
        // 評価値が上限を超えた場合、自動で拡大
        for (int limit : s_availableYLimits) {
            if (limit >= absCp) {
                setYAxisLimit(limit);
                return;
            }
        }
        // リストにない場合は最大値を設定
        setYAxisLimit(s_availableYLimits.last());
    }
}

void EvaluationChartWidget::autoExpandXAxisIfNeeded(int ply)
{
    if (ply > m_xLimit) {
        // 手数が上限を超えた場合、自動で拡大
        for (int limit : s_availableXLimits) {
            if (limit >= ply) {
                setXAxisLimit(limit);
                return;
            }
        }
        // リストにない場合は最大値を設定
        setXAxisLimit(s_availableXLimits.last());
    }
}

void EvaluationChartWidget::appendScoreP1(int ply, int cp, bool invert)
{
    if (!m_s1) { qDebug() << "[CHART][P1][ERR] series null"; return; }

    const qreal y = invert ? -cp : cp;
    if (!qIsFinite(y)) qDebug() << "[CHART][P1][WARN] non-finite y" << y << "from cp=" << cp;

    const int before = m_s1->count();
    qDebug() << "[CHART][P1] append"
             << "ply=" << ply
             << "cp=" << cp
             << "invert=" << invert
             << "y=" << y
             << "beforeCount=" << before
             << "xRange=" << (m_axX ? QPointF(m_axX->min(), m_axX->max()) : QPointF(-1, -1))
             << "yRange=" << (m_axY ? QPointF(m_axY->min(), m_axY->max()) : QPointF(-1, -1))
             << "chartView=" << static_cast<const void*>(m_chartView)
             << "thread=" << QThread::currentThread();

    // 自動拡大チェック
    autoExpandYAxisIfNeeded(cp);
    autoExpandXAxisIfNeeded(ply);

    m_s1->append(ply, y);

    const int after = m_s1->count();
    QPointF lastPt;
    if (after > 0) lastPt = m_s1->at(after - 1);

    qDebug() << "[CHART][P1] appended"
             << "afterCount=" << after
             << "lastPoint=" << lastPt;

    if (m_chartView) {
        m_chartView->update();
        qDebug() << "[CHART][P1] chartView updated";
    } else {
        qDebug() << "[CHART][P1][WARN] chartView is null; not updated";
    }
}

void EvaluationChartWidget::appendScoreP2(int ply, int cp, bool invert)
{
    if (!m_s2) { qDebug() << "[CHART][P2][ERR] series null"; return; }

    const qreal y = invert ? -cp : cp;
    if (!qIsFinite(y)) qDebug() << "[CHART][P2][WARN] non-finite y" << y << "from cp=" << cp;

    const int before = m_s2->count();
    qDebug() << "[CHART][P2] append"
             << "ply=" << ply
             << "cp=" << cp
             << "invert=" << invert
             << "y=" << y
             << "beforeCount=" << before
             << "xRange=" << (m_axX ? QPointF(m_axX->min(), m_axX->max()) : QPointF(-1, -1))
             << "yRange=" << (m_axY ? QPointF(m_axY->min(), m_axY->max()) : QPointF(-1, -1))
             << "chartView=" << static_cast<const void*>(m_chartView)
             << "thread=" << QThread::currentThread();

    // 自動拡大チェック
    autoExpandYAxisIfNeeded(cp);
    autoExpandXAxisIfNeeded(ply);

    m_s2->append(ply, y);

    const int after = m_s2->count();
    QPointF lastPt;
    if (after > 0) lastPt = m_s2->at(after - 1);

    qDebug() << "[CHART][P2] appended"
             << "afterCount=" << after
             << "lastPoint=" << lastPt;

    if (m_chartView) {
        m_chartView->update();
        qDebug() << "[CHART][P2] chartView updated";
    } else {
        qDebug() << "[CHART][P2][WARN] chartView is null; not updated";
    }
}

void EvaluationChartWidget::clearAll()
{
    if (m_s1) m_s1->clear();
    if (m_s2) m_s2->clear();

    // 設定はリセットしない（ユーザーの好みを維持）
    updateXAxis();
    updateYAxis();

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

int EvaluationChartWidget::countP1() const { return m_s1 ? m_s1->count() : 0; }
int EvaluationChartWidget::countP2() const { return m_s2 ? m_s2->count() : 0; }
