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

    /**
     * @brief MatchCoordinator メソッドへのコールバック群
     *
     * 対局開始フロー (configureAndStart) で MC の状態更新や GUI 操作を行うために使用。
     * 前半5つは MC::Hooks からのパススルー、後半6つは MC メソッドへの直接コールバック。
     *
     * @note MC::ensureGameStartOrchestrator() で設定される。
     *       GameStartOrchestrator は非 QObject のため Signal/Slot は使用不可。
     * @see MatchCoordinator::ensureGameStartOrchestrator
     */
    struct Hooks {
        // --- MC::Hooks パススルー ---

        /// @brief GUI固有の新規対局初期化処理を実行する
        /// @note 配線元: MC→m_hooks.initializeNewGame (パススルー)
        std::function<void(const QString&)> initializeNewGame;

        /// @brief 対局者名をUIに設定する
        /// @note 配線元: MC→m_hooks.setPlayersNames (パススルー)
        std::function<void(const QString&, const QString&)> setPlayersNames;

        /// @brief エンジン名をUIに設定する
        /// @note 配線元: MC→m_hooks.setEngineNames (パススルー)
        std::function<void(const QString&, const QString&)> setEngineNames;

        /// @brief 対局中メニュー項目のON/OFF
        /// @note 配線元: MC→m_hooks.setGameActions (パススルー、未配線)
        std::function<void(bool)> setGameActions;

        /// @brief GC の盤面状態を View に反映する
        /// @note 配線元: MC→m_hooks.renderBoardFromGc (パススルー)
        std::function<void()> renderBoardFromGc;

        // --- MC メソッドへの直接コールバック ---

        /// @brief 終局状態をクリアする
        /// @note 配線元: MC lambda → MC::clearGameOverState
        std::function<void()> clearGameOverState;

        /// @brief 手番表示を更新する
        /// @note 配線元: MC lambda → MC::updateTurnDisplay (内部で m_hooks.updateTurnDisplay を呼ぶ)
        std::function<void(Player)> updateTurnDisplay;

        /// @brief 開始局面から USI position 文字列を初期化する
        /// @note 配線元: MC lambda → initPositionStringsFromSfen
        std::function<void(const QString&)> initializePositionStringsForStart;

        /// @brief PlayMode に応じた GameModeStrategy を生成し開始する
        /// @note 配線元: MC lambda → MC::createAndStartModeStrategy
        std::function<void(const StartOptions&)> createAndStartModeStrategy;

        /// @brief 盤面の上下を反転する（後手が下になるように）
        /// @note 配線元: MC lambda → MC::flipBoard
        std::function<void()> flipBoard;

        /// @brief エンジンが先手の場合に初手の思考を開始する
        /// @note 配線元: MC lambda → MC::startInitialEngineMoveIfNeeded
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
