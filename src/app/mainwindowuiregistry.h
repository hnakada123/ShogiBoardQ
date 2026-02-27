#ifndef MAINWINDOWUIREGISTRY_H
#define MAINWINDOWUIREGISTRY_H

/// @file mainwindowuiregistry.h
/// @brief UI系（ウィジェット・プレゼンター・ビュー・ドック・メニュー・通知）の ensure* を集約するサブレジストリ

#include <QObject>

class MainWindow;
class MainWindowServiceRegistry;

/**
 * @brief UI系の ensure* メソッドを集約するサブレジストリ
 *
 * 責務:
 * - ウィジェット・プレゼンター・ビュー・ドック・メニュー・通知に関する遅延初期化を一箇所に集約する
 * - カテゴリ外の ensure* 呼び出しは MainWindowServiceRegistry（ファサード）経由で解決する
 *
 * MainWindowServiceRegistry が所有し、同名メソッドへの転送で呼び出される。
 */
class MainWindowUiRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowUiRegistry(MainWindow& mw,
                                   MainWindowServiceRegistry& registry,
                                   QObject* parent = nullptr);

    // --- ensure* メソッド ---
    void ensureEvaluationGraphController();
    void ensureRecordPresenter();
    void ensurePlayerInfoController();
    void ensurePlayerInfoWiring();
    void ensureDialogCoordinator();
    void ensureMenuWiring();
    void ensureDockLayoutManager();
    void ensureDockCreationService();
    void ensureUiStatePolicyManager();
    void ensureUiNotificationService();
    void ensureRecordNavigationHandler();
    void ensureLanguageController();

    // --- 非ensure メソッド ---
    void unlockGameOverStyle();
    void createMenuWindowDock();
    void clearEvalState();

private:
    MainWindow& m_mw;                       ///< MainWindow への参照（生涯有効）
    MainWindowServiceRegistry& m_registry;  ///< ファサードへの参照（カテゴリ外呼び出し用）
};

#endif // MAINWINDOWUIREGISTRY_H
