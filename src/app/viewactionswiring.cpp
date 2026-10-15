/// @file viewactionswiring.cpp
/// @brief 盤操作・表示関連アクションの配線クラスの実装

#include "viewactionswiring.h"
#include "ui_mainwindow.h"
#include "gamesession.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "dialoglauncher.h"
#include "boardimageexporter.h"
#include "shogiview.h"
#include "evaluationchartwidget.h"
#include "logcategories.h"
#include "boardcolordialog.h"
#include "fontsettingsdialog.h"

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
    BoardImageExporter::saveImage(m_d.window, m_d.shogiView);
}

void ViewActionsWiring::saveEvaluationGraphImage()
{
    if (!m_d.evalChart) return;
    BoardImageExporter::saveImage(m_d.window, m_d.evalChart->chartViewWidget(),
                                  QStringLiteral("EvalGraph"));
}

void ViewActionsWiring::showBoardAppearance()
{
    if (!m_boardColorDialog) {
        m_boardColorDialog = new BoardColorDialog(m_d.window);
        m_boardColorDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_boardColorDialog->show();
    m_boardColorDialog->raise();
    m_boardColorDialog->activateWindow();
}

void ViewActionsWiring::showFontSettings()
{
    FontSettingsDialog dialog(m_d.window);
    dialog.exec();
}

void ViewActionsWiring::wire()
{
    auto* ui  = m_d.ui;
    auto* mw  = m_d.actions;
    auto* dlw = m_d.dlw;
    auto* session = m_d.session;
    auto* app = m_d.appearance;

    if (Q_UNLIKELY(!ui || !mw)) {
        qCWarning(lcUi, "ViewActionsWiring::wire(): ui or mw is null");
        return;
    }
    if (Q_UNLIKELY(!dlw || !session || !app)) {
        qCWarning(lcUi, "ViewActionsWiring::wire(): required wiring dependency is null "
                  "(dlw=%p, session=%p, appearance=%p)",
                  static_cast<void*>(dlw), static_cast<void*>(session), static_cast<void*>(app));
        return;
    }

    // 盤操作・表示（外観コントローラへ委譲）
    QObject::connect(ui->actionBoardAppearance, &QAction::triggered,
                     this, &ViewActionsWiring::showBoardAppearance, Qt::UniqueConnection);
    QObject::connect(ui->actionFontSettings, &QAction::triggered,
                     this, &ViewActionsWiring::showFontSettings, Qt::UniqueConnection);

    QObject::connect(ui->actionFlipBoard,                  &QAction::triggered, app, &MainWindowAppearanceController::flipBoard,                     Qt::UniqueConnection);
    QObject::connect(ui->actionCopyBoardToClipboard,       &QAction::triggered, this, &ViewActionsWiring::copyBoardToClipboard,          Qt::UniqueConnection);
    QObject::connect(ui->actionCopyEvalGraphToClipboard,   &QAction::triggered, this, &ViewActionsWiring::copyEvalGraphToClipboard,      Qt::UniqueConnection);
    QObject::connect(ui->actionMakeImmediateMove,          &QAction::triggered, session, &GameSession::makeEngineMoveNow,          Qt::UniqueConnection);
    QObject::connect(ui->actionMenuWindow,                 &QAction::triggered, dlw, &DialogLauncher::displayMenuWindow,    Qt::UniqueConnection);
    QObject::connect(ui->actionEnlargeBoard,               &QAction::triggered, app, &MainWindowAppearanceController::onActionEnlargeBoardTriggered, Qt::UniqueConnection);
    QObject::connect(ui->actionShrinkBoard,                &QAction::triggered, app, &MainWindowAppearanceController::onActionShrinkBoardTriggered,  Qt::UniqueConnection);
    QObject::connect(ui->actionUndoMove,                   &QAction::triggered, session, &GameSession::undoTwoMoves,               Qt::UniqueConnection);
    QObject::connect(ui->actionSaveBoardImage,             &QAction::triggered, this, &ViewActionsWiring::saveShogiBoardImage,           Qt::UniqueConnection);
    QObject::connect(ui->actionSaveEvaluationGraph,        &QAction::triggered, this, &ViewActionsWiring::saveEvaluationGraphImage,      Qt::UniqueConnection);

    // ツールバー表示切替（外観コントローラへ委譲）
    QObject::connect(ui->actionToolBar,                    &QAction::toggled,   app, &MainWindowAppearanceController::onToolBarVisibilityToggled,    Qt::UniqueConnection);
}
