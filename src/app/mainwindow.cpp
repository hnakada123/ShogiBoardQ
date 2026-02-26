/// @file mainwindow.cpp
/// @brief アプリケーションのメインウィンドウ実装

#include <QMessageBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QMetaType>
#include <QSettings>
#include <QSignalBlocker>
#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>
#include <QFontDatabase>

#include "mainwindow.h"
#include "mainwindowgamestartservice.h"
#include "mainwindowresetservice.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "boardinteractioncontroller.h"
#include "shogiutils.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"
#include "ui_mainwindow.h"
#include "shogiclock.h"
#include "apptooltipfilter.h"
#include "boardimageexporter.h"
#include "engineinfowidget.h"
#include "engineanalysistab.h"
#include "matchcoordinator.h"
#include "turnmanager.h"
#include "errorbus.h"
#include "kifuloadcoordinator.h"
#include "evaluationchartwidget.h"
#include "positioneditcontroller.h"
#include "analysisresultspresenter.h"
#include "boardsyncpresenter.h"
#include "boardloadservice.h"
#include "considerationpositionservice.h"
#include "gamestartcoordinator.h"
#include "gamerecordpresenter.h"
#include "sfenutils.h"
#include "turnsyncbridge.h"
#include "settingsservice.h"
#include "analysistabwiring.h"
#include "recordpanewiring.h"
#include "recordpane.h"
#include "uiactionswiring.h"
#include "gamerecordmodel.h"
#include "gameinfopanecontroller.h"
#include "evaluationgraphcontroller.h"
#include "timecontrolcontroller.h"
#include "replaycontroller.h"
#include "dialogcoordinator.h"
#include "kifuexportcontroller.h"
#include "kifufilecontroller.h"
#include "gamestatecontroller.h"
#include "playerinfocontroller.h"
#include "boardsetupcontroller.h"
#include "pvclickcontroller.h"
#include "positioneditcoordinator.h"
#include "csagamecoordinator.h"
#include "josekiwindowwiring.h"
#include "menuwindowwiring.h"
#include "csagamewiring.h"
#include "playerinfowiring.h"
#include "considerationwiring.h"
#include "dialoglaunchwiring.h"
#include "dialogcoordinatorwiring.h"
#include "matchcoordinatorwiring.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowcompositionroot.h"
#include "livegamesessionupdater.h"
#include "gamerecordloadservice.h"
#include "gamerecordupdateservice.h"
#include "kifunavigationcoordinator.h"
#include "branchnavigationwiring.h"
#include "mainwindowuibootstrapper.h"
#include "mainwindowruntimerefsfactory.h"
#include "mainwindowwiringassembler.h"
#include "kifuloadcoordinatorfactory.h"
#include "kifuexportdepsassembler.h"
#include "gamerecordmodelbuilder.h"

#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifudisplaycoordinator.h"
#include "branchtreewidget.h"
#include "livegamesession.h"
#include "prestartcleanuphandler.h"
#include "jishogiscoredialogcontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "consecutivegamescontroller.h"
#include "languagecontroller.h"
#include "docklayoutmanager.h"
#include "dockcreationservice.h"
#include "commentcoordinator.h"
#include "usicommandcontroller.h"
#include "recordnavigationhandler.h"
#include "recordnavigationwiring.h"
#include "uistatepolicymanager.h"
#include "playmodepolicyservice.h"
#include "turnstatesyncservice.h"
#include "undoflowservice.h"
#include "sessionlifecyclecoordinator.h"
#include "logcategories.h"

#ifdef QT_DEBUG
#include "debugscreenshotwiring.h"
#endif

// MainWindow を初期化し、主要コンポーネントを構築する。
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
{
    m_compositionRoot = new MainWindowCompositionRoot(this);

    m_models.commLog1 = new UsiCommLogModel(this);
    m_models.commLog2 = new UsiCommLogModel(this);

    ui->setupUi(this);

    // 起動時の初期化順序は依存関係に合わせて固定する。
    // 1. UI骨格（central/toolbar）
    // 2. コア部品（GameController/View/Clock）
    // 3. ドック・各タブの構築
    // 4. 設定復元と信号配線
    // この順序を崩すと null 参照や初期表示不整合が起きやすい。
    // ドックネスティングを有効化（同じエリア内でドックを左右に分割可能）
    // setDockNestingEnabled() は QMainWindow で定義された関数（本プロジェクト内には定義なし）
    setDockNestingEnabled(true);

    setupCentralWidgetContainer();

    configureToolBarFromUi();

    // コア部品（GC, View, 盤モデル etc.）は既存関数で初期化
    initializeComponents();

    // プレイモード判定サービスを生成し、初期依存を設定
    m_playModePolicy = new PlayModePolicyService();
    {
        PlayModePolicyService::Deps policyDeps;
        policyDeps.playMode = &m_state.playMode;
        policyDeps.gameController = m_gameController;
        m_playModePolicy->updateDeps(policyDeps);
    }

    if (!m_timePresenter) m_timePresenter = new TimeDisplayPresenter(m_shogiView, this);

    // TimeControlControllerを初期化してTimeDisplayPresenterに設定
    ensureTimeController();
    if (m_timePresenter && m_timeController) {
        m_timePresenter->setClock(m_timeController->clock());
    }

    // 画面骨格（棋譜/分岐/レイアウト/タブ/中央表示）
    buildGamePanels();

    // ウィンドウ設定の復元（位置/サイズなど）
    restoreWindowAndSync();

    // ダイアログ起動配線（connectAllActions より先に初期化）
    initializeDialogLaunchWiring();

    // メニュー/アクションのconnect（関数ポインタで統一）
    connectAllActions();

    // コアシグナル（昇格ダイアログ・エラー・ドラッグ終了・指し手確定 等）
    connectCoreSignals();

    // ツールチップをコンパクト表示へ
    installAppToolTips();

    // 司令塔やUIフォント/位置編集コントローラの最終初期化
    finalizeCoordinators();

    // UI状態ポリシーマネージャを初期化し、アイドル状態を適用
    ensureUiStatePolicyManager();
    m_uiStatePolicy->applyState(UiStatePolicyManager::AppState::Idle);

    // 言語メニューをグループ化（相互排他）して現在の設定を反映
    // LanguageControllerに委譲（setActions内でupdateMenuStateも呼び出し）
    ensureLanguageController();

    // ドックレイアウト関連のメニュー配線をDockLayoutManagerへ移譲
    ensureDockLayoutManager();

#ifdef QT_DEBUG
    // デバッグ用スクリーンショット機能（F12キーで /tmp/shogiboardq-debug/ にPNG保存）
    m_debugScreenshotWiring = new DebugScreenshotWiring(this, this);
#endif

    // 評価値グラフ高さ調整用タイマーを初期化（デバウンス処理用）
    m_evalChartResizeTimer = new QTimer(this);
    m_evalChartResizeTimer->setSingleShot(true);
    connect(m_evalChartResizeTimer, &QTimer::timeout,
            this, &MainWindow::performDeferredEvalChartResize);
}

MainWindow::~MainWindow()
{
    delete m_playModePolicy;
}

// `setupCentralWidgetContainer`: Central Widget Container をセットアップする。
void MainWindow::setupCentralWidgetContainer()
{
    m_central = ui->centralwidget;
    m_centralLayout = new QVBoxLayout(m_central);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);
}

// `configureToolBarFromUi`: Tool Bar From Ui を設定する。
void MainWindow::configureToolBarFromUi()
{
    if (QToolBar* tb = ui->toolBar) {
        tb->setIconSize(QSize(18, 18)); // お好みで 16/18/20/24
        tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
        tb->setStyleSheet(
            "QToolBar{margin:0px; padding:0px; spacing:2px;}"
            "QToolButton{margin:0px; padding:2px;}"
            );

        // 保存された設定からツールバーの表示状態を復元
        bool visible = SettingsService::toolbarVisible();
        tb->setVisible(visible);

        // メニュー内でアイコンを非表示にしてチェックマークを表示
        ui->actionToolBar->setIconVisibleInMenu(false);
        ui->actionToolBar->setChecked(visible);
    }
}

// `buildGamePanels`: Game Panels を構築する（MainWindowUiBootstrapper に委譲）。
void MainWindow::buildGamePanels()
{
    MainWindowUiBootstrapper bootstrapper(*this);
    bootstrapper.buildGamePanels();
}

// `restoreWindowAndSync`: Window And Sync を復元する（MainWindowUiBootstrapper に委譲）。
void MainWindow::restoreWindowAndSync()
{
    MainWindowUiBootstrapper bootstrapper(*this);
    bootstrapper.restoreWindowAndSync();
}

void MainWindow::initializeDialogLaunchWiring()
{
    MainWindowWiringAssembler::initializeDialogLaunchWiring(*this);
}

// `connectAllActions`: All Actions のシグナル接続を行う。
void MainWindow::connectAllActions()
{
    // DialogCoordinatorWiring を先行生成（cancelKifuAnalysis 接続先として必要）
    if (!m_dialogCoordinatorWiring) {
        m_dialogCoordinatorWiring = new DialogCoordinatorWiring(this);
    }

    // KifuFileController を先行生成（ファイル操作スロット接続先として必要）
    ensureKifuFileController();

    // 既存があれば使い回し
    if (!m_actionsWiring) {
        UiActionsWiring::Deps d;
        d.ui  = ui.get();
        d.ctx = this;
        d.dlw = m_dialogLaunchWiring;
        d.dcw = m_dialogCoordinatorWiring;
        d.kfc = m_kifuFileController;
        m_actionsWiring = new UiActionsWiring(d, this);
    }
    m_actionsWiring->wire();
}

// `connectCoreSignals`: Core Signals のシグナル接続を行う。
void MainWindow::connectCoreSignals()
{
    qCDebug(lcApp) << "connectCoreSignals_ called";

    // 将棋盤表示・昇格・ドラッグ終了・指し手確定
    if (m_gameController) {
        connect(m_gameController, &ShogiGameController::showPromotionDialog,
                m_dialogLaunchWiring, &DialogLaunchWiring::displayPromotionDialog, Qt::UniqueConnection);

        connect(m_gameController, &ShogiGameController::endDragSignal,
                m_shogiView,      &ShogiView::endDrag, Qt::UniqueConnection);

        connect(m_gameController, &ShogiGameController::moveCommitted,
                this, &MainWindow::onMoveCommitted, Qt::UniqueConnection);
    }
    if (m_shogiView) {
        bool connected = connect(m_shogiView, &ShogiView::fieldSizeChanged,
                this, &MainWindow::onBoardSizeChanged, Qt::UniqueConnection);
        qCDebug(lcApp) << "fieldSizeChanged -> onBoardSizeChanged connected:" << connected
                 << "m_shogiView=" << m_shogiView;
    } else {
        qCDebug(lcApp) << "m_shogiView is NULL, cannot connect fieldSizeChanged";
    }

    // ErrorBus はラムダを使わず専用スロットへ
    connect(&ErrorBus::instance(), &ErrorBus::errorOccurred,
            this, &MainWindow::onErrorBusOccurred, Qt::UniqueConnection);
}

// `installAppToolTips`: ツールバー配下ボタンにツールチップ用イベントフィルタを導入する。
void MainWindow::installAppToolTips()
{
    auto* tipFilter = new AppToolTipFilter(this);
    tipFilter->setPointSizeF(12.0);
    tipFilter->setCompact(true);

    const auto toolbars = findChildren<QToolBar*>();
    for (QToolBar* tb : toolbars) {
        const auto buttons = tb->findChildren<QToolButton*>();
        for (QToolButton* b : buttons) {
            b->installEventFilter(tipFilter);
        }
    }
}

// `finalizeCoordinators`: Coordinators の最終初期化を行う（MainWindowUiBootstrapper に委譲）。
void MainWindow::finalizeCoordinators()
{
    MainWindowUiBootstrapper bootstrapper(*this);
    bootstrapper.finalizeCoordinators();
}

// `onErrorBusOccurred`: Error Bus Occurred のイベント受信時処理を行う。
void MainWindow::onErrorBusOccurred(const QString& msg)
{
    displayErrorMessage(msg);
}

// GUIを構成するWidgetなどを生成する。（リファクタ後）
void MainWindow::initializeComponents()
{
    initializeGameControllerAndKifu();
    initializeOrResetShogiView();

    // 盤・駒台操作の配線（BoardInteractionController など）
    setupBoardInteractionController();

    initializeBoardModel();

    // --- Turn 初期化 & 同期 ---
    setCurrentTurn();
    ensureTurnSyncBridge();

    // --- 表示名・ログモデル名の初期反映 ---
    setPlayersNamesForMode();
    setEngineNamesBasedOnMode();
    updateSecondEngineVisibility();
}

