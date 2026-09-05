#include "piecestylecontroller.h"
#include "pieceimageprovider.h"

#include <QAction>
#include <QActionGroup>

PieceStyleController::PieceStyleController(const QList<StyleAction>& actions, QObject* parent)
    : QObject(parent), m_group(new QActionGroup(this))
{
    m_group->setExclusive(true);
    for (const auto& entry : actions) {
        QAction* action = entry.action;
        if (!action) continue;
        action->setData(entry.id);
        action->setCheckable(true);
        action->setIconVisibleInMenu(false);
        m_group->addAction(action);
    }
    connect(m_group, &QActionGroup::triggered, this, &PieceStyleController::selectStyle);
    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged,
            this, &PieceStyleController::updateCheckedAction);
    updateCheckedAction();
}

void PieceStyleController::selectStyle(QAction* action)
{
    PieceImageProvider::instance().setStyle(action->data().toString());
}

void PieceStyleController::updateCheckedAction()
{
    const QString style = PieceImageProvider::instance().style();
    const auto actions = m_group->actions();
    for (QAction* action : actions) {
        action->setChecked(action->data().toString() == style);
    }
}
