#ifndef GAMESUBREGISTRY_H
#define GAMESUBREGISTRY_H

/// @file gamesubregistry.h
/// @brief Game系（対局・ゲーム進行・ターン管理・セッションライフサイクル）のサブレジストリ
///
/// MainWindowServiceRegistry から Game 系 ensure* を抽出した独立クラス。
/// 内部は3層に分かれる:
/// - GameSubRegistry: 状態・コントローラ管理（TimeController, ReplayController 等）
/// - GameSessionSubRegistry: セッション管理（ライフサイクル・クリーンアップ等）
/// - GameWiringSubRegistry: 配線管理（MC配線・CSA配線等）

#include <QObject>

class GameSessionSubRegistry;
class GameWiringSubRegistry;
class MainWindow;
class MainWindowFoundationRegistry;
class MainWindowServiceRegistry;
class QString;

class GameSubRegistry : public QObject
{
    Q_OBJECT

public:
    explicit GameSubRegistry(MainWindow& mw,
                             MainWindowServiceRegistry* registry,
                             MainWindowFoundationRegistry* foundation,
                             QObject* parent = nullptr);

    /// セッション管理サブレジストリへのアクセサ
    GameSessionSubRegistry* session() const { return m_session; }

    /// 配線管理サブレジストリへのアクセサ
    GameWiringSubRegistry* wiring() const { return m_wiring; }

    // ===== ensure* (状態・コントローラ管理) =====
    void ensureTimeController();
    void ensureReplayController();
    void ensureGameStateController();
    void ensureGameStartCoordinator();
    void ensureTurnStateSyncService();
    void ensureUndoFlowService();

    // ===== refresh deps (依存更新のみ、生成済み前提) =====
    void refreshTurnStateSyncDeps();
    void refreshUndoFlowDeps();

    // ===== helpers =====
    void updateTurnStatus(int currentPlayer);

    // ===== delegation (MainWindow スロットからの転送先) =====
    void setReplayMode(bool on);

private:
    void createTurnStateSyncService();
    void createUndoFlowService();

    MainWindow& m_mw;
    MainWindowServiceRegistry* m_registry;
    MainWindowFoundationRegistry* m_foundation;
    GameSessionSubRegistry* m_session;
    GameWiringSubRegistry* m_wiring;
};

#endif // GAMESUBREGISTRY_H
