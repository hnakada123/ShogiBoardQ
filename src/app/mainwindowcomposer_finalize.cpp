/// @file mainwindowcomposer_finalize.cpp
/// @brief 部品どうしのシグナル接続と起動時の初期表示

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "appsettings.h"
#include "boardinteractioncontroller.h"
#include "boardloadservice.h"
#include "boardsetupcontroller.h"
#include "branchnavigationwiring.h"
#include "commentcoordinator.h"
#include "csagamewiring.h"
#include "dialogcoordinatorwiring.h"
#include "dialoglaunchwiring.h"
#include "docklayoutmanager.h"
#include "engineanalysistab.h"
#include "errorbus.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "gamerecordmodel.h"
#include "gamerecordpresenter.h"
#include "gamesessionorchestrator.h"
#include "josekiwindowwiring.h"
#include "kifuexportcontroller.h"
#include "kifufilecontroller.h"
#include "kifunavigationcoordinator.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "piecesoundplayer.h"
#include "playerinfowiring.h"
#include "positioneditcoordinator.h"
#include "pvclickcontroller.h"
#include "recordpane.h"
#include "sessionlifecyclecoordinator.h"
#include "settingsresetcontroller.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "turnstatesyncservice.h"
#include "uiactionswiring.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#ifdef QT_DEBUG
#include "debugscreenshotwiring.h"
#endif

#include <QAction>
#include <QStatusBar>
#include <QTimer>
#include <functional>

