#ifndef EVALUATIONCHARTRANGE_H
#define EVALUATIONCHARTRANGE_H

/// @file evaluationchartrange.h
/// @brief 評価値グラフの軸の範囲と目盛りの計算

#include <QSizeF>

#include "analysissettings.h"

/// 評価値グラフの軸の範囲と目盛り
struct EvaluationChartAxes {
    int yLimit = 1000;     ///< 評価値の範囲（±）
    int yInterval = 500;
    int xLimit = 20;       ///< 手数の上限
    int xInterval = 5;

    friend bool operator==(const EvaluationChartAxes& a, const EvaluationChartAxes& b)
    {
        return a.yLimit == b.yLimit && a.yInterval == b.yInterval && a.xLimit == b.xLimit && a.xInterval == b.xInterval;
    }
    friend bool operator!=(const EvaluationChartAxes& a, const EvaluationChartAxes& b) { return !(a == b); }
};

namespace EvaluationChartRange {

/**
 * @brief 表示の設定・点の範囲・描く場所の大きさから軸を決める
 *
 * 自動なら点を収める切りのいい範囲（評価値は ±1000、手数は 20 以上）、固定なら設定の範囲。
 * 目盛りは文字（設定の大きさ）が重ならないように間引き、希望の間隔があればその倍数にする。
 * 評価値の目盛りは正負対称にする。
 */
EvaluationChartAxes compute(const EvaluationChartSettings& settings, int maxCp, int maxPly, const QSizeF& plotSize);

} // namespace EvaluationChartRange

#endif // EVALUATIONCHARTRANGE_H
