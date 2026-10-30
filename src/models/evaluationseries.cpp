/// @file evaluationseries.cpp
/// @brief 評価値グラフの点の実装

#include "evaluationseries.h"

#include <utility>

EvaluationSeries::EvaluationSeries(QObject* parent)
    : QObject(parent)
{
}

void EvaluationSeries::setScore(int series, int ply, Turn viewpoint, const UsiScore& score)
{
    if (ply < 0 || score.isNone()) return;
    m_scores[index(series)].insert(ply, score.blackView(viewpoint));
    emit scoreAdded(series == 1 ? 1 : 0, ply);
}

void EvaluationSeries::setName(int series, const QString& name)
{
    QString& current = m_names[index(series)];
    if (current == name) return;
    current = name;
    emit labelsChanged();
}

void EvaluationSeries::setAnalysisLine(int lineEndNodeId, const QString& name)
{
    if (m_analysisLineEnd == lineEndNodeId && m_analysisLineName == name) return;
    m_analysisLineEnd = lineEndNodeId;
    m_analysisLineName = name;
    emit labelsChanged();
}

void EvaluationSeries::clear()
{
    for (auto& scores : m_scores) scores.clear();
    m_analysisLineEnd = -1;
    m_analysisLineName.clear();
    emit scoresTrimmed(-1);
}

void EvaluationSeries::trimToPly(int maxPly)
{
    for (auto& scores : m_scores) scores.erase(std::as_const(scores).upperBound(maxPly), scores.cend());
    emit scoresTrimmed(qMax(0, maxPly));
}

