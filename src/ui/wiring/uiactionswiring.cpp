/// @file uiactionswiring.cpp
/// @brief メニュー/ツールバーアクションのシグナル/スロット接続クラスの実装

#include "uiactionswiring.h"
#include "ui_mainwindow.h"        // Ui::MainWindow
#include "shogiview.h"
#include "mainwindow.h"           // MainWindow のスロットを型安全に結ぶため
#include "kifuexportcontroller.h"
#include <QApplication>

void UiActionsWiring::wire()
{
    auto* ui = m_d.ui;
    auto* mw = qobject_cast<MainWindow*>(m_d.ctx);
    Q_ASSERT(ui && mw);

    // ============================================================
    // ファイル/アプリ
    // ============================================================
    QObject::connect(ui->actionQuit,        &QAction::triggered, mw, &MainWindow::saveSettingsAndClose,      Qt::UniqueConnection);
    QObject::connect(ui->actionSaveAs,      &QAction::triggered, mw, &MainWindow::saveKifuToFile,            Qt::UniqueConnection);
    QObject::connect(ui->actionSave,        &QAction::triggered, mw, &MainWindow::overwriteKifuFile,         Qt::UniqueConnection);
    QObject::connect(ui->actionOpenKifuFile,&QAction::triggered, mw, &MainWindow::chooseAndLoadKifuFile,     Qt::UniqueConnection);
    QObject::connect(ui->actionVersionInfo, &QAction::triggered, mw, &MainWindow::displayVersionInformation, Qt::UniqueConnection);
    QObject::connect(ui->actionOpenWebsite, &QAction::triggered, mw, &MainWindow::openWebsiteInExternalBrowser, Qt::UniqueConnection);
    QObject::connect(ui->actionAboutQt,     &QAction::triggered, mw, []() { QApplication::aboutQt(); });

    // ============================================================
    // 対局
    // ============================================================
    QObject::connect(ui->actionNewGame,     &QAction::triggered, mw, &MainWindow::resetToInitialState, Qt::UniqueConnection);
    QObject::connect(ui->actionStartGame,   &QAction::triggered, mw, &MainWindow::initializeGame,      Qt::UniqueConnection);
    QObject::connect(ui->actionCSA,        &QAction::triggered, mw, &MainWindow::displayCsaGameDialog, Qt::UniqueConnection);
    QObject::connect(ui->actionResign,      &QAction::triggered, mw, &MainWindow::handleResignation,   Qt::UniqueConnection);
    QObject::connect(ui->actionBreakOffGame,      &QAction::triggered, mw, &MainWindow::handleBreakOffGame,  Qt::UniqueConnection);

    // ============================================================
    // 盤操作・表示
    // ============================================================
    QObject::connect(ui->actionFlipBoard,            &QAction::triggered, mw,           &MainWindow::onActionFlipBoardTriggered, Qt::UniqueConnection);
    QObject::connect(ui->actionCopyBoardToClipboard, &QAction::triggered, mw,           &MainWindow::copyBoardToClipboard,       Qt::UniqueConnection);
    QObject::connect(ui->actionMakeImmediateMove,    &QAction::triggered, mw,           &MainWindow::movePieceImmediately,       Qt::UniqueConnection);
    QObject::connect(ui->actionMenuWindow,                 &QAction::triggered, mw,           &MainWindow::displayMenuWindow,          Qt::UniqueConnection);
    QObject::connect(ui->actionEnlargeBoard,     &QAction::triggered, mw, &MainWindow::onActionEnlargeBoardTriggered, Qt::UniqueConnection);
    QObject::connect(ui->actionShrinkBoard,      &QAction::triggered, mw, &MainWindow::onActionShrinkBoardTriggered,  Qt::UniqueConnection);
    QObject::connect(ui->actionUndoMove,             &QAction::triggered, mw, &MainWindow::undoLastTwoMoves,   Qt::UniqueConnection);
    QObject::connect(ui->actionSaveBoardImage,       &QAction::triggered, mw, &MainWindow::saveShogiBoardImage, Qt::UniqueConnection);

    // ============================================================
    // 解析/検討/詰み・エンジン設定
    // ============================================================
    QObject::connect(ui->actionEngineSettings,       &QAction::triggered, mw, &MainWindow::displayEngineSettingsDialog,    Qt::UniqueConnection);
    QObject::connect(ui->actionAnalyzeKifu,          &QAction::triggered, mw, &MainWindow::displayKifuAnalysisDialog,      Qt::UniqueConnection);
    QObject::connect(ui->actionCancelAnalyzeKifu,     &QAction::triggered, mw, &MainWindow::cancelKifuAnalysis,             Qt::UniqueConnection);
    QObject::connect(ui->actionStartEditPosition,    &QAction::triggered, mw, &MainWindow::beginPositionEditing,           Qt::UniqueConnection);
    QObject::connect(ui->actionEndEditPosition,      &QAction::triggered, mw, &MainWindow::finishPositionEditing,          Qt::UniqueConnection);
    QObject::connect(ui->actionTsumeShogiSearch,     &QAction::triggered, mw, &MainWindow::displayTsumeShogiSearchDialog,  Qt::UniqueConnection);
    QObject::connect(ui->actionStopTsumeSearch,     &QAction::triggered, mw, &MainWindow::stopTsumeSearch,                Qt::UniqueConnection);
    QObject::connect(ui->actionJishogiScore,        &QAction::triggered, mw, &MainWindow::displayJishogiScoreDialog,      Qt::UniqueConnection);
    QObject::connect(ui->actionNyugyokuDeclaration, &QAction::triggered, mw, &MainWindow::handleNyugyokuDeclaration,      Qt::UniqueConnection);

    // ============================================================
    // 棋譜コピー（編集メニュー）
    // ============================================================
    KifuExportController* kec = mw->kifuExportController();
    QObject::connect(ui->actionCopyKIF,        &QAction::triggered, kec, &KifuExportController::copyKifToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyKI2,        &QAction::triggered, kec, &KifuExportController::copyKi2ToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyCSA,        &QAction::triggered, kec, &KifuExportController::copyCsaToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyUSICurrent, &QAction::triggered, kec, &KifuExportController::copyUsiCurrentToClipboard, Qt::UniqueConnection);
    QObject::connect(ui->actionCopyUSIAll,     &QAction::triggered, kec, &KifuExportController::copyUsiToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyJKF,        &QAction::triggered, kec, &KifuExportController::copyJkfToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyUSEN,       &QAction::triggered, kec, &KifuExportController::copyUsenToClipboard,       Qt::UniqueConnection);

    // ============================================================
    // 局面コピー（編集メニュー）
    // ============================================================
    QObject::connect(ui->actionCopySFEN,       &QAction::triggered, kec, &KifuExportController::copySfenToClipboard,       Qt::UniqueConnection);
    QObject::connect(ui->actionCopyBOD,        &QAction::triggered, kec, &KifuExportController::copyBodToClipboard,        Qt::UniqueConnection);

    // ============================================================
    // 棋譜貼り付け（編集メニュー）
    // ============================================================
    QObject::connect(ui->actionPasteKifu,            &QAction::triggered, mw, &MainWindow::pasteKifuFromClipboard,         Qt::UniqueConnection);

    // ============================================================
    // 局面集ビューア（編集メニュー）
    // ============================================================
    QObject::connect(ui->actionSfenCollectionViewer, &QAction::triggered, mw, &MainWindow::displaySfenCollectionViewer,   Qt::UniqueConnection);

    // 言語設定: LanguageControllerで直接接続（MainWindow::ensureLanguageController参照）

    // ============================================================
    // ツールバー表示切替
    // ============================================================
    QObject::connect(ui->actionToolBar,          &QAction::toggled,   mw, &MainWindow::onToolBarVisibilityToggled,  Qt::UniqueConnection);
}
