#ifndef DOCKUICOORDINATOR_H
#define DOCKUICOORDINATOR_H

class QAction;
class QMenu;
class DockLayoutManager;

class DockUiCoordinator
{
public:
    static void wireLayoutMenu(DockLayoutManager* manager,
                               QAction* resetAction,
                               QAction* saveAction,
                               QAction* lockAction,
                               QMenu* savedLayoutsMenu);
};

#endif // DOCKUICOORDINATOR_H