void MainWindow::initializeGameControllerAndKifu()
{
    // ShogiGameController は QObject 親を付けてリーク防止
    if (!m_gameController) {
        m_gameController = new ShogiGameController(this);
    }

    // 局面履歴（SFEN列）は MatchCoordinator 側で所有する。
    // MainWindow初期化時点で司令塔が未生成の場合があるため、ここでは触らない。

    // 棋譜表示用のレコードリストをクリア
    m_kifu.moveRecords.clear();

    // ゲーム中の指し手リストをクリア
    m_kifu.gameMoves.clear();
    m_kifu.gameUsiMoves.clear();
}

void MainWindow::initializeOrResetShogiView()
{
    if (!m_shogiView) {
        m_shogiView = new ShogiView(this);     // 親をMainWindowに
        m_shogiView->setNameFontScale(0.30);   // 好みの倍率（表示前に設定）
    } else {
        // 再初期化時も念のためパラメータを揃える
        m_shogiView->setParent(this);
        m_shogiView->setNameFontScale(0.30);
    }
}

void MainWindow::initializeBoardModel()
{
    // m_state.startSfenStr が "startpos ..." の場合は必ず完全 SFEN に正規化してから newGame。
    QString start = m_state.startSfenStr;
    if (start.isEmpty()) {
        // 既定：平手初期局面の完全SFEN
        start = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    } else if (start.startsWith(QLatin1String("startpos"))) {
        start = SfenUtils::normalizeStart(start);
    }

    // 盤データを生成してビューへ接続（view->board() が常に有効になるように順序を固定）
    m_gameController->newGame(start);
    if (m_shogiView->board() != m_gameController->board()) {
        m_shogiView->setBoard(m_gameController->board());
    }
}

void MainWindow::displayErrorMessage(const QString& errorMessage)
{
    m_state.errorOccurred = true;

    m_shogiView->setErrorOccurred(m_state.errorOccurred);

    QMessageBox::critical(this, tr("Error"), errorMessage);
}


// 待ったボタンを押すと、2手戻る。
void MainWindow::undoLastTwoMoves()
{
    ensureUndoFlowService();
    m_undoFlowService->undoLastTwoMoves();
}

// `initializeNewGame`: New Game を初期化する。
void MainWindow::initializeNewGame(QString& startSfenStr)
{
    if (m_gameController) {
        m_gameController->newGame(startSfenStr);
    }
    // ← ここは Coordinator に委譲
    GameStartCoordinator::applyResumePositionIfAny(m_gameController, m_shogiView, m_state.resumeSfenStr);
}

// 対局モードに応じて将棋盤上部に表示される対局者名をセットする。
void MainWindow::setPlayersNamesForMode()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_state.playMode);
        m_playerInfoController->setHumanNames(m_player.humanName1, m_player.humanName2);
        m_playerInfoController->setEngineNames(m_player.engineName1, m_player.engineName2);
        m_playerInfoController->applyPlayersNamesForMode();
    }
}

// 駒台を含む将棋盤全体の画像をクリップボードにコピーする。
void MainWindow::copyBoardToClipboard()
{
    BoardImageExporter::copyToClipboard(m_shogiView);
}

// 評価値グラフの画像をクリップボードにコピーする。
void MainWindow::copyEvalGraphToClipboard()
{
    if (!m_evalChart) return;
    BoardImageExporter::copyToClipboard(m_evalChart->chartViewWidget());
}

// `saveShogiBoardImage`: Shogi Board Image を保存する。
void MainWindow::saveShogiBoardImage()
{
    BoardImageExporter::saveImage(this, m_shogiView);
}

// 評価値グラフの画像をファイルに保存する。
void MainWindow::saveEvaluationGraphImage()
{
    if (!m_evalChart) return;
    BoardImageExporter::saveImage(this, m_evalChart->chartViewWidget(),
                                  QStringLiteral("EvalGraph"));
}

// 対局モードに応じて将棋盤下部に表示されるエンジン名をセットする。
void MainWindow::setEngineNamesBasedOnMode()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_state.playMode);
        m_playerInfoController->setEngineNames(m_player.engineName1, m_player.engineName2);
        m_playerInfoController->applyEngineNamesToLogModels();
    }
}

void MainWindow::updateSecondEngineVisibility()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_state.playMode);
        m_playerInfoController->updateSecondEngineVisibility();
    }
}

// `startNewShogiGame`: New Shogi Game を開始する（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::startNewShogiGame(QString& startSfenStr)
{
    Q_UNUSED(startSfenStr)
    ensureReplayController();
    ensureSessionLifecycleCoordinator();
    if (m_sessionLifecycle) {
        m_sessionLifecycle->startNewGame();
    }
}

void MainWindow::clearEvalState()
{
    if (m_evalChart) {
        m_evalChart->clearAll();
    }
    ensureEvaluationGraphController();
    if (m_evalGraphController) {
        m_evalGraphController->clearScores();
    }
    if (m_recordPresenter) {
        m_recordPresenter->clearLiveDisp();
    }
}

void MainWindow::unlockGameOverStyle()
{
    if (m_shogiView) {
        m_shogiView->setGameOverStyleLock(false);
    }
}

void MainWindow::invokeStartGame()
{
    if (!m_match) {
        initMatchCoordinator();
    }
    if (!m_match) return;

    m_match->prepareAndStartGame(
        m_state.playMode,
        m_state.startSfenStr,
        sfenRecord(),
        nullptr,
        m_player.bottomIsP1
        );
}

// 棋譜欄の下の矢印ボタンを有効にする。
// UiStatePolicyManagerがナビゲーション無効状態の場合はスキップする。
void MainWindow::enableArrowButtons()
{
    if (m_uiStatePolicy &&
        !m_uiStatePolicy->isEnabled(UiStatePolicyManager::UiElement::WidgetNavigation)) {
        return;
    }
    if (m_recordPane) m_recordPane->setArrowButtonsEnabled(true);
}



// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    // CSA通信対局モードの場合
    if (m_state.playMode == PlayMode::CsaNetworkMode && m_csaGameCoordinator) {
        m_csaGameCoordinator->onResign();
        return;
    }

    // 通常対局モードの場合
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->handleResignation();
    }
}


void MainWindow::ensureEvaluationGraphController()
{
    if (m_evalGraphController) return;

    m_evalGraphController = new EvaluationGraphController(this);
    m_evalGraphController->setEvalChart(m_evalChart);
    m_evalGraphController->setMatchCoordinator(m_match);
    m_evalGraphController->setSfenRecord(sfenRecord());
    m_evalGraphController->setEngine1Name(m_player.engineName1);
    m_evalGraphController->setEngine2Name(m_player.engineName2);

    // PlayerInfoControllerが既に存在する場合は、EvalGraphControllerを設定
    if (m_playerInfoController) {
        m_playerInfoController->setEvalGraphController(m_evalGraphController);
    }
}

// 将棋クロックの手番を設定する。
void MainWindow::updateTurnStatus(int currentPlayer)
{
    if (!m_shogiView) return;

    ensureTimeController();
    ShogiClock* clock = m_timeController ? m_timeController->clock() : nullptr;
    if (!clock) {
        qCWarning(lcApp) << "ShogiClock not ready yet";
        return;
    }

    clock->setCurrentPlayer(currentPlayer);
    m_shogiView->setActiveSide(currentPlayer == 1);
}

// `openWebsiteInExternalBrowser`: Website In External Browser を開く。
void MainWindow::openWebsiteInExternalBrowser()
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->openProjectWebsite();
    }
}

// `updateJosekiWindow`: Joseki Window を更新する。
void MainWindow::updateJosekiWindow()
{
    // ドックが非表示の場合は更新をスキップ
    if (m_docks.josekiWindow && !m_docks.josekiWindow->isVisible()) {
        return;
    }
    if (m_josekiWiring) {
        m_josekiWiring->updateJosekiWindow();
    }
}

// 詰み探索エンジンを終了する
void MainWindow::stopTsumeSearch()
{
    qCDebug(lcApp).noquote() << "stopTsumeSearch called";
    if (m_match) {
        m_match->stopAnalysisEngine();
    }
}


// TurnManager::changed を受けて UI/Clock を更新（＋手番を GameController に同期）
void MainWindow::onTurnManagerChanged(ShogiGameController::Player now)
{
    ensureTurnStateSyncService();
    m_turnStateSync->onTurnManagerChanged(now);
}

// 現在の手番を設定する。
void MainWindow::setCurrentTurn()
{
    ensureTurnStateSyncService();
    m_turnStateSync->setCurrentTurn();
}

QStringList* MainWindow::sfenRecord()
{
    return m_match ? m_match->sfenRecordPtr() : nullptr;
}

const QStringList* MainWindow::sfenRecord() const
{
    return m_match ? m_match->sfenRecordPtr() : nullptr;
}

// 対局を開始する。
void MainWindow::initializeGame()
{
    ensureGameStartCoordinator();

    MainWindowGameStartService::PrepareDeps prep;
    prep.branchTree = m_branchNav.branchTree;
    prep.navState = m_branchNav.navState;
    prep.sfenRecord = sfenRecord();
    prep.currentSelectedPly = m_kifu.currentSelectedPly;
    prep.currentSfenStr = &m_state.currentSfenStr;

    const MainWindowGameStartService gameStartService;
    gameStartService.prepareForInitializeGame(prep);

    qCDebug(lcApp).noquote() << "initializeGame: FINAL m_state.currentSfenStr=" << m_state.currentSfenStr.left(50)
                       << " m_state.startSfenStr=" << m_state.startSfenStr.left(50)
                       << " m_kifu.currentSelectedPly=" << m_kifu.currentSelectedPly;

    MainWindowGameStartService::ContextDeps ctx;
    ctx.view = m_shogiView;
    ctx.gc = m_gameController;
    ctx.clock = m_timeController ? m_timeController->clock() : nullptr;
    ctx.sfenRecord = sfenRecord();
    ctx.kifuModel = m_models.kifuRecord;
    ctx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    ctx.currentSfenStr = &m_state.currentSfenStr;
    ctx.startSfenStr = &m_state.startSfenStr;
    ctx.selectedPly = m_kifu.currentSelectedPly;
    ctx.isReplayMode = m_replayController ? m_replayController->isReplayMode() : false;
    ctx.bottomIsP1 = m_player.bottomIsP1;

    m_gameStart->initializeGame(gameStartService.buildContext(ctx));
}

// 設定ファイルにGUI全体のウィンドウサイズを書き込む。
// また、将棋盤のマスサイズも書き込む。その後、GUIを終了する。
void MainWindow::saveSettingsAndClose()
{
    saveWindowAndBoardSettings();

    // エンジンが起動していれば終了する
    if (m_match) {
        m_match->destroyEngines();
    }

    QCoreApplication::quit();
}

// GUIを初期画面表示に戻す（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::resetToInitialState()
{
    ensureSessionLifecycleCoordinator();
    m_sessionLifecycle->resetToInitialState();
}

void MainWindow::resetEngineState()
{
    if (m_match) {
        m_match->stopAnalysisEngine();
        m_match->clearGameOverState();
    }
    if (m_csaGameCoordinator) {
        m_csaGameCoordinator->stopGame();
    }
    if (m_consecutiveGamesController) {
        m_consecutiveGamesController->reset();
    }
}

// ゲーム状態変数のリセット（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::resetGameState()
{
    ensureSessionLifecycleCoordinator();
    m_sessionLifecycle->resetGameState();
}

// m_state/m_player/m_kifu のフィールドをデフォルト値にクリアする。
void MainWindow::clearGameStateFields()
{
    m_state.lastMove.clear();
    m_state.resumeSfenStr.clear();
    m_state.errorOccurred = false;

    m_player.humanName1.clear();
    m_player.humanName2.clear();
    m_player.engineName1.clear();
    m_player.engineName2.clear();

    m_kifu.positionStrList.clear();

    m_player.lastP1Turn = true;
    m_player.lastP1Ms = 0;
    m_player.lastP2Ms = 0;

    m_kifu.commentsByRow.clear();
    m_kifu.saveFileName.clear();

    m_state.skipBoardSyncForBranchNav = false;
    m_kifu.onMainRowGuard = false;

    m_kifu.gameUsiMoves.clear();
    m_kifu.gameMoves.clear();
}

void MainWindow::resetModels(const QString& hirateStartSfen)
{
    m_kifu.moveRecords.clear();

    MainWindowResetService::ModelResetDeps deps;
    deps.navState = m_branchNav.navState;
    deps.recordPresenter = m_recordPresenter;
    deps.displayCoordinator = m_branchNav.displayCoordinator;
    deps.boardController = m_boardController;
    deps.gameRecord = m_models.gameRecord;
    deps.evalGraphController = m_evalGraphController;
    deps.timeController = m_timeController;
    deps.sfenRecord = sfenRecord();
    deps.gameInfoController = m_gameInfoController;
    deps.kifuLoadCoordinator = m_kifuLoadCoordinator;
    deps.branchTree = m_branchNav.branchTree;
    deps.liveGameSession = m_branchNav.liveGameSession;

    const MainWindowResetService resetService;
    resetService.resetModels(deps, hirateStartSfen);
}

