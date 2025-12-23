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
#include <QComboBox>
#include <QSettings>

// 利用可能な上限値のリスト（Y軸：評価値上限）
// 100から1000まで100刻み、1000から30000まで1000刻み
const QList<int> EvaluationChartWidget::s_availableYLimits = {
    100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
    2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
    11000, 12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000, 20000,
    21000, 22000, 23000, 24000, 25000, 26000, 27000, 28000, 29000, 30000
};

// 利用可能な間隔値のリスト（Y軸：評価値間隔）
const QList<int> EvaluationChartWidget::s_availableYIntervals = {
    100, 250, 500, 1000, 2000, 5000, 10000
};

// 利用可能な上限値のリスト（X軸：手数上限）
const QList<int> EvaluationChartWidget::s_availableXLimits = {
    500, 600, 700, 800, 900, 1000
};

// 利用可能な間隔値のリスト（X軸：手数間隔）
const QList<int> EvaluationChartWidget::s_availableXIntervals = {
    2, 5, 10, 20, 25, 50, 100
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
    m_controlPanel->setFixedHeight(32);

    auto* layout = new QHBoxLayout(m_controlPanel);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    // ラベルスタイル（GUIメニューと同じフォントサイズ）
    const QString labelStyle = QStringLiteral("QLabel { color: #333; font-size: 9pt; }");

    // ComboBoxスタイル
    const QString comboStyle = QStringLiteral(
        "QComboBox { "
        "  background-color: #f0f0f0; "
        "  border: 1px solid #999; "
        "  border-radius: 2px; "
        "  padding: 2px 4px; "
        "  font-size: 9pt; "
        "  min-width: 60px; "
        "} "
        "QComboBox:hover { background-color: #e8e8e8; } "
        "QComboBox::drop-down { "
        "  border: none; "
        "  width: 16px; "
        "} "
        "QComboBox::down-arrow { "
        "  image: none; "
        "  border-left: 4px solid transparent; "
        "  border-right: 4px solid transparent; "
        "  border-top: 5px solid #666; "
        "}"
    );

    // ボタンスタイル（フォントサイズ用）
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

    // 評価値上限ComboBox
    auto* lblYLimit = new QLabel(QStringLiteral("評価値上限:"), m_controlPanel);
    lblYLimit->setStyleSheet(labelStyle);
    m_comboYLimit = new QComboBox(m_controlPanel);
    m_comboYLimit->setStyleSheet(comboStyle);
    m_comboYLimit->setToolTip(tr("評価値の表示上限を選択"));
    for (int val : s_availableYLimits) {
        m_comboYLimit->addItem(QString::number(val), val);
    }

    // 評価値間隔ComboBox
    auto* lblYInterval = new QLabel(QStringLiteral("評価値間隔:"), m_controlPanel);
    lblYInterval->setStyleSheet(labelStyle);
    m_comboYInterval = new QComboBox(m_controlPanel);
    m_comboYInterval->setStyleSheet(comboStyle);
    m_comboYInterval->setToolTip(tr("評価値の目盛り間隔を選択"));
    for (int val : s_availableYIntervals) {
        m_comboYInterval->addItem(QString::number(val), val);
    }

    // 手数上限ComboBox
    auto* lblXLimit = new QLabel(QStringLiteral("手数上限:"), m_controlPanel);
    lblXLimit->setStyleSheet(labelStyle);
    m_comboXLimit = new QComboBox(m_controlPanel);
    m_comboXLimit->setStyleSheet(comboStyle);
    m_comboXLimit->setToolTip(tr("手数の表示上限を選択"));
    for (int val : s_availableXLimits) {
        m_comboXLimit->addItem(QString::number(val), val);
    }

    // 手数間隔ComboBox
    auto* lblXInterval = new QLabel(QStringLiteral("手数間隔:"), m_controlPanel);
    lblXInterval->setStyleSheet(labelStyle);
    m_comboXInterval = new QComboBox(m_controlPanel);
    m_comboXInterval->setStyleSheet(comboStyle);
    m_comboXInterval->setToolTip(tr("手数の目盛り間隔を選択"));
    for (int val : s_availableXIntervals) {
        m_comboXInterval->addItem(QString::number(val), val);
    }

    // フォントサイズボタン（USI通信ログと同じサイズ 28x24）
    m_btnFontDown = new QPushButton(QStringLiteral("A-"), m_controlPanel);
    m_btnFontUp = new QPushButton(QStringLiteral("A+"), m_controlPanel);
    m_btnFontDown->setFixedSize(28, 24);
    m_btnFontUp->setFixedSize(28, 24);
    m_btnFontDown->setStyleSheet(btnStyle);
    m_btnFontUp->setStyleSheet(btnStyle);
    m_btnFontDown->setToolTip(tr("目盛りの文字を小さく"));
    m_btnFontUp->setToolTip(tr("目盛りの文字を大きく"));

    // エンジン情報ラベル
    m_lblEngineInfo = new QLabel(m_controlPanel);
    m_lblEngineInfo->setStyleSheet(labelStyle);
    m_lblEngineInfo->setMinimumWidth(300);
    m_lblEngineInfo->setText(QStringLiteral("(エンジン情報)"));  // デバッグ用初期テキスト
    qDebug() << "[EVAL_CHART] m_lblEngineInfo created:" << m_lblEngineInfo;

    // レイアウトに追加（左端から配置）
    layout->addWidget(lblYLimit);
    layout->addWidget(m_comboYLimit);
    layout->addSpacing(8);

    layout->addWidget(lblYInterval);
    layout->addWidget(m_comboYInterval);
    layout->addSpacing(8);

    layout->addWidget(lblXLimit);
    layout->addWidget(m_comboXLimit);
    layout->addSpacing(8);

    layout->addWidget(lblXInterval);
    layout->addWidget(m_comboXInterval);
    layout->addSpacing(8);

    layout->addWidget(m_btnFontDown);
    layout->addWidget(m_btnFontUp);
    layout->addSpacing(16);

    layout->addWidget(m_lblEngineInfo);

    layout->addStretch();  // 残りのスペースを右側に

    m_controlPanel->setLayout(layout);

    // 現在の値でComboBoxを初期化
    updateComboBoxSelections();

    // シグナル接続（ラムダ不使用）
    connect(m_comboYLimit, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartWidget::onYLimitChanged);
    connect(m_comboYInterval, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartWidget::onYIntervalChanged);
    connect(m_comboXLimit, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartWidget::onXLimitChanged);
    connect(m_comboXInterval, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartWidget::onXIntervalChanged);

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
        
        // 評価値上限に応じて適切な間隔を自動設定
        int appropriateInterval = calculateAppropriateYInterval(m_yLimit);
        if (appropriateInterval != m_yInterval) {
            m_yInterval = appropriateInterval;
        }
        
        updateYAxis();
        // ComboBoxの選択を更新
        if (m_comboYLimit) {
            m_comboYLimit->blockSignals(true);
            int idx = s_availableYLimits.indexOf(m_yLimit);
            if (idx >= 0) m_comboYLimit->setCurrentIndex(idx);
            m_comboYLimit->blockSignals(false);
        }
        if (m_comboYInterval) {
            m_comboYInterval->blockSignals(true);
            int idx = s_availableYIntervals.indexOf(m_yInterval);
            if (idx >= 0) m_comboYInterval->setCurrentIndex(idx);
            m_comboYInterval->blockSignals(false);
        }
    }
}

