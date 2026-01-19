#include "uiactionswiring.h"
#include "ui_mainwindow.h"        // Ui::MainWindow
#include "shogiview.h"
#include "mainwindow.h"           // MainWindow のスロットを型安全に結ぶため
#include <QApplication>

void UiActionsWiring::wire()
{
    auto* ui = m_d.ui;
    auto* mw = qobject_cast<MainWindow*>(m_d.ctx);
    Q_ASSERT(ui && mw);

    // ファイル/アプリ
    QObject::connect(ui->actionQuit,        &QAction::triggered, mw, &MainWindow::saveSettingsAndClose,      Qt::UniqueConnection);
    QObject::connect(ui->actionSaveAs,      &QAction::triggered, mw, &MainWindow::saveKifuToFile,            Qt::UniqueConnection);
    QObject::connect(ui->actionSave,        &QAction::triggered, mw, &MainWindow::overwriteKifuFile,         Qt::UniqueConnection);
    QObject::connect(ui->actionOpenKifuFile,&QAction::triggered, mw, &MainWindow::chooseAndLoadKifuFile,     Qt::UniqueConnection);
    QObject::connect(ui->actionVersionInfo, &QAction::triggered, mw, &MainWindow::displayVersionInformation, Qt::UniqueConnection);
    QObject::connect(ui->actionOpenWebsite, &QAction::triggered, mw, &MainWindow::openWebsiteInExternalBrowser, Qt::UniqueConnection);
    QObject::connect(ui->actionAboutQt,     &QAction::triggered, mw, []() { QApplication::aboutQt(); });

    // 対局
    QObject::connect(ui->actionNewGame,     &QAction::triggered, mw, &MainWindow::resetToInitialState, Qt::UniqueConnection);
    QObject::connect(ui->actionStartGame,   &QAction::triggered, mw, &MainWindow::initializeGame,      Qt::UniqueConnection);
    QObject::connect(ui->actionCSA,        &QAction::triggered, mw, &MainWindow::displayCsaGameDialog, Qt::UniqueConnection);
    QObject::connect(ui->actionResign,      &QAction::triggered, mw, &MainWindow::handleResignation,   Qt::UniqueConnection);
    QObject::connect(ui->breakOffGame,      &QAction::triggered, mw, &MainWindow::handleBreakOffGame,  Qt::UniqueConnection);

    // 盤操作・表示
    QObject::connect(ui->actionFlipBoard,            &QAction::triggered, mw,           &MainWindow::onActionFlipBoardTriggered, Qt::UniqueConnection);
    QObject::connect(ui->actionCopyBoardToClipboard, &QAction::triggered, mw,           &MainWindow::copyBoardToClipboard,       Qt::UniqueConnection);
    QObject::connect(ui->actionMakeImmediateMove,    &QAction::triggered, mw,           &MainWindow::movePieceImmediately,       Qt::UniqueConnection);
    QObject::connect(ui->actionStandardMove,         &QAction::triggered, mw,           &MainWindow::displayJosekiWindow,        Qt::UniqueConnection);
    QObject::connect(ui->actionMenu,                 &QAction::triggered, mw,           &MainWindow::displayMenuWindow,          Qt::UniqueConnection);
    if (m_d.shogiView) {
        QObject::connect(ui->actionEnlargeBoard,     &QAction::triggered, m_d.shogiView, &ShogiView::enlargeBoard, Qt::UniqueConnection);
        QObject::connect(ui->actionShrinkBoard,      &QAction::triggered, m_d.shogiView, &ShogiView::reduceBoard,  Qt::UniqueConnection);
    }
    QObject::connect(ui->actionUndoMove,             &QAction::triggered, mw, &MainWindow::undoLastTwoMoves,   Qt::UniqueConnection);
    QObject::connect(ui->actionSaveBoardImage,       &QAction::triggered, mw, &MainWindow::saveShogiBoardImage, Qt::UniqueConnection);

    // 解析/検討/詰み・エンジン設定
    QObject::connect(ui->actionEngineSettings,       &QAction::triggered, mw, &MainWindow::displayEngineSettingsDialog,    Qt::UniqueConnection);
    QObject::connect(ui->actionConsideration,        &QAction::triggered, mw, &MainWindow::displayConsiderationDialog,     Qt::UniqueConnection);
    QObject::connect(ui->actionAnalyzeKifu,          &QAction::triggered, mw, &MainWindow::displayKifuAnalysisDialog,      Qt::UniqueConnection);
    QObject::connect(ui->actionCacelAnalyzeKifu,     &QAction::triggered, mw, &MainWindow::cancelKifuAnalysis,             Qt::UniqueConnection);
    QObject::connect(ui->actionStartEditPosition,    &QAction::triggered, mw, &MainWindow::beginPositionEditing,           Qt::UniqueConnection);
    QObject::connect(ui->actionEndEditPosition,      &QAction::triggered, mw, &MainWindow::finishPositionEditing,          Qt::UniqueConnection);
    QObject::connect(ui->actionTsumeShogiSearch,     &QAction::triggered, mw, &MainWindow::displayTsumeShogiSearchDialog,  Qt::UniqueConnection);
    QObject::connect(ui->actionJishogiScore,        &QAction::triggered, mw, &MainWindow::displayJishogiScoreDialog,      Qt::UniqueConnection);
    QObject::connect(ui->actionNyugyokuDeclaration, &QAction::triggered, mw, &MainWindow::handleNyugyokuDeclaration,      Qt::UniqueConnection);

    // 棋譜コピー (編集メニュー)
    QObject::connect(ui->kifFormat,                  &QAction::triggered, mw, &MainWindow::copyKifToClipboard,             Qt::UniqueConnection);
    QObject::connect(ui->ki2Format,                  &QAction::triggered, mw, &MainWindow::copyKi2ToClipboard,             Qt::UniqueConnection);
    QObject::connect(ui->csaFormat,                  &QAction::triggered, mw, &MainWindow::copyCsaToClipboard,             Qt::UniqueConnection);  // CSA形式
    QObject::connect(ui->universal,                  &QAction::triggered, mw, &MainWindow::copyUsiCurrentToClipboard,      Qt::UniqueConnection);  // USI形式（現在の指し手まで）
    QObject::connect(ui->actionUSI,                  &QAction::triggered, mw, &MainWindow::copyUsiToClipboard,             Qt::UniqueConnection);  // USI形式（全て）
    QObject::connect(ui->actionJSON,                 &QAction::triggered, mw, &MainWindow::copyJkfToClipboard,             Qt::UniqueConnection);  // JKF形式
    QObject::connect(ui->actionUSEN,                 &QAction::triggered, mw, &MainWindow::copyUsenToClipboard,            Qt::UniqueConnection);  // USEN形式

    // 局面コピー (編集メニュー)
    QObject::connect(ui->Sfen,                       &QAction::triggered, mw, &MainWindow::copySfenToClipboard,            Qt::UniqueConnection);  // SFEN形式
    QObject::connect(ui->BOD,                        &QAction::triggered, mw, &MainWindow::copyBodToClipboard,             Qt::UniqueConnection);  // BOD形式

    // 棋譜貼り付け (編集メニュー)
    QObject::connect(ui->pasteGameRecord,            &QAction::triggered, mw, &MainWindow::pasteKifuFromClipboard,         Qt::UniqueConnection);

    // 言語設定
    QObject::connect(ui->actionLanguageSystem,   &QAction::triggered, mw, &MainWindow::onLanguageSystemTriggered,   Qt::UniqueConnection);
    QObject::connect(ui->actionLanguageJapanese, &QAction::triggered, mw, &MainWindow::onLanguageJapaneseTriggered, Qt::UniqueConnection);
    QObject::connect(ui->actionLanguageEnglish,  &QAction::triggered, mw, &MainWindow::onLanguageEnglishTriggered,  Qt::UniqueConnection);
}
