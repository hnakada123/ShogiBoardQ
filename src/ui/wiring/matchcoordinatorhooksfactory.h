#ifndef MATCHCOORDINATORHOOKSFACTORY_H
#define MATCHCOORDINATORHOOKSFACTORY_H

#include <functional>

#include "matchcoordinator.h"
#include "shogigamecontroller.h"

class EvaluationGraphController;
class Usi;

class MatchCoordinatorHooksFactory
{
public:
    struct HookDeps {
        EvaluationGraphController* evalGraphController = nullptr;

        std::function<void(ShogiGameController::Player)> onTurnChanged;
        std::function<void(Usi*, const MatchCoordinator::GoTimes&)> sendGo;
        std::function<void(Usi*)> sendStop;
        std::function<void(Usi*, const QString&)> sendRaw;

        std::function<void(const QString&)> initializeNewGame;
        std::function<void()> renderBoard;
        std::function<void(const QPoint&, const QPoint&)> showMoveHighlights;
        std::function<void(const QString&, const QString&)> appendKifuLine;
        std::function<void(const QString&, const QString&)> showGameOverDialog;
        std::function<qint64(MatchCoordinator::Player)> remainingMsFor;
        std::function<qint64(MatchCoordinator::Player)> incrementMsFor;
        std::function<qint64()> byoyomiMs;

        std::function<void(const QString&, const QString&)> setPlayersNames;
        std::function<void(const QString&, const QString&)> setEngineNames;
        std::function<void(const QString&, PlayMode,
                           const QString&, const QString&,
                           const QString&, const QString&)> autoSaveKifu;
    };

    struct UndoDeps {
        std::function<bool()> getMainRowGuard;
        std::function<void(bool)> setMainRowGuard;
        std::function<void(int)> updateHighlightsForPly;
        std::function<void()> updateTurnAndTimekeepingDisplay;
        std::function<bool(ShogiGameController::Player)> isHumanSide;
        std::function<bool()> isHvH;
    };

    static MatchCoordinator::Hooks buildHooks(const HookDeps& deps);
    static MatchCoordinator::UndoHooks buildUndoHooks(const UndoDeps& deps);
};

#endif // MATCHCOORDINATORHOOKSFACTORY_H