// 評価値上限に応じた適切な間隔を計算
// 目盛り数が最大5個になるように間隔を選択
int EvaluationChartWidget::calculateAppropriateYInterval(int yLimit) const
{
    // 目盛り数 = (yLimit * 2) / interval + 1
    // 目盛り数を最大5個にしたい場合: interval >= yLimit * 2 / 4 = yLimit / 2
    // つまり interval >= yLimit / 2 であれば目盛りは5個以下
    
    // 利用可能な間隔の中から、目盛り数が5個以下になる最小の間隔を選択
    for (int interval : s_availableYIntervals) {
        int tickCount = (yLimit * 2) / interval + 1;
        if (tickCount <= 5) {
            return interval;
        }
    }
    
    // 見つからない場合は最大間隔を返す
    return s_availableYIntervals.last();
}

void EvaluationChartWidget::setYAxisInterval(int interval)
{
    if (interval > 0 && interval <= m_yLimit && interval != m_yInterval) {
        m_yInterval = interval;
        updateYAxis();
        // ComboBoxの選択を更新
        if (m_comboYInterval) {
            m_comboYInterval->blockSignals(true);
            int idx = s_availableYIntervals.indexOf(m_yInterval);
            if (idx >= 0) m_comboYInterval->setCurrentIndex(idx);
            m_comboYInterval->blockSignals(false);
        }
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
        // ComboBoxの選択を更新
        if (m_comboXLimit) {
            m_comboXLimit->blockSignals(true);
            int idx = s_availableXLimits.indexOf(m_xLimit);
            if (idx >= 0) m_comboXLimit->setCurrentIndex(idx);
            m_comboXLimit->blockSignals(false);
        }
        if (m_comboXInterval) {
            m_comboXInterval->blockSignals(true);
            int idx = s_availableXIntervals.indexOf(m_xInterval);
            if (idx >= 0) m_comboXInterval->setCurrentIndex(idx);
            m_comboXInterval->blockSignals(false);
        }
    }
}

