#ifndef MAINWINDOWKIFUREGISTRY_H
#define MAINWINDOWKIFUREGISTRY_H

/// @file mainwindowkifuregistry.h
/// @brief Kifu系（棋譜・ナビゲーション・ファイルI/O・コメント・定跡）の ensure* を集約するサブレジストリ

#include <QObject>

class MainWindow;
class MainWindowServiceRegistry;

/**
 * @brief Kifu系の ensure* メソッドを集約するサブレジストリ
 *
 * 責務:
 * - 棋譜・ナビゲーション・ファイルI/O・コメント・定跡に関する遅延初期化を一箇所に集約する
 * - カテゴリ外の ensure* 呼び出しは MainWindowServiceRegistry（ファサード）経由で解決する
 *
 * MainWindowServiceRegistry が所有し、同名メソッドへの転送で呼び出される。
 */
class MainWindowKifuRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowKifuRegistry(MainWindow& mw,
                                     MainWindowServiceRegistry& registry,
                                     QObject* parent = nullptr);

    // --- ensure* メソッド ---
    void ensureBranchNavigationWiring();
    void ensureKifuFileController();
    void ensureKifuExportController();
    void ensureGameRecordUpdateService();
    void ensureGameRecordLoadService();
    void ensureKifuLoadCoordinatorForLive();
    void ensureGameRecordModel();
    void ensureCommentCoordinator();
    void ensureKifuNavigationCoordinator();
    void ensureJosekiWiring();

    // --- 非ensure メソッド ---
    void clearUiBeforeKifuLoad();
    void updateJosekiWindow();

private:
    /// KifuLoadCoordinator を生成・配線し m_kifuLoadCoordinator に設定するヘルパー
    void createAndWireKifuLoadCoordinator();

    MainWindow& m_mw;                       ///< MainWindow への参照（生涯有効）
    MainWindowServiceRegistry& m_registry;  ///< ファサードへの参照（カテゴリ外呼び出し用）
};

#endif // MAINWINDOWKIFUREGISTRY_H
