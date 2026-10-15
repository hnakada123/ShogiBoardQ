/// @file mainwindowactions.cpp
/// @brief 起動後の利用者操作の振り分け

#include "mainwindowactions.h"

#include "appstate.h"
#include "boardinteractioncontroller.h"
#include "boardsetupcontroller.h"
#include "branchnavigationwiring.h"
#include "branchtreeeditcontroller.h"
#include "commentcoordinator.h"
#include "gamesession.h"
#include "josekiwindowcontroller.h"
#include "kifudisplaycoordinator.h"
#include "kifufilecontroller.h"
#include "positioneditcoordinator.h"
#include "recordnavigationhandler.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "uistatepolicymanager.h"

#include <QCoreApplication>
#include <QTimer>
#include <QWidget>
#include <utility>

MainWindowActions::MainWindowActions(Deps deps, QObject* parent)
    : QObject(parent)
    , m_deps(std::move(deps))
{
}

void MainWindowActions::handleMoveRequested(const QPoint& from, const QPoint& to)
{
    m_deps.boardSetup->onMoveRequested(from, to);
}

bool MainWindowActions::canRecordVariationMove() const
{
    // 待機中（対局・解析・検討・局面編集などをしていない）だけ受け付ける
    if (m_deps.uiPolicy->currentState() != UiStatePolicyManager::AppState::Idle) return false;
    if (m_deps.boardController && m_deps.boardController->mode() == BoardInteractionController::Mode::Edit) {
        return false;
    }
    BranchTreeEditController* editor = m_deps.branchWiring->editController();
    return editor != nullptr && editor->canRecordBoardMove();
}

bool MainWindowActions::recordVariationMove(QPoint& from, QPoint& to)
{
    BranchTreeEditController* editor = m_deps.branchWiring->editController();
    return editor != nullptr && editor->recordBoardMove(from, to, m_deps.gameController);
}

void MainWindowActions::handleMoveCommitted()
{
    m_deps.josekiController->updateJosekiWindow();
}

void MainWindowActions::onRecordPaneMainRowChanged(int row)
{
    // 未更新のコメントがあれば移る前に確認する。やめたときは、同じ行変更を受け取る
    // ほかの処理（棋譜欄の強調）が終わってから、選択と強調を現在の手に戻す
    if (!m_deps.comments->confirmLeaveEditingMove()) {
        QTimer::singleShot(0, m_deps.displayCoordinator, &KifuDisplayCoordinator::restoreCurrentPositionHighlights);
        return;
    }
    m_deps.recordNavigation->onMainRowChanged(row);
}

void MainWindowActions::beginPositionEditing()
{
    // 編集を始めると棋譜は開始局面だけになるため、未保存の棋譜は先に保存するか確認する。
    // やめたときは中断した対局も残す
    if (!m_deps.kifuFileController->confirmDiscardUnsaved()) return;
    m_deps.gameSession->discardInterruptedGame();
    m_deps.positionEdit->beginPositionEditing();
}

void MainWindowActions::finishPositionEditing()
{
    m_deps.positionEdit->finishPositionEditing();
}

void MainWindowActions::resetRecordForEditedPosition()
{
    if (!m_deps.view->board()) return;

    ShogiBoard* board = m_deps.view->board();
    board->setMoveNumber(1);
    const QString sfen = board->toSfen();
    m_deps.state->startSfenStr = sfen;
    m_deps.state->currentSfenStr = sfen;

    MainWindowResetService().resetModels(m_deps.editedPositionReset, sfen);
    m_deps.comments->broadcastComment(QString(), false);
}

void MainWindowActions::resetToInitialState()
{
    if (!m_deps.kifuFileController->confirmDiscardUnsaved()) return;
    m_deps.gameSession->resetToInitialState();
}

void MainWindowActions::requestQuit()
{
    // 閉じる確認と終了処理は MainWindow::closeEvent が行う
    if (m_deps.window->close()) QCoreApplication::quit();
}
