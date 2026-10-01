/// @file shogiview_state.cpp
/// @brief ShogiView の状態切替・委譲メソッド実装

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "boardappearance.h"

void ShogiView::refreshBoardVisuals()
{
    m_boardVisuals = BoardAppearance::instance().visuals();
    update();
}

void ShogiView::refreshBoardColors()
{
    m_boardColors = BoardAppearance::instance().colors();
    m_highlighting->refreshBackgroundColors();
    update();
}

void ShogiView::setGameOverStyleLock(bool locked)
{
    m_gameOverStyleLock = locked;
}

void ShogiView::setActiveIsBlack(bool activeIsBlack)
{
    m_highlighting->setActiveIsBlack(activeIsBlack);
}

void ShogiView::setArrows(const QList<Arrow>& arrows)
{
    m_highlighting->setArrows(arrows);
}

const QList<ShogiView::Arrow>& ShogiView::arrows() const
{
    return m_highlighting->arrows();
}

void ShogiView::clearArrows()
{
    m_highlighting->clearArrows();
}
