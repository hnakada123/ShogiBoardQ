#ifndef GAMEENDHANDLER_H
#define GAMEENDHANDLER_H

/// @file gameendhandler.h
/// @brief 終局処理ハンドラの定義

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "matchcoordinator.h"

class ShogiGameController;
class ShogiClock;
class Usi;
class GameModeStrategy;
class EngineVsEngineStrategy;

/**
 * @brief 終局判定・処理を担当するハンドラ
 *
 * MatchCoordinator から終局関連のロジックを分離し、
 * 投了・中断・千日手・持将棋・入玉宣言を統一的に扱う。
 *
 * MatchCoordinator 内部状態への参照は Refs struct で受け取り、
 * UI 通知や副作用は Hooks struct のコールバックで実行する。
 */
class GameEndHandler : public QObject
{
    Q_OBJECT

public:
    using Player = MatchCoordinator::Player;
    using Cause = MatchCoordinator::Cause;
    using GameEndInfo = MatchCoordinator::GameEndInfo;
    using GameOverState = MatchCoordinator::GameOverState;

    /// MatchCoordinator の内部状態への参照群
    struct Refs {
        ShogiGameController* gc = nullptr;
        ShogiClock* clock = nullptr;
        Usi** usi1 = nullptr;               ///< ポインタのポインタ（エンジンは動的に変わるため）
        Usi** usi2 = nullptr;
        PlayMode* playMode = nullptr;
        GameOverState* gameOver = nullptr;
        std::function<GameModeStrategy*()> strategyProvider; ///< 現在のStrategy取得
        QStringList* sfenHistory = nullptr;
    };

    /// MatchCoordinator メソッドへのコールバック群
    struct Hooks {
        std::function<void()> disarmHumanTimerIfNeeded;
        std::function<Usi*()> primaryEngine;
        std::function<qint64(Player)> turnEpochFor;
        std::function<void(bool)> setGameInProgressActions;
        std::function<void(const GameEndInfo&, bool, bool)> setGameOver;
        std::function<void()> markGameOverMoveAppended;
        std::function<void(const QString&, const QString&)> appendKifuLine;
        std::function<void(const QString&, const QString&)> showGameOverDialog;
        std::function<void(const QString&)> log;
        std::function<void()> autoSaveKifuIfEnabled;    ///< 棋譜自動保存（有効時のみ実行）
    };

    explicit GameEndHandler(QObject* parent = nullptr);

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    // --- 終局処理 API ---

    void handleResign();
    void handleEngineResign(int idx);
    void handleEngineWin(int idx);
    void handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw);
    void handleBreakOff();
    void handleMaxMovesJishogi();
    bool checkAndHandleSennichite();
    void handleSennichite();
    void handleOuteSennichite(bool p1Loses);

    void appendBreakOffLineAndMark();
    void appendGameOverLineAndMark(Cause cause, Player loser);

signals:
    void gameEnded(const MatchCoordinator::GameEndInfo& info);
    void gameOverStateChanged(const MatchCoordinator::GameOverState& st);

private:
    void displayResultsAndUpdateGui(const GameEndInfo& info);

    /// USI gameover コマンド送信ヘルパー
    void sendRawToEngineHelper(Usi* which, const QString& cmd);

    Refs m_refs;
    Hooks m_hooks;
};

#endif // GAMEENDHANDLER_H
