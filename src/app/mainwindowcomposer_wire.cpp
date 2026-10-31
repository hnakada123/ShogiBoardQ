/// @file mainwindowcomposer_wire.cpp
/// @brief 部品どうしのシグナル接続（部品は全部できている）

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "analysissession.h"
#include "branchtreeeditcontroller.h"
#include "branchtreeview.h"
#include "candidatearrowpresenter.h"
#include "commentcoordinator.h"
#include "commenteditorpanel.h"
#include "consecutivegameseries.h"
#include "considerationpanel.h"
#include "considerationuicontroller.h"
#include "csagamecoordinator.h"
#include "csalogpanel.h"
#include "dialoglauncher.h"
#include "errorbus.h"
#include "docklayoutmanager.h"
#include "evaluationchartwidget.h"
#include "enginedisplaypresenter.h"
#include "evaluationgraphpresenter.h"
#include "gameinfopane.h"
#include "gamesession.h"
#include "boardinteractioncontroller.h"
#include "boardmovecontroller.h"
#include "analysisresultspanel.h"
#include "josekiregistrationcontroller.h"
#include "josekiwindowcontroller.h"
#include "kifudisplaypresenter.h"
#include "kifudocument.h"
#include "kifufilecontroller.h"
#include "kifuloader.h"
#include "kifunavigationcontroller.h"
#include "kifurecordlistmodel.h"
#include "mainwindowactions.h"
#include "matchcoordinator.h"
#include "menuactionswiring.h"
#include "piecesoundplayer.h"
#include "playerinfopresenter.h"
#include "positioneditsession.h"
#include "pvclickcontroller.h"
#include "recordpane.h"
#include "gameboard.h"
#include "shogiview.h"
#include "thinkingpanel.h"
#include "timedisplaypresenter.h"
#include "notificationpresenter.h"
#include "uistatepolicymanager.h"
#include "usicommandcontroller.h"
#include "usilogpanel.h"

#include <QPushButton>
#include <QStatusBar>

// ---------------------------------------------------------------------------
// 棋譜（移動・分岐・コメント・読み込み）
// ---------------------------------------------------------------------------

void MainWindowComposer::connectRecord()
{
    KifuNavigationController* nav = m_c.kifuNavController;
    RecordPane* pane = m_c.recordPane;
    EvaluationChartWidget* chart = m_c.evalChart;

    // 棋譜を移る操作（棋譜欄と評価値グラフのボタン・行・分岐候補・分岐ツリー・グラフの点）
    // 局面送りのボタンは2組とも同じ QAction に結び付ける（有効・無効も QAction に従う）
    nav->bindButtons(pane->stepButtons());
    nav->bindButtons(chart->stepButtons());
    connect(pane, &RecordPane::mainRowChanged, nav, &KifuNavigationController::selectRecordRow);
    connect(pane, &RecordPane::branchRowActivated, nav, &KifuNavigationController::selectBranchCandidateRow);
    BranchTreeView* tree = m_c.branchTreeView;
    connect(tree, &BranchTreeView::nodeActivated, nav, &KifuNavigationController::selectBranchTreeNode);
    connect(chart, &EvaluationChartWidget::plyClicked, nav, &KifuNavigationController::selectPly);
    // 棋譜解析の点は、解析した手順の手へ移る（分岐を足しても手順の末尾の手で探す）
    connect(chart, &EvaluationChartWidget::analysisPlyClicked, nav, &KifuNavigationController::goToPlyOnLine);
    // 移るのをやめたら、クリックした行の強調を表示中の手に戻す（選択の変更の処理を抜けてから）
    connect(nav, &KifuNavigationController::navigationRejected,
            m_c.kifuDisplay, &KifuDisplayPresenter::refresh, Qt::QueuedConnection);

    // 分岐の編集（分岐ツリーと分岐候補欄の右クリック）
    connect(tree, &BranchTreeView::nodeContextMenuAboutToShow,
            m_c.branchEdit, &BranchTreeEditController::onNodeContextMenuAboutToShow);
    connect(pane, &RecordPane::branchContextMenuRequested,
            m_c.branchEdit, &BranchTreeEditController::onCandidateContextMenuRequested);

    // コメント・しおり（棋譜欄の行は書いたノードの行だけ描き直す）
    connect(m_c.commentEditor, &CommentEditorPanel::commentUpdated,
            m_c.commentCoordinator, &CommentCoordinator::onCommentUpdated);
    connect(pane, &RecordPane::bookmarkEditRequested,
            m_c.commentCoordinator, &CommentCoordinator::onBookmarkEditRequested);
    KifuDocument* document = m_c.document;
    connect(document, &KifuDocument::commentChanged, m_c.kifuRecord, &KifuRecordListModel::refreshNode);
    connect(document, &KifuDocument::commentChanged, tree, &BranchTreeView::refreshNode);
    connect(document, &KifuDocument::bookmarkChanged, m_c.kifuRecord, &KifuRecordListModel::refreshNode);

    // 対局情報（対局情報欄は文書の対局情報を編集する画面）
    GameInfoPane* gameInfo = m_c.gameInfoPane;
    // 入力中の編集（対局情報欄のセル・コメント欄の本文）は、保存・書き出しの前に文書へ確定し、未保存に数える
    document->addPendingEditor({std::bind(&GameInfoPane::commitPendingEditor, m_c.gameInfoPane),
                                std::bind(&GameInfoPane::hasPendingEditor, m_c.gameInfoPane)});
    document->addPendingEditor({std::bind(&CommentCoordinator::commitPendingComment, m_c.commentCoordinator),
                                std::bind(&CommentCoordinator::hasPendingComment, m_c.commentCoordinator)});
    // 文書を置き換えたら、ツリーから作られない表示（評価値グラフ・エンジンの表示・解析結果）を消す
    connect(document, &KifuDocument::replaced, m_c.evalGraph, &EvaluationGraphPresenter::clear);
    connect(document, &KifuDocument::replaced, m_c.engineDisplay, &EngineDisplayPresenter::clearAll);
    connect(document, &KifuDocument::headerReset, gameInfo, &GameInfoPane::setGameInfo);
    connect(document, &KifuDocument::headerValueSet, gameInfo, &GameInfoPane::updateGameInfoValue);
    connect(gameInfo, &GameInfoPane::gameInfoEdited, document, &KifuDocument::editHeader);
    // 起動時の対局情報を表に出す（対局を始める前の案内つき）
    gameInfo->setGameInfo(document->header(), true);

    // 読み込み・書き出し
    KifuLoader* loader = m_c.kifuLoader;
    connect(loader, &KifuLoader::errorOccurred,
            m_c.notifications, &NotificationPresenter::displayErrorMessage);
    connect(loader, &KifuLoader::warningOccurred,
            m_c.notifications, &NotificationPresenter::displayWarningMessage);
    connect(m_c.dialogLauncher, &DialogLauncher::engineListChanged,
            m_c.considerationTab, &ConsiderationPanel::reloadEngines);
    connect(m_c.evalChart, &EvaluationChartWidget::settingsRequested,
            m_c.dialogLauncher, &DialogLauncher::displayEvaluationChartSettings);
    connect(m_c.kifuFileController, &KifuFileController::statusMessage, m_ui.statusbar, &QStatusBar::showMessage);
}

