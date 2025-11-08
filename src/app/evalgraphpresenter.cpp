#include "matchcoordinator.h"
#include "usi.h"
#include <QList>

namespace EvalGraphPresenter {

// 主エンジンのスコアを m_scoreCp へ追記
void appendPrimaryScore(QList<int>& scoreCp, MatchCoordinator* match)
{
    Usi* eng = match ? match->primaryEngine() : nullptr;
    const int cp = eng ? eng->lastScoreCp() : 0;
    scoreCp.append(cp);
}

// 2ndエンジンのスコアを m_scoreCp へ追記
void appendSecondaryScore(QList<int>& scoreCp, MatchCoordinator* match)
{
    Usi* eng2 = match ? match->secondaryEngine() : nullptr;
    const int cp = eng2 ? eng2->lastScoreCp() : 0;
    scoreCp.append(cp);
}

} // namespace EvalGraphPresenter
