/// @file evalgraphpresenter.cpp
/// @brief 評価値グラフプレゼンタクラスの実装

#include "evalgraphpresenter.h"
#include "matchcoordinator.h"
#include "usi.h"
#include <QList>
#include "logcategories.h"

namespace EvalGraphPresenter {

// 主エンジンのスコアを m_scoreCp へ追記
void appendPrimaryScore(QList<int>& scoreCp, MatchCoordinator* match)
{
    qCDebug(lcUi) << "appendPrimaryScore called, match=" << (match ? "valid" : "NULL");
    Usi* eng = match ? match->primaryEngine() : nullptr;
    qCDebug(lcUi) << "primaryEngine=" << (eng ? "valid" : "NULL");
    const int cp = eng ? eng->lastScoreCp() : 0;
    qCDebug(lcUi) << "primaryEngine lastScoreCp=" << cp;
    scoreCp.append(cp);
    qCDebug(lcUi) << "scoreCp size after append=" << scoreCp.size();
}

// 2ndエンジンのスコアを m_scoreCp へ追記
// HvEの場合、secondaryEngineはNULLなので、primaryEngineから取得する
void appendSecondaryScore(QList<int>& scoreCp, MatchCoordinator* match)
{
    qCDebug(lcUi) << "appendSecondaryScore called, match=" << (match ? "valid" : "NULL");

    // まず secondaryEngine を試す（EvE用）
    Usi* eng2 = match ? match->secondaryEngine() : nullptr;
    qCDebug(lcUi) << "secondaryEngine=" << (eng2 ? "valid" : "NULL");

    // secondaryEngine が NULL の場合は primaryEngine を使う（HvE用）
    if (!eng2) {
        Usi* eng1 = match ? match->primaryEngine() : nullptr;
        qCDebug(lcUi) << "fallback to primaryEngine=" << (eng1 ? "valid" : "NULL");
        const int cp = eng1 ? eng1->lastScoreCp() : 0;
        qCDebug(lcUi) << "primaryEngine lastScoreCp=" << cp;
        scoreCp.append(cp);
    } else {
        const int cp = eng2->lastScoreCp();
        qCDebug(lcUi) << "secondaryEngine lastScoreCp=" << cp;
        scoreCp.append(cp);
    }

    qCDebug(lcUi) << "scoreCp size after append=" << scoreCp.size();
}

} // namespace EvalGraphPresenter
