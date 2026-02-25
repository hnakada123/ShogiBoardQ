/// @file mainwindowcompositionroot.cpp
/// @brief ensure* の生成ロジックを集約する CompositionRoot の実装

#include "mainwindowcompositionroot.h"
#include "dialogcoordinatorwiring.h"
#include "kifufilecontroller.h"
#include "recordnavigationwiring.h"

MainWindowCompositionRoot::MainWindowCompositionRoot(QObject* parent)
    : QObject(parent)
{
}

void MainWindowCompositionRoot::ensureDialogCoordinator(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::DialogCoordinatorCallbacks& callbacks,
    QObject* parent,
    DialogCoordinatorWiring*& wiring,
    DialogCoordinator*& coordinator)
{
    if (coordinator) return;

    if (!wiring) {
        wiring = new DialogCoordinatorWiring(parent);
    }

    auto deps = MainWindowDepsFactory::createDialogCoordinatorDeps(refs, callbacks);
    wiring->ensure(deps);
    coordinator = wiring->coordinator();
}

void MainWindowCompositionRoot::ensureKifuFileController(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::KifuFileCallbacks& callbacks,
    QObject* parent,
    KifuFileController*& controller)
{
    if (!controller) {
        controller = new KifuFileController(parent);
    }

    auto deps = MainWindowDepsFactory::createKifuFileControllerDeps(refs, callbacks);
    controller->updateDeps(deps);
}

void MainWindowCompositionRoot::ensureRecordNavigationWiring(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    RecordNavigationWiring*& wiring)
{
    if (!wiring) {
        wiring = new RecordNavigationWiring(parent);
    }

    auto deps = MainWindowDepsFactory::createRecordNavigationDeps(refs);
    wiring->ensure(deps);
}
