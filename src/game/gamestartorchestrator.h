#ifndef GAMESTARTORCHESTRATOR_H
#define GAMESTARTORCHESTRATOR_H

/// @file gamestartorchestrator.h
/// @brief 対局開始フローを管理するオーケストレータの定義

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "matchcoordinator.h"

class ShogiGameController;
class StartGameDialog;

/**
 * @brief 対局開始フロー（オプション構築・履歴探索・position文字列構築）を担当する
 *
 * MatchCoordinator から対局開始に関わるロジックを分離し、
 * configureAndStart / buildStartOptions / prepareAndStartGame を統合的に扱う。
 *
 * MatchCoordinator 内部状態への参照は Refs struct で受け取り、
 * MC メソッドへの委譲は Hooks struct のコールバックで実行する。
 */
class GameStartOrchestrator
{
public:
    using Player = MatchCoordinator::Player;
    using StartOptions = MatchCoordinator::StartOptions;

    /// MatchCoordinator の内部状態への参照群
    struct Refs {
        ShogiGameController* gc = nullptr;

        // 可変状態
        PlayMode* playMode = nullptr;
        int* maxMoves = nullptr;
        Player* currentTurn = nullptr;
        int* currentMoveIndex = nullptr;

        // 棋譜自動保存設定
        bool* autoSaveKifu = nullptr;
        QString* kifuSaveDir = nullptr;

        // 対局者名
        QString* humanName1 = nullptr;
        QString* humanName2 = nullptr;
        QString* engineNameForSave1 = nullptr;
        QString* engineNameForSave2 = nullptr;

        // USI position 文字列
        QString* positionStr1 = nullptr;
        QString* positionPonder1 = nullptr;
        QStringList* positionStrHistory = nullptr;

        // 対局履歴
        QVector<QStringList>* allGameHistories = nullptr;
    };

    /// MatchCoordinator メソッドへのコールバック群
    struct Hooks {
        // MC の GUI コールバック passthrough
        std::function<void(const QString&)> initializeNewGame;
        std::function<void(const QString&, const QString&)> setPlayersNames;
        std::function<void(const QString&, const QString&)> setEngineNames;
        std::function<void(bool)> setGameActions;
        std::function<void()> renderBoardFromGc;

        // MC メソッドへのコールバック
        std::function<void()> clearGameOverState;
        std::function<void(Player)> updateTurnDisplay;
        std::function<void(const QString&)> initializePositionStringsForStart;
        std::function<void(const StartOptions&)> createAndStartModeStrategy;
        std::function<void()> flipBoard;
        std::function<void()> startInitialEngineMoveIfNeeded;
    };

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    // --- 対局開始フロー API ---

    void configureAndStart(const StartOptions& opt);

    static StartOptions buildStartOptions(PlayMode mode,
                                          const QString& startSfenStr,
                                          const QStringList* sfenRecord,
                                          const StartGameDialog* dlg);

    void ensureHumanAtBottomIfApplicable(const StartGameDialog* dlg, bool bottomIsP1);

    void prepareAndStartGame(PlayMode mode,
                             const QString& startSfenStr,
                             const QStringList* sfenRecord,
                             const StartGameDialog* dlg,
                             bool bottomIsP1);

private:
    /// 過去対局の履歴探索結果
    struct HistorySearchResult {
        QString bestBaseFull;      ///< 一致したゲームのフルposition文字列
        int bestGameIdx  = -1;     ///< 一致したゲームのインデックス
        int bestMatchPly = -1;     ///< 一致した手数
    };

    HistorySearchResult syncAndSearchGameHistory(const QString& targetSfen);
    void applyStartOptionsAndHooks(const StartOptions& opt);
    void buildPositionStringsFromHistory(const StartOptions& opt,
                                         const QString& targetSfen,
                                         const HistorySearchResult& searchResult);

    static constexpr int kMaxGameHistories = 10;

    Refs m_refs;
    Hooks m_hooks;
};

#endif // GAMESTARTORCHESTRATOR_H
