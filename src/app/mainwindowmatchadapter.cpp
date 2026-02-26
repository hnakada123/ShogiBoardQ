/// @file mainwindowmatchadapter.cpp
/// @brief MatchCoordinator callback 群アダプタの実装

#include "mainwindowmatchadapter.h"
#include "shogiview.h"
#include "shogigamecontroller.h"
#include "boardinteractioncontroller.h"
#include "dialogcoordinator.h"
#include "turnstatesyncservice.h"
#include "playerinfowiring.h"
#include "playmode.h"
#include "logcategories.h"

void MainWindowMatchAdapter::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void MainWindowMatchAdapter::initializeNewGameHook(const QString& s)
{
    qCDebug(lcApp) << "MainWindowMatchAdapter::initializeNewGameHook called with sfen:" << s;
    if (m_deps.playMode)
        qCDebug(lcApp) << "  PlayMode:" << static_cast<int>(*m_deps.playMode);

    // 盤モデルの初期化（従来の UI 側初期化）
    QString startSfenStr = s;
    if (m_deps.initializeNewGame) {
        m_deps.initializeNewGame(startSfenStr);
    }

    // 盤の再描画・サイズ調整
    if (m_deps.shogiView && m_deps.gameController && m_deps.gameController->board()) {
        m_deps.shogiView->applyBoardAndRender(m_deps.gameController->board());
    }

    // 表示名の更新
    if (m_deps.ensureAndGetPlayerInfoWiring) {
        auto* wiring = m_deps.ensureAndGetPlayerInfoWiring();
        if (wiring) {
            wiring->applyPlayersNamesForMode();
            wiring->applyEngineNamesToLogModels();
            wiring->applySecondEngineVisibility();
        }
    }
}

void MainWindowMatchAdapter::renderBoardFromGc()
{
    if (m_deps.shogiView && m_deps.gameController && m_deps.gameController->board()) {
        m_deps.shogiView->applyBoardAndRender(m_deps.gameController->board());
    }
}

void MainWindowMatchAdapter::showMoveHighlights(const QPoint& from, const QPoint& to)
{
    if (m_deps.boardController) m_deps.boardController->showMoveHighlights(from, to);
}

void MainWindowMatchAdapter::showGameOverMessageBox(const QString& title, const QString& message)
{
    if (m_deps.ensureAndGetDialogCoordinator) {
        auto* coordinator = m_deps.ensureAndGetDialogCoordinator();
        if (coordinator) {
            coordinator->showGameOverMessage(title, message);
        }
    }
}

void MainWindowMatchAdapter::updateTurnAndTimekeepingDisplay()
{
    if (m_deps.ensureAndGetTurnStateSync) {
        auto* sync = m_deps.ensureAndGetTurnStateSync();
        if (sync) {
            sync->updateTurnAndTimekeepingDisplay();
        }
    }
}
