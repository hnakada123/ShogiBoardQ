/// @file scoretext.cpp
/// @brief 詰みの評価値の表記の実装

#include "scoretext.h"

QString ScoreText::mateIn(int plies)
{
    return tr("%1手詰").arg(plies);
}

QString ScoreText::mateUnknown()
{
    return tr("詰み");
}

QString ScoreText::mate(const UsiScore& score)
{
    if (!score.isMate()) return QString();
    return score.mateCountKnown ? mateIn(score.value) : mateUnknown();
}
