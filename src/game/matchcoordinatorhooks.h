#ifndef MATCHCOORDINATORHOOKS_H
#define MATCHCOORDINATORHOOKS_H

/// @file matchcoordinatorhooks.h
/// @brief MatchCoordinator の UI/Engine/Game コールバック構造体の定義
///
/// MatchCoordinator から上位層（MainWindow/Wiring層）への逆方向呼び出しに使用。
/// MatchCoordinator::Hooks は本型の using エイリアス。

#include <functional>

#include <QString>
#include <QPoint>

#include "matchtypes.h"
#include "playmode.h"

class Usi;

/**
 * @brief UI/描画系の委譲コールバック群
 *
 * MatchCoordinator から上位層（MainWindow/Wiring層）への逆方向呼び出しに使用。
 * 子ハンドラ（GameEndHandler, GameStartOrchestrator, EngineLifecycleManager 等）にも
 * パススルーされる。
 *
 * @note MatchCoordinatorHooksFactory::buildHooks() で一括生成される。
 *       入力は MainWindowMatchWiringDepsService::buildDeps() → HookDeps。
 * @see MatchCoordinatorHooksFactory, MainWindowMatchWiringDepsService
 */
struct MatchCoordinatorHooks {
    /// UI更新・描画系コールバック
    struct UI {
        std::function<void(MatchPlayer cur)> updateTurnDisplay;
        std::function<void(const QString& p1, const QString& p2)> setPlayersNames;
        std::function<void(const QString& e1, const QString& e2)> setEngineNames;
        std::function<void()> renderBoardFromGc;
        std::function<void(const QString& title, const QString& message)> showGameOverDialog;
        /// 対局者を下手・上手と呼ぶか（棋譜の書き出しと同じく、対局情報の見出しに合わせる）
        std::function<bool()> usesHandicapNames;
        std::function<void(const QPoint& from, const QPoint& to)> showMoveHighlights;
    };

    /// 時計読み出し系コールバック
    struct Time {
        std::function<qint64(MatchPlayer)> remainingMsFor;
        std::function<qint64(MatchPlayer)> incrementMsFor;
        std::function<qint64()> byoyomiMs;
    };

    /// USI送受系コールバック
    struct Engine {
        std::function<void(Usi* which)> sendStopToEngine;
    };

    /// ゲーム初期化・棋譜系コールバック
    struct Game {
        std::function<void(const QString& sfenStart)> initializeNewGame;
        std::function<void(const QString& text, const QString& elapsed)> appendKifuLine;
        std::function<void()> appendEvalP1;
        std::function<void()> appendEvalP2;
        std::function<void(const QString& saveDir)> autoSaveKifu;
    };

    UI ui;         ///< UI更新・描画系
    Time time;     ///< 時計読み出し系
    Engine engine; ///< USI送受系
    Game game;     ///< ゲーム初期化・棋譜系
};

#endif // MATCHCOORDINATORHOOKS_H
