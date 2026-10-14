/// @file mainwindowcoreinitcoordinator.cpp
/// @brief MainWindow のコア初期化ロジック実装

#include "mainwindowcoreinitcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "sfenutils.h"

void MainWindowCoreInitCoordinator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void MainWindowCoreInitCoordinator::initialize()
{
    if (m_deps.gameMoves) {
        m_deps.gameMoves->clear();
    }
    if (m_deps.shogiView) {
        m_deps.shogiView->setNameFontScale(0.30);
    }

    // 盤・駒台操作の配線（BoardInteractionController など）
    if (m_deps.setupBoardInteractionController) {
        m_deps.setupBoardInteractionController();
    }

    initializeBoardModel();

    // Turn 初期化 & 同期
    if (m_deps.setCurrentTurn) {
        m_deps.setCurrentTurn();
    }
    if (m_deps.prepareTurnSyncBridge) {
        m_deps.prepareTurnSyncBridge();
    }

    // 表示名・ログモデル名の初期反映
    if (m_deps.ensurePlayerInfoWiringAndApply) {
        m_deps.ensurePlayerInfoWiringAndApply();
    }
}

void MainWindowCoreInitCoordinator::initializeNewGame(QString& startSfenStr)
{
    if (m_deps.gameController) {
        m_deps.gameController->newGame(startSfenStr);
    }
}

void MainWindowCoreInitCoordinator::initializeBoardModel()
{
    ShogiGameController* gc = m_deps.gameController;
    ShogiView* view = m_deps.shogiView;
    if (!gc || !view) return;

    // m_state.startSfenStr が "startpos ..." の場合は完全 SFEN に正規化
    QString start = m_deps.startSfenStr ? *m_deps.startSfenStr : QString();
    if (start.isEmpty()) {
        start = SfenUtils::hirateSfen();
    } else if (start.startsWith(QLatin1String("startpos"))) {
        start = SfenUtils::normalizeStart(start);
    }

    gc->newGame(start);
    if (view->board() != gc->board()) {
        view->setBoard(gc->board());
    }
}
