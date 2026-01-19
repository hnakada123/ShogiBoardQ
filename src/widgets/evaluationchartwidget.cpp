#include "evaluationchartwidget.h"
#include "settingsservice.h"

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

// 利用可能な上限値のリスト（Y軸：評価値上限）
// 100から1000まで100刻み、1000から30000まで1000刻み
// 31111は詰み評価値（やねうら王系エンジンの優等局面スコア）
const QList<int> EvaluationChartWidget::s_availableYLimits = {
    100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
    2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
    11000, 12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000, 20000,
    21000, 22000, 23000, 24000, 25000, 26000, 27000, 28000, 29000, 30000,
    31111, 32000
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

    // ツールチップ（付箋）のセットアップ
    setupTooltip();

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

void EvaluationChartWidget::setupCursorLine()
{
    qDebug() << "[CHART] setupCursorLine called: m_currentPly=" << m_currentPly << "m_yLimit=" << m_yLimit;
    
    m_cursorLine = new QLineSeries(this);

    // カーソルラインのスタイル（オレンジ色の縦線）
    QPen cursorPen(QColor(255, 165, 0, 220));  // オレンジ、やや透明
    cursorPen.setWidth(2);
    m_cursorLine->setPen(cursorPen);
    m_cursorLine->setPointsVisible(false);

    // 初期状態では手数0の位置に縦線を描画
    m_cursorLine->append(m_currentPly, -m_yLimit);
    m_cursorLine->append(m_currentPly, m_yLimit);

    m_chart->addSeries(m_cursorLine);
    m_cursorLine->attachAxis(m_axX);
    m_cursorLine->attachAxis(m_axY);
    
    qDebug() << "[CHART] setupCursorLine done";
}

void EvaluationChartWidget::updateCursorLine()
{
    if (!m_cursorLine) return;

    m_cursorLine->clear();
    m_cursorLine->append(m_currentPly, -m_yLimit);
    m_cursorLine->append(m_currentPly, m_yLimit);
}

void EvaluationChartWidget::setCurrentPly(int ply)
{
    qDebug() << "[CHART] setCurrentPly called: ply=" << ply << "m_currentPly(before)=" << m_currentPly;
    
    if (m_currentPly == ply) {
        qDebug() << "[CHART] setCurrentPly: same value, skipping";
        return;
    }

    m_currentPly = ply;
    updateCursorLine();

    if (m_chartView) {
        m_chartView->update();
    }
    
    qDebug() << "[CHART] setCurrentPly done: m_currentPly(after)=" << m_currentPly;
}

void EvaluationChartWidget::setupTooltip()
{
    m_tooltip = new QLabel(m_chartView);
    m_tooltip->setStyleSheet(
        "QLabel {"
        "  background-color: #FFFACD;"  // レモンシフォン（薄い黄色）
        "  border: 1px solid #DAA520;"  // ゴールデンロッド（枠線）
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  color: #333333;"
        "}"
    );
    m_tooltip->setAlignment(Qt::AlignCenter);
    m_tooltip->hide();
}

void EvaluationChartWidget::onSeriesHovered(const QPointF& point, bool state)
{
    if (!m_tooltip || !m_chartView || !m_chart) return;

    if (state) {
        // 送信元のシリーズを特定
        QLineSeries* series = qobject_cast<QLineSeries*>(sender());
        if (!series || series->count() == 0) {
            m_tooltip->hide();
            return;
        }
        
        QString engineName;
        QString sideMarker;  // ▲（先手）または △（後手）
        if (series == m_s1) {
            engineName = m_engine1Name;
            sideMarker = QStringLiteral("▲");
            qDebug() << "[CHART] onSeriesHovered: series=m_s1, m_engine1Name=" << m_engine1Name;
        } else if (series == m_s2) {
            engineName = m_engine2Name;
            sideMarker = QStringLiteral("△");
            qDebug() << "[CHART] onSeriesHovered: series=m_s2, m_engine2Name=" << m_engine2Name;
        }
        
        // 最も近いデータポイントを探す
        int closestIndex = -1;
        qreal minDist = std::numeric_limits<qreal>::max();
        for (int i = 0; i < series->count(); ++i) {
            const QPointF pt = series->at(i);
            const qreal dist = qAbs(pt.x() - point.x());
            if (dist < minDist) {
                minDist = dist;
                closestIndex = i;
            }
        }
        
        // プロット点から離れすぎている場合はツールチップを非表示
        // X軸方向で0.3手分（約30%）以上離れていたら表示しない
        const qreal threshold = 0.3;
        if (closestIndex < 0 || minDist > threshold) {
            m_tooltip->hide();
            return;
        }
        
        // 最も近いデータポイントの値を使用
        const QPointF closestPt = series->at(closestIndex);
        const int ply = qRound(closestPt.x());
        const int cp = qRound(closestPt.y());
        
        // ツールチップのテキストを設定
        // フォーマット: "▲エンジン名\nN手目: 評価値" または "△エンジン名\nN手目: 評価値"
        QString text;
        if (!engineName.isEmpty()) {
            text = tr("%1%2\nMove %3: %4").arg(sideMarker, engineName).arg(ply).arg(cp);
        } else {
            // エンジン名がない場合は「先手」「後手」を表示
            const QString sideName = (series == m_s1) ? tr("先手") : tr("後手");
            text = tr("%1%2\nMove %3: %4").arg(sideMarker, sideName).arg(ply).arg(cp);
        }
        qDebug() << "[CHART] onSeriesHovered: tooltip text=" << text;
        m_tooltip->setText(text);
        m_tooltip->adjustSize();
        
        // 最も近いデータポイントの位置にツールチップを表示
        const QPointF chartPos = m_chart->mapToPosition(closestPt);
        const QPoint viewPos = m_chartView->mapFromScene(chartPos);
        
        // ツールチップの位置を調整（プロットの少し上に表示）
        int tooltipX = viewPos.x() - m_tooltip->width() / 2;
        int tooltipY = viewPos.y() - m_tooltip->height() - 10;
        
        // 画面外にはみ出さないように調整
        if (tooltipX < 5) tooltipX = 5;
        if (tooltipX + m_tooltip->width() > m_chartView->width() - 5) {
            tooltipX = m_chartView->width() - m_tooltip->width() - 5;
        }
        if (tooltipY < 5) {
            // 上にはみ出す場合は下に表示
            tooltipY = viewPos.y() + 15;
        }
        
        m_tooltip->move(tooltipX, tooltipY);
        m_tooltip->show();
        m_tooltip->raise();
    } else {
        // ホバー終了：ツールチップを非表示
        m_tooltip->hide();
    }
}

void EvaluationChartWidget::setupControlPanel()
{
    m_controlPanel = new QWidget(this);
    m_controlPanel->setFixedHeight(26);  // コンパクトな高さに変更

    auto* layout = new QHBoxLayout(m_controlPanel);
    layout->setContentsMargins(4, 0, 4, 0);  // 上下のマージンを削除
    layout->setSpacing(4);

    // ラベルスタイル（少し大きめのフォント）
    const QString labelStyle = QStringLiteral("QLabel { color: #333; font-size: 10pt; }");

    // ComboBoxスタイル
    const QString comboStyle = QStringLiteral(
        "QComboBox { "
        "  background-color: #f0f0f0; "
        "  border: 1px solid #999; "
        "  border-radius: 2px; "
        "  padding: 2px 4px; "
        "  font-size: 10pt; "
        "  min-width: 65px; "
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
        "  font-size: 10pt; "
        "  padding: 1px; "
        "} "
        "QPushButton:hover { background-color: #d0d0d0; } "
        "QPushButton:pressed { background-color: #c0c0c0; }"
    );

    // 評価値上限ComboBox
    auto* lblYLimit = new QLabel(tr("評価値上限:"), m_controlPanel);
    lblYLimit->setStyleSheet(labelStyle);
    m_comboYLimit = new QComboBox(m_controlPanel);
    m_comboYLimit->setStyleSheet(comboStyle);
    m_comboYLimit->setToolTip(tr("評価値の表示上限を選択（目盛り間隔は自動設定）"));
    for (int val : s_availableYLimits) {
        m_comboYLimit->addItem(QString::number(val), val);
    }

    // 手数上限ComboBox
    auto* lblXLimit = new QLabel(tr("手数上限:"), m_controlPanel);
    lblXLimit->setStyleSheet(labelStyle);
    m_comboXLimit = new QComboBox(m_controlPanel);
    m_comboXLimit->setStyleSheet(comboStyle);
    m_comboXLimit->setToolTip(tr("手数の表示上限を選択"));
    for (int val : s_availableXLimits) {
        m_comboXLimit->addItem(QString::number(val), val);
    }

    // 手数間隔ComboBox
    auto* lblXInterval = new QLabel(tr("手数間隔:"), m_controlPanel);
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

    // レイアウトに追加（左端から配置）
    layout->addWidget(lblYLimit);
    layout->addWidget(m_comboYLimit);
    layout->addSpacing(8);

    layout->addWidget(lblXLimit);
    layout->addWidget(m_comboXLimit);
    layout->addSpacing(8);

    layout->addWidget(lblXInterval);
    layout->addWidget(m_comboXInterval);
    layout->addSpacing(8);

    layout->addWidget(m_btnFontDown);
    layout->addWidget(m_btnFontUp);

    layout->addStretch();  // 残りのスペースを右側に

    m_controlPanel->setLayout(layout);

    // 現在の値でComboBoxを初期化
    updateComboBoxSelections();

    // 評価値上限の選択肢を初期化
    updateYLimitComboItems();

    // シグナル接続（ラムダ不使用）
    connect(m_comboYLimit, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartWidget::onYLimitChanged);
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
    SettingsService::setEvalChartYLimit(m_yLimit);
    // yIntervalは保存しない（上限の半分に固定）
    SettingsService::setEvalChartXLimit(m_xLimit);
    SettingsService::setEvalChartXInterval(m_xInterval);
    SettingsService::setEvalChartLabelFontSize(m_labelFontSize);
}

void EvaluationChartWidget::loadSettings()
{
    m_yLimit = SettingsService::evalChartYLimit();
    m_xLimit = SettingsService::evalChartXLimit();
    m_xInterval = SettingsService::evalChartXInterval();
    m_labelFontSize = SettingsService::evalChartLabelFontSize();

    // 範囲チェック
    if (!s_availableYLimits.contains(m_yLimit)) m_yLimit = 2000;
    if (!s_availableXLimits.contains(m_xLimit)) m_xLimit = 500;
    if (!s_availableXIntervals.contains(m_xInterval)) m_xInterval = 10;
    if (!s_availableFontSizes.contains(m_labelFontSize)) m_labelFontSize = 7;

    // 評価値間隔は上限の半分に固定（目盛り数5個）
    m_yInterval = calculateAppropriateYInterval(m_yLimit);

    // 手数間隔が上限を超えないように調整
    if (m_xInterval > m_xLimit) m_xInterval = m_xLimit;
}

void EvaluationChartWidget::updateYAxis()
{
    if (!m_axY) return;

    m_axY->setRange(-m_yLimit, m_yLimit);
    m_axY->setTickInterval(m_yInterval);

    // カーソルラインもY軸範囲に合わせて更新
    updateCursorLine();

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
        
        // 評価値間隔は上限の半分に自動設定（目盛り数5個固定）
        m_yInterval = calculateAppropriateYInterval(m_yLimit);
        
        updateYAxis();
        // ComboBoxの選択を更新
        if (m_comboYLimit) {
            m_comboYLimit->blockSignals(true);
            qsizetype idx = s_availableYLimits.indexOf(m_yLimit);
            if (idx >= 0) m_comboYLimit->setCurrentIndex(static_cast<int>(idx));
            m_comboYLimit->blockSignals(false);
        }
    }
}

