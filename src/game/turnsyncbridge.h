#ifndef TURNSYNCBRIDGE_H
#define TURNSYNCBRIDGE_H

/// @file turnsyncbridge.h
/// @brief GC・TurnManager・UI間の手番同期配線を集約するブリッジ

#include <QObject>

#include "shogigamecontroller.h"
#include "turnmanager.h"

/**
 * @brief GC ↔ TurnManager ↔ UI(MainWindow) の手番同期配線を一箇所に集約する
 */
class TurnSyncBridge : public QObject
{
    Q_OBJECT
public:
    explicit TurnSyncBridge(QObject* parent = nullptr);

    /**
     * @brief GC→TurnManager→UI の配線を接続し、初期同期を行う
     *
     * 重複接続は Qt::UniqueConnection により抑止。
     * @tparam Receiver uiReceiver の具象型（ShogiGameController::Player を受けるスロットを持つこと）
     */
    template<typename Receiver>
    static void wire(ShogiGameController* gc, TurnManager* tm, Receiver* uiReceiver,
                     void (Receiver::*slot)(ShogiGameController::Player))
    {
        if (!gc || !tm || !uiReceiver) return;

        QObject::connect(gc, &ShogiGameController::currentPlayerChanged,
                         tm, &TurnManager::setFromGc,
                         Qt::UniqueConnection);

        QObject::connect(tm, &TurnManager::changed,
                         uiReceiver, slot,
                         Qt::UniqueConnection);

        // 初期同期
        tm->setFromGc(gc->currentPlayer());
    }
};

#endif // TURNSYNCBRIDGE_H
