#ifndef MAINWINDOWGAMEREGISTRY_H
#define MAINWINDOWGAMEREGISTRY_H

/// @file mainwindowgameregistry.h
/// @brief Game系（対局・ゲーム進行・ターン管理・セッションライフサイクル）の ensure* を集約するサブレジストリ

#include <QObject>

class MainWindow;
class MainWindowServiceRegistry;
class QString;

/**
 * @brief Game系の ensure* メソッドを集約するサブレジストリ
 *
 * 責務:
 * - 対局・ゲーム進行・ターン管理・セッションライフサイクルに関する遅延初期化を一箇所に集約する
 * - カテゴリ外の ensure* 呼び出しは MainWindowServiceRegistry（ファサード）経由で解決する
 *
 * MainWindowServiceRegistry が所有し、同名メソッドへの転送で呼び出される。
 */
class MainWindowGameRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowGameRegistry(MainWindow& mw,
                                     MainWindowServiceRegistry& registry,
                                     QObject* parent = nullptr);

    // --- ensure* メソッド ---
    void ensureTimeController();
    void ensureReplayController();
    void ensureMatchCoordinatorWiring();
    void ensureGameStateController();
    void ensureGameStartCoordinator();
    void ensureCsaGameWiring();
    void ensurePreStartCleanupHandler();
    void ensureTurnSyncBridge();
    void ensureTurnStateSyncService();
    void ensureLiveGameSessionStarted();
    void ensureLiveGameSessionUpdater();
    void ensureUndoFlowService();
    void ensureJishogiController();
    void ensureNyugyokuHandler();
    void ensureConsecutiveGamesController();
    void ensureGameSessionOrchestrator();
    void ensureSessionLifecycleCoordinator();
    void ensureCoreInitCoordinator();

    // --- 非ensure メソッド ---
    void clearGameStateFields();
    void resetEngineState();
    void updateTurnStatus(int currentPlayer);
    void initMatchCoordinator();

private:
    MainWindow& m_mw;                       ///< MainWindow への参照（生涯有効）
    MainWindowServiceRegistry& m_registry;  ///< ファサードへの参照（カテゴリ外呼び出し用）
};

#endif // MAINWINDOWGAMEREGISTRY_H
