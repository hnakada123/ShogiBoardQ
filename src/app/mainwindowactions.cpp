#include "mainwindowactions.h"
#include "boardsetupcontroller.h"
#include "boardinteractioncontroller.h"
#include "branchnavigationwiring.h"
#include "branchtreeeditcontroller.h"
#include "gamerecordupdateservice.h"
#include "gamesessionorchestrator.h"
#include "positioneditcoordinator.h"
#include "commentcoordinator.h"
#include "uistatepolicymanager.h"
#include "shogiboard.h"
#include "shogiview.h"
#include <QScopedValueRollback>
#include <utility>

MainWindowActions::MainWindowActions(Deps deps) : m_deps(std::move(deps)) {}

void MainWindowActions::handleMoveRequested(const QPoint& from, const QPoint& to)
{
    m_deps.boardSetup->setPlayMode(*m_deps.playMode);
    m_deps.boardSetup->onMoveRequested(from, to);
}

bool MainWindowActions::canRecordVariationMove() const
{
    // 待機中（対局・解析・検討・局面編集などをしていない）だけ受け付ける
    if (m_deps.uiPolicy == nullptr
        || m_deps.uiPolicy->currentState() != UiStatePolicyManager::AppState::Idle) {
        return false;
    }
    if (m_deps.boardController != nullptr
        && m_deps.boardController->mode() == BoardInteractionController::Mode::Edit) {
        return false;
    }
    BranchTreeEditController* editor = m_deps.branchWiring ? m_deps.branchWiring->editController() : nullptr;
    return editor != nullptr && editor->canRecordBoardMove();
}

bool MainWindowActions::recordVariationMove(QPoint& from, QPoint& to)
{
    BranchTreeEditController* editor = m_deps.branchWiring ? m_deps.branchWiring->editController() : nullptr;
    return editor != nullptr && editor->recordBoardMove(from, to, m_deps.gameController);
}

void MainWindowActions::handleMoveCommitted(int mover, int ply)
{
    if (m_deps.boardSetup) {
        m_deps.boardSetup->setPlayMode(*m_deps.playMode);
        m_deps.boardSetup->onMoveCommitted(
            static_cast<Turn>(mover), ply);
    }

    m_deps.recordUpdates->updateCurrentSfenAfterMove();

    m_deps.updateJosekiWindow();
}

void MainWindowActions::handleBeginPositionEditing()
{
    // 編集を始めると棋譜は開始局面だけになるため、未保存の棋譜は先に保存するか確認する。
    // やめたときは中断した対局も残す
    if (!m_deps.confirmDiscardUnsavedKifu()) return;
    if (m_deps.gameSession) {
        m_deps.gameSession->discardInterruptedGame();
    }

    if (m_deps.positionEdit) {
        m_deps.positionEdit->beginPositionEditing();
    }
}

void MainWindowActions::handleFinishPositionEditing()
{
    if (m_deps.positionEdit) {
        m_deps.positionEdit->finishPositionEditing();
    }
}

void MainWindowActions::resetRecordForEditedPosition()
{
    if (!m_deps.view || !m_deps.view->board()) return;

    // 行選択の通知で編集局面が以前の棋譜に巻き戻されるのを防ぐ。
    const QScopedValueRollback<bool> guard(*m_deps.rowGuard, true);
    ShogiBoard* board = m_deps.view->board();
    const QString sfen = QStringLiteral("%1 %2 %3 1")
                             .arg(board->convertBoardToSfen(),
                                  turnToSfen(board->currentPlayer()),
                                  board->convertStandToSfen());
    *m_deps.startSfen = sfen;
    *m_deps.currentSfen = sfen;
    m_deps.gameMoves->clear();

    const auto& deps = m_deps.editedPositionReset;
    const MainWindowResetService resetService;
    resetService.resetModels(deps, sfen);

    if (m_deps.comments) {
        m_deps.comments->broadcastComment(QString(), false);
    }
}
