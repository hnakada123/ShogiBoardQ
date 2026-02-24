/// @file gameactionswiring.cpp
/// @brief 対局/解析/検討/詰み関連アクションの配線クラスの実装

#include "gameactionswiring.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"

void GameActionsWiring::wire()
{
    auto* ui = m_d.ui;
    auto* mw = m_d.mw;

    // 対局
    QObject::connect(ui->actionNewGame,      &QAction::triggered, mw, &MainWindow::resetToInitialState,  Qt::UniqueConnection);
    QObject::connect(ui->actionStartGame,    &QAction::triggered, mw, &MainWindow::initializeGame,       Qt::UniqueConnection);
    QObject::connect(ui->actionCSA,          &QAction::triggered, mw, &MainWindow::displayCsaGameDialog, Qt::UniqueConnection);
    QObject::connect(ui->actionResign,       &QAction::triggered, mw, &MainWindow::handleResignation,    Qt::UniqueConnection);
    QObject::connect(ui->actionBreakOffGame, &QAction::triggered, mw, &MainWindow::handleBreakOffGame,   Qt::UniqueConnection);

    // 解析/検討/詰み・エンジン設定
    QObject::connect(ui->actionEngineSettings,       &QAction::triggered, mw, &MainWindow::displayEngineSettingsDialog,       Qt::UniqueConnection);
    QObject::connect(ui->actionAnalyzeKifu,          &QAction::triggered, mw, &MainWindow::displayKifuAnalysisDialog,         Qt::UniqueConnection);
    QObject::connect(ui->actionCancelAnalyzeKifu,    &QAction::triggered, mw, &MainWindow::cancelKifuAnalysis,                Qt::UniqueConnection);
    QObject::connect(ui->actionStartEditPosition,    &QAction::triggered, mw, &MainWindow::beginPositionEditing,              Qt::UniqueConnection);
    QObject::connect(ui->actionEndEditPosition,      &QAction::triggered, mw, &MainWindow::finishPositionEditing,             Qt::UniqueConnection);
    QObject::connect(ui->actionTsumeShogiSearch,     &QAction::triggered, mw, &MainWindow::displayTsumeShogiSearchDialog,     Qt::UniqueConnection);
    QObject::connect(ui->actionStopTsumeSearch,      &QAction::triggered, mw, &MainWindow::stopTsumeSearch,                   Qt::UniqueConnection);
    QObject::connect(ui->actionTsumeshogiGenerator,  &QAction::triggered, mw, &MainWindow::displayTsumeshogiGeneratorDialog,  Qt::UniqueConnection);
    QObject::connect(ui->actionJishogiScore,         &QAction::triggered, mw, &MainWindow::displayJishogiScoreDialog,         Qt::UniqueConnection);
    QObject::connect(ui->actionNyugyokuDeclaration,  &QAction::triggered, mw, &MainWindow::handleNyugyokuDeclaration,         Qt::UniqueConnection);
}
