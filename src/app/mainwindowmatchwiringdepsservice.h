#ifndef MAINWINDOWMATCHWIRINGDEPSSERVICE_H
#define MAINWINDOWMATCHWIRINGDEPSSERVICE_H

/// @file mainwindowmatchwiringdepsservice.h
/// @brief MainWindow の MatchCoordinatorWiring::Deps 構築処理を分離したサービス

#include "matchcoordinatorwiring.h"

#include <functional>

class EvaluationGraphController;
class PlayerInfoWiring;
class KifuFileController;
class ShogiClock;
class TimeControlController;
class UiStatePolicyManager;

/**
 * @brief MatchCoordinatorWiring::Deps 構築サービス
 *
 * MainWindow の buildMatchWiringDeps() が保持していた
 * BuilderInputs 生成処理を分離する。
 */
class MainWindowMatchWiringDepsService
{
public:
    struct Inputs {
        // --- Hook 構築に使う依存 ---
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

        // --- Undo Hook 構築に使う依存 ---
        std::function<void(int)> updateHighlightsForPly;
        std::function<void()> updateTurnAndTimekeepingDisplay;
        std::function<bool(ShogiGameController::Player)> isHumanSide;

        // --- Deps に直接設定する値 ---
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;
        Usi* usi1 = nullptr;
        Usi* usi2 = nullptr;
        UsiCommLogModel* comm1 = nullptr;
        ShogiEngineThinkingModel* think1 = nullptr;
        UsiCommLogModel* comm2 = nullptr;
        ShogiEngineThinkingModel* think2 = nullptr;
        QStringList* sfenRecord = nullptr;
        PlayMode* playMode = nullptr;
        TimeDisplayPresenter* timePresenter = nullptr;
        BoardInteractionController* boardController = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;
        QStringList* positionStrList = nullptr;
        int* currentMoveIndex = nullptr;

        // --- 遅延初期化コールバック ---
        std::function<void()> ensureTimeController;
        std::function<void()> ensureEvaluationGraphController;
        std::function<void()> ensurePlayerInfoWiring;
        std::function<void()> ensureUsiCommandController;
        std::function<void()> ensureUiStatePolicyManager;
        std::function<void()> connectBoardClicks;
        std::function<void()> connectMoveRequested;

        // --- 遅延初期化オブジェクトのゲッター ---
        std::function<ShogiClock*()> getClock;
        std::function<TimeControlController*()> getTimeController;
        std::function<EvaluationGraphController*()> getEvalGraphController;
        std::function<UiStatePolicyManager*()> getUiStatePolicy;
    };

    MatchCoordinatorWiring::Deps buildDeps(const Inputs& in) const;
};

#endif // MAINWINDOWMATCHWIRINGDEPSSERVICE_H
