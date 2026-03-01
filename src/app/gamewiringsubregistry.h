#ifndef GAMEWIRINGSUBREGISTRY_H
#define GAMEWIRINGSUBREGISTRY_H

/// @file gamewiringsubregistry.h
/// @brief Game系配線管理サブレジストリ
///
/// GameSubRegistry から配線管理（MatchCoordinator 配線・CSA 配線・
/// 連続対局・ターン同期ブリッジ・MC初期化）関連の ensure* を抽出した独立クラス。

#include <QObject>

#include "matchcoordinatorwiring.h"

class GameSubRegistry;
class MainWindow;
class MainWindowFoundationRegistry;
class MainWindowServiceRegistry;

class GameWiringSubRegistry : public QObject
{
    Q_OBJECT

public:
    explicit GameWiringSubRegistry(MainWindow& mw,
                                   GameSubRegistry* gameReg,
                                   MainWindowServiceRegistry* registry,
                                   MainWindowFoundationRegistry* foundation,
                                   QObject* parent = nullptr);

    // ===== ensure* =====
    void ensureMatchCoordinatorWiring();
    void ensureCsaGameWiring();
    void ensureConsecutiveGamesController();
    void ensureTurnSyncBridge();

    // ===== refresh deps (依存更新のみ、生成済み前提) =====
    void refreshMatchWiringDeps();

    // ===== helpers =====
    void initMatchCoordinator();

private:
    void createMatchCoordinatorWiring();

    /// MatchCoordinatorWiring::Deps を構築する
    MatchCoordinatorWiring::Deps buildMatchWiringDeps();

    MainWindow& m_mw;
    GameSubRegistry* m_gameReg;
    MainWindowServiceRegistry* m_registry;
    MainWindowFoundationRegistry* m_foundation;
};

#endif // GAMEWIRINGSUBREGISTRY_H
