/// @file evaluationseries.cpp
/// @brief 評価値グラフの点の実装

#include "evaluationseries.h"

EvaluationSeries::EvaluationSeries(QObject* parent)
    : QObject(parent)
{
}

UsiScore EvaluationSeries::blackView(Turn viewpoint, const UsiScore& score)
{
    // USI エンジンは手番側（指した側）から見た評価値を出すので、後手から見た値は反転する
    return viewpoint == Turn::White ? score.negated() : score;
}

void EvaluationSeries::setScore(int series, int ply, Turn viewpoint, const UsiScore& score)
{
    if (ply < 0 || score.isNone()) return;
    m_scores[index(series)].insert(ply, blackView(viewpoint, score));
    emit scoreAdded(series == 1 ? 1 : 0, ply);
}

void EvaluationSeries::setName(int series, const QString& name)
{
    QString& current = m_names[index(series)];
    if (current == name) return;
    current = name;
    emit labelsChanged();
}

void EvaluationSeries::setAnalysisLineIndex(int lineIndex)
{
    if (m_analysisLineIndex == lineIndex) return;
    m_analysisLineIndex = lineIndex;
    emit labelsChanged();
}

void EvaluationSeries::clear()
{
    for (auto& scores : m_scores) scores.clear();
    m_analysisLineIndex = -1;
    emit scoresTrimmed(-1);
}

void EvaluationSeries::trimToPly(int maxPly)
{
    for (auto& scores : m_scores) {
        auto it = scores.upperBound(maxPly);
        while (it != scores.end()) it = scores.erase(it);
    }
    emit scoresTrimmed(qMax(0, maxPly));
}

