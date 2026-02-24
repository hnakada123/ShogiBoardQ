/// @file viewactionswiring.cpp
/// @brief 盤操作・表示関連アクションの配線クラスの実装

#include "viewactionswiring.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "dialoglaunchwiring.h"

void ViewActionsWiring::wire()
{
    auto* ui  = m_d.ui;
    auto* mw  = m_d.mw;
    auto* dlw = m_d.dlw;

    // 盤操作・表示
    QObject::connect(ui->actionFlipBoard,                  &QAction::triggered, mw, &MainWindow::onActionFlipBoardTriggered,    Qt::UniqueConnection);
    QObject::connect(ui->actionCopyBoardToClipboard,       &QAction::triggered, mw, &MainWindow::copyBoardToClipboard,          Qt::UniqueConnection);
    QObject::connect(ui->actionCopyEvalGraphToClipboard,   &QAction::triggered, mw, &MainWindow::copyEvalGraphToClipboard,      Qt::UniqueConnection);
    QObject::connect(ui->actionMakeImmediateMove,          &QAction::triggered, mw, &MainWindow::movePieceImmediately,          Qt::UniqueConnection);
    QObject::connect(ui->actionMenuWindow,                 &QAction::triggered, dlw, &DialogLaunchWiring::displayMenuWindow,    Qt::UniqueConnection);
    QObject::connect(ui->actionEnlargeBoard,               &QAction::triggered, mw, &MainWindow::onActionEnlargeBoardTriggered, Qt::UniqueConnection);
    QObject::connect(ui->actionShrinkBoard,                &QAction::triggered, mw, &MainWindow::onActionShrinkBoardTriggered,  Qt::UniqueConnection);
    QObject::connect(ui->actionUndoMove,                   &QAction::triggered, mw, &MainWindow::undoLastTwoMoves,              Qt::UniqueConnection);
    QObject::connect(ui->actionSaveBoardImage,             &QAction::triggered, mw, &MainWindow::saveShogiBoardImage,           Qt::UniqueConnection);
    QObject::connect(ui->actionSaveEvaluationGraph,        &QAction::triggered, mw, &MainWindow::saveEvaluationGraphImage,      Qt::UniqueConnection);

    // ツールバー表示切替
    QObject::connect(ui->actionToolBar,                    &QAction::toggled,   mw, &MainWindow::onToolBarVisibilityToggled,    Qt::UniqueConnection);
}
