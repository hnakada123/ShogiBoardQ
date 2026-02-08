#ifndef TURNSYNCBRIDGE_H
#define TURNSYNCBRIDGE_H

/// @file turnsyncbridge.h
/// @brief GC・TurnManager・UI間の手番同期配線を集約するブリッジ


#include <QObject>

class ShogiGameController;
class TurnManager;

/**
 * @brief GC ↔ TurnManager ↔ UI(MainWindow) の手番同期配線を一箇所に集約する
 * @todo remove コメントスタイルガイド適用済み
 */
class TurnSyncBridge : public QObject
{
    Q_OBJECT
public:
    explicit TurnSyncBridge(QObject* parent=nullptr);

    /**
     * @brief GC→TurnManager→UI の配線を接続し、初期同期を行う
     *
     * 重複接続は Qt::UniqueConnection により抑止。
     * uiReceiver は onTurnManagerChanged(ShogiGameController::Player) スロットを持つ想定。
     */
    static void wire(ShogiGameController* gc, TurnManager* tm, QObject* uiReceiver);
};

#endif // TURNSYNCBRIDGE_H
