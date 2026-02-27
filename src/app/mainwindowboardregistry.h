#ifndef MAINWINDOWBOARDREGISTRY_H
#define MAINWINDOWBOARDREGISTRY_H

/// @file mainwindowboardregistry.h
/// @brief 共通/Board系（盤面操作・リセット・横断的操作）の ensure* を集約するサブレジストリ

#include <QObject>
#include <QPoint>

class MainWindow;
class MainWindowServiceRegistry;
class QString;

/**
 * @brief 共通/Board系の ensure* メソッドおよび盤面操作を集約するサブレジストリ
 *
 * 責務:
 * - 盤面操作・局面編集・リセット関連の遅延初期化を一箇所に集約する
 * - カテゴリ外の ensure* 呼び出しは MainWindowServiceRegistry（ファサード）経由で解決する
 *
 * MainWindowServiceRegistry が所有し、同名メソッドへの転送で呼び出される。
 */
class MainWindowBoardRegistry : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowBoardRegistry(MainWindow& mw,
                                      MainWindowServiceRegistry& registry,
                                      QObject* parent = nullptr);

    // --- ensure* メソッド ---
    void ensureBoardSetupController();
    void ensurePositionEditCoordinator();
    void ensurePositionEditController();
    void ensureBoardSyncPresenter();
    void ensureBoardLoadService();

    // --- 盤面操作 ---
    void setupBoardInteractionController();
    void handleMoveRequested(const QPoint& from, const QPoint& to);
    void handleMoveCommitted(int mover, int ply);
    void handleBeginPositionEditing();
    void handleFinishPositionEditing();

    // --- リセット / UI クリア ---
    void resetModels(const QString& hirateStartSfen);
    void resetUiState(const QString& hirateStartSfen);
    void clearSessionDependentUi();

private:
    MainWindow& m_mw;                       ///< MainWindow への参照（生涯有効）
    MainWindowServiceRegistry& m_registry;  ///< ファサードへの参照（カテゴリ外呼び出し用）
};

#endif // MAINWINDOWBOARDREGISTRY_H
