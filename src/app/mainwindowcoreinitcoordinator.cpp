/// @file mainwindowcoreinitcoordinator.cpp
/// @brief MainWindow のコア初期化ロジック実装

#include "mainwindowcoreinitcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "sfenutils.h"
#include "gamestartcoordinator.h"

void MainWindowCoreInitCoordinator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void MainWindowCoreInitCoordinator::initialize()
{
    initializeGameControllerAndKifu();
    initializeOrResetShogiView();

    // 盤・駒台操作の配線（BoardInteractionController など）
    if (m_deps.setupBoardInteractionController) {
        m_deps.setupBoardInteractionController();
    }

    initializeBoardModel();

    // Turn 初期化 & 同期
    if (m_deps.setCurrentTurn) {
        m_deps.setCurrentTurn();
    }
    if (m_deps.ensureTurnSyncBridge) {
        m_deps.ensureTurnSyncBridge();
    }

    // 表示名・ログモデル名の初期反映
    if (m_deps.ensurePlayerInfoWiringAndApply) {
        m_deps.ensurePlayerInfoWiringAndApply();
    }
}

void MainWindowCoreInitCoordinator::initializeNewGame(QString& startSfenStr)
{
    auto* gc = m_deps.gameController ? *m_deps.gameController : nullptr;
    if (gc) {
        gc->newGame(startSfenStr);
    }

    auto* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    GameStartCoordinator::applyResumePositionIfAny(
        gc, view, m_deps.resumeSfenStr ? *m_deps.resumeSfenStr : QString());
}

void MainWindowCoreInitCoordinator::initializeGameControllerAndKifu()
{
    auto** gc = m_deps.gameController;
    if (gc && !*gc) {
        *gc = new ShogiGameController(m_deps.parent);
    }

    if (m_deps.moveRecords) {
        m_deps.moveRecords->clear();
    }
    if (m_deps.gameMoves) {
        m_deps.gameMoves->clear();
    }
    if (m_deps.gameUsiMoves) {
        m_deps.gameUsiMoves->clear();
    }
}

void MainWindowCoreInitCoordinator::initializeOrResetShogiView()
{
    auto** view = m_deps.shogiView;
    if (!view) return;

    if (!*view) {
        *view = new ShogiView(m_deps.parent);
        (*view)->setNameFontScale(0.30);
    } else {
        (*view)->setParent(m_deps.parent);
        (*view)->setNameFontScale(0.30);
    }
}

void MainWindowCoreInitCoordinator::initializeBoardModel()
{
    auto* gc = m_deps.gameController ? *m_deps.gameController : nullptr;
    auto* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (!gc || !view) return;

    // m_state.startSfenStr が "startpos ..." の場合は完全 SFEN に正規化
    QString start = m_deps.startSfenStr ? *m_deps.startSfenStr : QString();
    if (start.isEmpty()) {
        start = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    } else if (start.startsWith(QLatin1String("startpos"))) {
        start = SfenUtils::normalizeStart(start);
    }

    gc->newGame(start);
    if (view->board() != gc->board()) {
        view->setBoard(gc->board());
    }
}