// 評価値上限に応じた適切な間隔を計算
// 目盛り数を常に5個に固定するため、評価値上限の半分を返す
// 目盛り: -yLimit, -yLimit/2, 0, yLimit/2, yLimit (5個)
int EvaluationChartWidget::calculateAppropriateYInterval(int yLimit) const
{
    // 評価値上限の半分を間隔とする（目盛り数5個固定）
    return yLimit / 2;
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
            qsizetype idx = s_availableXLimits.indexOf(m_xLimit);
            if (idx >= 0) m_comboXLimit->setCurrentIndex(static_cast<int>(idx));
            m_comboXLimit->blockSignals(false);
        }
        if (m_comboXInterval) {
            m_comboXInterval->blockSignals(true);
            qsizetype idx = s_availableXIntervals.indexOf(m_xInterval);
            if (idx >= 0) m_comboXInterval->setCurrentIndex(static_cast<int>(idx));
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
            qsizetype idx = s_availableXIntervals.indexOf(m_xInterval);
            if (idx >= 0) m_comboXInterval->setCurrentIndex(static_cast<int>(idx));
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
    if (index < 0 || index >= m_comboYLimit->count()) return;
    int newLimit = m_comboYLimit->itemData(index).toInt();
    if (newLimit != m_yLimit) {
        setYAxisLimit(newLimit);
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
        qsizetype idx = s_availableYLimits.indexOf(m_yLimit);
        if (idx >= 0) m_comboYLimit->setCurrentIndex(static_cast<int>(idx));
        m_comboYLimit->blockSignals(false);
    }

    if (m_comboXLimit) {
        m_comboXLimit->blockSignals(true);
        qsizetype idx = s_availableXLimits.indexOf(m_xLimit);
        if (idx >= 0) m_comboXLimit->setCurrentIndex(static_cast<int>(idx));
        m_comboXLimit->blockSignals(false);
    }

    if (m_comboXInterval) {
        m_comboXInterval->blockSignals(true);
        qsizetype idx = s_availableXIntervals.indexOf(m_xInterval);
        if (idx >= 0) m_comboXInterval->setCurrentIndex(static_cast<int>(idx));
        m_comboXInterval->blockSignals(false);
    }
}

