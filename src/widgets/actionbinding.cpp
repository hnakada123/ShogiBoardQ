/// @file actionbinding.cpp
/// @brief パネルのチェックと、正本の checkable な QAction の結び付けの実装

#include "actionbinding.h"

#include <QAbstractButton>
#include <QAction>

namespace ActionBinding {

void bindCheckable(QAbstractButton* button, QAction* action)
{
    Q_ASSERT(button && button->isCheckable());
    Q_ASSERT(action && action->isCheckable());
    button->setChecked(action->isChecked());
    QObject::connect(button, &QAbstractButton::toggled, action, &QAction::setChecked);
    QObject::connect(action, &QAction::toggled, button, &QAbstractButton::setChecked);
}

} // namespace ActionBinding
