#include "analysisflowcontroller.h"
#include "analysisresulthandler.h"
#include <limits>

void AnalysisFlowController::publishCommittedEvaluation()
{
    if (m_resultHandler->lastCommittedPly() < 0) return;
    // 定跡など評価値のない結果は表には残し、グラフへの通知は出さない。
    if (m_resultHandler->lastCommittedScoreCp() != std::numeric_limits<int>::min()) {
        emit analysisProgressReported(m_resultHandler->lastCommittedPly(), m_resultHandler->lastCommittedScoreCp(),
                                      m_resultHandler->lastCommittedMate());
    }
    m_resultHandler->resetLastCommitted();
}

void AnalysisFlowController::onPositionPrepared(int ply, const QString& /*sfen*/)
{
    publishCommittedEvaluation();
    emit analysisPositionPrepared(ply);
}
