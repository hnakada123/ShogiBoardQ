/// @file evalgraphpresenter.cpp
/// @brief 評価値グラフプレゼンタクラスの実装

#include "evalgraphpresenter.h"
#include "matchcoordinator.h"
#include "usi.h"
#include <QList>
#include <QDebug>

namespace EvalGraphPresenter {

// 主エンジンのスコアを m_scoreCp へ追記
void appendPrimaryScore(QList<int>& scoreCp, MatchCoordinator* match)
{
    qDebug() << "[EvalGraphPresenter] appendPrimaryScore called, match=" << (match ? "valid" : "NULL");
    Usi* eng = match ? match->primaryEngine() : nullptr;
    qDebug() << "[EvalGraphPresenter] primaryEngine=" << (eng ? "valid" : "NULL");
    const int cp = eng ? eng->lastScoreCp() : 0;
    qDebug() << "[EvalGraphPresenter] primaryEngine lastScoreCp=" << cp;
    scoreCp.append(cp);
    qDebug() << "[EvalGraphPresenter] scoreCp size after append=" << scoreCp.size();
}

// 2ndエンジンのスコアを m_scoreCp へ追記
// HvEの場合、secondaryEngineはNULLなので、primaryEngineから取得する
void appendSecondaryScore(QList<int>& scoreCp, MatchCoordinator* match)
{
    qDebug() << "[EvalGraphPresenter] appendSecondaryScore called, match=" << (match ? "valid" : "NULL");

    // まず secondaryEngine を試す（EvE用）
    Usi* eng2 = match ? match->secondaryEngine() : nullptr;
    qDebug() << "[EvalGraphPresenter] secondaryEngine=" << (eng2 ? "valid" : "NULL");

    // secondaryEngine が NULL の場合は primaryEngine を使う（HvE用）
    if (!eng2) {
        Usi* eng1 = match ? match->primaryEngine() : nullptr;
        qDebug() << "[EvalGraphPresenter] fallback to primaryEngine=" << (eng1 ? "valid" : "NULL");
        const int cp = eng1 ? eng1->lastScoreCp() : 0;
        qDebug() << "[EvalGraphPresenter] primaryEngine lastScoreCp=" << cp;
        scoreCp.append(cp);
    } else {
        const int cp = eng2->lastScoreCp();
        qDebug() << "[EvalGraphPresenter] secondaryEngine lastScoreCp=" << cp;
        scoreCp.append(cp);
    }

    qDebug() << "[EvalGraphPresenter] scoreCp size after append=" << scoreCp.size();
}

} // namespace EvalGraphPresenter