void MainWindow::resetUiState(const QString& hirateStartSfen)
{
    MainWindowResetService::UiResetDeps deps;
    deps.gameController = m_gameController;
    deps.shogiView = m_shogiView;
    deps.uiStatePolicy = m_uiStatePolicy;
    deps.updateJosekiWindow = [this]() {
        updateJosekiWindow();
    };

    const MainWindowResetService resetService;
    resetService.resetUiState(deps, hirateStartSfen);
}

// `displayGameRecord`: Game Record を表示する（GameRecordLoadService に委譲）。
void MainWindow::displayGameRecord(const QList<KifDisplayItem> disp)
{
    if (!m_models.kifuRecord) return;

    ensureGameRecordLoadService();
    m_gameRecordLoadService->loadGameRecord(disp);
}

// `loadWindowSettings`: Window Settings を読み込む。
void MainWindow::loadWindowSettings()
{
    SettingsService::loadWindowSize(this);
}

// `saveWindowAndBoardSettings`: Window And Board Settings を保存する。
void MainWindow::saveWindowAndBoardSettings()
{
    SettingsService::saveWindowAndBoard(this, m_shogiView);

    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->saveDockStates();
    }
}

// `closeEvent`: 終了時に設定保存とエンジン停止を行ってから親クラス処理へ委譲する。
void MainWindow::closeEvent(QCloseEvent* e)
{
    saveWindowAndBoardSettings();

    // エンジンが起動していれば終了する（quitコマンドを送信してプロセスを停止）
    if (m_match) {
        m_match->destroyEngines();
    }

    QMainWindow::closeEvent(e);
}



// `beginPositionEditing`: 局面編集モードへの遷移処理をコーディネータへ委譲する。
void MainWindow::beginPositionEditing()
{
    ensurePositionEditCoordinator();
    if (m_posEditCoordinator) {
        // 最新の依存オブジェクトを同期
        m_posEditCoordinator->setPositionEditController(m_posEdit);
        m_posEditCoordinator->setBoardController(m_boardController);
        m_posEditCoordinator->setMatchCoordinator(m_match);
        m_posEditCoordinator->beginPositionEditing();
    }
}

// `finishPositionEditing`: 局面編集モード終了処理をコーディネータへ委譲する。
void MainWindow::finishPositionEditing()
{
    ensurePositionEditCoordinator();
    if (m_posEditCoordinator) {
        // 最新の依存オブジェクトを同期
        m_posEditCoordinator->setPositionEditController(m_posEdit);
        m_posEditCoordinator->setBoardController(m_boardController);
        m_posEditCoordinator->finishPositionEditing();
    }
}

// 「すぐ指させる」
void MainWindow::movePieceImmediately()
{
    if (m_match) {
        m_match->forceImmediateMove();
    }
}

// `setGameOverMove`: Game Over Move を設定する。
void MainWindow::setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->setGameOverMove(cause, loserIsPlayerOne);
    }
}

// `appendKifuLine`: GameRecordUpdateService へ委譲。
void MainWindow::appendKifuLine(const QString& text, const QString& elapsedTime)
{
    ensureGameRecordUpdateService();
    m_gameRecordUpdateService->appendKifuLine(text, elapsedTime);
}

// 人間の手番かどうかを判定する（PlayModePolicyService に委譲）
bool MainWindow::isHumanTurnNow() const
{
    if (m_playModePolicy) {
        return m_playModePolicy->isHumanTurnNow();
    }
    return true;
}

// 対局者名確定時のスロット（PlayerInfoWiring経由）
void MainWindow::onPlayerNamesResolved(const QString& human1, const QString& human2,
                                        const QString& engine1, const QString& engine2,
                                        int playMode)
{
    qCDebug(lcApp).noquote() << "onPlayerNamesResolved_: playMode=" << playMode;

    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        if (m_timeController) {
            // 終了日時をクリア（新しい対局が始まるため）
            m_timeController->clearGameEndTime();

            // 対局者名確定と対局情報の設定を一括で行う
            PlayerInfoWiring::TimeControlInfo tcInfo;
            tcInfo.hasTimeControl = m_timeController->hasTimeControl();
            tcInfo.baseTimeMs = m_timeController->baseTimeMs();
            tcInfo.byoyomiMs = m_timeController->byoyomiMs();
            tcInfo.incrementMs = m_timeController->incrementMs();
            tcInfo.gameStartDateTime = m_timeController->gameStartDateTime();

            m_playerInfoWiring->resolveNamesAndSetupGameInfo(
                human1, human2, engine1, engine2, playMode,
                m_state.startSfenStr, tcInfo);
        } else {
            m_playerInfoWiring->onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);
        }
    }
}

// 連続対局設定を受信
void MainWindow::onConsecutiveGamesConfigured(int totalGames, bool switchTurn)
{
    qCDebug(lcApp).noquote() << "onConsecutiveGamesConfigured_: totalGames=" << totalGames << " switchTurn=" << switchTurn;

    ensureConsecutiveGamesController();
    if (m_consecutiveGamesController) {
        m_consecutiveGamesController->configure(totalGames, switchTurn);
    }
}

void MainWindow::onGameStarted(const MatchCoordinator::StartOptions& opt)
{
    // 対局開始時は MatchCoordinator 側の確定モードをUI側にも同期する。
    m_state.playMode = opt.mode;

    // 開始局面のSFENを同期（定跡ウィンドウ等が参照する）
    if (!opt.sfenStart.isEmpty()) {
        m_state.currentSfenStr = opt.sfenStart;
    } else if (sfenRecord() && !sfenRecord()->isEmpty()) {
        m_state.currentSfenStr = sfenRecord()->first();
    }
    updateJosekiWindow();

    ensureConsecutiveGamesController();
    if (m_consecutiveGamesController) {
        m_consecutiveGamesController->onGameStarted(opt, m_lastTimeControl);
    }
}

void MainWindow::startNextConsecutiveGame()
{
    qCDebug(lcApp).noquote() << "startNextConsecutiveGame_ (delegated to controller)";

    ensureConsecutiveGamesController();
    if (m_consecutiveGamesController && m_consecutiveGamesController->shouldStartNextGame()) {
        m_consecutiveGamesController->startNextGame();
    }
}

// `onRequestSelectKifuRow`: Request Select Kifu Row のイベント受信時処理を行う。
void MainWindow::onRequestSelectKifuRow(int row)
{
    qCDebug(lcApp).noquote() << "onRequestSelectKifuRow: row=" << row;

    // 棋譜欄の指定行を選択
    if (m_recordPane) {
        if (QTableView* view = m_recordPane->kifuView()) {
            if (view->model() && row >= 0 && row < view->model()->rowCount()) {
                const QModelIndex idx = view->model()->index(row, 0);
                view->setCurrentIndex(idx);
                qCDebug(lcApp).noquote() << "onRequestSelectKifuRow: selected row=" << row;

                // 盤面・ハイライトも同期
                syncBoardAndHighlightsAtRow(row);
            }
        }
    }
}

void MainWindow::onBranchTreeResetForNewGame()
{
    ensureBranchNavigationWiring();
    m_branchNavWiring->onBranchTreeResetForNewGame();
}

// `syncBoardAndHighlightsAtRow`: KifuNavigationCoordinator へ委譲。
void MainWindow::syncBoardAndHighlightsAtRow(int ply)
{
    ensureKifuNavigationCoordinator();
    m_kifuNavCoordinator->syncBoardAndHighlightsAtRow(ply);
}

// `navigateKifuViewToRow`: KifuNavigationCoordinator へ委譲。
void MainWindow::navigateKifuViewToRow(int ply)
{
    ensureKifuNavigationCoordinator();
    m_kifuNavCoordinator->navigateToRow(ply);
}

// `kifuExportController`: 棋譜エクスポートコントローラを取得する。
KifuExportController* MainWindow::kifuExportController()
{
    ensureKifuExportController();
    return m_kifuExportController;
}

// `initializeBranchNavigationClasses`: BranchNavigationWiring に委譲。
void MainWindow::initializeBranchNavigationClasses()
{
    ensureBranchNavigationWiring();
    m_branchNavWiring->initialize();
}

void MainWindow::ensureBranchNavigationWiring()
{
    if (!m_branchNavWiring) {
        m_branchNavWiring = new BranchNavigationWiring(this);

        // 転送シグナルを MainWindow スロットに接続
        connect(m_branchNavWiring, &BranchNavigationWiring::boardWithHighlightsRequired,
                this, &MainWindow::loadBoardWithHighlights);
        connect(m_branchNavWiring, &BranchNavigationWiring::boardSfenChanged,
                this, &MainWindow::loadBoardFromSfen);
        connect(m_branchNavWiring, &BranchNavigationWiring::branchNodeHandled,
                this, &MainWindow::onBranchNodeHandled);
    }

    BranchNavigationWiring::Deps deps;
    deps.branchTree = &m_branchNav.branchTree;
    deps.navState = &m_branchNav.navState;
    deps.kifuNavController = &m_branchNav.kifuNavController;
    deps.displayCoordinator = &m_branchNav.displayCoordinator;
    deps.branchTreeWidget = &m_branchNav.branchTreeWidget;
    deps.liveGameSession = &m_branchNav.liveGameSession;
    deps.recordPane = m_recordPane;
    deps.analysisTab = m_analysisTab;
    deps.kifuRecordModel = m_models.kifuRecord;
    deps.kifuBranchModel = m_models.kifuBranch;
    deps.gameRecordModel = m_models.gameRecord;
    deps.gameController = m_gameController;
    deps.commentCoordinator = m_commentCoordinator;
    deps.startSfenStr = &m_state.startSfenStr;
    deps.ensureCommentCoordinator = [this]() { ensureCommentCoordinator(); };
    m_branchNavWiring->updateDeps(deps);
}

// `setupRecordPane`: Record Pane をセットアップする。
void MainWindow::setupRecordPane()
{
    // モデルの用意（従来どおり）
    if (!m_models.kifuRecord) m_models.kifuRecord = new KifuRecordListModel(this);
    if (!m_models.kifuBranch) m_models.kifuBranch = new KifuBranchListModel(this);

    // Wiring の生成
    if (!m_recordPaneWiring) {
        ensureCommentCoordinator();
        RecordPaneWiring::Deps d;
        d.parent             = m_central;                               // 親ウィジェット
        d.ctx                = this;                                    // RecordPane::mainRowChanged の受け先
        d.recordModel        = m_models.kifuRecord;
        d.branchModel        = m_models.kifuBranch;
        d.commentCoordinator = m_commentCoordinator;

        m_recordPaneWiring = new RecordPaneWiring(d, this);
    }

    // RecordPane の構築と配線
    m_recordPaneWiring->buildUiAndWire();

    // 生成物の取得
    m_recordPane = m_recordPaneWiring->pane();

}

// `setupEngineAnalysisTab`: Engine Analysis Tab をセットアップする。
void MainWindow::setupEngineAnalysisTab()
{
    // 既に配線クラスがあれば再利用し、タブ取得だけを行う
    if (!m_analysisWiring) {
        AnalysisTabWiring::Deps d;
        d.centralParent = m_central;         // 既存の central エリア
        d.log1          = m_models.commLog1;  // USIログ(先手)
        d.log2          = m_models.commLog2;  // USIログ(後手)

        m_analysisWiring = new AnalysisTabWiring(d, this);
        m_analysisWiring->buildUiAndWire();
    }

    // 配線クラスから出来上がった部品を受け取る（MainWindow の既存フィールドへ反映）
    m_analysisTab    = m_analysisWiring->analysisTab();
    m_tab            = m_analysisWiring->tab();
    m_models.thinking1 = m_analysisWiring->thinking1();
    m_models.thinking2 = m_analysisWiring->thinking2();

    if (Q_UNLIKELY(!m_analysisTab) || Q_UNLIKELY(!m_models.thinking1) || Q_UNLIKELY(!m_models.thinking2)) {
        qCWarning(lcApp, "ensureAnalysisTabWiring: analysis wiring produced null components "
                  "(tab=%p, thinking1=%p, thinking2=%p)",
                  static_cast<void*>(m_analysisTab),
                  static_cast<void*>(m_models.thinking1),
                  static_cast<void*>(m_models.thinking2));
        return;
    }

    connectAnalysisTabSignals();
    configureAnalysisTabDependencies();
}