void EvaluationChartWidget::setXAxisInterval(int interval)
{
    if (interval > 0 && interval <= m_xLimit && interval != m_xInterval) {
        m_xInterval = interval;
        updateXAxis();
        // ComboBoxの選択を更新
        if (m_comboXInterval) {
            m_comboXInterval->blockSignals(true);
            int idx = s_availableXIntervals.indexOf(m_xInterval);
            if (idx >= 0) m_comboXInterval->setCurrentIndex(idx);
            m_comboXInterval->blockSignals(false);
        }
    }
}

void EvaluationChartWidget::setLabelFontSize(int size)
{
    if (size > 0 && size != m_labelFontSize) {
        m_labelFontSize = size;
        updateLabelFonts();
    }
}

void EvaluationChartWidget::onYLimitChanged(int index)
{
    if (index < 0 || index >= s_availableYLimits.size()) return;
    int newLimit = s_availableYLimits.at(index);
    if (newLimit != m_yLimit) {
        setYAxisLimit(newLimit);
    }
}

void EvaluationChartWidget::onYIntervalChanged(int index)
{
    if (index < 0 || index >= s_availableYIntervals.size()) return;
    int newInterval = s_availableYIntervals.at(index);
    if (newInterval != m_yInterval && newInterval <= m_yLimit) {
        setYAxisInterval(newInterval);
    }
}

void EvaluationChartWidget::onXLimitChanged(int index)
{
    if (index < 0 || index >= s_availableXLimits.size()) return;
    int newLimit = s_availableXLimits.at(index);
    if (newLimit != m_xLimit) {
        setXAxisLimit(newLimit);
    }
}

