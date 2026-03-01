#ifndef KIFUSUBREGISTRY_H
#define KIFUSUBREGISTRY_H

/// @file kifusubregistry.h
/// @brief Kifu系（棋譜・ナビゲーション・ファイルI/O・コメント・定跡）のサブレジストリ
///
/// MainWindowServiceRegistry から Kifu 系 ensure* を抽出した独立クラス。

#include <QObject>

class MainWindow;
class MainWindowFoundationRegistry;
class MainWindowServiceRegistry;
class QString;

class KifuSubRegistry : public QObject
{
    Q_OBJECT

public:
    explicit KifuSubRegistry(MainWindow& mw,
                             MainWindowServiceRegistry* registry,
                             MainWindowFoundationRegistry* foundation,
                             QObject* parent = nullptr);

    // ===== ensure* =====
    void ensureBranchNavigationWiring();
    void ensureKifuFileController();
    void ensureKifuExportController();
    void ensureGameRecordUpdateService();
    void ensureGameRecordLoadService();
    void ensureKifuLoadCoordinatorForLive();
    void ensureGameRecordModel();
    void ensureJosekiWiring();

    // ===== refresh deps (依存更新のみ、生成済み前提) =====
    void refreshBranchNavWiringDeps();
    void refreshGameRecordUpdateDeps();
    void refreshGameRecordLoadDeps();

    // ===== helpers =====
    void clearUiBeforeKifuLoad();
    void updateJosekiWindow();
    void updateKifuExportDeps();

private:
    void createBranchNavigationWiring();
    void createGameRecordUpdateService();
    void createGameRecordLoadService();

    /// KifuLoadCoordinator を生成・配線し m_kifuLoadCoordinator に設定するヘルパー
    void createAndWireKifuLoadCoordinator();

    MainWindow& m_mw;
    MainWindowServiceRegistry* m_registry;
    MainWindowFoundationRegistry* m_foundation;
};

#endif // KIFUSUBREGISTRY_H