void MainWindow::connectAnalysisTabSignals()
{
    // 分岐ツリーのアクティベートを MainWindow スロットへ
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
        this,          &MainWindow::onBranchNodeActivated,
        Qt::UniqueConnection);

    ensureCommentCoordinator();
    // CommentCoordinator は setupRecordPane() で m_analysisTab 生成前に作られる場合がある。
    // その場合 setCommentEditor() が呼ばれないため、ここで補完する。
    m_commentCoordinator->setCommentEditor(m_analysisTab->commentEditor());
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::commentUpdated,
        m_commentCoordinator, &CommentCoordinator::onCommentUpdated,
        Qt::UniqueConnection);

    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::pvRowClicked,
        this,          &MainWindow::onPvRowClicked,
        Qt::UniqueConnection);

    ensureUsiCommandController();
    if (m_usiCommandController) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::usiCommandRequested,
            m_usiCommandController, &UsiCommandController::sendCommand,
            Qt::UniqueConnection);
    }

    // 検討開始・エンジン設定・エンジン変更シグナルをConsiderationWiringに接続
    ensureConsiderationWiring();
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::startConsiderationRequested,
        m_considerationWiring, &ConsiderationWiring::displayConsiderationDialog,
        Qt::UniqueConnection);
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::engineSettingsRequested,
        m_considerationWiring, &ConsiderationWiring::onEngineSettingsRequested,
        Qt::UniqueConnection);
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::considerationEngineChanged,
        m_considerationWiring, &ConsiderationWiring::onEngineChanged,
        Qt::UniqueConnection);
}

void MainWindow::configureAnalysisTabDependencies()
{
    // PlayerInfoControllerにもm_analysisTabを設定
    if (m_playerInfoController) {
        m_playerInfoController->setAnalysisTab(m_analysisTab);
    }

    // PlayerInfoWiringにも検討タブを設定
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->setAnalysisTab(m_analysisTab);
        // 起動時に対局情報タブを追加
        if (m_tab) {
            m_playerInfoWiring->setTabWidget(m_tab);
        }
        m_playerInfoWiring->addGameInfoTabAtStartup();
        m_gameInfoController = m_playerInfoWiring->gameInfoController();
    }

    if (!m_models.consideration) {
        m_models.consideration = new ShogiEngineThinkingModel(this);
    }
    if (m_analysisTab) {
        m_analysisTab->setConsiderationThinkingModel(m_models.consideration);
    }
}

// `createEvalChartDock`: Eval Chart Dock を作成する。
void MainWindow::createEvalChartDock()
{
    // 評価値グラフウィジェットを作成
    m_evalChart = new EvaluationChartWidget(this);

    // 既存のEvaluationGraphControllerがあれば、評価値グラフを設定
    if (m_evalGraphController) {
        m_evalGraphController->setEvalChart(m_evalChart);
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setEvalChart(m_evalChart);
    m_docks.evalChart = m_dockCreationService->createEvalChartDock();
}

// `createRecordPaneDock`: Record Pane Dock を作成する。
void MainWindow::createRecordPaneDock()
{
    if (!m_recordPane) {
        qCWarning(lcApp) << "createRecordPaneDock: m_recordPane is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setRecordPane(m_recordPane);
    m_docks.recordPane = m_dockCreationService->createRecordPaneDock();
}

// `createAnalysisDocks`: Analysis Docks を作成する。
void MainWindow::createAnalysisDocks()
{
    if (!m_analysisTab) {
        qCWarning(lcApp) << "createAnalysisDocks: m_analysisTab is null!";
        return;
    }

    // 対局情報コントローラを準備し、デフォルト値を設定
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->ensureGameInfoController();
        m_gameInfoController = m_playerInfoWiring->gameInfoController();
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setAnalysisTab(m_analysisTab);
    m_dockCreationService->setGameInfoController(m_gameInfoController);
    m_dockCreationService->setModels(m_models.thinking1, m_models.thinking2, m_models.commLog1, m_models.commLog2);
    m_dockCreationService->createAnalysisDocks();

    // 作成されたドックへの参照を取得
    m_docks.gameInfo = m_dockCreationService->gameInfoDock();
    m_docks.thinking = m_dockCreationService->thinkingDock();
    m_docks.consideration = m_dockCreationService->considerationDock();
    m_docks.usiLog = m_dockCreationService->usiLogDock();
    m_docks.csaLog = m_dockCreationService->csaLogDock();
    m_docks.comment = m_dockCreationService->commentDock();
    m_docks.branchTree = m_dockCreationService->branchTreeDock();
}

// `setupBoardInCenter`: Board In Center をセットアップする。
void MainWindow::setupBoardInCenter()
{
    if (!m_shogiView) {
        qCWarning(lcApp) << "setupBoardInCenter: m_shogiView is null!";
        return;
    }

    if (!m_centralLayout) {
        qCWarning(lcApp) << "setupBoardInCenter: m_centralLayout is null!";
        return;
    }

    // 将棋盤をセントラルウィジェットに配置
    m_centralLayout->addWidget(m_shogiView);

    // セントラルウィジェットのサイズを将棋盤に合わせて固定
    // これにより余白がなくなる
    if (m_central) {
        const QSize boardSize = m_shogiView->sizeHint();
        m_central->setFixedSize(boardSize);
    }

    // 将棋盤を表示
    m_shogiView->show();
}

// `createMenuWindowDock`: Menu Window Dock を作成する。
void MainWindow::createMenuWindowDock()
{
    // MenuWindowWiringを確保
    ensureMenuWiring();
    if (!m_menuWiring) {
        qCWarning(lcApp) << "createMenuWindowDock: MenuWindowWiring is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setMenuWiring(m_menuWiring);
    m_docks.menuWindow = m_dockCreationService->createMenuWindowDock();
}

// `createJosekiWindowDock`: Joseki Window Dock を作成する。
void MainWindow::createJosekiWindowDock()
{
    // JosekiWindowWiringを確保
    ensureJosekiWiring();
    if (!m_josekiWiring) {
        qCWarning(lcApp) << "createJosekiWindowDock: JosekiWindowWiring is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setJosekiWiring(m_josekiWiring);
    m_docks.josekiWindow = m_dockCreationService->createJosekiWindowDock();

    // ドックが表示されたときに定跡ウィンドウを更新する
    // （トグルアクション経由で表示された場合にも対応）
    if (m_docks.josekiWindow) {
        connect(m_docks.josekiWindow, &QDockWidget::visibilityChanged,
                this, &MainWindow::updateJosekiWindow);
    }
}

// `createAnalysisResultsDock`: Analysis Results Dock を作成する。
void MainWindow::createAnalysisResultsDock()
{
    // AnalysisResultsPresenterを確保
    ensureAnalysisPresenter();
    if (!m_analysisPresenter) {
        qCWarning(lcApp) << "createAnalysisResultsDock: AnalysisResultsPresenter is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setAnalysisPresenter(m_analysisPresenter);
    m_docks.analysisResults = m_dockCreationService->createAnalysisResultsDock();
}

// `ensureMatchCoordinatorWiring`: MatchCoordinatorWiring を遅延初期化し Deps を更新する。
void MainWindow::ensureMatchCoordinatorWiring()
{
    const bool firstTime = !m_matchWiring;
    if (!m_matchWiring) {
        m_matchWiring = new MatchCoordinatorWiring(this);
    }

    ensureEvaluationGraphController();
    ensurePlayerInfoWiring();

    auto deps = buildMatchWiringDeps();
    m_matchWiring->updateDeps(deps);

    // 初回のみ: 転送シグナルを MainWindow スロットに接続
    if (firstTime) {
        wireMatchWiringSignals();
    }
}

MatchCoordinatorWiring::Deps MainWindow::buildMatchWiringDeps()
{
    return MainWindowWiringAssembler::buildMatchWiringDeps(*this);
}

void MainWindow::wireMatchWiringSignals()
{
    MainWindowWiringAssembler::wireMatchWiringSignals(*this);
}

// MatchCoordinator 構築と配線の集約ポイント。
void MainWindow::initMatchCoordinator()
{
    if (!m_gameController || !m_shogiView) return;

    ensureMatchCoordinatorWiring();

    if (!m_matchWiring->initializeSession(
            std::bind(&MainWindow::ensureMatchCoordinatorWiring, this))) {
        return;
    }

    // Facade が生成したオブジェクトを MainWindow に反映
    m_match = m_matchWiring->match();
    m_gameStartCoordinator = m_matchWiring->gameStartCoordinator();
}

// `ensureTimeController`: Time Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureTimeController()
{
    if (m_timeController) return;

    m_timeController = new TimeControlController(this);
    m_timeController->setTimeDisplayPresenter(m_timePresenter);
    m_timeController->ensureClock();
}

// `onMatchGameEnded`: Match Game Ended のイベント受信時処理を行う（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    ensureGameStateController();
    ensureConsecutiveGamesController();
    ensureSessionLifecycleCoordinator();
    if (m_sessionLifecycle) {
        m_sessionLifecycle->handleGameEnded(info);
    }
}

// `onActionFlipBoardTriggered`: Action Flip Board Triggered のイベント受信時処理を行う。
void MainWindow::onActionFlipBoardTriggered(bool /*checked*/)
{
    if (m_match) m_match->flipBoard();
}

// `onActionEnlargeBoardTriggered`: Action Enlarge Board Triggered のイベント受信時処理を行う。
void MainWindow::onActionEnlargeBoardTriggered(bool /*checked*/)
{
    if (!m_shogiView) return;
    m_shogiView->enlargeBoard(true);
}

// `onActionShrinkBoardTriggered`: Action Shrink Board Triggered のイベント受信時処理を行う。
void MainWindow::onActionShrinkBoardTriggered(bool /*checked*/)
{
    if (!m_shogiView) return;
    m_shogiView->reduceBoard(true);
}

// `onRequestAppendGameOverMove`: Request Append Game Over Move のイベント受信時処理を行う。
void MainWindow::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->onRequestAppendGameOverMove(info);
    }

    if (m_branchNav.liveGameSession != nullptr && m_branchNav.liveGameSession->isActive()) {
        qCDebug(lcApp) << "onRequestAppendGameOverMove: committing live game session";
        m_branchNav.liveGameSession->commit();
    }
}

// `setupBoardInteractionController`: Board Interaction Controller をセットアップする。
void MainWindow::setupBoardInteractionController()
{
    ensureBoardSetupController();
    if (m_boardSetupController) {
        m_boardSetupController->setupBoardInteractionController();
        m_boardController = m_boardSetupController->boardController();

        // 人間の手番かどうかを判定するコールバックを設定
        if (m_boardController) {
            m_boardController->setIsHumanTurnCallback([this]() -> bool {
                return isHumanTurnNow();
            });
        }
    }
}

// `connectBoardClicks`: Board Clicks のシグナル接続を行う。
void MainWindow::connectBoardClicks()
{
    ensureBoardSetupController();
    if (m_boardSetupController) {
        m_boardSetupController->connectBoardClicks();
    }
}

// `connectMoveRequested`: Move Requested のシグナル接続を行う。
void MainWindow::connectMoveRequested()
{
    // BoardInteractionControllerからのシグナルをMainWindow経由で処理
    if (m_boardController) {
        QObject::connect(
            m_boardController, &BoardInteractionController::moveRequested,
            this,              &MainWindow::onMoveRequested,
            Qt::UniqueConnection);
    }
}

// `onMoveRequested`: Move Requested のイベント受信時処理を行う。
void MainWindow::onMoveRequested(const QPoint& from, const QPoint& to)
{
    qCDebug(lcApp).noquote() << "onMoveRequested_: from=" << from << " to=" << to
                       << " m_state.playMode=" << static_cast<int>(m_state.playMode)
                       << " m_match=" << (m_match ? "valid" : "null")
                       << " matchMode=" << (m_match ? static_cast<int>(m_match->playMode()) : -1);
    
    ensureBoardSetupController();
    if (m_boardSetupController) {
        // 最新の状態を同期
        m_boardSetupController->setPlayMode(m_state.playMode);
        m_boardSetupController->setMatchCoordinator(m_match);
        m_boardSetupController->setSfenRecord(sfenRecord());
        m_boardSetupController->setPositionEditController(m_posEdit);
        m_boardSetupController->setTimeController(m_timeController);
        m_boardSetupController->onMoveRequested(from, to);
    }
}

// 再生モードの切替を ReplayController へ委譲
void MainWindow::setReplayMode(bool on)
{
    ensureReplayController();
    if (m_replayController) {
        m_replayController->setReplayMode(on);
    }
}



// `onBranchNodeActivated`: BranchNavigationWiring へ転送。
void MainWindow::onBranchNodeActivated(int row, int ply)
{
    ensureBranchNavigationWiring();
    m_branchNavWiring->onBranchNodeActivated(row, ply);
}

// `onBranchNodeHandled`: KifuNavigationCoordinator へ委譲。
void MainWindow::onBranchNodeHandled(int ply, const QString& sfen,
                                     int previousFileTo, int previousRankTo,
                                     const QString& lastUsiMove)
{
    ensureKifuNavigationCoordinator();
    m_kifuNavCoordinator->handleBranchNodeHandled(ply, sfen, previousFileTo, previousRankTo, lastUsiMove);
}

// `onBranchTreeBuilt`: BranchNavigationWiring へ転送。
void MainWindow::onBranchTreeBuilt()
{
    ensureBranchNavigationWiring();
    m_branchNavWiring->onBranchTreeBuilt();
}

void MainWindow::loadBoardFromSfen(const QString& sfen)
{
    ensureBoardLoadService();
    m_boardLoadService->loadFromSfen(sfen);
}

void MainWindow::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    ensureBoardLoadService();
    m_boardLoadService->loadWithHighlights(currentSfen, prevSfen);
}

// 毎手の着手確定時：ライブ分岐ツリー更新をイベントループ後段に遅延
void MainWindow::onMoveCommitted(ShogiGameController::Player mover, int ply)
{
    ensureBoardSetupController();
    if (m_boardSetupController) {
        m_boardSetupController->setPlayMode(m_state.playMode);
        m_boardSetupController->onMoveCommitted(mover, ply);
    }

    // USI追記・SFEN更新を GameRecordUpdateService へ委譲
    ensureGameRecordUpdateService();
    m_gameRecordUpdateService->recordUsiMoveAndUpdateSfen();

    updateJosekiWindow();
}

// `flipBoardAndUpdatePlayerInfo`: 盤面向きを反転し、名前/時計表示を再描画する。
void MainWindow::flipBoardAndUpdatePlayerInfo()
{
    qCDebug(lcApp) << "flipBoardAndUpdatePlayerInfo ENTER";
    if (!m_shogiView) return;

    // 盤の表示向きをトグル
    const bool flipped = !m_shogiView->getFlipMode();
    m_shogiView->setFlipMode(flipped);
    if (flipped) m_shogiView->setPiecesFlip();
    else         m_shogiView->setPieces();

    if (m_timePresenter) {
        m_timePresenter->onMatchTimeUpdated(
            m_player.lastP1Ms, m_player.lastP2Ms, m_player.lastP1Turn, /*urgencyMs(未使用)*/ 0);
    }

    m_shogiView->update();
    qCDebug(lcApp) << "flipBoardAndUpdatePlayerInfo LEAVE";
}

// `setupNameAndClockFonts`: Name And Clock Fonts をセットアップする。
void MainWindow::setupNameAndClockFonts()
{
    if (!m_shogiView) return;
    auto* n1 = m_shogiView->blackNameLabel();
    auto* n2 = m_shogiView->whiteNameLabel();
    auto* c1 = m_shogiView->blackClockLabel();
    auto* c2 = m_shogiView->whiteClockLabel();
    if (!n1 || !n2 || !c1 || !c2) return;

    // 例：等幅は環境依存のため FixedFont をベースに
    QFont nameFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    nameFont.setPointSize(12);         // お好みで
    nameFont.setWeight(QFont::DemiBold);

    QFont clockFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    clockFont.setPointSize(16);        // お好みで
    clockFont.setWeight(QFont::DemiBold);

    n1->setFont(nameFont);
    n2->setFont(nameFont);
    c1->setFont(clockFont);
    c2->setFont(clockFont);
}

// 盤反転の通知を受けたら、手前が先手かどうかのフラグをトグル
void MainWindow::onBoardFlipped(bool /*flipped*/)
{
    m_player.bottomIsP1 = !m_player.bottomIsP1;

    // （必要なら）プレイヤー名や駒台ラベルの入れ替えなど既存処理をここに
    flipBoardAndUpdatePlayerInfo();
}

// `onBoardSizeChanged`: Board Size Changed のイベント受信時処理を行う。
void MainWindow::onBoardSizeChanged(QSize fieldSize)
{
    // 将棋盤サイズ変更通知のハンドラ
    // セントラルウィジェットのサイズを将棋盤に合わせて調整
    Q_UNUSED(fieldSize)

    if (m_central && m_shogiView) {
        // 将棋盤の実際のサイズを取得
        const QSize boardSize = m_shogiView->sizeHint();

        // セントラルウィジェットの最大サイズを将棋盤サイズに制限
        // これにより余白がなくなる
        m_central->setFixedSize(boardSize);
    }
}

// `performDeferredEvalChartResize`: Deferred Eval Chart Resize を実行する。
void MainWindow::performDeferredEvalChartResize()
{
    // デバウンス後の評価値グラフ高さ調整
    // Ctrl+ホイールによる連続したサイズ変更が完了した後に一度だけ実行される
    if (m_shogiView) {
        onBoardSizeChanged(m_shogiView->fieldSize());
    }
}

// `onGameOverStateChanged`: Game Over State Changed のイベント受信時処理を行う。
void MainWindow::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->onGameOverStateChanged(st);
    }
}

