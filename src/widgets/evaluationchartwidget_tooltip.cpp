/// @file evaluationchartwidget_tooltip.cpp
/// @brief 評価値グラフウィジェット - ツールチップ・ホバー処理

#include "evaluationchartwidget.h"

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QLabel>
#include "logcategories.h"

// ============================================================
// ツールチップ
// ============================================================

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
