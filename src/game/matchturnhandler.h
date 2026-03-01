#ifndef MATCHTURNHANDLER_H
#define MATCHTURNHANDLER_H

/// @file matchturnhandler.h
/// @brief ターン進行・ストラテジー管理ハンドラの定義

#include "matchcoordinator.h"

#include <memory>
#include <functional>
#include <QString>
#include <QPoint>

class ShogiGameController;
class Usi;
class ShogiClock;
class GameModeStrategy;

/**
 * @brief ターン進行とストラテジー管理を担当するハンドラ
 *
 * MatchCoordinator からターン管理・ストラテジー生成・指し手適用の
 * ロジックを分離し、対局モード別の振る舞いを集約する。
 *
 * Refs/Hooks パターンで MatchCoordinator の内部状態にアクセスする。
 */
class MatchTurnHandler
{
public:
    using Player = MatchCoordinator::Player;
    using StartOptions = MatchCoordinator::StartOptions;
    using GameOverState = MatchCoordinator::GameOverState;

    struct Refs {
        ShogiGameController* gc = nullptr;
        Player* currentTurn = nullptr;
        PlayMode* playMode = nullptr;
        QString* positionStr1 = nullptr;
        QString* positionPonder1 = nullptr;
        QStringList* positionStrHistory = nullptr;
        GameOverState* gameOver = nullptr;
        QObject* mcAsParent = nullptr;          ///< Strategy の QObject 親
    };

    struct Hooks {
        std::function<void()> renderBoardFromGc;
        std::function<void(Player)> updateTurnDisplayCb;
        std::function<Usi*()> primaryEngine;
        std::function<Usi*()> secondaryEngine;
        std::function<void(Usi*)> sendStopToEngine;
        std::function<ShogiClock*()> clockProvider;
        std::function<void(Player, const QString&, const QString&)> initAndStartEngine;
        std::function<void(const QString&, const QString&)> initEnginesForEvE;
        std::function<void(bool)> emitBoardFlipped;
        std::function<void()> handleBreakOff;
        std::function<bool(const QString&)> handleEngineError;
        std::function<void(const QString&, const QString&)> showGameOverDialog;
    };

    explicit MatchTurnHandler(MatchCoordinator& mc);
    ~MatchTurnHandler();

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    MatchCoordinator::StrategyContext& strategyCtx();
    GameModeStrategy* strategy() const;

    // --- ストラテジー管理 ---

    void createAndStartModeStrategy(const StartOptions& opt);

    // --- ターン進行 ---

    void onHumanMove(const QPoint& from, const QPoint& to, const QString& prettyMove);
    void startInitialEngineMoveIfNeeded();
    void armTurnTimerIfNeeded();
    void finishTurnTimerAndSetConsiderationFor(Player mover);
    void disarmHumanTimerIfNeeded();

    // --- 盤面・表示 ---

    void flipBoard();
    void updateTurnDisplay(Player p);
    void initPositionStringsFromSfen(const QString& sfenBase);

    // --- 対局進行 ---

    void forceImmediateMove();
    void handlePlayerTimeOut(int player);
    void startMatchTimingAndMaybeInitialGo();
    void handleUsiError(const QString& msg);

private:
    Refs m_refs;
    Hooks m_hooks;
    std::unique_ptr<MatchCoordinator::StrategyContext> m_strategyCtx;
    std::unique_ptr<GameModeStrategy> m_strategy;
};

#endif // MATCHTURNHANDLER_H
