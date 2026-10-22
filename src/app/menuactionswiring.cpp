/// @file menuactionswiring.cpp
/// @brief メニュー・ツールバーの項目を、処理する部品へつなぐ

#include "menuactionswiring.h"
#include "ui_mainwindow.h"

#include "analysissession.h"
#include "boardinteractioncontroller.h"
#include "candidatearrowpresenter.h"
#include "dialoglauncher.h"
#include "gamesession.h"
#include "imageexportcontroller.h"
#include "kifuanalysiscontroller.h"
#include "kifufilecontroller.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "piecesoundplayer.h"
#include "positioneditsession.h"
#include "settingsresetcontroller.h"
#include "tsumesearchcontroller.h"

#include <QApplication>

namespace MenuActionsWiring {

namespace {

void wireFileMenu(const Deps& d)
{
    Ui::MainWindow* ui = d.ui;
    QObject::connect(ui->actionNewGame, &QAction::triggered, d.actions, &MainWindowActions::resetToInitialState);
    QObject::connect(ui->actionOpenKifuFile, &QAction::triggered, d.kifuFiles, &KifuFileController::chooseAndLoadKifuFile);
    QObject::connect(ui->actionSave, &QAction::triggered, d.kifuFiles, &KifuFileController::overwriteKifuFile);
    QObject::connect(ui->actionSaveAs, &QAction::triggered, d.kifuFiles, &KifuFileController::saveKifuToFile);
    QObject::connect(ui->actionSaveBoardImage, &QAction::triggered, d.images, &ImageExportController::saveBoardImage);
    QObject::connect(ui->actionSaveEvaluationGraph, &QAction::triggered, d.images, &ImageExportController::saveEvalGraphImage);
    QObject::connect(ui->actionQuit, &QAction::triggered, d.actions, &MainWindowActions::requestQuit);
}

void wireEditMenu(const Deps& d)
{
    Ui::MainWindow* ui = d.ui;
    KifuFileController* kifu = d.kifuFiles;
    QObject::connect(ui->actionCopyKIF, &QAction::triggered, kifu, &KifuFileController::copyKifToClipboard);
    QObject::connect(ui->actionCopyKI2, &QAction::triggered, kifu, &KifuFileController::copyKi2ToClipboard);
    QObject::connect(ui->actionCopyCSA, &QAction::triggered, kifu, &KifuFileController::copyCsaToClipboard);
    QObject::connect(ui->actionCopyUSICurrent, &QAction::triggered, kifu, &KifuFileController::copyUsiCurrentToClipboard);
    QObject::connect(ui->actionCopyUSIAll, &QAction::triggered, kifu, &KifuFileController::copyUsiToClipboard);
    QObject::connect(ui->actionCopyJKF, &QAction::triggered, kifu, &KifuFileController::copyJkfToClipboard);
    QObject::connect(ui->actionCopyUSEN, &QAction::triggered, kifu, &KifuFileController::copyUsenToClipboard);
    QObject::connect(ui->actionCopySFEN, &QAction::triggered, kifu, &KifuFileController::copySfenToClipboard);
    QObject::connect(ui->actionCopyBOD, &QAction::triggered, kifu, &KifuFileController::copyBodToClipboard);
    QObject::connect(ui->actionPasteKifu, &QAction::triggered, d.kifuFiles, &KifuFileController::pasteKifuFromClipboard);
    QObject::connect(ui->actionSfenCollectionViewer, &QAction::triggered, d.dialogs, &DialogLauncher::displaySfenCollectionViewer);
    QObject::connect(ui->actionCopyBoardToClipboard, &QAction::triggered, d.images, &ImageExportController::copyBoardToClipboard);
    QObject::connect(ui->actionCopyEvalGraphToClipboard, &QAction::triggered, d.images, &ImageExportController::copyEvalGraphToClipboard);
    QObject::connect(ui->actionStartEditPosition, &QAction::triggered, d.actions, &MainWindowActions::beginPositionEditing);
    QObject::connect(ui->actionEndEditPosition, &QAction::triggered, d.positionEdit, &PositionEditSession::finish);
}

void wireGameMenu(const Deps& d)
{
    Ui::MainWindow* ui = d.ui;
    GameSession* session = d.session;
    DialogLauncher* dialogs = d.dialogs;
    QObject::connect(ui->actionStartGame, &QAction::triggered, session, &GameSession::startGameFromDialog);
    QObject::connect(ui->actionCSA, &QAction::triggered, dialogs, &DialogLauncher::displayCsaGameDialog);
    QObject::connect(ui->actionResign, &QAction::triggered, session, &GameSession::resign);
    QObject::connect(ui->actionUndoMove, &QAction::triggered, session, &GameSession::undoTwoMoves);
    QObject::connect(ui->actionMakeImmediateMove, &QAction::triggered, session, &GameSession::makeEngineMoveNow);
    QObject::connect(ui->actionBreakOffGame, &QAction::triggered, session, &GameSession::breakOff);
    QObject::connect(ui->actionResumeGame, &QAction::triggered, session, &GameSession::resumeInterruptedGame);
    QObject::connect(ui->actionTsumePlay, &QAction::triggered, dialogs, &DialogLauncher::displayTsumePlayDialog);
    QObject::connect(ui->actionAnalyzeKifu, &QAction::triggered, d.kifuAnalysis, &KifuAnalysisController::showDialogAndStart);
    QObject::connect(ui->actionCancelAnalyzeKifu, &QAction::triggered, d.kifuAnalysis, &KifuAnalysisController::stop);
    QObject::connect(ui->actionTsumeShogiSearch, &QAction::triggered, d.tsumeSearch, &TsumeSearchController::showDialogAndStart);
    QObject::connect(ui->actionStopTsumeSearch, &QAction::triggered, d.analysis, &AnalysisSession::stop);
    QObject::connect(ui->actionTsumeshogiGenerator, &QAction::triggered, dialogs, &DialogLauncher::displayTsumeshogiGeneratorDialog);
    QObject::connect(ui->actionJishogiScore, &QAction::triggered, dialogs, &DialogLauncher::displayJishogiScoreDialog);
    QObject::connect(ui->actionNyugyokuDeclaration, &QAction::triggered, d.session, &GameSession::declareNyugyoku);
}

void wireDisplayMenu(const Deps& d)
{
    Ui::MainWindow* ui = d.ui;
    MainWindowAppearanceController* appearance = d.appearance;
    QObject::connect(ui->actionFlipBoard, &QAction::triggered, appearance, &MainWindowAppearanceController::flipBoard);
    QObject::connect(ui->actionEnlargeBoard, &QAction::triggered, appearance, &MainWindowAppearanceController::onActionEnlargeBoardTriggered);
    QObject::connect(ui->actionShrinkBoard, &QAction::triggered, appearance, &MainWindowAppearanceController::onActionShrinkBoardTriggered);
    QObject::connect(ui->actionBoardAppearance, &QAction::triggered, d.dialogs, &DialogLauncher::displayBoardAppearanceDialog);
    QObject::connect(ui->actionLegalMoves, &QAction::toggled, d.boardController, &BoardInteractionController::setLegalMovesVisible);
    QObject::connect(ui->actionShowJosekiArrows, &QAction::toggled, d.arrows, &CandidateArrowPresenter::setJosekiArrowsVisible);
    QObject::connect(ui->actionFontSettings, &QAction::triggered, d.dialogs, &DialogLauncher::displayFontSettingsDialog);
    QObject::connect(ui->actionToolBar, &QAction::toggled, appearance, &MainWindowAppearanceController::onToolBarVisibilityToggled);
    // ツールバーだけにある項目
    QObject::connect(ui->actionMenuWindow, &QAction::triggered, d.dialogs, &DialogLauncher::displayMenuWindow);
}

void wireSettingsMenu(const Deps& d)
{
    Ui::MainWindow* ui = d.ui;
    QObject::connect(ui->actionEngineSettings, &QAction::triggered, d.dialogs, &DialogLauncher::displayEngineSettingsDialog);
    QObject::connect(ui->actionPieceSound, &QAction::toggled, d.pieceSound, &PieceSoundPlayer::setEnabled);
    QObject::connect(ui->actionPieceSoundSettings, &QAction::triggered, d.dialogs, &DialogLauncher::displayPieceSoundSettingsDialog);
    QObject::connect(ui->actionResetSettings, &QAction::triggered, d.settingsReset, &SettingsResetController::confirmAndQuit);
}

void wireHelpMenu(const Deps& d)
{
    Ui::MainWindow* ui = d.ui;
    QObject::connect(ui->actionVersionInfo, &QAction::triggered, d.dialogs, &DialogLauncher::displayVersionInformation);
    QObject::connect(ui->actionOpenWebsite, &QAction::triggered, d.dialogs, &DialogLauncher::openProjectWebsite);
    QObject::connect(ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
}

} // namespace

void wire(const Deps& d)
{
    Q_ASSERT(d.ui && d.actions && d.dialogs && d.kifuFiles && d.session && d.analysis
             && d.positionEdit && d.kifuAnalysis && d.tsumeSearch && d.appearance && d.images
             && d.boardController && d.pieceSound && d.settingsReset);
    wireFileMenu(d);
    wireEditMenu(d);
    wireDisplayMenu(d);
    wireGameMenu(d);
    wireSettingsMenu(d);
    wireHelpMenu(d);
}

} // namespace MenuActionsWiring
