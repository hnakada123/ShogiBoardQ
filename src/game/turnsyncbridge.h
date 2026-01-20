#ifndef TURNSYNCBRIDGE_H
#define TURNSYNCBRIDGE_H

#include <QObject>

class ShogiGameController;
class TurnManager;

/**
 * GC ↔ TurnManager ↔ UI(MainWindow) の配線を一箇所に集約。
 */
class TurnSyncBridge : public QObject
{
    Q_OBJECT
public:
    explicit TurnSyncBridge(QObject* parent=nullptr);

    /**
     * 重複接続は Qt::UniqueConnection により抑止。
     * uiReceiver は onTurnManagerChanged(ShogiGameController::Player) スロットを持つ想定。
     */
    static void wire(ShogiGameController* gc, TurnManager* tm, QObject* uiReceiver);
};

#endif // TURNSYNCBRIDGE_H
