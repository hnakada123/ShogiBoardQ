/// @file viewactionswiring.cpp
/// @brief 盤操作・表示関連アクションの配線クラスの実装

#include "viewactionswiring.h"
#include "ui_mainwindow.h"
#include "gamesessionorchestrator.h"
#include "mainwindow.h"
#include "mainwindowappearancecontroller.h"
#include "dialoglaunchwiring.h"
#include "boardimageexporter.h"
#include "shogiview.h"
#include "evaluationchartwidget.h"

void ViewActionsWiring::copyBoardToClipboard()
{
    BoardImageExporter::copyToClipboard(m_d.shogiView);
}

void ViewActionsWiring::copyEvalGraphToClipboard()
{
    if (!m_d.evalChart) return;
    BoardImageExporter::copyToClipboard(m_d.evalChart->chartViewWidget());
}

void ViewActionsWiring::saveShogiBoardImage()
{
    BoardImageExporter::saveImage(m_d.mw, m_d.shogiView);
}

void ViewActionsWiring::saveEvaluationGraphImage()
{
    if (!m_d.evalChart) return;
    BoardImageExporter::saveImage(m_d.mw, m_d.evalChart->chartViewWidget(),
                                  QStringLiteral("EvalGraph"));
}

void ViewActionsWiring::wire()
{
    auto* ui  = m_d.ui;
    auto* mw  = m_d.mw;
    auto* dlw = m_d.dlw;
    auto* gso = m_d.gso;
    auto* app = m_d.appearance;

    // 盤操作・表示（外観コントローラへ委譲）
    QObject::connect(ui->actionFlipBoard,                  &QAction::triggered, app, &MainWindowAppearanceController::onActionFlipBoardTriggered,    Qt::UniqueConnection);
    QObject::connect(ui->actionCopyBoardToClipboard,       &QAction::triggered, this, &ViewActionsWiring::copyBoardToClipboard,          Qt::UniqueConnection);
    QObject::connect(ui->actionCopyEvalGraphToClipboard,   &QAction::triggered, this, &ViewActionsWiring::copyEvalGraphToClipboard,      Qt::UniqueConnection);
    QObject::connect(ui->actionMakeImmediateMove,          &QAction::triggered, gso, &GameSessionOrchestrator::movePieceImmediately, Qt::UniqueConnection);
    QObject::connect(ui->actionMenuWindow,                 &QAction::triggered, dlw, &DialogLaunchWiring::displayMenuWindow,    Qt::UniqueConnection);
    QObject::connect(ui->actionEnlargeBoard,               &QAction::triggered, app, &MainWindowAppearanceController::onActionEnlargeBoardTriggered, Qt::UniqueConnection);
    QObject::connect(ui->actionShrinkBoard,                &QAction::triggered, app, &MainWindowAppearanceController::onActionShrinkBoardTriggered,  Qt::UniqueConnection);
    QObject::connect(ui->actionUndoMove,                   &QAction::triggered, mw, &MainWindow::undoLastTwoMoves,              Qt::UniqueConnection);
    QObject::connect(ui->actionSaveBoardImage,             &QAction::triggered, this, &ViewActionsWiring::saveShogiBoardImage,           Qt::UniqueConnection);
    QObject::connect(ui->actionSaveEvaluationGraph,        &QAction::triggered, this, &ViewActionsWiring::saveEvaluationGraphImage,      Qt::UniqueConnection);

    // ツールバー表示切替（外観コントローラへ委譲）
    QObject::connect(ui->actionToolBar,                    &QAction::toggled,   app, &MainWindowAppearanceController::onToolBarVisibilityToggled,    Qt::UniqueConnection);
}