// `handleBreakOffGame`: Break Off Game を処理する。
void MainWindow::handleBreakOffGame()
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->handleBreakOffGame();
    }
}

// `ensureReplayController`: Replay Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureReplayController()
{
    if (m_replayController) return;

    m_replayController = new ReplayController(this);
    m_replayController->setClock(m_timeController ? m_timeController->clock() : nullptr);
    m_replayController->setShogiView(m_shogiView);
    m_replayController->setGameController(m_gameController);
    m_replayController->setMatchCoordinator(m_match);
    m_replayController->setRecordPane(m_recordPane);
}

// `buildRuntimeRefs`: 現在のメンバ変数から MainWindowRuntimeRefs スナップショットを構築する。
MainWindowRuntimeRefs MainWindow::buildRuntimeRefs()
{
    return MainWindowRuntimeRefsFactory::build(*this);
}

// `ensureDialogCoordinator`: CompositionRoot に委譲して生成・配線・コンテキスト設定を行う。
void MainWindow::ensureDialogCoordinator()
{
    if (m_dialogCoordinator) return;

    auto refs = buildRuntimeRefs();

    MainWindowDepsFactory::DialogCoordinatorCallbacks callbacks;
    callbacks.getBoardFlipped = [this]() { return m_shogiView ? m_shogiView->getFlipMode() : false; };
    callbacks.getConsiderationWiring = [this]() { ensureConsiderationWiring(); return m_considerationWiring; };
    callbacks.getUiStatePolicyManager = [this]() { ensureUiStatePolicyManager(); return m_uiStatePolicy; };
    callbacks.navigateKifuViewToRow = std::bind(&MainWindow::navigateKifuViewToRow, this, std::placeholders::_1);

    m_compositionRoot->ensureDialogCoordinator(refs, callbacks, this,
        m_dialogCoordinatorWiring, m_dialogCoordinator);
}

// `ensureKifuFileController`: CompositionRoot に委譲して生成・依存関係更新を行う。
void MainWindow::ensureKifuFileController()
{
    auto refs = buildRuntimeRefs();

    MainWindowDepsFactory::KifuFileCallbacks callbacks;
    callbacks.clearUiBeforeKifuLoad = std::bind(&MainWindow::clearUiBeforeKifuLoad, this);
    callbacks.setReplayMode = std::bind(&MainWindow::setReplayMode, this, std::placeholders::_1);
    callbacks.ensurePlayerInfoAndGameInfo = [this]() {
        ensurePlayerInfoWiring();
        if (m_playerInfoWiring) {
            m_playerInfoWiring->ensureGameInfoController();
            m_gameInfoController = m_playerInfoWiring->gameInfoController();
        }
    };
    callbacks.ensureGameRecordModel = std::bind(&MainWindow::ensureGameRecordModel, this);
    callbacks.ensureKifuExportController = std::bind(&MainWindow::ensureKifuExportController, this);
    callbacks.updateKifuExportDependencies = std::bind(&MainWindow::updateKifuExportDependencies, this);
    callbacks.createAndWireKifuLoadCoordinator = std::bind(&MainWindow::createAndWireKifuLoadCoordinator, this);
    callbacks.ensureKifuLoadCoordinatorForLive = std::bind(&MainWindow::ensureKifuLoadCoordinatorForLive, this);
    callbacks.getKifuExportController = [this]() { return m_kifuExportController; };
    callbacks.getKifuLoadCoordinator = [this]() { return m_kifuLoadCoordinator; };

    m_compositionRoot->ensureKifuFileController(refs, callbacks, this,
        m_kifuFileController);
}

// `ensureKifuExportController`: Kifu Export Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureKifuExportController()
{
    if (m_kifuExportController) return;

    m_kifuExportController = new KifuExportController(this, this);

    // 準備コールバックを設定（クリップボードコピー等の前に呼ばれる）
    m_kifuExportController->setPrepareCallback([this]() {
        ensureGameRecordModel();
        updateKifuExportDependencies();
    });

    // ステータスバーへのメッセージ転送
    connect(m_kifuExportController, &KifuExportController::statusMessage,
            this, [this](const QString& msg, int timeout) {
                if (ui && ui->statusbar) {
                    ui->statusbar->showMessage(msg, timeout);
                }
            });
}

// KifuExportControllerに依存を設定するヘルパー
void MainWindow::updateKifuExportDependencies()
{
    KifuExportDepsAssembler::assemble(*this);
}

// `ensureGameStateController`: CompositionRoot に委譲して生成・コールバック設定を行う。
void MainWindow::ensureGameStateController()
{
    if (m_gameStateController) return;

    ensureUiStatePolicyManager();

    MainWindowDepsFactory::GameStateControllerCallbacks cbs;
    cbs.enableArrowButtons = std::bind(&UiStatePolicyManager::transitionToIdle, m_uiStatePolicy);
    cbs.setReplayMode = std::bind(&MainWindow::setReplayMode, this, std::placeholders::_1);
    cbs.refreshBranchTree = [this]() { m_kifu.currentSelectedPly = 0; };
    cbs.updatePlyState = [this](int ap, int sp, int mi) {
        m_kifu.activePly = ap;
        m_kifu.currentSelectedPly = sp;
        m_state.currentMoveIndex = mi;
        syncNavStateToPly(sp);
    };

    m_compositionRoot->ensureGameStateController(buildRuntimeRefs(), cbs, this, m_gameStateController);
}

// 対局終了後、ナビゲーション状態をUIの表示位置と同期する。
void MainWindow::syncNavStateToPly(int selectedPly)
{
    if (!m_branchNav.navState || !m_branchNav.branchTree) return;

    // 本譜固定で探すと、分岐表示中に「1手戻る」が本譜へ飛ぶため、
    // 現在ライン上の同一plyノードを優先して同期する。
    KifuBranchNode* node = nullptr;
    const int currentLine = m_branchNav.navState->currentLineIndex();
    const QVector<BranchLine> lines = m_branchNav.branchTree->allLines();
    if (currentLine >= 0 && currentLine < lines.size()) {
        const BranchLine& line = lines.at(currentLine);
        for (KifuBranchNode* candidate : std::as_const(line.nodes)) {
            if (candidate != nullptr && candidate->ply() == selectedPly) {
                node = candidate;
                break;
            }
        }
    }

    if (!node) {
        node = m_branchNav.branchTree->findByPlyOnMainLine(selectedPly);
    }
    if (node) {
        m_branchNav.navState->setCurrentNode(node);
    }
}

// `ensurePlayerInfoController`: CompositionRoot に委譲して生成・依存設定を行う。
void MainWindow::ensurePlayerInfoController()
{
    if (m_playerInfoController) return;
    ensurePlayerInfoWiring();
    m_compositionRoot->ensurePlayerInfoController(buildRuntimeRefs(), this, m_playerInfoController);
}

// `ensureBoardSetupController`: CompositionRoot に委譲して生成・コールバック設定を行う。
void MainWindow::ensureBoardSetupController()
{
    if (m_boardSetupController) return;

    MainWindowDepsFactory::BoardSetupControllerCallbacks cbs;
    cbs.ensurePositionEdit = [this]() { ensurePositionEditController(); };
    cbs.ensureTimeController = [this]() { ensureTimeController(); };
    cbs.updateGameRecord = [this](const QString& moveText, const QString& elapsed) {
        m_state.lastMove = moveText;
        updateGameRecord(elapsed);
    };
    cbs.redrawEngine1Graph = [this](int ply) {
        ensureEvaluationGraphController();
        if (m_evalGraphController) m_evalGraphController->redrawEngine1Graph(ply);
    };
    cbs.redrawEngine2Graph = [this](int ply) {
        ensureEvaluationGraphController();
        if (m_evalGraphController) m_evalGraphController->redrawEngine2Graph(ply);
    };

    m_compositionRoot->ensureBoardSetupController(buildRuntimeRefs(), cbs, this, m_boardSetupController);
}

