/// @file evaluationgraphpresenter.cpp
/// @brief 対局中の評価値と表示中の手数の評価値グラフへの反映の実装

#include "evaluationgraphpresenter.h"

#include "evaluationchartwidget.h"
#include "evaluationseries.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "logcategories.h"

EvaluationGraphPresenter::EvaluationGraphPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_evalChart(deps.evalChart)
    , m_navState(deps.navState)
    , m_liveSession(deps.liveSession)
    , m_series(new EvaluationSeries(this))
{
    Q_ASSERT(m_evalChart);
    m_evalChart->setSeries(m_series);
    if (m_navState) connect(m_navState, &KifuNavigationState::changed, this, &EvaluationGraphPresenter::showCurrentPly);
}

EvaluationGraphPresenter::~EvaluationGraphPresenter() = default;

int EvaluationGraphPresenter::recordLength() const
{
    if (!m_navState) return 0;
    const QList<BranchLine> lines = m_navState->tree()->allLines();
    const int index = m_navState->currentLineIndex();
    return (index >= 0 && index < lines.size()) ? static_cast<int>(lines.at(index).nodes.size()) - 1 : 0;
}

void EvaluationGraphPresenter::setEngine1Name(const QString& name)
{
    m_series->setName(0, name);
}

void EvaluationGraphPresenter::setEngine2Name(const QString& name)
{
    m_series->setName(1, name);
}

void EvaluationGraphPresenter::clear()
{
    m_series->clear();
}

void EvaluationGraphPresenter::trimToPly(int maxPly)
{
    m_series->trimToPly(maxPly);
}

void EvaluationGraphPresenter::plotEngineScore(Turn side, int ply, const UsiScore& score)
{
    qCDebug(lcUi) << "plotEngineScore side=" << static_cast<int>(side) << "ply=" << ply;
    // 先手のエンジンは1つ目、後手のエンジンは2つ目の系列。評価値は指した側から見た値
    m_series->setScore(side == Turn::Black ? 0 : 1, ply, side, score);
    m_evalChart->setCurrentPly(ply);
}

void EvaluationGraphPresenter::beginAnalysis(const QString& engineName, int lineIndex)
{
    m_series->clear();
    m_series->setName(0, engineName);
    m_series->setName(1, QString());
    m_series->setAnalysisLineIndex(lineIndex);
}

void EvaluationGraphPresenter::plotAnalysisScore(int ply, const UsiScore& blackScore)
{
    // 解析の点はまとめて描く（グラフが少し待ってから描く）。表示中の手は動かさない
    m_series->setScore(0, ply, Turn::Black, blackScore);
}

void EvaluationGraphPresenter::showCurrentPly()
{
    // 対局中の手は評価値の点を足すときに描くので、縦線は棋譜をたどったときだけ動かす
    if (!m_evalChart || !m_navState || (m_liveSession && m_liveSession->isActive())) return;
    const int ply = m_navState->currentPly();

    // 記録の長さには現在手数も含める。棋譜の長さだけを先に渡すと横軸がいったん縮み、
    // 直後の setCurrentPly で元に戻って目盛りを2回作り直す（手ごとに数十ミリ秒かかる）。
    m_evalChart->setRecordLength(qMax(recordLength(), ply));
    m_evalChart->setCurrentPly(ply);
}
