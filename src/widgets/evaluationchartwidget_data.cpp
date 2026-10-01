/// @file evaluationchartwidget_data.cpp
/// @brief 生の評価データ保持・詰みと範囲外表示・バッチ更新
#include "evaluationchartwidget.h"
#include "evaluationchartconfigurator.h"
#include "evaluationchartview.h"

#include <QLineSeries>
#include <QTimer>
#include <limits>

void EvaluationChartWidget::appendScore(int side, int ply, int cp, bool invert, const QString& mate)
{
    if (ply < 0 || (cp == std::numeric_limits<int>::min() && mate.isEmpty())) return;
    Score score;
    score.cp = cp == std::numeric_limits<int>::min() ? 0 : (invert ? -cp : cp);
    score.mate = mate;
    if (!mate.isEmpty()) {
        const bool winning = mate.startsWith(QLatin1Char('+')) || mate.toLongLong() > 0;
        score.cp = (winning != invert) ? 1 : -1;
    }
    m_scores[side].insert(ply, score);
}

void EvaluationChartWidget::appendScoreP1(int ply, int cp, bool invert, const QString& mate)
{
    appendScore(0, ply, cp, invert, mate);
    setCurrentPly(ply);
}

void EvaluationChartWidget::appendScoreP2(int ply, int cp, bool invert, const QString& mate)
{
    appendScore(1, ply, cp, invert, mate);
    setCurrentPly(ply);
}

qreal EvaluationChartWidget::displayedValue(const Score& score) const
{
    if (!score.mate.isEmpty()) {
        // mate 0 は既に詰んだ側。+/- は手数不明の詰み。
        const bool winning = score.cp > 0;
        return winning ? yAxisLimit() : -yAxisLimit();
    }
    return qBound(-yAxisLimit(), score.cp, yAxisLimit());
}

QString EvaluationChartWidget::scoreText(const Score& score) const
{
    if (!score.mate.isEmpty()) {
        const bool winning = score.cp > 0;
        const QString side = winning ? tr("先手") : tr("後手");
        bool numeric = false;
        const qlonglong distance = score.mate.toLongLong(&numeric);
        return numeric ? tr("%1勝ち・詰みまで%2手").arg(side).arg(qAbs(distance))
                       : tr("%1勝ち・詰み").arg(side);
    }
    const QString value = score.cp > 0 ? QStringLiteral("+%1").arg(score.cp) : QString::number(score.cp);
    return qAbs(qint64(score.cp)) > yAxisLimit() ? tr("%1（表示範囲外）").arg(value) : value;
}

QString EvaluationChartWidget::seriesName(int side) const
{
    if (!m_engineNames[side].isEmpty()) return m_engineNames[side];
    return side == 0 ? tr("先手エンジン") : tr("後手エンジン");
}

void EvaluationChartWidget::rebuildSeries()
{
    for (int side = 0; side < 2; ++side) {
        QList<QPointF> points;
        points.reserve(m_scores[side].size());
        for (auto it = m_scores[side].cbegin(); it != m_scores[side].cend(); ++it)
            points.append(QPointF(it.key(), displayedValue(it.value())));
        m_series[side]->replace(points);
    }
}

void EvaluationChartWidget::refreshData()
{
    clearHover();
    int maxCp = 0;
    int maxPly = m_maxVisitedPly;
    for (const auto& scores : m_scores) {
        for (auto it = scores.cbegin(); it != scores.cend(); ++it) {
            maxPly = qMax(maxPly, it.key());
            if (it->mate.isEmpty()) maxCp = qMax(maxCp, qAbs(it->cp));
        }
    }
    m_configurator->updateDataRange(maxCp, maxPly);
    rebuildSeries();
    updateReferenceLines();
    updatePresentation();
}