// ---------------------------------------------------------------------------
// 盤（クリック・着手・局面編集・定跡手）
// ---------------------------------------------------------------------------

void MainWindowComposer::connectBoard()
{
    BoardInteractionController* board = m_c.boardController;
    connect(m_c.shogiView, &ShogiView::clicked, board, &BoardInteractionController::onLeftClick);
    connect(m_c.shogiView, &ShogiView::rightClicked, board, &BoardInteractionController::onRightClick);
    connect(m_c.gameBoard, &GameBoard::moveCommitted,
            m_c.pieceSoundPlayer, &PieceSoundPlayer::playMoveSound);

    // 局面編集（棋譜は編集した局面だけになる）
    PositionEditSession* edit = m_c.positionEdit;
    connect(edit, &PositionEditSession::editingStarted, m_c.actions, &MainWindowActions::resetRecordForEditedPosition);
    connect(edit, &PositionEditSession::editingFinished, m_c.actions, &MainWindowActions::resetRecordForEditedPosition);
}

// ---------------------------------------------------------------------------
// 対局（時計・評価値・終局・連続対局・通信対局）
// ---------------------------------------------------------------------------

void MainWindowComposer::connectGame()
{
    MatchCoordinator* match = m_c.match;
    GameSession* session = m_c.gameSession;

    connect(match, &MatchCoordinator::engineMovePlayed, m_c.evalGraph, &EvaluationGraphPresenter::plotEngineScore);
    connect(match, &MatchCoordinator::movesUndone, m_c.evalGraph, &EvaluationGraphPresenter::trimToPly);
    connect(match, &MatchCoordinator::resumeAvailabilityChanged,
            m_c.uiStatePolicy, &UiStatePolicyManager::refreshResumeAvailability);

    connect(match, &MatchCoordinator::gameEnded, session, &GameSession::onGameEnded);
    connect(match, &MatchCoordinator::gameOverMessage, m_c.notifications, &NotificationPresenter::showInformation);
    connect(match, &MatchCoordinator::gameEndProcessed, session, &GameSession::onGameEndProcessed);
    connect(match, &MatchCoordinator::gameInterrupted, session, &GameSession::onGameInterrupted);

    connect(m_c.consecutiveGames, &ConsecutiveGameSeries::nextGameReady,
            session, &GameSession::startNextGame);

    // 通信対局（棋譜と動作状態は司令塔が受け手と動作状態に書く。開始・終局の画面は CsaGameSession）。
    // 盤で選んだ手はサーバーへ送り、着手のハイライトはローカル対局と同じく棋譜の表示中の手から出る
    CsaGameCoordinator* csa = m_c.csaGame;
    connect(csa, &CsaGameCoordinator::engineMovePlayed, m_c.evalGraph, &EvaluationGraphPresenter::plotEngineScore);
    connect(csa, &CsaGameCoordinator::errorOccurred, m_c.notifications, &NotificationPresenter::displayErrorMessage);
    connect(m_c.boardMoves, &BoardMoveController::csaMoveRequested, csa, &CsaGameCoordinator::onHumanMove);
    CsaLogPanel* csaLog = m_c.csaLog;
    connect(csa, &CsaGameCoordinator::connectionStateChanged, csaLog, &CsaLogPanel::setConnected);
    connect(csa, &CsaGameCoordinator::csaCommLogAppended, csaLog, &CsaLogPanel::append);
    connect(csaLog, &CsaLogPanel::csaRawCommandRequested, csa, &CsaGameCoordinator::sendRawCommand);
}

