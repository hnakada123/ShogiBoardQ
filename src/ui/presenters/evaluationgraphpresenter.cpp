/// @file evaluationgraphpresenter.cpp
/// @brief 対局中の評価値と表示中の手数の評価値グラフへの反映の実装

#include "evaluationgraphpresenter.h"

#include "evaluationchartwidget.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "logcategories.h"

EvaluationGraphPresenter::EvaluationGraphPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_evalChart(deps.evalChart)
    , m_navState(deps.navState)
    , m_liveSession(deps.liveSession)
{
    Q_ASSERT(m_evalChart);
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
    m_engine1Name = name;
    if (m_evalChart) m_evalChart->setEngine1Name(name);
}

void EvaluationGraphPresenter::setEngine2Name(const QString& name)
{
    m_engine2Name = name;
    if (m_evalChart) m_evalChart->setEngine2Name(name);
}

void EvaluationGraphPresenter::clear()
{
    if (m_evalChart) m_evalChart->clearAll();
}

void EvaluationGraphPresenter::trimToPly(int maxPly)
{
    if (m_evalChart) m_evalChart->trimToPly(maxPly);
}

void EvaluationGraphPresenter::plotEngineScore(Turn side, int ply, const UsiScore& score)
{
    qCDebug(lcUi) << "plotEngineScore side=" << static_cast<int>(side) << "ply=" << ply;
    // USI エンジンは手番側から見た評価値を出すので、後手の値は反転して先手から見た値にする
    if (side == Turn::Black) {
        m_evalChart->setEngine1Name(m_engine1Name);
        m_evalChart->appendScoreP1(ply, score);
    } else {
        m_evalChart->setEngine2Name(m_engine2Name.isEmpty() ? m_engine1Name : m_engine2Name);
        m_evalChart->appendScoreP2(ply, score.negated());
    }
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