void MainWindowComposer::connectSignals()
{
    MainWindowActions* actions = m_c.actions.get();

    // --- メニュー・ツールバーのアクション ---
    UiActionsWiring::Deps ad;
    ad.ui = &m_ui;
    ad.window = &m_window;
    ad.actions = actions;
    ad.dlw = m_c.dialogLaunchWiring;
    ad.dcw = m_c.dialogCoordinatorWiring;
    ad.kfc = m_c.kifuFileController;
    ad.gso = m_c.gameSessionOrchestrator;
    ad.appearance = m_c.appearanceController.get();
    ad.shogiView = m_c.shogiView;
    ad.evalChart = m_c.evalChart;
    ad.kec = m_c.kifuExportController.get();
    m_c.actionsWiring = new UiActionsWiring(ad, &m_window);
    m_c.actionsWiring->wire();

    // --- 盤・着手 ---
    // 成り・不成を選べる着手では、成りダイアログで対局者に選ばせる
    m_c.gameController->setPromotionChooser(std::bind(&DialogLaunchWiring::choosePromotion, m_c.dialogLaunchWiring));
    connect(m_c.gameController, &ShogiGameController::endDragSignal, m_c.shogiView, &ShogiView::endDrag);
    connect(m_c.gameController, &ShogiGameController::moveCommitted, actions, &MainWindowActions::handleMoveCommitted);
    connect(m_c.gameController, &ShogiGameController::moveCommitted,
            m_c.pieceSoundPlayer, &PieceSoundPlayer::playMoveSound);
    connect(m_c.boardController, &BoardInteractionController::moveRequested,
            actions, &MainWindowActions::handleMoveRequested);
    m_c.boardSetupController->connectBoardClicks();
    connect(m_c.shogiView, &ShogiView::fieldSizeChanged,
            m_c.appearanceController.get(), &MainWindowAppearanceController::onBoardSizeChanged);

    // --- 棋譜欄・分岐・評価値グラフ ---
    connect(m_c.recordPane, &RecordPane::mainRowChanged, actions, &MainWindowActions::onRecordPaneMainRowChanged);
    connect(m_c.gameRecord, &GameRecordModel::commentChanged,
            m_c.commentCoordinator, &CommentCoordinator::onGameRecordCommentChanged);
    connect(m_c.gameRecord, &GameRecordModel::bookmarkChanged,
            m_c.commentCoordinator, &CommentCoordinator::onBookmarkChanged);
    connect(m_c.recordPresenter, &GameRecordPresenter::currentRowChanged,
            m_c.commentCoordinator, &CommentCoordinator::handleRecordRowChangeRequest);
    connect(m_c.branchNavWiring.get(), &BranchNavigationWiring::boardWithHighlightsRequired,
            m_c.boardLoadService, &BoardLoadService::loadWithHighlights);
    connect(m_c.branchNavWiring.get(), &BranchNavigationWiring::boardSfenChanged,
            m_c.boardLoadService, &BoardLoadService::loadFromSfen);
    connect(m_c.branchNavWiring.get(), &BranchNavigationWiring::branchNodeHandled,
            m_c.kifuNavCoordinator.get(), &KifuNavigationCoordinator::handleBranchNodeHandled);
    // ツリー選択時の棋譜欄更新はシグナルを抑止するため、グラフへも明示的に通知する
    connect(m_c.branchNavWiring.get(), &BranchNavigationWiring::currentPlyChanged,
            m_c.evalGraphController.get(), &EvaluationGraphController::setCurrentPly);
    connect(m_c.evalChart, &EvaluationChartWidget::plyClicked,
            m_c.kifuNavCoordinator.get(), &KifuNavigationCoordinator::navigateToRow);

    // --- 局面編集 ---
    connect(m_c.posEditCoordinator, &PositionEditCoordinator::positionEditingStarted,
            actions, &MainWindowActions::resetRecordForEditedPosition);
    connect(m_c.posEditCoordinator, &PositionEditCoordinator::positionEditingStarted,
            m_c.sessionLifecycle, &SessionLifecycleCoordinator::clearEvalState);
    connect(m_c.posEditCoordinator, &PositionEditCoordinator::positionEditingFinished,
            actions, &MainWindowActions::resetRecordForEditedPosition);

    // --- 定跡ウィンドウ ---
    connect(m_c.josekiWiring.get(), &JosekiWindowWiring::moveRequested,
            actions, &MainWindowActions::handleMoveRequested);
    connect(m_c.josekiWiring.get(), &JosekiWindowWiring::forcedPromotionRequested,
            m_c.gameController, &ShogiGameController::setForcedPromotion);

    // --- その他 ---
    connect(m_c.playerInfoWiring, &PlayerInfoWiring::tabCurrentChanged,
            m_c.appearanceController.get(), &MainWindowAppearanceController::onTabCurrentChanged);
    connect(m_c.pvClickController, &PvClickController::pvDialogClosed,
            m_c.analysisTab, &EngineAnalysisTab::clearThinkingViewSelection);
    connect(m_c.dialogLaunchWiring, &DialogLaunchWiring::sfenCollectionPositionSelected,
            m_c.kifuFileController, &KifuFileController::onSfenCollectionPositionSelected);
    connect(m_c.kifuExportController.get(), &KifuExportController::statusMessage,
            m_ui.statusbar, &QStatusBar::showMessage);
    connect(m_c.csaGameWiring.get(), &CsaGameWiring::moveAppliedToBoard,
            m_c.pieceSoundPlayer, &PieceSoundPlayer::playMoveSound);
    connect(m_ui.actionPieceSound, &QAction::toggled, m_c.pieceSoundPlayer, &PieceSoundPlayer::setEnabled);
    connect(&ErrorBus::instance(), &ErrorBus::messagePosted,
            m_c.notificationService, &UiNotificationService::displayMessage);

    auto* settingsReset = new SettingsResetController(&m_window);
    connect(m_ui.actionResetSettings, &QAction::triggered, settingsReset, &SettingsResetController::confirmAndQuit);

    // 評価値グラフの高さ調整（デバウンス）
    m_c.evalChartResizeTimer = std::make_unique<QTimer>();
    m_c.evalChartResizeTimer->setSingleShot(true);
    connect(m_c.evalChartResizeTimer.get(), &QTimer::timeout,
            m_c.appearanceController.get(), &MainWindowAppearanceController::performDeferredEvalChartResize);

#ifdef QT_DEBUG
    // デバッグ用スクリーンショット（F12 で /tmp/shogiboardq-debug/ に PNG 保存）
    m_c.debugScreenshotWiring = std::make_unique<DebugScreenshotWiring>(&m_window);
#endif
}

void MainWindowComposer::finalizeUi()
{
    // 盤・駒台・手番と対局者名の初期表示（対局は始めない。対局者名は空にして対局情報の初期値を出す）
    m_c.shogiView->applyBoardAndRender(m_c.gameController->board());
    m_c.playerInfoWiring->applyPlayersNamesForMode();
    m_c.playerInfoWiring->applyEngineNamesToLogModels();
    m_c.playerInfoWiring->applySecondEngineVisibility();
    m_c.playerInfoWiring->onSetPlayersNames(QString(), QString());
    m_c.playerInfoWiring->onSetEngineNames(QString(), QString());
    m_c.turnStateSync->setCurrentTurn();

    // ウィンドウの大きさと、起動時のドックのレイアウト
    AppSettings::loadWindowSize(&m_window);
    m_c.dockLayoutManager->restoreStartupLayoutIfSet();

    m_c.appearanceController->setupNameAndClockFonts();
    m_c.appearanceController->installAppToolTips(&m_window);

    m_c.uiStatePolicy->applyState(UiStatePolicyManager::AppState::Idle);
}
