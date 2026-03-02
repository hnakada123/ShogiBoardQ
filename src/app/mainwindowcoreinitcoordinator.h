#ifndef MAINWINDOWCOREINITCOORDINATOR_H
#define MAINWINDOWCOREINITCOORDINATOR_H

/// @file mainwindowcoreinitcoordinator.h
/// @brief MainWindow のコア初期化ロジックを集約するコーディネータ

#include <QList>
#include <QStringList>
#include <QList>
#include <functional>

class QWidget;
class ShogiGameController;
class ShogiView;
class KifuDisplay;
struct ShogiMove;

/**
 * @brief MainWindow のコア初期化ロジックを集約するコーディネータ
 *
 * 責務:
 * - ShogiGameController の生成・初期化
 * - ShogiView の生成・初期化
 * - 盤面モデルの初期化（SFEN 正規化・newGame 実行）
 * - 新対局初期化（initializeNewGame）
 *
 * QObject 非継承の純粋ロジッククラス。
 */
class MainWindowCoreInitCoordinator
{
public:
    struct Deps {
        // --- Core objects（ダブルポインタ：生成先） ---
        ShogiGameController** gameController = nullptr;
        ShogiView** shogiView = nullptr;

        // --- 状態参照 ---
        QString* startSfenStr = nullptr;
        QString* resumeSfenStr = nullptr;
        QList<KifuDisplay*>* moveRecords = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;
        QStringList* gameUsiMoves = nullptr;

        // --- 親ウィジェット ---
        QWidget* parent = nullptr;

        // --- コールバック ---
        std::function<void()> setupBoardInteractionController;
        std::function<void()> setCurrentTurn;
        std::function<void()> ensureTurnSyncBridge;
        std::function<void()> ensurePlayerInfoWiringAndApply;
    };

    void updateDeps(const Deps& deps);

    /// 全コア初期化を一括実行（MainWindow::initializeComponents の移譲先）
    void initialize();

    /// 新対局初期化（MatchAdapter callback 用）
    void initializeNewGame(QString& startSfenStr);

private:
    Deps m_deps;

    void initializeGameControllerAndKifu();
    void initializeOrResetShogiView();
    void initializeBoardModel();
};

#endif // MAINWINDOWCOREINITCOORDINATOR_H
