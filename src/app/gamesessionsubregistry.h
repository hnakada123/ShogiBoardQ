#ifndef GAMESESSIONSUBREGISTRY_H
#define GAMESESSIONSUBREGISTRY_H

/// @file gamesessionsubregistry.h
/// @brief Game系セッション管理サブレジストリ
///
/// GameSubRegistry からセッション管理（クリーンアップ・ライブセッション・
/// ライフサイクル・コア初期化）関連の ensure* を抽出した独立クラス。

#include <QObject>

class GameSubRegistry;
class MainWindow;
class MainWindowFoundationRegistry;
class MainWindowServiceRegistry;

class GameSessionSubRegistry : public QObject
{
    Q_OBJECT

public:
    explicit GameSessionSubRegistry(MainWindow& mw,
                                    GameSubRegistry* gameReg,
                                    MainWindowServiceRegistry* registry,
                                    MainWindowFoundationRegistry* foundation,
                                    QObject* parent = nullptr);

    // ===== ensure* =====
    void ensurePreStartCleanupHandler();
    void ensureLiveGameSessionStarted();
    void ensureLiveGameSessionUpdater();
    void ensureGameSessionOrchestrator();
    void ensureCoreInitCoordinator();
    void ensureSessionLifecycleCoordinator();

    // ===== refresh deps (依存更新のみ、生成済み前提) =====
    void refreshLiveGameSessionUpdaterDeps();
    void refreshGameSessionOrchestratorDeps();
    void refreshCoreInitDeps();
    void refreshSessionLifecycleDeps();

    // ===== helpers =====
    void clearGameStateFields();
    void resetEngineState();

private:
    void createLiveGameSessionUpdater();
    void createGameSessionOrchestrator();
    void createCoreInitCoordinator();
    void createSessionLifecycleCoordinator();

    MainWindow& m_mw;
    GameSubRegistry* m_gameReg;
    MainWindowServiceRegistry* m_registry;
    MainWindowFoundationRegistry* m_foundation;
};

#endif // GAMESESSIONSUBREGISTRY_H
