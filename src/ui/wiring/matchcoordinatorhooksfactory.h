#ifndef MATCHCOORDINATORHOOKSFACTORY_H
#define MATCHCOORDINATORHOOKSFACTORY_H

#include <functional>

#include "matchcoordinator.h"
#include "shogigamecontroller.h"

class EvaluationGraphController;
class Usi;

/**
 * @brief MatchCoordinator::Hooks / UndoHooks を一括生成するファクトリ
 *
 * MainWindowMatchWiringDepsService が構築した HookDeps/UndoDeps から
 * MC 用のコールバック群を組み立てる。
 *
 * @note コールバックチェーン: MainWindowWiringAssembler → HookDeps → buildHooks() → MC::Hooks
 * @see MainWindowMatchWiringDepsService, MatchCoordinatorWiring::buildDeps
 */
class MatchCoordinatorHooksFactory
{
public:
    /**
     * @brief MC::Hooks 生成用の入力コールバック群
     *
     * MainWindowWiringAssembler::buildMatchWiringDeps() で各コールバックが設定される。
     * 各メンバは MC::Hooks の同名メンバに対応する（一部は lambda でラップされる）。
     */
    struct HookDeps {
        EvaluationGraphController* evalGraphController = nullptr; ///< 評価値グラフ（appendEvalP1/P2 用）

        std::function<void(ShogiGameController::Player)> onTurnChanged;        ///< → MC::Hooks::updateTurnDisplay
        std::function<void(Usi*, const MatchCoordinator::GoTimes&)> sendGo;    ///< → MC::Hooks::sendGoToEngine
        std::function<void(Usi*)> sendStop;                                    ///< → MC::Hooks::sendStopToEngine
        std::function<void(Usi*, const QString&)> sendRaw;                     ///< → MC::Hooks::sendRawToEngine

        std::function<void(const QString&)> initializeNewGame;                 ///< → MC::Hooks::initializeNewGame
        std::function<void()> renderBoard;                                     ///< → MC::Hooks::renderBoardFromGc
        std::function<void(const QPoint&, const QPoint&)> showMoveHighlights;  ///< → MC::Hooks::showMoveHighlights
        std::function<void(const QString&, const QString&)> appendKifuLine;    ///< → MC::Hooks::appendKifuLine
        std::function<void(const QString&, const QString&)> showGameOverDialog; ///< → MC::Hooks::showGameOverDialog
        std::function<qint64(MatchCoordinator::Player)> remainingMsFor;        ///< → MC::Hooks::remainingMsFor
        std::function<qint64(MatchCoordinator::Player)> incrementMsFor;        ///< → MC::Hooks::incrementMsFor
        std::function<qint64()> byoyomiMs;                                     ///< → MC::Hooks::byoyomiMs

        std::function<void(const QString&, const QString&)> setPlayersNames;   ///< → MC::Hooks::setPlayersNames
        std::function<void(const QString&, const QString&)> setEngineNames;    ///< → MC::Hooks::setEngineNames
        std::function<void(const QString&, PlayMode,
                           const QString&, const QString&,
                           const QString&, const QString&)> autoSaveKifu;      ///< → MC::Hooks::autoSaveKifu
    };

    /**
     * @brief MC::UndoHooks 生成用の入力コールバック群
     *
     * MainWindowWiringAssembler::buildMatchWiringDeps() で設定される。
     */
    struct UndoDeps {
        std::function<void(int)> updateHighlightsForPly;                       ///< → UndoHooks::updateHighlightsForPly
        std::function<void()> updateTurnAndTimekeepingDisplay;                 ///< → UndoHooks::updateTurnAndTimekeepingDisplay
        std::function<bool(ShogiGameController::Player)> isHumanSide;          ///< → UndoHooks::isHumanSide
        std::function<void(bool)> setMouseClickMode;                           ///< → UndoHooks::setMouseClickMode
    };

    /// @brief HookDeps から MC::Hooks を生成する（appendEvalP1/P2 と updateTurnDisplay は lambda ラップ）
    static MatchCoordinator::Hooks buildHooks(const HookDeps& deps);
    /// @brief UndoDeps から MC::UndoHooks を生成する（直接コピー）
    static MatchCoordinator::UndoHooks buildUndoHooks(const UndoDeps& deps);
};

#endif // MATCHCOORDINATORHOOKSFACTORY_H
