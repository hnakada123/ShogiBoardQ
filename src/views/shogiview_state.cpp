/// @file shogiview_state.cpp
/// @brief ShogiView の状態切替・委譲メソッド実装

#include "shogiview.h"
#include "shogiviewhighlighting.h"

void ShogiView::setGameOverStyleLock(bool locked)
{
    m_gameOverStyleLock = locked;
}

void ShogiView::setUiMuted(bool on)
{
    m_uiMuted = on;
}

void ShogiView::setActiveIsBlack(bool activeIsBlack)
{
    m_highlighting->setActiveIsBlack(activeIsBlack);
}

void ShogiView::setArrows(const QList<Arrow>& arrows)
{
    m_highlighting->setArrows(arrows);
}

void ShogiView::clearArrows()
{
    m_highlighting->clearArrows();
}