void EvaluationChartWidget::updatePresentation()
{
    if (!m_chartView) return;
    QStringList values;
    m_chartView->markers.clear();
    for (int side = 0; side < 2; ++side) {
        m_chartView->legends[side] = m_scores[side].isEmpty() ? QString() : seriesName(side);
        const auto selected = m_scores[side].constFind(m_currentPly);
        if (selected != m_scores[side].cend()) values.append(tr("%1：%2").arg(seriesName(side), scoreText(*selected)));
        for (auto it = m_scores[side].cbegin(); it != m_scores[side].cend(); ++it) {
            const bool edge = !it->mate.isEmpty() || qAbs(qint64(it->cp)) > yAxisLimit();
            const bool highlight = it.key() == m_currentPly || (m_hoverSide == side && m_hoverPly == it.key());
            if (edge || highlight || m_scores[side].size() <= 40)
                m_chartView->markers.append({QPointF(it.key(), displayedValue(*it)),
                    EvaluationChartView::seriesColor(side), edge, highlight, side == 1});
        }
    }
    m_chartView->summary = tr("%1手目  |  %2").arg(m_currentPly)
        .arg(values.isEmpty() ? tr("評価値なし") : values.join(QStringLiteral("   /   ")));
    m_chartView->setAccessibleName(m_chartView->summary);
    m_chartView->currentPly = m_currentPly;
    m_chartView->xLimit = xAxisLimit();
    m_chartView->yLimit = yAxisLimit();
    m_chartView->viewport()->update();
}

void EvaluationChartWidget::clearAll()
{
    m_pending.clear();
    m_flushTimer->stop();
    for (auto& scores : m_scores) scores.clear();
    m_currentPly = 0;
    m_maxVisitedPly = 0;
    clearHover();
    refreshData();
}

void EvaluationChartWidget::removeLast(int side)
{
    // 待った時にバッファ内の点が後から復活しないよう、先に反映する。
    flushPendingScores();
    auto& scores = m_scores[side];
    if (!scores.isEmpty()) scores.erase(std::prev(scores.end()));
    clearHover();
    refreshData();
}
void EvaluationChartWidget::removeLastP1() { removeLast(0); }
void EvaluationChartWidget::removeLastP2() { removeLast(1); }

void EvaluationChartWidget::trimToPly(int maxPly)
{
    m_pending.removeIf([maxPly](const PendingScore& point) { return point.ply > maxPly; });
    for (auto& scores : m_scores) {
        auto it = scores.upperBound(maxPly);
        while (it != scores.end()) it = scores.erase(it);
    }
    m_currentPly = qMin(m_currentPly, qMax(0, maxPly));
    m_maxVisitedPly = qMax(0, maxPly);
    clearHover();
    refreshData();
}

int EvaluationChartWidget::countP1() const { return static_cast<int>(m_scores[0].size()); }
int EvaluationChartWidget::countP2() const { return static_cast<int>(m_scores[1].size()); }
void EvaluationChartWidget::setEngine1Name(const QString& name) { m_engineNames[0] = name; updatePresentation(); }
void EvaluationChartWidget::setEngine2Name(const QString& name) { m_engineNames[1] = name; updatePresentation(); }

void EvaluationChartWidget::bufferScore(int side, int ply, int cp, bool invert, const QString& mate)
{
    m_pending.append({side, ply, cp, invert, mate});
    if (!m_flushTimer->isActive()) m_flushTimer->start(100);
}
void EvaluationChartWidget::appendScoreP1Buffered(int ply, int cp, bool invert, const QString& mate)
{
    bufferScore(0, ply, cp, invert, mate);
}
void EvaluationChartWidget::appendScoreP2Buffered(int ply, int cp, bool invert, const QString& mate)
{
    bufferScore(1, ply, cp, invert, mate);
}
void EvaluationChartWidget::flushPendingScores()
{
    if (m_pending.isEmpty()) return;
    for (const auto& point : std::as_const(m_pending)) {
        appendScore(point.side, point.ply, point.cp, point.invert, point.mate);
        m_currentPly = point.ply;
        m_maxVisitedPly = qMax(m_maxVisitedPly, point.ply);
    }
    m_pending.clear();
    refreshData();
}