void EvaluationChartWidget::onXIntervalChanged(int index)
{
    if (index < 0 || index >= s_availableXIntervals.size()) return;
    int newInterval = s_availableXIntervals.at(index);
    if (newInterval != m_xInterval && newInterval <= m_xLimit) {
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

void EvaluationChartWidget::updateComboBoxSelections()
{
    // シグナルを一時的にブロックして無限ループを防ぐ
    if (m_comboYLimit) {
        m_comboYLimit->blockSignals(true);
        int idx = s_availableYLimits.indexOf(m_yLimit);
        if (idx >= 0) m_comboYLimit->setCurrentIndex(idx);
        m_comboYLimit->blockSignals(false);
    }

    if (m_comboYInterval) {
        m_comboYInterval->blockSignals(true);
        int idx = s_availableYIntervals.indexOf(m_yInterval);
        if (idx >= 0) m_comboYInterval->setCurrentIndex(idx);
        m_comboYInterval->blockSignals(false);
    }

    if (m_comboXLimit) {
        m_comboXLimit->blockSignals(true);
        int idx = s_availableXLimits.indexOf(m_xLimit);
        if (idx >= 0) m_comboXLimit->setCurrentIndex(idx);
        m_comboXLimit->blockSignals(false);
    }

    if (m_comboXInterval) {
        m_comboXInterval->blockSignals(true);
        int idx = s_availableXIntervals.indexOf(m_xInterval);
        if (idx >= 0) m_comboXInterval->setCurrentIndex(idx);
        m_comboXInterval->blockSignals(false);
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

    // エンジン情報を更新
    m_engine1Ply = ply;
    m_engine1Cp = invert ? -cp : cp;
    qDebug() << "[CHART][P1] updating engine info: ply=" << m_engine1Ply << "cp=" << m_engine1Cp << "name=" << m_engine1Name;
    updateEngineInfoLabel();

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

    // エンジン情報を更新
    m_engine2Ply = ply;
    m_engine2Cp = invert ? -cp : cp;
    qDebug() << "[CHART][P2] updating engine info: ply=" << m_engine2Ply << "cp=" << m_engine2Cp << "name=" << m_engine2Name;
    updateEngineInfoLabel();

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

void EvaluationChartWidget::setEngine1Info(const QString& name, int ply, int cp)
{
    qDebug() << "[ENGINE_INFO] setEngine1Info called: name=" << name << "ply=" << ply << "cp=" << cp;
    m_engine1Name = name;
    m_engine1Ply = ply;
    m_engine1Cp = cp;
    updateEngineInfoLabel();
}

void EvaluationChartWidget::setEngine2Info(const QString& name, int ply, int cp)
{
    qDebug() << "[ENGINE_INFO] setEngine2Info called: name=" << name << "ply=" << ply << "cp=" << cp;
    m_engine2Name = name;
    m_engine2Ply = ply;
    m_engine2Cp = cp;
    updateEngineInfoLabel();
}

void EvaluationChartWidget::setEngine1Name(const QString& name)
{
    qDebug() << "[ENGINE_INFO] setEngine1Name called: name=" << name;
    m_engine1Name = name;
    updateEngineInfoLabel();
}

void EvaluationChartWidget::setEngine2Name(const QString& name)
{
    qDebug() << "[ENGINE_INFO] setEngine2Name called: name=" << name;
    m_engine2Name = name;
    updateEngineInfoLabel();
}

void EvaluationChartWidget::updateEngineInfoLabel()
{
    qDebug() << "[ENGINE_INFO] updateEngineInfoLabel called";
    qDebug() << "[ENGINE_INFO] m_lblEngineInfo:" << m_lblEngineInfo;
    qDebug() << "[ENGINE_INFO] m_engine1Name:" << m_engine1Name << "m_engine1Ply:" << m_engine1Ply << "m_engine1Cp:" << m_engine1Cp;
    qDebug() << "[ENGINE_INFO] m_engine2Name:" << m_engine2Name << "m_engine2Ply:" << m_engine2Ply << "m_engine2Cp:" << m_engine2Cp;

    if (!m_lblEngineInfo) {
        qDebug() << "[ENGINE_INFO] m_lblEngineInfo is NULL!";
        return;
    }

    QString text;

    // エンジン1の情報（先手・▲）
    if (!m_engine1Name.isEmpty() && m_engine1Ply > 0) {
        QString status;
        if (m_engine1Cp > 100) {
            status = QStringLiteral("有利");
        } else if (m_engine1Cp < -100) {
            status = QStringLiteral("不利");
        } else {
            status = QStringLiteral("互角");
        }

        QString cpStr;
        if (m_engine1Cp >= 0) {
            cpStr = QStringLiteral("+%1").arg(m_engine1Cp);
        } else {
            cpStr = QString::number(m_engine1Cp);
        }

        text += QStringLiteral("▲%1 %2 %3手目 評価値 %4")
                    .arg(m_engine1Name, status)
                    .arg(m_engine1Ply)
                    .arg(cpStr);
    }

    // エンジン2の情報（後手・▽）
    if (!m_engine2Name.isEmpty() && m_engine2Ply > 0) {
        if (!text.isEmpty()) {
            text += QStringLiteral("  ");  // 区切りスペース
        }

        QString status;
        // 後手の評価値は反転（後手から見た有利不利）
        if (m_engine2Cp < -100) {
            status = QStringLiteral("有利");
        } else if (m_engine2Cp > 100) {
            status = QStringLiteral("不利");
        } else {
            status = QStringLiteral("互角");
        }

        QString cpStr;
        if (m_engine2Cp >= 0) {
            cpStr = QStringLiteral("+%1").arg(m_engine2Cp);
        } else {
            cpStr = QString::number(m_engine2Cp);
        }

        text += QStringLiteral("▽%1 %2 %3手目 評価値 %4")
                    .arg(m_engine2Name, status)
                    .arg(m_engine2Ply)
                    .arg(cpStr);
    }

    qDebug() << "[ENGINE_INFO] final text:" << text;
    m_lblEngineInfo->setText(text);
}