// `ensurePvClickController`: CompositionRoot に委譲して生成・依存設定を行う。
void MainWindow::ensurePvClickController()
{
    if (m_pvClickController) return;
    m_compositionRoot->ensurePvClickController(buildRuntimeRefs(), this, m_pvClickController);
    connect(m_pvClickController, &PvClickController::pvDialogClosed,
            this, &MainWindow::onPvDialogClosed, Qt::UniqueConnection);
}


// `ensurePositionEditCoordinator`: CompositionRoot に委譲して生成・コールバック設定を行う。
void MainWindow::ensurePositionEditCoordinator()
{
    if (m_posEditCoordinator) return;

    ensureUiStatePolicyManager();

    MainWindowDepsFactory::PositionEditCoordinatorCallbacks cbs;
    cbs.applyEditMenuState = [this](bool editing) {
        if (editing) m_uiStatePolicy->transitionToDuringPositionEdit();
        else m_uiStatePolicy->transitionToIdle();
    };
    cbs.ensurePositionEdit = [this]() {
        ensurePositionEditController();
        if (m_posEditCoordinator) m_posEditCoordinator->setPositionEditController(m_posEdit);
    };
    cbs.actionReturnAllPiecesToStand = ui->actionReturnAllPiecesToStand;
    cbs.actionSetHiratePosition = ui->actionSetHiratePosition;
    cbs.actionSetTsumePosition = ui->actionSetTsumePosition;
    cbs.actionChangeTurn = ui->actionChangeTurn;

    m_compositionRoot->ensurePositionEditCoordinator(buildRuntimeRefs(), cbs, this, m_posEditCoordinator);
}

// `ensureCsaGameWiring`: Csa Game Wiring を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureCsaGameWiring()
{
    if (m_csaGameWiring) return;

    CsaGameWiring::Dependencies deps;
    deps.coordinator = m_csaGameCoordinator;
    deps.gameController = m_gameController;
    deps.shogiView = m_shogiView;
    deps.kifuRecordModel = m_models.kifuRecord;
    deps.recordPane = m_recordPane;
    deps.boardController = m_boardController;
    deps.statusBar = ui ? ui->statusbar : nullptr;
    deps.sfenRecord = sfenRecord();
    // CSA対局開始用の追加依存オブジェクト
    deps.analysisTab = m_analysisTab;
    deps.boardSetupController = m_boardSetupController;
    deps.usiCommLog = m_models.commLog1;
    deps.engineThinking = m_models.thinking1;
    deps.timeController = m_timeController;
    deps.gameMoves = &m_kifu.gameMoves;
    deps.playMode = &m_state.playMode;
    deps.parentWidget = this;

    m_csaGameWiring = new CsaGameWiring(deps, this);

    wireCsaGameWiringSignals();

    qCDebug(lcApp).noquote() << "ensureCsaGameWiring_: created and connected";
}

// CsaGameWiringのシグナル/スロット接続を行う。
void MainWindow::wireCsaGameWiringSignals()
{
    // CSA対局開始/終了時のUI状態遷移
    ensureUiStatePolicyManager();
    connect(m_csaGameWiring, &CsaGameWiring::disableNavigationRequested,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToDuringCsaGame);
    connect(m_csaGameWiring, &CsaGameWiring::enableNavigationRequested,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToIdle);
    connect(m_csaGameWiring, &CsaGameWiring::appendKifuLineRequested,
            this, &MainWindow::appendKifuLine);
    // refreshBranchTreeRequested: LiveGameSession + KifuDisplayCoordinator が自動更新するため接続不要
    connect(m_csaGameWiring, &CsaGameWiring::errorMessageRequested,
            this, &MainWindow::displayErrorMessage);
}

// `ensureJosekiWiring`: Joseki Wiring を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureJosekiWiring()
{
    if (m_josekiWiring) return;

    JosekiWindowWiring::Dependencies deps;
    deps.parentWidget = this;
    deps.gameController = m_gameController;
    deps.kifuRecordModel = m_models.kifuRecord;
    deps.sfenRecord = sfenRecord();
    deps.usiMoves = &m_kifu.gameUsiMoves;
    deps.currentSfenStr = &m_state.currentSfenStr;
    deps.currentMoveIndex = &m_state.currentMoveIndex;
    deps.currentSelectedPly = &m_kifu.currentSelectedPly;
    deps.playMode = &m_state.playMode;

    m_josekiWiring = new JosekiWindowWiring(deps, this);

    // JosekiWindowWiringからのシグナルをMainWindowに接続
    connect(m_josekiWiring, &JosekiWindowWiring::moveRequested,
            this, &MainWindow::onMoveRequested);
    connect(m_josekiWiring, &JosekiWindowWiring::forcedPromotionRequested,
            this, &MainWindow::onJosekiForcedPromotion);

    qCDebug(lcApp).noquote() << "ensureJosekiWiring_: created and connected";
}

// `ensureMenuWiring`: Menu Wiring を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureMenuWiring()
{
    if (m_menuWiring) return;

    MenuWindowWiring::Dependencies deps;
    deps.parentWidget = this;
    deps.menuBar = menuBar();

    m_menuWiring = new MenuWindowWiring(deps, this);

    qCDebug(lcApp).noquote() << "ensureMenuWiring_: created and connected";
}

// `ensurePlayerInfoWiring`: Player Info Wiring を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensurePlayerInfoWiring()
{
    if (m_playerInfoWiring) return;

    PlayerInfoWiring::Dependencies deps;
    deps.parentWidget = this;
    deps.tabWidget = m_tab;
    deps.shogiView = m_shogiView;
    deps.playMode = &m_state.playMode;
    deps.humanName1 = &m_player.humanName1;
    deps.humanName2 = &m_player.humanName2;
    deps.engineName1 = &m_player.engineName1;
    deps.engineName2 = &m_player.engineName2;

    m_playerInfoWiring = new PlayerInfoWiring(deps, this);

    // 検討タブが既に作成済みなら設定
    if (m_analysisTab) {
        m_playerInfoWiring->setAnalysisTab(m_analysisTab);
    }

    // PlayerInfoWiringからのシグナルをMainWindowに接続
    connect(m_playerInfoWiring, &PlayerInfoWiring::tabCurrentChanged,
            this, &MainWindow::onTabCurrentChanged);

    // PlayerInfoControllerも同期
    m_playerInfoController = m_playerInfoWiring->playerInfoController();

    qCDebug(lcApp).noquote() << "ensurePlayerInfoWiring_: created and connected";
}

// `ensurePreStartCleanupHandler`: Pre Start Cleanup Handler を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensurePreStartCleanupHandler()
{
    if (m_preStartCleanupHandler) return;

    PreStartCleanupHandler::Dependencies deps;
    deps.boardController = m_boardController;
    deps.shogiView = m_shogiView;
    deps.kifuRecordModel = m_models.kifuRecord;
    deps.kifuBranchModel = m_models.kifuBranch;
    deps.lineEditModel1 = m_models.commLog1;
    deps.lineEditModel2 = m_models.commLog2;
    deps.timeController = m_timeController;
    deps.evalChart = m_evalChart;
    deps.evalGraphController = m_evalGraphController;
    deps.recordPane = m_recordPane;
    deps.startSfenStr = &m_state.startSfenStr;
    deps.currentSfenStr = &m_state.currentSfenStr;
    deps.activePly = &m_kifu.activePly;
    deps.currentSelectedPly = &m_kifu.currentSelectedPly;
    deps.currentMoveIndex = &m_state.currentMoveIndex;
    deps.liveGameSession = m_branchNav.liveGameSession;
    deps.branchTree = m_branchNav.branchTree;
    deps.navState = m_branchNav.navState;

    m_preStartCleanupHandler = new PreStartCleanupHandler(deps, this);

    qCDebug(lcApp).noquote() << "ensurePreStartCleanupHandler_: created and connected";
}

// `ensureTurnSyncBridge`: Turn Sync Bridge を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureTurnSyncBridge()
{
    auto* gc = m_gameController;
    auto* tm = findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, this);
}

// `ensurePositionEditController`: Position Edit Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensurePositionEditController()
{
    if (m_posEdit) return;
    m_posEdit = new PositionEditController(this); // 親=MainWindow にして寿命管理
}

// `ensureBoardSyncPresenter`: Board Sync Presenter を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureBoardSyncPresenter()
{
    if (m_boardSync) {
        // sfenRecord ポインタが変わっている場合は更新する
        // （MatchCoordinator 再生成時に sfenRecord のアドレスが変わるため）
        const QStringList* current = sfenRecord();
        m_boardSync->setSfenRecord(current);
        return;
    }

    qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: creating BoardSyncPresenter"
                       << "sfenRecord()*=" << static_cast<const void*>(sfenRecord())
                       << "sfenRecord().size=" << (sfenRecord() ? sfenRecord()->size() : -1);

    BoardSyncPresenter::Deps d;
    d.gc         = m_gameController;
    d.view       = m_shogiView;
    d.bic        = m_boardController;
    d.sfenRecord = sfenRecord();
    d.gameMoves  = &m_kifu.gameMoves;

    m_boardSync = new BoardSyncPresenter(d, this);

    qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: created m_boardSync*=" << static_cast<const void*>(m_boardSync);
}

// `ensureBoardLoadService`: BoardLoadService を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureBoardLoadService()
{
    if (!m_boardLoadService) {
        m_boardLoadService = new BoardLoadService(this);
    }

    ensureBoardSyncPresenter();

    BoardLoadService::Deps deps;
    deps.gc = m_gameController;
    deps.view = m_shogiView;
    deps.boardSync = m_boardSync;
    deps.currentSfenStr = &m_state.currentSfenStr;
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, this);
    deps.ensureBoardSyncPresenter = std::bind(&MainWindow::ensureBoardSyncPresenter, this);
    deps.beginBranchNavGuard = [this]() {
        ensureKifuNavigationCoordinator();
        m_kifuNavCoordinator->beginBranchNavGuard();
    };
    m_boardLoadService->updateDeps(deps);
}

// `ensureConsiderationPositionService`: ConsiderationPositionService を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureConsiderationPositionService()
{
    if (!m_considerationPositionService) {
        m_considerationPositionService = new ConsiderationPositionService(this);
    }

    ConsiderationPositionService::Deps deps;
    deps.playMode = &m_state.playMode;
    deps.positionStrList = &m_kifu.positionStrList;
    deps.gameUsiMoves = &m_kifu.gameUsiMoves;
    deps.gameMoves = &m_kifu.gameMoves;
    deps.startSfenStr = &m_state.startSfenStr;
    deps.kifuLoadCoordinator = m_kifuLoadCoordinator;
    deps.kifuRecordModel = m_models.kifuRecord;
    deps.branchTree = m_branchNav.branchTree;
    deps.navState = m_branchNav.navState;
    deps.match = m_match;
    deps.analysisTab = m_analysisTab;
    m_considerationPositionService->updateDeps(deps);
}

// `ensureAnalysisPresenter`: Analysis Presenter を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureAnalysisPresenter()
{
    if (!m_analysisPresenter)
        m_analysisPresenter = new AnalysisResultsPresenter(this);
}

// `ensureGameStartCoordinator`: MatchCoordinatorWiring に委譲して Menu GameStartCoordinator を遅延生成する。
void MainWindow::ensureGameStartCoordinator()
{
    if (m_gameStart) return;

    ensureMatchCoordinatorWiring();
    m_matchWiring->ensureMenuGameStartCoordinator();
    m_gameStart = m_matchWiring->menuGameStartCoordinator();
}

// `onPreStartCleanupRequested`: Pre Start Cleanup Requested のイベント受信時処理を行う。
void MainWindow::onPreStartCleanupRequested()
{
    ensurePreStartCleanupHandler();
    if (m_preStartCleanupHandler) {
        m_preStartCleanupHandler->performCleanup();
    }
    clearSessionDependentUi();
}

// セッション依存UIコンポーネント（思考・検討・USI/CSAログ・棋譜解析）をクリアする。
void MainWindow::clearSessionDependentUi()
{
    ensureCommentCoordinator();

    MainWindowResetService::SessionUiDeps deps;
    deps.commLog1 = m_models.commLog1;
    deps.commLog2 = m_models.commLog2;
    deps.thinking1 = m_models.thinking1;
    deps.thinking2 = m_models.thinking2;
    deps.consideration = m_models.consideration;
    deps.analysisTab = m_analysisTab;
    deps.analysisModel = m_models.analysis;
    deps.evalChart = m_evalChart;
    deps.evalGraphController = m_evalGraphController;
    deps.broadcastComment = std::bind(&CommentCoordinator::broadcastComment,
                                       m_commentCoordinator,
                                       std::placeholders::_1, std::placeholders::_2);

    const MainWindowResetService resetService;
    resetService.clearSessionDependentUi(deps);
}

