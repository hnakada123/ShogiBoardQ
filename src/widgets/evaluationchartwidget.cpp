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
#include <QThread>
#include <QLabel>

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
}

EvaluationChartWidget::~EvaluationChartWidget()
{
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

// --- ツールチップ ---

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
            qCDebug(lcUi) << "onSeriesHovered: series=m_s1, m_engine1Name=" << m_engine1Name;
        } else if (series == m_s2) {
            engineName = m_engine2Name;
            sideMarker = QStringLiteral("△");
            qCDebug(lcUi) << "onSeriesHovered: series=m_s2, m_engine2Name=" << m_engine2Name;
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
        qCDebug(lcUi) << "onSeriesHovered: tooltip text=" << text;
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

    const qreal y = invert ? -cp : cp;
    if (!qIsFinite(y)) qCWarning(lcUi) << "P1 non-finite y" << y << "from cp=" << cp;

    const int before = m_s1->count();
    qCDebug(lcUi) << "P1 append"
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
    m_configurator->autoExpandYAxisIfNeeded(cp);
    m_configurator->autoExpandXAxisIfNeeded(ply);

    m_s1->append(ply, y);

    // エンジン情報を更新
    m_engine1Ply = ply;
    m_engine1Cp = invert ? -cp : cp;
    qCDebug(lcUi) << "P1 updating engine info: ply=" << m_engine1Ply << "cp=" << m_engine1Cp << "name=" << m_engine1Name;

    // 対局中は最新のプロット位置に縦線を更新
    setCurrentPly(ply);

    const int after = m_s1->count();
    QPointF lastPt;
    if (after > 0) lastPt = m_s1->at(after - 1);

    qCDebug(lcUi) << "P1 appended"
             << "afterCount=" << after
             << "lastPoint=" << lastPt;

    if (m_chartView) {
        m_chartView->update();
        qCDebug(lcUi) << "P1 chartView updated";
    } else {
        qCWarning(lcUi) << "P1 chartView is null; not updated";
    }
}

void EvaluationChartWidget::appendScoreP2(int ply, int cp, bool invert)
{
    if (!m_s2) { qCWarning(lcUi) << "P2 series null"; return; }

    const qreal y = invert ? -cp : cp;
    if (!qIsFinite(y)) qCWarning(lcUi) << "P2 non-finite y" << y << "from cp=" << cp;

    const int before = m_s2->count();
    qCDebug(lcUi) << "P2 append"
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
    m_configurator->autoExpandYAxisIfNeeded(cp);
    m_configurator->autoExpandXAxisIfNeeded(ply);

    m_s2->append(ply, y);

    // エンジン情報を更新
    m_engine2Ply = ply;
    m_engine2Cp = invert ? -cp : cp;
    qCDebug(lcUi) << "P2 updating engine info: ply=" << m_engine2Ply << "cp=" << m_engine2Cp << "name=" << m_engine2Name;

    // 対局中は最新のプロット位置に縦線を更新
    setCurrentPly(ply);

    const int after = m_s2->count();
    QPointF lastPt;
    if (after > 0) lastPt = m_s2->at(after - 1);

    qCDebug(lcUi) << "P2 appended"
             << "afterCount=" << after
             << "lastPoint=" << lastPt;

    if (m_chartView) {
        m_chartView->update();
        qCDebug(lcUi) << "P2 chartView updated";
    } else {
        qCWarning(lcUi) << "P2 chartView is null; not updated";
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
    // maxPly以降の手数のデータポイントを削除
    auto trimSeries = [maxPly](QLineSeries* series) {
        if (!series) return;
        QList<QPointF> points = series->points();
        QList<QPointF> kept;
        for (const QPointF& p : std::as_const(points)) {
            if (p.x() <= maxPly) {
                kept.append(p);
            }
        }
        series->clear();
        series->append(kept);
    };

    trimSeries(m_s1);
    trimSeries(m_s2);

    if (m_chartView) m_chartView->update();
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

void EvaluationChartWidget::setFloating(bool floating)
{
    Q_UNUSED(floating)
    // 現在は特別な処理なし（将来の拡張用に残す）
}
