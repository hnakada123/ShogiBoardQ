/// @file evaluationgraphcontroller.cpp
/// @brief 評価値グラフコントローラクラスの実装

#include "evaluationgraphcontroller.h"

#include <QTimer>
#include <utility>

#include "enginelifecyclemanager.h"
#include "evaluationchartwidget.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "logcategories.h"
#include "usi.h"

namespace {
/// bestmove と同時に届いた info 行を読み終えるまで待つ時間
constexpr int kPlotDelayMs = 50;
} // namespace

EvaluationGraphController::EvaluationGraphController(QObject* parent)
    : QObject(parent)
{
}

EvaluationGraphController::~EvaluationGraphController() = default;

void EvaluationGraphController::setEvalChart(EvaluationChartWidget* chart)
{
    m_evalChart = chart;
}

void EvaluationGraphController::setEngines(EngineLifecycleManager* engines)
{
    m_engines = engines;
}

void EvaluationGraphController::setNavigation(const KifuBranchTree* tree, const KifuNavigationState* navState)
{
    m_branchTree = tree;
    m_navState = navState;
}

int EvaluationGraphController::recordLength() const
{
    if (!m_branchTree || !m_navState) return 0;
    const QList<BranchLine> lines = m_branchTree->allLines();
    const int index = m_navState->currentLineIndex();
    return (index >= 0 && index < lines.size()) ? static_cast<int>(lines.at(index).nodes.size()) - 1 : 0;
}

void EvaluationGraphController::setEngine1Name(const QString& name)
{
    m_engine1Name = name;
    if (m_evalChart) m_evalChart->setEngine1Name(name);
}

void EvaluationGraphController::setEngine2Name(const QString& name)
{
    m_engine2Name = name;
    if (m_evalChart) m_evalChart->setEngine2Name(name);
}

void EvaluationGraphController::clear()
{
    m_pendingBlackPly = -1;
    m_pendingWhitePly = -1;
    if (m_evalChart) m_evalChart->clearAll();
}

void EvaluationGraphController::trimToPly(int maxPly)
{
    if (m_evalChart) m_evalChart->trimToPly(maxPly);
}

void EvaluationGraphController::plotEngineScore(Turn side, int ply)
{
    qCDebug(lcUi) << "plotEngineScore side=" << static_cast<int>(side) << "ply=" << ply;
    if (side == Turn::Black) {
        m_pendingBlackPly = ply;
        QTimer::singleShot(kPlotDelayMs, this, &EvaluationGraphController::plotPendingBlackScore);
    } else {
        m_pendingWhitePly = ply;
        QTimer::singleShot(kPlotDelayMs, this, &EvaluationGraphController::plotPendingWhiteScore);
    }
}

void EvaluationGraphController::plotPendingBlackScore()
{
    const int ply = std::exchange(m_pendingBlackPly, -1);
    if (ply >= 0) plotScore(Turn::Black, ply);
}

void EvaluationGraphController::plotPendingWhiteScore()
{
    const int ply = std::exchange(m_pendingWhitePly, -1);
    if (ply >= 0) plotScore(Turn::White, ply);
}

void EvaluationGraphController::plotScore(Turn side, int ply)
{
    if (!m_evalChart) return;
    Usi* engine = m_engines ? m_engines->engineFor(side) : nullptr;
    // 遅らせている間に届いた info 行を読み終えて、最後の評価値を確定させる
    if (engine) engine->flushThinkingInfoBuffer();
    const int cp = engine ? engine->lastScoreCp() : 0;
    const QString mate = engine ? engine->lastScoreMate() : QString();

    if (side == Turn::Black) {
        m_evalChart->setEngine1Name(m_engine1Name);
        m_evalChart->appendScoreP1(ply, cp, false, mate);
    } else {
        // USI エンジンは手番側から見た評価値を出すので、後手の値は符号を反転して先手から見た値にする
        m_evalChart->setEngine2Name(m_engine2Name.isEmpty() ? m_engine1Name : m_engine2Name);
        m_evalChart->appendScoreP2(ply, cp, true, mate);
    }
}

void EvaluationGraphController::setCurrentPly(int ply)
{
    if (!m_evalChart) return;
    // 記録の長さには現在手数も含める。棋譜の長さだけを先に渡すと横軸がいったん縮み、
    // 直後の setCurrentPly で元に戻って目盛りを2回作り直す（手ごとに数十ミリ秒かかる）。
    m_evalChart->setRecordLength(qMax(recordLength(), ply));
    m_evalChart->setCurrentPly(ply);
}
