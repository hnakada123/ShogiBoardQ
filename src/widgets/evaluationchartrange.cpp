/// @file evaluationchartrange.cpp
/// @brief 評価値グラフの軸の範囲と目盛りの計算の実装

#include "evaluationchartrange.h"

#include <QFont>
#include <QFontMetrics>
#include <QString>

#include <cmath>

namespace {
/// value 以上の 1・2・5×10^n の値
int niceCeiling(double value)
{
    const double magnitude = std::pow(10.0, std::floor(std::log10(qMax(1.0, value))));
    for (int step : {1, 2, 5, 10}) {
        const double candidate = step * magnitude;
        if (candidate >= value) return qMax(1, static_cast<int>(qMin(candidate, 100000000.0)));
    }
    return 1;
}

/// 間引いた最小の間隔 minimum を、希望の間隔 requested（0 は希望なし）の倍数にする
int interval(int minimum, int requested)
{
    if (requested == 0) return minimum;
    return requested * qMax(1, static_cast<int>(std::ceil(double(minimum) / requested)));
}
} // namespace

EvaluationChartAxes EvaluationChartRange::compute(const EvaluationChartSettings& settings, int maxCp, int maxPly,
                                                  const QSizeF& plotSize)
{
    EvaluationChartAxes axes;
    axes.yLimit = settings.automatic ? niceCeiling(qMax(1000, maxCp)) : settings.yLimit;
    axes.xLimit = settings.automatic ? qMax(20, ((maxPly + 9) / 10) * 10) : settings.xLimit;
    QFont labelFont;
    labelFont.setPointSize(settings.labelFontSize);
    const QFontMetrics fm(labelFont);
    const int xSlots = qMax(1, static_cast<int>(plotSize.width()
                                                / qMax(64, fm.horizontalAdvance(QString::number(axes.xLimit)) + 28)));
    // 正負対称の目盛りを保ち、文字サイズに応じて縦方向も間引く。
    const int halfYSlots = qBound(1, static_cast<int>(plotSize.height() / (fm.height() * 3.0)), 2);
    axes.yInterval = interval(qMax(1, axes.yLimit / halfYSlots), settings.yInterval);
    axes.xInterval = interval(niceCeiling(double(axes.xLimit) / xSlots), settings.xInterval);
    return axes;
}
