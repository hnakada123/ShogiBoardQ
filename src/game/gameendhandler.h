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

    /**
     * @brief MatchCoordinator の内部状態への参照群
     * @note MC::ensureGameEndHandler() で設定される。動的に変化するポインタは
     *       プロバイダ関数経由で取得する。
     */
    struct Refs {
        ShogiGameController* gc = nullptr;
        ShogiClock* clock = nullptr;
        std::function<Usi*()> usi1Provider;          ///< エンジン1取得（動的に変わるためプロバイダ）
        std::function<Usi*()> usi2Provider;           ///< エンジン2取得
        PlayMode* playMode = nullptr;
        GameOverState* gameOver = nullptr;
        std::function<GameModeStrategy*()> strategyProvider; ///< 現在のStrategy取得
        QStringList* sfenHistory = nullptr;
    };

    /**
     * @brief MatchCoordinator メソッドへのコールバック群
     *
     * 終局処理の各フェーズ（投了/時間切れ/千日手 → 結果表示 → 棋譜追記 → 自動保存）で
     * MC の状態更新や GUI 操作を行うために使用する。
     *
     * @note MC::ensureGameEndHandler() で lambda 経由で設定される。
     *       appendKifuLine, showGameOverDialog は MC::Hooks からのパススルー。
     * @see MatchCoordinator::ensureGameEndHandler
     */
    struct Hooks {
        /// @brief HvE 時の人間側タイマーを停止する
        /// @note 配線元: MC lambda (人間タイマー停止処理)
        std::function<void()> disarmHumanTimerIfNeeded;

        /// @brief メインエンジン（usi1）を返す
        /// @note 配線元: MC lambda → m_usi1
        std::function<Usi*()> primaryEngine;

        /// @brief 指定プレイヤーの手番開始エポック（ms）を返す
        /// @note 配線元: MC lambda → ShogiClock::turnEpochFor
        std::function<qint64(Player)> turnEpochFor;

        /// @brief 終局状態を MC に設定し、gameEnded/gameOverStateChanged シグナルを発火する
        /// @note 配線元: MC lambda → MC::setGameOver
        std::function<void(const GameEndInfo&, bool, bool)> setGameOver;

        /// @brief 棋譜に終局手を追記済みとしてマークする
        /// @note 配線元: MC lambda → m_gameOver.moveAppended = true
        std::function<void()> markGameOverMoveAppended;

        /// @brief 棋譜に1行追記する（終局行: "▲投了" 等）
        /// @note 配線元: MC→m_hooks.appendKifuLine (パススルー)
        std::function<void(const QString&, const QString&)> appendKifuLine;

        /// @brief 終局結果ダイアログを表示する
        /// @note 配線元: MC→m_hooks.showGameOverDialog (パススルー)
        std::function<void(const QString&, const QString&)> showGameOverDialog;

        /// @brief 棋譜自動保存（autoSaveKifu が有効な場合のみ実行）
        /// @note 配線元: MC lambda → KifuFileController::autoSaveKifuToFile
        std::function<void()> autoSaveKifuIfEnabled;
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
