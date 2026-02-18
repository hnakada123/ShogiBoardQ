#include "dockuicoordinator.h"

#include "docklayoutmanager.h"

void DockUiCoordinator::wireLayoutMenu(DockLayoutManager* manager,
                                       QAction* resetAction,
                                       QAction* saveAction,
                                       QAction* lockAction,
                                       QMenu* savedLayoutsMenu)
{
    if (!manager) {
        return;
    }

    manager->wireMenuActions(
        resetAction,
        saveAction,
        /*clearStartupLayout*/ nullptr,
        lockAction,
        savedLayoutsMenu);
}