// ---------------------------------------------------------------------------
// 検討・思考・通信ログのドック・メニュー・通知
// ---------------------------------------------------------------------------

void MainWindowComposer::connectAnalysisAndUi()
{
    ConsiderationPanel* consideration = m_c.considerationTab;
    ThinkingPanel* thinking = m_c.thinkingPanel;
    AnalysisSession* analysis = m_c.analysisSession;

    // 検討タブの操作は ConsiderationUiController、表示は EngineDisplayPresenter が自分でつなぐ
    connect(analysis, &AnalysisSession::messageRequested,
            m_c.notifications, &NotificationPresenter::showInformation);

    // 候補手の矢印の表示状態（検討中か・先読みの説明）を思考ドックの設定欄に出す
    connect(m_c.arrows, &CandidateArrowPresenter::displayStateChanged, thinking, &ThinkingPanel::setArrowDisplayState);
    // 盤で指した手と対局で指された手（定跡ウィンドウの「盤の手を登録」がオンなら定跡に登録する）
    JosekiRegistrationController* registration = m_c.josekiRegistration;
    connect(m_c.branchEdit, &BranchTreeEditController::boardMoveRecorded,
            registration, &JosekiRegistrationController::recordBoardMove);
    connect(m_c.gameSession, &GameSession::gameMoveRecorded, registration, &JosekiRegistrationController::recordBoardMove);
    // 思考の表・検討の表・棋譜解析の結果の表の右クリックに「定跡に登録」を足す
    connect(thinking, &ThinkingPanel::rowContextMenuAboutToShow,
            registration, &JosekiRegistrationController::onThinkingRowContextMenu);
    connect(consideration, &ConsiderationPanel::rowContextMenuAboutToShow,
            registration, &JosekiRegistrationController::onThinkingRowContextMenu);
    connect(m_c.analysisResults, &AnalysisResultsPanel::rowContextMenuAboutToShow,
            registration, &JosekiRegistrationController::onAnalysisRowContextMenu);

    // 読み筋のクリック（思考の表と検討の表）。読み筋ダイアログを閉じたら、その表の選択を外す
    PvClickController* pv = m_c.pvClickController;
    connect(thinking, &ThinkingPanel::pvRowClicked, pv, &PvClickController::onPvRowClicked);
    connect(consideration, &ConsiderationPanel::pvRowClicked, pv, &PvClickController::onPvRowClicked);
    connect(pv, &PvClickController::pvDialogClosed, thinking, &ThinkingPanel::clearSelection);
    connect(pv, &PvClickController::pvDialogClosed, consideration, &ConsiderationPanel::clearViewSelection);

    // USI コマンドの手入力
    connect(m_c.usiLog, &UsiLogPanel::usiCommandRequested, m_c.usiCommandController, &UsiCommandController::sendCommand);

    // メニュー・ツールバー
    MenuActionsWiring::Deps md;
    md.ui = &m_ui;
    md.actions = m_c.actions;
    md.dialogs = m_c.dialogLauncher;
    md.kifuFiles = m_c.kifuFileController;
    md.session = m_c.gameSession;
    md.csaSession = m_c.csaSession;
    md.menuWindow = m_c.menuWindowController;
    md.analysis = analysis;
    md.positionEdit = m_c.positionEdit;
    md.kifuAnalysis = m_c.kifuAnalysis;
    md.tsumeSearch = m_c.tsumeSearch;
    md.appearance = m_c.appearance;
    md.images = m_c.images;
    md.boardController = m_c.boardController;
    md.pieceSound = m_c.pieceSoundPlayer;
    md.settingsReset = m_c.settingsReset;
    md.dockLayout = m_c.dockLayoutManager;
    md.language = m_c.languageController;
    MenuActionsWiring::wire(md);
    // 保存したレイアウトの一覧の項目は、作る DockLayoutManager がつなぐ
    m_c.dockLayoutManager->setSavedLayoutsMenu(m_ui.menuSavedLayouts);

    connect(&ErrorBus::instance(), &ErrorBus::messagePosted,
            m_c.notifications, &NotificationPresenter::displayMessage);
}
