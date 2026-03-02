/// @file evaluationchartwidget_data.cpp
/// @brief 評価値グラフウィジェットのデータ操作・バッチ更新の実装

#include "evaluationchartwidget.h"
#include "evaluationchartconfigurator.h"

#include <QtCharts/QLineSeries>
#include <QtCharts/QChartView>
#include <QTimer>
#include "logcategories.h"


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
