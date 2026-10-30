/// @file boardarrowactions.cpp
/// @brief 盤の矢印を出すかの切り替えの実装

#include "boardarrowactions.h"

#include "analysissettings.h"
#include "josekisettings.h"

#include <QAction>

namespace {
/// checkable な QAction を作る
QAction* createToggle(QObject* owner)
{
    auto* action = new QAction(owner);
    action->setCheckable(true);
    return action;
}
} // namespace

BoardArrowActions::BoardArrowActions(QAction* joseki, QObject* parent)
    : QObject(parent)
    , m_match(createToggle(this))
    , m_ponder(createToggle(this))
    , m_consideration(createToggle(this))
    , m_joseki(joseki ? joseki : createToggle(this))
{
    m_joseki->setCheckable(true);
    m_match->setChecked(AnalysisSettings::matchArrowsVisible());
    m_ponder->setChecked(AnalysisSettings::ponderArrowsVisible());
    m_consideration->setChecked(AnalysisSettings::considerationArrowsVisible());
    m_joseki->setChecked(JosekiSettings::boardArrowsVisible());
    for (QAction* action : {m_match, m_ponder, m_consideration, m_joseki})
        connect(action, &QAction::toggled, this, &BoardArrowActions::save);
}

void BoardArrowActions::save()
{
    AnalysisSettings::matchArrowsVisible.set(m_match->isChecked());
    AnalysisSettings::ponderArrowsVisible.set(m_ponder->isChecked());
    AnalysisSettings::considerationArrowsVisible.set(m_consideration->isChecked());
    JosekiSettings::boardArrowsVisible.set(m_joseki->isChecked());
}
