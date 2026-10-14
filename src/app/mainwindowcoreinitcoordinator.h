#ifndef MAINWINDOWCOREINITCOORDINATOR_H
#define MAINWINDOWCOREINITCOORDINATOR_H

/// @file mainwindowcoreinitcoordinator.h
/// @brief MainWindow のコア初期化ロジックを集約するコーディネータ

#include <QList>
#include <QStringList>
#include <functional>

class ShogiGameController;
class ShogiView;
struct ShogiMove;

/**
 * @brief MainWindow のコア初期化ロジックを集約するコーディネータ
 *
 * 責務:
 * - ShogiGameController・ShogiView の初期化（生成はレジストリが起動時に行って渡す）
 * - 盤面モデルの初期化（SFEN 正規化・newGame 実行）
 * - 新対局初期化（initializeNewGame）
 *
 * QObject 非継承の純粋ロジッククラス。
 */
class MainWindowCoreInitCoordinator
{
public:
    struct Deps {
        // --- Core objects（起動時にレジストリが作る） ---
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;

        // --- 状態参照 ---
        QString* startSfenStr = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;

        // --- コールバック ---
        std::function<void()> setupBoardInteractionController;
        std::function<void()> setCurrentTurn;
        std::function<void()> prepareTurnSyncBridge;
        std::function<void()> ensurePlayerInfoWiringAndApply;
    };

    void updateDeps(const Deps& deps);

    /// 全コア初期化を一括実行（MainWindow::initializeComponents の移譲先）
    void initialize();

    /// 新対局初期化（MatchAdapter callback 用）
    void initializeNewGame(QString& startSfenStr);

private:
    Deps m_deps;

    void initializeBoardModel();
};

#endif // MAINWINDOWCOREINITCOORDINATOR_H
