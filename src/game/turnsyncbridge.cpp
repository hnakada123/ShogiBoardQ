/// @file turnsyncbridge.cpp
/// @brief GC・TurnManager・UI間の手番同期配線の実装

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

    // TurnManager::changed → MainWindow::onTurnManagerChanged
    QObject::connect(tm, SIGNAL(changed(ShogiGameController::Player)),
                     uiReceiver, SLOT(onTurnManagerChanged(ShogiGameController::Player)),
                     Qt::UniqueConnection);

    // 初期同期
    tm->setFromGc(gc->currentPlayer());
}