// 棋譜読み込み前に全UIをクリアする（共通クリア＋評価値グラフ＋コメント）。
void MainWindow::clearUiBeforeKifuLoad()
{
    ensureCommentCoordinator();

    MainWindowResetService::SessionUiDeps deps;
    deps.commLog1 = m_models.commLog1;
    deps.commLog2 = m_models.commLog2;
    deps.thinking1 = m_models.thinking1;
    deps.thinking2 = m_models.thinking2;
    deps.consideration = m_models.consideration;
    deps.analysisTab = m_analysisTab;
    deps.analysisModel = m_models.analysis;
    deps.evalChart = m_evalChart;
    deps.evalGraphController = m_evalGraphController;
    deps.broadcastComment = std::bind(&CommentCoordinator::broadcastComment,
                                       m_commentCoordinator,
                                       std::placeholders::_1, std::placeholders::_2);

    const MainWindowResetService resetService;
    resetService.clearUiBeforeKifuLoad(deps);
}

// `onApplyTimeControlRequested`: Apply Time Control Requested のイベント受信時処理を行う（SessionLifecycleCoordinatorへ委譲）。
void MainWindow::onApplyTimeControlRequested(const GameStartCoordinator::TimeControl& tc)
{
    ensureSessionLifecycleCoordinator();
    if (m_sessionLifecycle) {
        m_sessionLifecycle->applyTimeControl(tc);
    }
}

// `ensureRecordPresenter`: Record Presenter を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureRecordPresenter()
{
    if (m_recordPresenter) return;

    GameRecordPresenter::Deps d;
    d.model      = m_models.kifuRecord;
    d.recordPane = m_recordPane;

    m_recordPresenter = new GameRecordPresenter(d, this);

    // Presenter → MainWindow：「現在行＋コメント」通知（新方式）
    QObject::connect(
        m_recordPresenter,
        &GameRecordPresenter::currentRowChanged,          // シグナル
        this,
        &MainWindow::onRecordRowChangedByPresenter,       // スロット
        Qt::UniqueConnection
        );
}

// `ensureLiveGameSessionStarted`: Live Game Session Started を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureLiveGameSessionStarted()
{
    ensureLiveGameSessionUpdater();
    m_liveGameSessionUpdater->ensureSessionStarted();
}

void MainWindow::ensureLiveGameSessionUpdater()
{
    if (!m_liveGameSessionUpdater) {
        m_liveGameSessionUpdater = std::make_unique<LiveGameSessionUpdater>();
    }

    LiveGameSessionUpdater::Deps deps;
    deps.liveSession = m_branchNav.liveGameSession;
    deps.branchTree = m_branchNav.branchTree;
    deps.gameController = m_gameController;
    deps.sfenRecord = sfenRecord();
    deps.currentSfenStr = &m_state.currentSfenStr;
    m_liveGameSessionUpdater->updateDeps(deps);
}

void MainWindow::ensureGameRecordUpdateService()
{
    if (!m_gameRecordUpdateService) {
        m_gameRecordUpdateService = std::make_unique<GameRecordUpdateService>();
    }

    GameRecordUpdateService::Deps deps;
    deps.ensureRecordPresenter = [this]() -> GameRecordPresenter* {
        ensureRecordPresenter();
        return m_recordPresenter;
    };
    deps.ensureLiveGameSessionUpdater = [this]() -> LiveGameSessionUpdater* {
        ensureLiveGameSessionUpdater();
        return m_liveGameSessionUpdater.get();
    };
    deps.match = m_match;
    deps.liveGameSession = m_branchNav.liveGameSession;
    deps.gameMoves = &m_kifu.gameMoves;
    deps.gameUsiMoves = &m_kifu.gameUsiMoves;
    deps.currentSfenStr = &m_state.currentSfenStr;
    deps.sfenRecord = sfenRecord();
    m_gameRecordUpdateService->updateDeps(deps);
}

void MainWindow::ensureUndoFlowService()
{
    if (!m_undoFlowService) {
        m_undoFlowService = std::make_unique<UndoFlowService>();
    }

    UndoFlowService::Deps deps;
    deps.match = m_match;
    deps.evalGraphController = m_evalGraphController;
    deps.playMode = &m_state.playMode;
    deps.sfenRecord = sfenRecord();
    m_undoFlowService->updateDeps(deps);
}

void MainWindow::ensureGameRecordLoadService()
{
    if (!m_gameRecordLoadService) {
        m_gameRecordLoadService = std::make_unique<GameRecordLoadService>();
    }

    GameRecordLoadService::Deps deps;
    deps.gameUsiMoves = &m_kifu.gameUsiMoves;
    deps.gameMoves = &m_kifu.gameMoves;
    deps.commentsByRow = &m_kifu.commentsByRow;
    deps.recordPane = m_recordPane;
    deps.ensureRecordPresenter = [this]() -> GameRecordPresenter* {
        ensureRecordPresenter();
        return m_recordPresenter;
    };
    deps.ensureGameRecordModel = [this]() -> GameRecordModel* {
        ensureGameRecordModel();
        return m_models.gameRecord;
    };
    deps.sfenRecord = [this]() -> const QStringList* {
        return sfenRecord();
    };
    m_gameRecordLoadService->updateDeps(deps);
}

void MainWindow::ensureTurnStateSyncService()
{
    if (!m_turnStateSync) {
        m_turnStateSync = std::make_unique<TurnStateSyncService>();
    }

    TurnStateSyncService::Deps deps;
    deps.gameController = m_gameController;
    deps.shogiView = m_shogiView;
    deps.timeController = m_timeController;
    deps.timePresenter = m_timePresenter;
    deps.playMode = &m_state.playMode;
    deps.turnManagerParent = this;
    deps.updateTurnStatus = std::bind(&MainWindow::updateTurnStatus, this, std::placeholders::_1);
    deps.onTurnManagerCreated = [this](TurnManager* tm) {
        connect(tm, &TurnManager::changed,
                this, &MainWindow::onTurnManagerChanged,
                Qt::UniqueConnection);
    };
    m_turnStateSync->updateDeps(deps);
}

// UIスレッド安全のため queued 呼び出しにしています
void MainWindow::initializeNewGameHook(const QString& s)
{
    // --- デバッグ：誰がこの関数を呼び出したか追跡 ---
    qCDebug(lcApp) << "MainWindow::initializeNewGameHook called with sfen:" << s;
    qCDebug(lcApp) << "  PlayMode:" << static_cast<int>(m_state.playMode);
    qCDebug(lcApp) << "  Call stack trace requested";
    
    // --- 司令塔からのコールバック：UI側の初期化のみ行う ---
    QString startSfenStr = s;              // initializeNewGame(QString&) が参照で受けるため可変にコピー

    // 盤モデルの初期化（従来の UI 側初期化）
    initializeNewGame(startSfenStr);

    // 盤の再描画・サイズ調整
    if (m_shogiView && m_gameController && m_gameController->board()) {
        m_shogiView->applyBoardAndRender(m_gameController->board());
        // configureFixedSizing()は削除：Splitterでのリサイズを有効にするため
        // SizePolicyはPreferred（デフォルト）のままにして、resizeEventで自動調整する
    }

    // 表示名の更新（必要に応じて）
    setPlayersNamesForMode();
    setEngineNamesBasedOnMode();
    updateSecondEngineVisibility();
}

// GCの盤面状態をShogiViewへ反映する（MatchCoordinatorフック用）
void MainWindow::renderBoardFromGc()
{
    if (m_shogiView && m_gameController && m_gameController->board()) {
        m_shogiView->applyBoardAndRender(m_gameController->board());
    }
}

// `showMoveHighlights`: 指し手ハイライト表示を盤面コントローラに委譲する。
void MainWindow::showMoveHighlights(const QPoint& from, const QPoint& to)
{
    if (m_boardController) m_boardController->showMoveHighlights(from, to);
}

// ============================================================
void MainWindow::appendKifuLineHook(const QString& text, const QString& elapsed)
{
    // 既存：棋譜欄へ 1手追記（Presenter がモデルへ反映）
    appendKifuLine(text, elapsed);
}

// `onRecordRowChangedByPresenter`: Record Row Changed By Presenter のイベント受信時処理を行う。
void MainWindow::onRecordRowChangedByPresenter(int row, const QString& comment)
{
    qCDebug(lcApp) << "onRecordRowChangedByPresenter called: row=" << row
             << "comment=" << comment.left(30) << "..."
             << "playMode=" << static_cast<int>(m_state.playMode);


    // 未保存コメントの確認
    const int editingRow = (m_analysisTab ? m_analysisTab->currentMoveIndex() : -1);
    if (m_analysisTab && m_analysisTab->hasUnsavedComment()) {
        if (row != editingRow) {
            if (!m_analysisTab->confirmDiscardUnsavedComment()) {
                // キャンセル：元の行に戻す
                if (m_recordPane && m_recordPane->kifuView()) {
                    QTableView* kifuView = m_recordPane->kifuView();
                    if (kifuView->model() && editingRow >= 0 && editingRow < kifuView->model()->rowCount()) {
                        QSignalBlocker blocker(kifuView->selectionModel());
                        kifuView->setCurrentIndex(kifuView->model()->index(editingRow, 0));
                    }
                }
                return;
            }
        }
    }

    // 局面同期は RecordPane::mainRowChanged → RecordNavigationHandler 経路で一元管理する。
    // ここで再度 syncBoardAndHighlightsAtRow() を呼ぶと、本譜ベース同期で
    // 分岐ラインの反映を上書きしてしまうため、コメント通知のみに限定する。

    // コメント表示
    ensureCommentCoordinator();
    const QString cmt = comment.trimmed();
    m_commentCoordinator->broadcastComment(cmt.isEmpty() ? tr("コメントなし") : cmt, true);
}


// `onResignationTriggered`: Resignation Triggered のイベント受信時処理を行う。
void MainWindow::onResignationTriggered()
{
    // 既存の投了処理に委譲（m_matchの有無は中で判定）
    handleResignation();
}

// `getRemainingMsFor`: Remaining Ms For を取得する。
qint64 MainWindow::getRemainingMsFor(MatchCoordinator::Player p) const
{
    if (!m_timeController) {
        qCDebug(lcApp) << "getRemainingMsFor_: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_timeController->getRemainingMs(player);
}

// `getIncrementMsFor`: Increment Ms For を取得する。
qint64 MainWindow::getIncrementMsFor(MatchCoordinator::Player p) const
{
    if (!m_timeController) {
        qCDebug(lcApp) << "getIncrementMsFor_: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_timeController->getIncrementMs(player);
}

// `getByoyomiMs`: Byoyomi Ms を取得する。
qint64 MainWindow::getByoyomiMs() const
{
    if (!m_timeController) {
        qCDebug(lcApp) << "getByoyomiMs_: timeController=null";
        return 0;
    }
    return m_timeController->getByoyomiMs();
}

// 対局終了時のタイトルと本文を受け取り、情報ダイアログを表示するだけのヘルパ
void MainWindow::showGameOverMessageBox(const QString& title, const QString& message)
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showGameOverMessage(title, message);
    }
}

// `onRecordPaneMainRowChanged`: Record Pane Main Row Changed のイベント受信時処理を行う。
void MainWindow::onRecordPaneMainRowChanged(int row)
{
    ensureRecordNavigationHandler();
    m_recordNavWiring->handler()->onMainRowChanged(row);
}

// `onBuildPositionRequired`: Build Position Required のイベント受信時処理を行う。
void MainWindow::onBuildPositionRequired(int row)
{
    ensureConsiderationPositionService();
    m_considerationPositionService->handleBuildPositionRequired(row);
}

// ============================================================
void MainWindow::createAndWireKifuLoadCoordinator()
{
    KifuLoadCoordinatorFactory::createAndWire(*this);
}

// ============================================================
void MainWindow::ensureKifuLoadCoordinatorForLive()
{
    if (m_kifuLoadCoordinator) {
        return; // 既に用意済み
    }

    createAndWireKifuLoadCoordinator();
}

// ============================================================

// 対局が進行中（終局前）かどうかを判定する（PlayModePolicyService に委譲）
bool MainWindow::isGameActivelyInProgress() const
{
    if (m_playModePolicy) {
        return m_playModePolicy->isGameActivelyInProgress();
    }
    return false;
}

// 人間 vs 人間モードかどうかを判定する（PlayModePolicyService に委譲）
bool MainWindow::isHvH() const
{
    if (m_playModePolicy) {
        return m_playModePolicy->isHvH();
    }
    return false;
}

