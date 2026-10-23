/// @file mainwindowactions.cpp
/// @brief 複数の部品にまたがる利用者の操作

#include "mainwindowactions.h"

#include "gamesession.h"
#include "kifufilecontroller.h"
#include "positioneditsession.h"
#include "sessionresetcoordinator.h"

#include <QCoreApplication>
#include <QWidget>

MainWindowActions::MainWindowActions(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.window && m_deps.gameSession && m_deps.positionEdit
             && m_deps.kifuFileController && m_deps.reset);
}

void MainWindowActions::beginPositionEditing()
{
    // 編集を始めると棋譜は開始局面だけになるため、未保存の棋譜は先に保存するか確認する。
    // やめたときは中断した対局も残す
    if (!m_deps.kifuFileController->confirmDiscardUnsaved()) return;
    m_deps.gameSession->discardInterruptedGame();
    m_deps.positionEdit->begin();
}

void MainWindowActions::resetRecordForEditedPosition(const QString& sfen)
{
    m_deps.reset->resetToNewRecord(sfen);
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
