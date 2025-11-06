#include "turnsyncbridge.h"

#include "shogigamecontroller.h"
#include "turnmanager.h"

#include <QObject>

TurnSyncBridge::TurnSyncBridge(QObject* parent)
    : QObject(parent)
{}

void TurnSyncBridge::wire(ShogiGameController* gc, TurnManager* tm, QObject* uiReceiver)
{
    if (!gc || !tm || !uiReceiver) return;

    QObject::connect(gc, &ShogiGameController::currentPlayerChanged,
                     tm, &TurnManager::setFromGc,
                     Qt::UniqueConnection);

    // TurnManager::changed(ShogiGameController::Player) -> MainWindow::onTurnManagerChanged(...)
    QObject::connect(tm, SIGNAL(changed(ShogiGameController::Player)),
                     uiReceiver, SLOT(onTurnManagerChanged(ShogiGameController::Player)),
                     Qt::UniqueConnection);

    // 初期同期
    tm->setFromGc(gc->currentPlayer());
}