// 指定プレイヤーが人間側かどうかを判定する（PlayModePolicyService に委譲）
bool MainWindow::isHumanSide(ShogiGameController::Player p) const
{
    if (m_playModePolicy) {
        return m_playModePolicy->isHumanSide(p);
    }
    return true;
}

// `updateTurnAndTimekeepingDisplay`: Turn And Timekeeping Display を更新する。
void MainWindow::updateTurnAndTimekeepingDisplay()
{
    ensureTurnStateSyncService();
    m_turnStateSync->updateTurnAndTimekeepingDisplay();
}

// `updateGameRecord`: GameRecordUpdateService へ委譲。
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    ensureGameRecordUpdateService();
    m_gameRecordUpdateService->updateGameRecord(m_state.lastMove, elapsedTime);
    m_state.lastMove.clear();
}

void MainWindow::ensureGameRecordModel()
{
    if (m_models.gameRecord) return;

    ensureCommentCoordinator();

    GameRecordModelBuilder::Deps deps;
    deps.parent = this;
    deps.recordPresenter = m_recordPresenter;
    deps.branchTree = m_branchNav.branchTree;
    deps.navState = m_branchNav.navState;
    deps.commentCoordinator = m_commentCoordinator;
    deps.kifuRecordModel = m_models.kifuRecord;

    m_models.gameRecord = GameRecordModelBuilder::build(deps);
}

void MainWindow::onPvRowClicked(int engineIndex, int row)
{
    ensurePvClickController();
    if (m_pvClickController) {
        // 状態を同期
        m_pvClickController->setPlayMode(m_state.playMode);
        m_pvClickController->setPlayerNames(m_player.humanName1, m_player.humanName2, m_player.engineName1, m_player.engineName2);
        m_pvClickController->setCurrentSfen(m_state.currentSfenStr);
        m_pvClickController->setStartSfen(m_state.startSfenStr);
        // 現在選択されている棋譜行のインデックスを設定（ハイライト用）
        m_pvClickController->setCurrentRecordIndex(m_state.currentMoveIndex);
        // 検討モデルを更新（検討タブから呼ばれた場合に必要）
        m_pvClickController->setConsiderationModel(m_models.consideration);
        m_pvClickController->setBoardFlipped(m_shogiView ? m_shogiView->getFlipMode() : false);
        m_pvClickController->onPvRowClicked(engineIndex, row);
    }
}

// `onPvDialogClosed`: Pv Dialog Closed のイベント受信時処理を行う。
void MainWindow::onPvDialogClosed(int engineIndex)
{
    if (m_analysisTab) {
        m_analysisTab->clearThinkingViewSelection(engineIndex);
    }
}

void MainWindow::onTabCurrentChanged(int index)
{
    // タブインデックスを設定ファイルに保存
    SettingsService::setLastSelectedTabIndex(index);
    qCDebug(lcApp).noquote() << "onTabCurrentChanged: saved tab index =" << index;
}

// ============================================================
// CSA通信対局関連のスロット（CsaGameWiring経由）
// ============================================================

// `onJosekiForcedPromotion`: Joseki Forced Promotion のイベント受信時処理を行う。
void MainWindow::onJosekiForcedPromotion(bool forced, bool promote)
{
    if (m_gameController) {
        m_gameController->setForcedPromotion(forced, promote);
    }
}

// ============================================================
// 言語設定
// ============================================================

void MainWindow::onToolBarVisibilityToggled(bool visible)
{
    if (ui->toolBar) {
        ui->toolBar->setVisible(visible);
    }
    SettingsService::setToolbarVisible(visible);
}

// ============================================================
// 新規コントローラのensure関数
// ============================================================

void MainWindow::ensureJishogiController()
{
    if (m_jishogiController) return;
    m_jishogiController = new JishogiScoreDialogController(this);
}

// `ensureNyugyokuHandler`: Nyugyoku Handler を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureNyugyokuHandler()
{
    if (m_nyugyokuHandler) return;
    m_nyugyokuHandler = new NyugyokuDeclarationHandler(this);
}

// `ensureConsecutiveGamesController`: Consecutive Games Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureConsecutiveGamesController()
{
    if (m_consecutiveGamesController) return;

    m_consecutiveGamesController = new ConsecutiveGamesController(this);
    m_consecutiveGamesController->setTimeController(m_timeController);
    m_consecutiveGamesController->setGameStartCoordinator(m_gameStart);

    // シグナル接続
    connect(m_consecutiveGamesController, &ConsecutiveGamesController::requestPreStartCleanup,
            this, &MainWindow::onPreStartCleanupRequested);
    connect(m_consecutiveGamesController, &ConsecutiveGamesController::requestStartNextGame,
            this, [this](const MatchCoordinator::StartOptions& opt, const GameStartCoordinator::TimeControl& tc) {
                ensureGameStartCoordinator();
                if (m_gameStart) {
                    GameStartCoordinator::StartParams params;
                    params.opt = opt;
                    params.tc = tc;
                    params.autoStartEngineMove = true;
                    m_gameStart->start(params);
                }
            });
}

// `ensureLanguageController`: Language Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureLanguageController()
{
    if (m_languageController) return;

    m_languageController = new LanguageController(this);
    m_languageController->setParentWidget(this);
    m_languageController->setActions(
        ui->actionLanguageSystem,
        ui->actionLanguageJapanese,
        ui->actionLanguageEnglish);
}

// `ensureConsiderationWiring`: CompositionRoot に委譲して生成・依存設定を行う。
void MainWindow::ensureConsiderationWiring()
{
    if (m_considerationWiring) return;

    MainWindowDepsFactory::ConsiderationWiringCallbacks cbs;
    cbs.ensureDialogCoordinator = [this]() {
        ensureDialogCoordinator();
        if (m_considerationWiring)
            m_considerationWiring->setDialogCoordinator(m_dialogCoordinator);
    };

    m_compositionRoot->ensureConsiderationWiring(buildRuntimeRefs(), cbs, this, m_considerationWiring);
}

// `ensureDockLayoutManager`: Dock Layout Manager を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureDockLayoutManager()
{
    if (m_dockLayoutManager) return;

    m_dockLayoutManager = new DockLayoutManager(this, this);

    // ドックを登録
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Menu, m_docks.menuWindow);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Joseki, m_docks.josekiWindow);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Record, m_docks.recordPane);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::GameInfo, m_docks.gameInfo);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Thinking, m_docks.thinking);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Consideration, m_docks.consideration);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::UsiLog, m_docks.usiLog);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::CsaLog, m_docks.csaLog);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Comment, m_docks.comment);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::BranchTree, m_docks.branchTree);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::EvalChart, m_docks.evalChart);

    // 保存済みレイアウトメニューを設定
    m_dockLayoutManager->setSavedLayoutsMenu(ui->menuSavedLayouts);
}

// `ensureDockCreationService`: Dock Creation Service を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureDockCreationService()
{
    if (m_dockCreationService) return;

    m_dockCreationService = new DockCreationService(this, this);
    m_dockCreationService->setDisplayMenu(ui->Display);
}

// `ensureCommentCoordinator`: CompositionRoot に委譲して生成・依存設定を行う。
void MainWindow::ensureCommentCoordinator()
{
    if (m_commentCoordinator) return;
    m_compositionRoot->ensureCommentCoordinator(buildRuntimeRefs(), this, m_commentCoordinator);
    connect(m_commentCoordinator, &CommentCoordinator::ensureGameRecordModelRequested,
            this, &MainWindow::ensureGameRecordModel);
}

// `ensureUsiCommandController`: Usi Command Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureUsiCommandController()
{
    if (!m_usiCommandController) {
        m_usiCommandController = new UsiCommandController(this);
    }
    m_usiCommandController->setMatchCoordinator(m_match);
    m_usiCommandController->setAnalysisTab(m_analysisTab);
}

// `ensureRecordNavigationHandler`: CompositionRoot に委譲して生成・配線・依存設定を行う。
void MainWindow::ensureRecordNavigationHandler()
{
    auto refs = buildRuntimeRefs();
    m_compositionRoot->ensureRecordNavigationWiring(refs, this, m_recordNavWiring);
}

// `ensureUiStatePolicyManager`: CompositionRoot に委譲して生成・依存設定を行う。
void MainWindow::ensureUiStatePolicyManager()
{
    m_compositionRoot->ensureUiStatePolicyManager(buildRuntimeRefs(), this, m_uiStatePolicy);
}

// `ensureKifuNavigationCoordinator`: KifuNavigationCoordinator を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureKifuNavigationCoordinator()
{
    if (!m_kifuNavCoordinator) {
        m_kifuNavCoordinator = new KifuNavigationCoordinator(this);
    }

    KifuNavigationCoordinator::Deps deps;

    // UI
    deps.recordPane = m_recordPane;
    deps.kifuRecordModel = m_models.kifuRecord;

    // Board sync
    deps.boardSync = m_boardSync;
    deps.shogiView = m_shogiView;

    // Navigation state
    deps.navState = m_branchNav.navState;
    deps.branchTree = m_branchNav.branchTree;

    // External state pointers
    deps.activePly = &m_kifu.activePly;
    deps.currentSelectedPly = &m_kifu.currentSelectedPly;
    deps.currentMoveIndex = &m_state.currentMoveIndex;
    deps.currentSfenStr = &m_state.currentSfenStr;
    deps.skipBoardSyncForBranchNav = &m_state.skipBoardSyncForBranchNav;
    deps.playMode = &m_state.playMode;

    // Coordinators
    deps.match = m_match;
    deps.analysisTab = m_analysisTab;
    deps.uiStatePolicy = m_uiStatePolicy;

    // Callbacks
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, this);
    deps.updateJosekiWindow = std::bind(&MainWindow::updateJosekiWindow, this);
    deps.ensureBoardSyncPresenter = std::bind(&MainWindow::ensureBoardSyncPresenter, this);
    deps.isGameActivelyInProgress = std::bind(&MainWindow::isGameActivelyInProgress, this);
    deps.getSfenRecord = [this]() -> QStringList* { return sfenRecord(); };

    m_kifuNavCoordinator->updateDeps(deps);
}

// `ensureSessionLifecycleCoordinator`: SessionLifecycleCoordinator を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureSessionLifecycleCoordinator()
{
    if (!m_sessionLifecycle) {
        m_sessionLifecycle = new SessionLifecycleCoordinator(this);
    }

    SessionLifecycleCoordinator::Deps deps;

    // コントローラ
    deps.replayController = m_replayController;
    deps.gameController = m_gameController;
    deps.gameStateController = m_gameStateController;

    // 状態ポインタ
    deps.playMode = &m_state.playMode;
    deps.startSfenStr = &m_state.startSfenStr;
    deps.currentSfenStr = &m_state.currentSfenStr;
    deps.currentSelectedPly = &m_kifu.currentSelectedPly;

    // コールバック（リセット処理用）
    deps.clearGameStateFields = std::bind(&MainWindow::clearGameStateFields, this);
    deps.resetEngineState = std::bind(&MainWindow::resetEngineState, this);
    deps.onPreStartCleanupRequested = std::bind(&MainWindow::onPreStartCleanupRequested, this);
    deps.resetModels = std::bind(&MainWindow::resetModels, this, std::placeholders::_1);
    deps.resetUiState = std::bind(&MainWindow::resetUiState, this, std::placeholders::_1);

    // コールバック（startNewGame用）
    deps.clearEvalState = std::bind(&MainWindow::clearEvalState, this);
    deps.unlockGameOverStyle = std::bind(&MainWindow::unlockGameOverStyle, this);
    deps.startGame = std::bind(&MainWindow::invokeStartGame, this);

    // handleGameEnded 用
    deps.timeController = m_timeController;
    deps.consecutiveGamesController = m_consecutiveGamesController;
    deps.updateEndTime = [this](const QDateTime& endTime) {
        ensurePlayerInfoWiring();
        if (m_playerInfoWiring) {
            m_playerInfoWiring->updateGameInfoWithEndTime(endTime);
        }
    };
    deps.startNextConsecutiveGame = std::bind(&MainWindow::startNextConsecutiveGame, this);

    // applyTimeControl 用
    deps.match = m_match;
    deps.shogiView = m_shogiView;
    deps.lastTimeControl = &m_lastTimeControl;
    deps.updateGameInfoWithTimeControl = [this](bool enabled, qint64 baseMs, qint64 byoyomiMs, qint64 incMs) {
        ensurePlayerInfoWiring();
        if (m_playerInfoWiring) {
            m_playerInfoWiring->updateGameInfoWithTimeControl(enabled, baseMs, byoyomiMs, incMs);
        }
    };

    m_sessionLifecycle->updateDeps(deps);
}