void EvaluationChartWidget::updateYLimitComboItems()
{
    if (!m_comboYLimit) return;

    m_comboYLimit->blockSignals(true);
    m_comboYLimit->clear();

    // 全ての上限値を追加（制限なし）
    // 間隔は上限変更時に自動調整されるため、上限は自由に選択可能
    for (int limit : s_availableYLimits) {
        m_comboYLimit->addItem(QString::number(limit), limit);
    }

    // 現在の値を選択
    int currentIdx = -1;
    for (int i = 0; i < m_comboYLimit->count(); ++i) {
        if (m_comboYLimit->itemData(i).toInt() == m_yLimit) {
            currentIdx = i;
            break;
        }
    }
    if (currentIdx >= 0) {
        m_comboYLimit->setCurrentIndex(currentIdx);
    } else if (m_comboYLimit->count() > 0) {
        // 現在の値がリストにない場合は最初の項目を選択
        m_comboYLimit->setCurrentIndex(0);
    }

    m_comboYLimit->blockSignals(false);
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

    // 対局中は最新のプロット位置に縦線を更新
    setCurrentPly(ply);

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

    // 対局中は最新のプロット位置に縦線を更新
    setCurrentPly(ply);

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

    // 縦線を初期位置（0手目）にリセット
    m_currentPly = 0;
    updateCursorLine();

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
    // エンジン情報ラベルは廃止されたため、何もしない
}
