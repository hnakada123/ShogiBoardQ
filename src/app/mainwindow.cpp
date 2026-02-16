/// @file mainwindow.cpp
/// @brief アプリケーションのメインウィンドウ実装

#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QFileInfo>
#include <QToolButton>
#include <QWidgetAction>
#include <QMetaType>
#include <QScrollBar>
#include <QScrollArea>
#include <QPushButton>
#include <QDialog>
#include <QSettings>
#include <QSignalBlocker>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QElapsedTimer>
#include <QTimer>
#include <QSizePolicy>
#include <functional>
#include <limits>
#include <QPainter>
#include <QFontDatabase>
#include <QRegularExpression>

#include "mainwindow.h"
#include "considerationflowcontroller.h"
#include "shogiutils.h"
#include "gamelayoutbuilder.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"
#include "tsumesearchflowcontroller.h"
#include "tsumepositionutil.h"
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
#include "timekeepingservice.h"
#include "positioneditcontroller.h"
#include "analysisresultspresenter.h"
#include "boardsyncpresenter.h"
#include "gamestartcoordinator.h"
#include "navigationpresenter.h"
#include "analysiscoordinator.h"
#include "kifuanalysislistmodel.h"
#include "analysiscoordinator.h"
#include "gamerecordpresenter.h"
#include "timecontrolutil.h"
#include "analysisflowcontroller.h"
#include "sfenutils.h"
#include "turnsyncbridge.h"
#include "promotionflow.h"
#include "kifusavecoordinator.h"
#include "evalgraphpresenter.h"
#include "settingsservice.h"
#include "aboutcoordinator.h"
#include "enginesettingscoordinator.h"
#include "analysistabwiring.h"
#include "recordpanewiring.h"
#include "recordpane.h"
#include "uiactionswiring.h"
#include "gamerecordmodel.h"
#include "pvboarddialog.h"
#include "kifupastedialog.h"
#include "sfencollectiondialog.h"
#include "gameinfopanecontroller.h"
#include "kifuclipboardservice.h"
#include "evaluationgraphcontroller.h"
#include "timecontrolcontroller.h"
#include "replaycontroller.h"
#include "dialogcoordinator.h"
#include "kifuexportcontroller.h"
#include "gamestatecontroller.h"
#include "playerinfocontroller.h"
#include "boardsetupcontroller.h"
#include "pvclickcontroller.h"
#include "positioneditcoordinator.h"
#include "csagamedialog.h"
#include "csagamecoordinator.h"
#include "csawaitingdialog.h"
#include "josekiwindowwiring.h"
#include "menuwindowwiring.h"
#include "csagamewiring.h"
#include "playerinfowiring.h"
#include "considerationwiring.h"        // 検討モードUI配線

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
#include "considerationmodeuicontroller.h"
#include "docklayoutmanager.h"
#include "dockcreationservice.h"
#include "commentcoordinator.h"
#include "usicommandcontroller.h"
#include "recordnavigationhandler.h"
#include "uistatepolicymanager.h"

Q_LOGGING_CATEGORY(lcApp, "shogi.app")

// MainWindow を初期化し、主要コンポーネントを構築する。
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_lineEditModel1(new UsiCommLogModel(this))
    , m_lineEditModel2(new UsiCommLogModel(this))
{
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

    // 評価値グラフ高さ調整用タイマーを初期化（デバウンス処理用）
    m_evalChartResizeTimer = new QTimer(this);
    m_evalChartResizeTimer->setSingleShot(true);
    connect(m_evalChartResizeTimer, &QTimer::timeout,
            this, &MainWindow::performDeferredEvalChartResize);
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

// `buildGamePanels`: Game Panels を構築する。
void MainWindow::buildGamePanels()
{
    // MainWindow が管理する主要パネルを依存順に構築する。
    // RecordPane を先に用意することで、後続の分岐ナビ初期化が
    // ボタン/ビュー参照を安全に取得できる。

    // 1) 記録ペイン（RecordPane）など UI 部の初期化
    setupRecordPane();

    // 2) 棋譜欄をQDockWidgetとして作成
    createRecordPaneDock();

    // 3) 将棋盤・駒台の初期化（従来順序を維持）
    startNewShogiGame(m_startSfenStr);

    // 4) 将棋盤をQDockWidgetとして作成
    setupBoardInCenter();

    // 5) エンジン解析タブの構築
    setupEngineAnalysisTab();

    // 6) 解析用ドックを作成（独立した複数ドック）
    createAnalysisDocks();

    // 7) 分岐ナビゲーションクラスの初期化
    initializeBranchNavigationClasses();

    // 8) 評価値グラフのQDockWidget作成
    createEvalChartDock();

    // 9) ドックを固定アクションの初期状態を設定
    ui->actionLockDocks->setChecked(SettingsService::docksLocked());

    // 10) メニューウィンドウのQDockWidget作成（デフォルトは非表示）
    createMenuWindowDock();

    // 11) 定跡ウィンドウのQDockWidget作成（デフォルトは非表示）
    createJosekiWindowDock();

    // 12) 棋譜解析結果のQDockWidget作成（初期状態は非表示）
    createAnalysisResultsDock();

    // 13) central への初期化（主要ウィジェットはドックへ移行済み）
    initializeCentralGameDisplay();

    // 14) ドックレイアウト関連アクションをDockLayoutManagerに配線
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->wireMenuActions(
            ui->actionResetDockLayout,
            ui->actionSaveDockLayout,
            /*clearStartupLayout*/nullptr,
            ui->actionLockDocks,
            ui->menuSavedLayouts);
    }
}

// `restoreWindowAndSync`: Window And Sync を復元する。
void MainWindow::restoreWindowAndSync()
{
    loadWindowSettings();

    // 起動時のカスタムレイアウトがあれば復元
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->restoreStartupLayoutIfSet();
    }
}

// `connectAllActions`: All Actions のシグナル接続を行う。
void MainWindow::connectAllActions()
{
    // 既存があれば使い回し
    if (!m_actionsWiring) {
        UiActionsWiring::Deps d;
        d.ui        = ui;
        d.shogiView = m_shogiView;
        d.ctx       = this; // MainWindow のスロットに繋ぐ
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
                this, &MainWindow::displayPromotionDialog, Qt::UniqueConnection);

        connect(m_gameController, &ShogiGameController::endDragSignal,
                m_shogiView,      &ShogiView::endDrag, Qt::UniqueConnection);

        connect(m_gameController, &ShogiGameController::moveCommitted,
                this, &MainWindow::onMoveCommitted, Qt::UniqueConnection);
    }
    if (m_shogiView) {
        connect(m_shogiView, &ShogiView::errorOccurred,
                this, &MainWindow::displayErrorMessage, Qt::UniqueConnection);
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

// `finalizeCoordinators`: Coordinators の最終初期化を行う。
void MainWindow::finalizeCoordinators()
{
    initMatchCoordinator();
    setupNameAndClockFonts();
    ensurePositionEditController();
    ensureBoardSyncPresenter();
    ensureAnalysisPresenter();
}

// `onErrorBusOccurred`: Error Bus Occurred のイベント受信時処理を行う。
void MainWindow::onErrorBusOccurred(const QString& msg)
{
    displayErrorMessage(msg);
}

// GUIを構成するWidgetなどを生成する。（リファクタ後）
void MainWindow::initializeComponents()
{
    // --- Core models ---
    // ShogiGameController は QObject 親を付けてリーク防止
    if (!m_gameController) {
        m_gameController = new ShogiGameController(this);
    } else {
        // 再初期化に備えて、必要ならシグナル切断などをここで行う
        // m_gameController->disconnect(this);
    }

    // 局面履歴（SFEN列）は MatchCoordinator 側で所有する。
    // MainWindow初期化時点で司令塔が未生成の場合があるため、ここでは触らない。

    // 棋譜表示用のレコードリストを確保（ここでは容器だけ用意）
    if (!m_moveRecords) m_moveRecords = new QList<KifuDisplay *>;
    else                m_moveRecords->clear();

    // ゲーム中の指し手リストをクリア
    m_gameMoves.clear();
    m_gameUsiMoves.clear();


    // --- View ---
    if (!m_shogiView) {
        m_shogiView = new ShogiView(this);     // 親をMainWindowに
        m_shogiView->setNameFontScale(0.30);   // 好みの倍率（表示前に設定）
    } else {
        // 再初期化時も念のためパラメータを揃える
        m_shogiView->setParent(this);
        m_shogiView->setNameFontScale(0.30);
    }

    // 盤・駒台操作の配線（BoardInteractionController など）
    setupBoardInteractionController();

    // --- Board model 初期化 ---
    // m_startSfenStr が "startpos ..." の場合は必ず完全 SFEN に正規化してから newGame。
    QString start = m_startSfenStr;
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

    // --- Turn 初期化 & 同期 ---
    // 1) TurnManager 側の初期手番（b→Player1）を立ち上げる
    setCurrentTurn();

    // 2) GC ↔ TurnManager のブリッジ確立＆初期同期（内部で gc->currentPlayer() を反映）
    ensureTurnSyncBridge();

    // --- 表示名・ログモデル名の初期反映 ---
    // setPlayersNamesForMode / setEngineNamesBasedOnMode がサービスへ移設済みでも呼び出し名は同じ
    setPlayersNamesForMode();
    setEngineNamesBasedOnMode();
    updateSecondEngineVisibility();
}

void MainWindow::displayErrorMessage(const QString& errorMessage)
{
    m_errorOccurred = true;

    m_shogiView->setErrorOccurred(m_errorOccurred);

    QMessageBox::critical(this, tr("Error"), errorMessage);
}


// 待ったボタンを押すと、2手戻る。
void MainWindow::undoLastTwoMoves()
{
    if (m_match) {
        // 2手戻しを実行し、成功した場合は分岐ツリーを更新する
        if (m_match->undoTwoPlies()) {
            // 短くなった棋譜データに基づいて分岐ツリー（長方形と罫線）を再描画・同期する
            refreshBranchTreeLive();

            // 評価値グラフのプロットを1つ削除（2手で1プロット）
            // ※EvEモードでは「待った」は使用しない想定
            if (m_evalGraphController) {
                // HvE: 先手(Human) vs 後手(Engine) → P2（後手エンジン）のグラフを使用
                // EvH: 先手(Engine) vs 後手(Human) → P1（先手エンジン）のグラフを使用
                switch (m_playMode) {
                case PlayMode::EvenHumanVsEngine:
                case PlayMode::HandicapHumanVsEngine:
                    // 後手がエンジン → P2を削除
                    m_evalGraphController->removeLastP2Score();
                    break;
                case PlayMode::EvenEngineVsHuman:
                case PlayMode::HandicapEngineVsHuman:
                    // 先手がエンジン → P1を削除
                    m_evalGraphController->removeLastP1Score();
                    break;
                default:
                    // その他のモード（EvE、HvH、解析モード等）では何もしない
                    break;
                }

                // 縦線（カーソルライン）を現在の手数位置に更新
                // sfenRecordのサイズ - 1 が現在の手数（ply）
                // sfenRecord: [開局SFEN, 1手目後SFEN, 2手目後SFEN, ...]
                // size=1 → ply=0（開局）, size=2 → ply=1, size=3 → ply=2, ...
                const int currentPly = sfenRecord() 
                    ? static_cast<int>(qMax(static_cast<qsizetype>(0), sfenRecord()->size() - 1)) 
                    : 0;
                m_evalGraphController->setCurrentPly(currentPly);
            }
        }
    }
}

// `initializeNewGame`: New Game を初期化する。
void MainWindow::initializeNewGame(QString& startSfenStr)
{
    if (m_gameController) {
        m_gameController->newGame(startSfenStr);
    }
    // ← ここは Coordinator に委譲
    GameStartCoordinator::applyResumePositionIfAny(m_gameController, m_shogiView, m_resumeSfenStr);
}

// 対局モードに応じて将棋盤上部に表示される対局者名をセットする。
void MainWindow::setPlayersNamesForMode()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_playMode);
        m_playerInfoController->setHumanNames(m_humanName1, m_humanName2);
        m_playerInfoController->setEngineNames(m_engineName1, m_engineName2);
        m_playerInfoController->applyPlayersNamesForMode();
    }
}

// 駒台を含む将棋盤全体の画像をクリップボードにコピーする。
void MainWindow::copyBoardToClipboard()
{
    BoardImageExporter::copyToClipboard(m_shogiView);
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
        m_playerInfoController->setPlayMode(m_playMode);
        m_playerInfoController->setEngineNames(m_engineName1, m_engineName2);
        m_playerInfoController->applyEngineNamesToLogModels();
    }
}

void MainWindow::updateSecondEngineVisibility()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_playMode);
        m_playerInfoController->updateSecondEngineVisibility();
    }
}

// 以前は対局者名と残り時間、将棋盤のレイアウトを構築していた。
void MainWindow::setupHorizontalGameLayout()
{
    // 何もしない（すべてのウィジェットはドックに移動）
}

// `initializeCentralGameDisplay`: Central Game Display を初期化する。
void MainWindow::initializeCentralGameDisplay()
{
    // セントラルウィジェットには将棋盤が配置されている
    // setupBoardInCenter() で設定済みのため、レイアウトはクリアしない

    // GameLayoutBuilder は不要になったのでクリーンアップ
    if (m_layoutBuilder) {
        delete m_layoutBuilder;
        m_layoutBuilder = nullptr;
    }
    m_hsplit = nullptr;  // 使用しなくなったためクリア
}

// `startNewShogiGame`: New Shogi Game を開始する。
void MainWindow::startNewShogiGame(QString& startSfenStr)
{
    ensureReplayController();
    const bool resume = m_replayController ? m_replayController->isResumeFromCurrent() : false;

    // 対局終了時のスタイルロックを解除
    if (m_shogiView) m_shogiView->setGameOverStyleLock(false);

    // 評価値グラフ等の初期化
    if (m_evalChart && !resume) {
        m_evalChart->clearAll();
    }
    if (!resume) {
        ensureEvaluationGraphController();
        if (m_evalGraphController) {
            m_evalGraphController->clearScores();
        }
        if (m_recordPresenter) {
            m_recordPresenter->clearLiveDisp();
        }
    }

    // 司令塔が未用意なら作る
    if (!m_match) {
        initMatchCoordinator();
    }
    if (!m_match) return;

    m_match->prepareAndStartGame(
        m_playMode,
        startSfenStr,
        sfenRecord(),
        m_startGameDialog,
        m_bottomIsP1
        );
}

// 棋譜欄の下の矢印ボタンを無効にする。
void MainWindow::disableArrowButtons()
{
    if (m_recordPane) m_recordPane->setArrowButtonsEnabled(false);
}

// 棋譜欄の下の矢印ボタンを有効にする。
void MainWindow::enableArrowButtons()
{
    if (m_recordPane) m_recordPane->setArrowButtonsEnabled(true);
}



// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    // CSA通信対局モードの場合
    if (m_playMode == PlayMode::CsaNetworkMode && m_csaGameCoordinator) {
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
    m_evalGraphController->setEngine1Name(m_engineName1);
    m_evalGraphController->setEngine2Name(m_engineName2);

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

// `displayVersionInformation`: Version Information を表示する。
void MainWindow::displayVersionInformation()
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showVersionInformation();
    }
}

// `displayJishogiScoreDialog`: Jishogi Score Dialog を表示する。
void MainWindow::displayJishogiScoreDialog()
{
    if (!m_shogiView || !m_shogiView->board()) {
        QMessageBox::warning(this, tr("エラー"), tr("盤面データがありません。"));
        return;
    }

    ensureJishogiController();
    if (m_jishogiController) {
        m_jishogiController->showDialog(this, m_shogiView->board());
    }
}

// `handleNyugyokuDeclaration`: Nyugyoku Declaration を処理する。
void MainWindow::handleNyugyokuDeclaration()
{
    // 盤面データの確認
    if (!m_shogiView || !m_shogiView->board()) {
        QMessageBox::warning(this, tr("エラー"), tr("盤面データがありません。"));
        return;
    }

    ensureNyugyokuHandler();
    if (m_nyugyokuHandler) {
        m_nyugyokuHandler->setGameController(m_gameController);
        m_nyugyokuHandler->setMatchCoordinator(m_match);
        m_nyugyokuHandler->handleDeclaration(this, m_shogiView->board(), static_cast<int>(m_playMode));
    }
}

// `openWebsiteInExternalBrowser`: Website In External Browser を開く。
void MainWindow::openWebsiteInExternalBrowser()
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->openProjectWebsite();
    }
}

// `displayEngineSettingsDialog`: Engine Settings Dialog を表示する。
void MainWindow::displayEngineSettingsDialog()
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showEngineSettingsDialog();
    }
}

// 成る・不成の選択ダイアログを起動する。
void MainWindow::displayPromotionDialog()
{
    if (!m_gameController) return;
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        const bool promote = m_dialogCoordinator->showPromotionDialog();
        m_gameController->setPromote(promote);
    }
}

// 定跡ウィンドウ（ドック）を表示/非表示にトグルする。
void MainWindow::displayJosekiWindow()
{
    // ドックが未作成の場合は作成
    if (!m_josekiWindowDock) {
        createJosekiWindowDock();
    }

    // ドックの表示/非表示をトグル
    if (m_josekiWindowDock) {
        if (m_josekiWindowDock->isVisible()) {
            m_josekiWindowDock->hide();
        } else {
            m_josekiWindowDock->show();
            m_josekiWindowDock->raise();
        }
    }
}

// `updateJosekiWindow`: Joseki Window を更新する。
void MainWindow::updateJosekiWindow()
{
    // ドックが非表示の場合は更新をスキップ
    if (m_josekiWindowDock && !m_josekiWindowDock->isVisible()) {
        return;
    }
    if (m_josekiWiring) {
        m_josekiWiring->updateJosekiWindow();
    }
}

// `displayMenuWindow`: Menu Window を表示する。
void MainWindow::displayMenuWindow()
{
    // ドックが未作成の場合は作成
    if (!m_menuWindowDock) {
        createMenuWindowDock();
    }

    // ドックの表示/非表示をトグル
    if (m_menuWindowDock) {
        if (m_menuWindowDock->isVisible()) {
            m_menuWindowDock->hide();
        } else {
            m_menuWindowDock->show();
            m_menuWindowDock->raise();
        }
    }
}

// `displayCsaGameDialog`: Csa Game Dialog を表示する。
void MainWindow::displayCsaGameDialog()
{
    // ダイアログが未作成の場合は作成する
    if (!m_csaGameDialog) {
        m_csaGameDialog = new CsaGameDialog(this);
    }

    // ダイアログを表示する
    if (m_csaGameDialog->exec() == QDialog::Accepted) {
        // CsaGameWiringを確保
        ensureCsaGameWiring();
        if (!m_csaGameWiring) {
            qCWarning(lcApp).noquote() << "displayCsaGameDialog: CsaGameWiring is null";
            return;
        }

        // BoardSetupControllerを確保して設定
        ensureBoardSetupController();
        m_csaGameWiring->setBoardSetupController(m_boardSetupController);
        m_csaGameWiring->setAnalysisTab(m_analysisTab);

        // プレイモードをCSA通信対局に設定
        m_playMode = PlayMode::CsaNetworkMode;

        // CSA対局を開始（ロジックはCsaGameWiringに委譲）
        m_csaGameWiring->startCsaGame(m_csaGameDialog, this);

        // CsaGameCoordinatorの参照を保持（他のコードとの互換性のため）
        m_csaGameCoordinator = m_csaGameWiring->coordinator();

        // CSA対局でエンジンを使用する場合、評価値グラフ更新用のシグナル接続
        if (m_csaGameCoordinator) {
            connect(m_csaGameCoordinator, &CsaGameCoordinator::engineScoreUpdated,
                    this, &MainWindow::onCsaEngineScoreUpdated,
                    Qt::UniqueConnection);
        }
    }
}

// `displayTsumeShogiSearchDialog`: Tsume Shogi Search Dialog を表示する。
void MainWindow::displayTsumeShogiSearchDialog()
{
    // 解析モード切替
    m_playMode = PlayMode::TsumiSearchMode;

    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showTsumeSearchDialogFromContext();
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

// 棋譜解析ダイアログを表示する。
void MainWindow::displayKifuAnalysisDialog()
{
    qCDebug(lcApp).noquote() << "displayKifuAnalysisDialog START";

    // 解析モードに遷移
    m_playMode = PlayMode::AnalysisMode;

    // 解析モデルが未生成ならここで作成
    if (!m_analysisModel) {
        m_analysisModel = new KifuAnalysisListModel(this);
    }

    ensureDialogCoordinator();
    if (!m_dialogCoordinator) return;

    // 依存オブジェクトを確保
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->ensureGameInfoController();
        m_gameInfoController = m_playerInfoWiring->gameInfoController();
    }
    ensureAnalysisPresenter();

    // 解析に必要な依存オブジェクトを設定
    m_dialogCoordinator->setAnalysisModel(m_analysisModel);
    m_dialogCoordinator->setAnalysisTab(m_analysisTab);
    m_dialogCoordinator->setUsiEngine(m_usi1);
    m_dialogCoordinator->setLogModel(m_lineEditModel1);
    m_dialogCoordinator->setThinkingModel(m_modelThinking1);

    // 棋譜解析コンテキストを更新（遅延初期化されたオブジェクトを反映）
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.sfenRecord = sfenRecord();
    kifuCtx.moveRecords = m_moveRecords;
    kifuCtx.recordModel = m_kifuRecordModel;
    kifuCtx.activePly = &m_activePly;
    kifuCtx.gameController = m_gameController;
    kifuCtx.gameInfoController = m_gameInfoController;
    kifuCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    kifuCtx.evalChart = m_evalChart;
    kifuCtx.gameUsiMoves = &m_gameUsiMoves;
    kifuCtx.presenter = m_analysisPresenter;
    kifuCtx.getBoardFlipped = [this]() { return m_shogiView ? m_shogiView->getFlipMode() : false; };
    m_dialogCoordinator->setKifuAnalysisContext(kifuCtx);

    // コンテキストから自動パラメータ構築してダイアログを表示
    m_dialogCoordinator->showKifuAnalysisDialogFromContext();
}

// `cancelKifuAnalysis`: Kifu Analysis を中止する。
void MainWindow::cancelKifuAnalysis()
{
    qCDebug(lcApp).noquote() << "cancelKifuAnalysis called";
    
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        if (m_dialogCoordinator->isKifuAnalysisRunning()) {
            m_dialogCoordinator->stopKifuAnalysis();
            
            // 解析前のモードに戻す
            m_playMode = PlayMode::NotStarted;
            
            qCDebug(lcApp).noquote() << "cancelKifuAnalysis: analysis cancelled";
        } else {
            qCDebug(lcApp).noquote() << "cancelKifuAnalysis: no analysis running";
        }
    }
}

// `onKifuAnalysisProgress`: Kifu Analysis Progress のイベント受信時処理を行う。
void MainWindow::onKifuAnalysisProgress(int ply, int scoreCp)
{
    qCDebug(lcApp).noquote() << "onKifuAnalysisProgress: ply=" << ply << "scoreCp=" << scoreCp;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    navigateKifuViewToRow(ply);

    // 2) 評価値グラフに評価値をプロット
    //    ※AnalysisFlowControllerで既に先手視点に変換済み（後手番は符号反転済み）
    //    ※scoreCpがINT_MINの場合は位置移動のみ（評価値追加しない）
    static constexpr int POSITION_ONLY_MARKER = std::numeric_limits<int>::min();
    if (scoreCp != POSITION_ONLY_MARKER && m_evalChart) {
        m_evalChart->appendScoreP1(ply, scoreCp, false);
        qCDebug(lcApp).noquote() << "onKifuAnalysisProgress: appended score to chart: ply=" << ply
                           << "scoreCp=" << scoreCp;
    } else if (scoreCp == POSITION_ONLY_MARKER) {
        qCDebug(lcApp).noquote() << "onKifuAnalysisProgress: position only (no score update)";
    } else {
        qCDebug(lcApp).noquote() << "onKifuAnalysisProgress: m_recordPane is null!";
    }
}

// `onKifuAnalysisResultRowSelected`: Kifu Analysis Result Row Selected のイベント受信時処理を行う。
void MainWindow::onKifuAnalysisResultRowSelected(int row)
{
    qCDebug(lcApp).noquote() << "onKifuAnalysisResultRowSelected: row=" << row;

    // rowは解析結果の行番号で、plyに相当する
    int ply = row;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    navigateKifuViewToRow(ply);

    // 2) 分岐ツリーの該当手数をハイライト
    if (m_analysisTab) {
        qCDebug(lcApp).noquote() << "onKifuAnalysisResultRowSelected: calling highlightBranchTreeAt(0, " << ply << ")";
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, ply, /*centerOn=*/true);
    }
}

// TurnManager::changed を受けて UI/Clock を更新（＋手番を GameController に同期）
void MainWindow::onTurnManagerChanged(ShogiGameController::Player now)
{
    // 1) UI側（先手=1, 後手=2）に反映
    const int cur = (now == ShogiGameController::Player2) ? 2 : 1;
    updateTurnStatus(cur);

    // 2) 盤モデルの手番は GC に集約
    if (m_gameController) {
        m_gameController->setCurrentPlayer(now);
    }

    if (m_shogiView) {
        m_shogiView->updateTurnIndicator(now);
    }

    // （必要なら：ここでログやステータスバー更新などを追加）
}

// 現在の手番を設定する。
void MainWindow::setCurrentTurn()
{
    // TurnManager を確保
    TurnManager* tm = this->findChild<TurnManager*>(QStringLiteral("TurnManager"));
    if (!tm) {
        tm = new TurnManager(this);
        tm->setObjectName(QStringLiteral("TurnManager"));
        // ラムダを使わず、メンバ関数に接続
        connect(tm, &TurnManager::changed,
                this, &MainWindow::onTurnManagerChanged,
                Qt::UniqueConnection);
    }

    const bool isLivePlayMode =
        (m_playMode == PlayMode::HumanVsHuman) ||
        (m_playMode == PlayMode::EvenHumanVsEngine) ||
        (m_playMode == PlayMode::EvenEngineVsHuman) ||
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsHuman) ||
        (m_playMode == PlayMode::HandicapHumanVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine) ||
        (m_playMode == PlayMode::CsaNetworkMode);

    // ライブ対局中は GC を手番の単一ソースとする。
    // ShogiBoard::currentPlayer() は setSfen() 経由でのみ更新されるため、
    // 指し手適用直後に board 側を参照すると古い手番へ巻き戻ることがある。
    if (isLivePlayMode && m_gameController) {
        const auto gcTurn = m_gameController->currentPlayer();
        if (gcTurn == ShogiGameController::Player1 || gcTurn == ShogiGameController::Player2) {
            tm->setFromGc(gcTurn);
            return;
        }
    }

    // 非ライブ用途（棋譜ナビゲーション等）は盤面SFENの手番を優先。
    const QString bw = (m_shogiView && m_shogiView->board())
                           ? m_shogiView->board()->currentPlayer()
                           : QStringLiteral("b");
    tm->setFromSfenToken(bw);

    // 盤面起点の同期時のみ GC へ反映する。
    if (m_gameController) m_gameController->setCurrentPlayer(tm->toGc());
}

// `resolveCurrentSfenForGameStart`: Current Sfen For Game Start を解決する。
QString MainWindow::resolveCurrentSfenForGameStart() const
{
    qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: m_currentSelectedPly=" << m_currentSelectedPly
                       << " sfenRecord()=" << (sfenRecord() ? "exists" : "null")
                       << " size=" << (sfenRecord() ? sfenRecord()->size() : -1);

    // 1) 棋譜SFENリストの「選択手」から取得（最優先）
    if (sfenRecord()) {
        const qsizetype size = sfenRecord()->size();
        // m_currentSelectedPly が [0..size-1] のインデックスである前提（本プロジェクトの慣習）
        // 1始まりの場合はプロジェクト実装に合わせて +0 / -1 調整してください。
        int idx = m_currentSelectedPly;
        qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: initial idx=" << idx;
        if (idx < 0) {
            // 0手目（開始局面）などのとき
            idx = 0;
        }
        // 最後の有効なSFEN（対局終了時点の局面）を使用する
        if (idx >= size && size > 0) {
            qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: idx(" << idx << ") >= size(" << size << "), clamping to " << (size - 1);
            idx = static_cast<int>(size - 1);
        }
        if (idx >= 0 && idx < size) {
            const QString s = sfenRecord()->at(idx).trimmed();
            qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: at(" << idx << ")=" << s.left(50);
            if (!s.isEmpty()) return s;
        }
    }

    // 2) フォールバックなし（司令塔側が安全に処理）
    qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart: returning EMPTY";
    return QString();
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

    // 注意: m_startSfenStr は局面編集時にセットされている場合があるため、
    //       ここではクリアしない。プリセット選択時のSFEN上書きは
    //       GameStartCoordinator::initializeGame() 側で行う。

    // 現在の局面SFEN（棋譜レコードから最優先で取得）
    {
        qCDebug(lcApp).noquote() << "initializeGame: BEFORE resolve, m_currentSfenStr=" << m_currentSfenStr.left(50);
        const QString sfen = resolveCurrentSfenForGameStart().trimmed();
        qCDebug(lcApp).noquote() << "initializeGame: resolved sfen=" << sfen.left(50);
        if (!sfen.isEmpty()) {
            m_currentSfenStr = sfen;
            qCDebug(lcApp).noquote() << "initializeGame: SET m_currentSfenStr=" << m_currentSfenStr.left(50);
        } else {
            // 何も取れないケースは珍しいが、空のままでも司令塔側で安全にフォールバックされる。
            // ここでは何もしない（ログのみ）
            qCDebug(lcApp).noquote() << "resolveCurrentSfenForGameStart_: empty. delegate to coordinator.";
        }
    }

    qCDebug(lcApp).noquote() << "initializeGame: FINAL m_currentSfenStr=" << m_currentSfenStr.left(50)
                       << " m_startSfenStr=" << m_startSfenStr.left(50)
                       << " m_currentSelectedPly=" << m_currentSelectedPly;

    // 分岐ツリーから途中局面で再対局する場合、sfenRecord() を
    // 現在のラインのSFENで再構築する。これにより、前の対局の異なる分岐の
    // SFENが混在してSFEN差分によるハイライトが誤動作する問題を防ぐ。
    if (m_branchTree != nullptr && m_navState != nullptr
        && sfenRecord() != nullptr && m_currentSelectedPly > 0) {
        const int lineIndex = m_navState->currentLineIndex();
        const QStringList branchSfens = m_branchTree->getSfenListForLine(lineIndex);
        if (!branchSfens.isEmpty() && m_currentSelectedPly < branchSfens.size()) {
            sfenRecord()->clear();
            for (int i = 0; i <= m_currentSelectedPly; ++i) {
                sfenRecord()->append(branchSfens.at(i));
            }
            qCDebug(lcApp).noquote()
                << "initializeGame: rebuilt sfenRecord from branchTree line=" << lineIndex
                << " entries=" << sfenRecord()->size();
        }
    }

    GameStartCoordinator::Ctx c;
    c.view            = m_shogiView;
    c.gc              = m_gameController;
    c.clock           = m_timeController ? m_timeController->clock() : nullptr;
    c.sfenRecord      = sfenRecord();          // QStringList*
    c.kifuModel       = m_kifuRecordModel;     // 棋譜欄モデル（終端行削除に使用）
    c.kifuLoadCoordinator = m_kifuLoadCoordinator;  // 分岐構造の設定用
    c.currentSfenStr  = &m_currentSfenStr;     // 現局面の SFEN（ここで事前決定済み）
    c.startSfenStr    = &m_startSfenStr;       // 開始SFENは明示的に空（優先度を逆転）
    c.selectedPly     = m_currentSelectedPly;  // 1始まり/0始まりはプロジェクト規約に準拠
    c.isReplayMode    = m_replayController ? m_replayController->isReplayMode() : false;
    c.bottomIsP1      = m_bottomIsP1;

    m_gameStart->initializeGame(c);
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

// GUIを初期画面表示に戻す。
void MainWindow::resetToInitialState()
{
    // 既存呼び出し互換のため残し、内部は司令塔フックへ集約
    onPreStartCleanupRequested();
}

// 棋譜ファイルをダイアログから選択し、そのファイルを開く。
void MainWindow::chooseAndLoadKifuFile()
{
    qCDebug(lcApp).noquote() << "chooseAndLoadKifuFile ENTER"
                       << "sfenRecord()*=" << static_cast<const void*>(sfenRecord())
                       << "sfenRecord().size=" << (sfenRecord() ? sfenRecord()->size() : -1)
                       << "m_boardSync*=" << static_cast<const void*>(m_boardSync);

    // --- 1) ファイル選択（UI層に残す） ---
    // 以前開いたディレクトリを取得
    const QString lastDir = SettingsService::lastKifuDirectory();
    
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("棋譜ファイルを開く"), lastDir,
        tr("Kifu Files (*.kif *.kifu *.ki2 *.csa *.jkf *.usi *.sfen *.usen);;KIF Files (*.kif *.kifu *.ki2);;CSA Files (*.csa);;JKF Files (*.jkf);;USI Files (*.usi *.sfen);;USEN Files (*.usen)")
        );

    if (filePath.isEmpty()) return;

    // 選択したファイルのディレクトリを保存
    QFileInfo fileInfo(filePath);
    SettingsService::setLastKifuDirectory(fileInfo.absolutePath());

    setReplayMode(true);
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->ensureGameInfoController();
        m_gameInfoController = m_playerInfoWiring->gameInfoController();
    }

    // --- 2) KifuLoadCoordinator の作成・配線・読み込み実行 ---
    createAndWireKifuLoadCoordinator();

    qCDebug(lcApp).noquote() << "chooseAndLoadKifuFile: KifuLoadCoordinator created"
                       << "m_kifuLoadCoordinator*=" << static_cast<const void*>(m_kifuLoadCoordinator)
                       << "passed sfenRecord()*=" << static_cast<const void*>(sfenRecord());

    qCDebug(lcApp).noquote() << "chooseAndLoadKifuFile: loading file=" << filePath;
    dispatchKifuLoad(filePath);

    // デバッグ: 読み込み後のsfenRecord状態を確認
    qCDebug(lcApp).noquote() << "chooseAndLoadKifuFile LEAVE"
                       << "sfenRecord()*=" << static_cast<const void*>(sfenRecord())
                       << "sfenRecord().size=" << (sfenRecord() ? sfenRecord()->size() : -1);
    if (sfenRecord() && !sfenRecord()->isEmpty()) {
        qCDebug(lcApp).noquote() << "sfenRecord()[0]=" << sfenRecord()->first().left(60);
        if (sfenRecord()->size() > 1) {
            qCDebug(lcApp).noquote() << "sfenRecord()[1]=" << sfenRecord()->at(1).left(60);
        }
    }
}

// `displayGameRecord`: Game Record を表示する。
void MainWindow::displayGameRecord(const QList<KifDisplayItem> disp)
{
    QElapsedTimer timer;
    timer.start();

    // 棋譜読み込み時は対局で記録したUSI形式指し手リストをクリア
    // （代わりに KifuLoadCoordinator::kifuUsiMovesPtr() を使用する）
    m_gameUsiMoves.clear();
    m_gameMoves.clear();

    if (!m_kifuRecordModel) return;

    ensureRecordPresenter();
    if (!m_recordPresenter) return;

    const qsizetype moveCount = disp.size();
    const int rowCount  = (sfenRecord() && !sfenRecord()->isEmpty())
                             ? static_cast<int>(sfenRecord()->size())
                             : static_cast<int>(moveCount + 1);

    ensureGameRecordModel();
    if (m_gameRecord) {
        m_gameRecord->initializeFromDisplayItems(disp, rowCount);
    }
    qCDebug(lcApp).noquote() << QStringLiteral("GameRecordModel init: %1 ms").arg(timer.elapsed());

    m_commentsByRow.clear();
    m_commentsByRow.resize(rowCount);
    for (qsizetype i = 0; i < disp.size() && i < rowCount; ++i) {
        m_commentsByRow[i] = disp[i].comment;
    }
    qCDebug(lcApp).noquote() << "displayGameRecord: initialized with" << rowCount << "entries";

    // ← まとめて Presenter 側に委譲
    QElapsedTimer presenterTimer;
    presenterTimer.start();
    m_recordPresenter->displayAndWire(disp, rowCount, m_recordPane);
    qCDebug(lcApp).noquote() << QStringLiteral("displayAndWire: %1 ms").arg(presenterTimer.elapsed());
    qCDebug(lcApp).noquote() << QStringLiteral("displayGameRecord TOTAL: %1 ms").arg(timer.elapsed());
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


// `onDocksLockToggled`: Docks Lock Toggled のイベント受信時処理を行う。
void MainWindow::onDocksLockToggled(bool locked)
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        // AnalysisResultsドックも登録（遅延生成のため毎回チェック）
        m_dockLayoutManager->registerDock(DockLayoutManager::DockType::AnalysisResults, m_analysisResultsDock);
        m_dockLayoutManager->setDocksLocked(locked);
    }

    // 設定を保存
    SettingsService::setDocksLocked(locked);
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

// `appendKifuLine`: 棋譜1行の追記を記録更新フローへ橋渡しする。
void MainWindow::appendKifuLine(const QString& text, const QString& elapsedTime)
{
    qCDebug(lcApp).noquote() << "appendKifuLine ENTER: text=" << text
                       << "elapsedTime=" << elapsedTime;

    // KIF 追記の既存フローに合わせて m_lastMove を経由し、updateGameRecord() を1回だけ呼ぶ
    m_lastMove = text;

    // ここで棋譜へ 1 行追加（手数インクリメントやモデル反映は updateGameRecord が面倒を見る）
    updateGameRecord(elapsedTime);

    qCDebug(lcApp).noquote() << "appendKifuLine AFTER updateGameRecord:"
                       << "kifuModel.rowCount=" << (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);

    // 二重追記防止のためクリア
    m_lastMove.clear();
}

// 人間の手番かどうかを判定するヘルパー
// 対局中で、現在手番が人間なら true を返す
bool MainWindow::isHumanTurnNow() const
{
    // 対局中でなければ常に true（編集等では制限しない）
    switch (m_playMode) {
    case PlayMode::HumanVsHuman:
        // HvH では両者人間なので常に true
        return true;

    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        // 人間が先手（Player1）の場合、Player1の手番なら true
        return (m_gameController && m_gameController->currentPlayer() == ShogiGameController::Player1);

    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        // 人間が後手（Player2）の場合、Player2の手番なら true
        return (m_gameController && m_gameController->currentPlayer() == ShogiGameController::Player2);

    case PlayMode::CsaNetworkMode:
        // CSA通信対局では CsaGameCoordinator の isMyTurn() を使用
        if (m_csaGameCoordinator) {
            return m_csaGameCoordinator->isMyTurn();
        }
        return false;

    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        // EvE では人間は操作しない
        return false;

    default:
        // PlayMode::NotStarted, PlayMode::AnalysisMode, PlayMode::ConsiderationMode, PlayMode::TsumiSearchMode 等は制限なし
        return true;
    }
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
                m_startSfenStr, tcInfo);
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
    m_playMode = opt.mode;

    // 開始局面のSFENを同期（定跡ウィンドウ等が参照する）
    if (!opt.sfenStart.isEmpty()) {
        m_currentSfenStr = opt.sfenStart;
    } else if (sfenRecord() && !sfenRecord()->isEmpty()) {
        m_currentSfenStr = sfenRecord()->first();
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

// `syncBoardAndHighlightsAtRow`: 指定手数の盤面・ハイライト・関連UI状態を同期する。
void MainWindow::syncBoardAndHighlightsAtRow(int ply)
{
    qCDebug(lcApp) << "syncBoardAndHighlightsAtRow ENTER ply=" << ply;

    // 分岐ナビゲーション中に発生する再入を抑止する。
    // 分岐側の同期は `loadBoardWithHighlights()` が責務を持つため、
    // 通常経路の同期をここで走らせると二重反映になる。
    if (m_skipBoardSyncForBranchNav) {
        qCDebug(lcApp) << "syncBoardAndHighlightsAtRow skipped (branch navigation in progress)";
        return;
    }

    // 位置編集モード中はスキップ
    if (m_shogiView && m_shogiView->positionEditMode()) {
        qCDebug(lcApp) << "syncBoardAndHighlightsAtRow skipped (edit-mode)";
        return;
    }

    // BoardSyncPresenterを確保して盤面同期
    ensureBoardSyncPresenter();
    if (m_boardSync) {
        m_boardSync->syncBoardAndHighlightsAtRow(ply);
    }

    // 矢印ボタンの活性化
    enableArrowButtons();

    // 現在局面SFENの更新:
    // 分岐ライン表示中は `sfenRecord()` が本譜ベースのため不整合が起こり得る。
    // そのため分岐中は branchTree を優先し、通常時のみ sfenRecord を使う。
    bool foundInBranch = false;
    if (m_navState != nullptr && !m_navState->isOnMainLine() && m_branchTree != nullptr) {
        const int lineIndex = m_navState->currentLineIndex();
        QVector<BranchLine> lines = m_branchTree->allLines();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            const BranchLine& line = lines.at(lineIndex);
            if (ply >= 0 && ply < line.nodes.size()) {
                m_currentSfenStr = line.nodes.at(ply)->sfen();
                foundInBranch = true;
                qCDebug(lcApp) << "syncBoardAndHighlightsAtRow: updated m_currentSfenStr from branchTree";
            }
        }
    }
    if (!foundInBranch && sfenRecord() && ply >= 0 && ply < sfenRecord()->size()) {
        m_currentSfenStr = sfenRecord()->at(ply);
        qCDebug(lcApp) << "syncBoardAndHighlightsAtRow: updated m_currentSfenStr=" << m_currentSfenStr;
    }

    // 定跡ウィンドウを更新
    updateJosekiWindow();

    qCDebug(lcApp) << "syncBoardAndHighlightsAtRow LEAVE";
}

// `navigateKifuViewToRow`: 棋譜表の対象行へ移動し、盤面と手番表示を追従更新する。
void MainWindow::navigateKifuViewToRow(int ply)
{
    qCDebug(lcApp).noquote() << "navigateKifuViewToRow ENTER ply=" << ply;

    if (!m_recordPane || !m_kifuRecordModel) {
        qCDebug(lcApp).noquote() << "navigateKifuViewToRow ABORT: recordPane or kifuRecordModel is null";
        return;
    }

    QTableView* view = m_recordPane->kifuView();
    if (!view) {
        qCDebug(lcApp).noquote() << "navigateKifuViewToRow ABORT: kifuView is null";
        return;
    }

    const int rows = m_kifuRecordModel->rowCount();
    const int safe = (rows > 0) ? qBound(0, ply, rows - 1) : 0;

    qCDebug(lcApp).noquote() << "navigateKifuViewToRow: ply=" << ply
                       << "rows=" << rows << "safe=" << safe;

    const QModelIndex idx = m_kifuRecordModel->index(safe, 0);
    if (idx.isValid()) {
        if (auto* sel = view->selectionModel()) {
            sel->setCurrentIndex(
                idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            view->setCurrentIndex(idx);
        }
        view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }

    // 棋譜欄のハイライト行を更新
    m_kifuRecordModel->setCurrentHighlightRow(safe);

    // 盤・ハイライト即時同期
    syncBoardAndHighlightsAtRow(safe);

    // トラッキングを更新
    m_activePly = safe;
    m_currentSelectedPly = safe;
    m_currentMoveIndex = safe;

    // 手番表示を更新
    setCurrentTurn();

    qCDebug(lcApp).noquote() << "navigateKifuViewToRow LEAVE";
}

// `kifuExportController`: 棋譜エクスポートコントローラを取得する。
KifuExportController* MainWindow::kifuExportController()
{
    ensureKifuExportController();
    return m_kifuExportController;
}

// `initializeBranchNavigationClasses`: Branch Navigation Classes を初期化する。
void MainWindow::initializeBranchNavigationClasses()
{
    // ツリーの作成
    if (m_branchTree == nullptr) {
        m_branchTree = new KifuBranchTree(this);
    }

    // ナビゲーション状態の作成
    if (m_navState == nullptr) {
        m_navState = new KifuNavigationState(this);
        m_navState->setTree(m_branchTree);
    }

    // ナビゲーションコントローラの作成
    if (m_kifuNavController == nullptr) {
        m_kifuNavController = new KifuNavigationController(this);
        m_kifuNavController->setTreeAndState(m_branchTree, m_navState);

        // ボタン接続（RecordPaneが存在する場合）
        if (m_recordPane != nullptr) {
            KifuNavigationController::Buttons buttons;
            buttons.first = m_recordPane->firstButton();
            buttons.back10 = m_recordPane->back10Button();
            buttons.prev = m_recordPane->prevButton();
            buttons.next = m_recordPane->nextButton();
            buttons.fwd10 = m_recordPane->fwd10Button();
            buttons.last = m_recordPane->lastButton();
            m_kifuNavController->connectButtons(buttons);
            qCDebug(lcApp).noquote() << "initializeBranchNavigationClasses: KifuNavigationController buttons connected";
        }
    }

    // ライブゲームセッションの作成
    if (m_liveGameSession == nullptr) {
        m_liveGameSession = new LiveGameSession(this);
        m_liveGameSession->setTree(m_branchTree);
    }

    // 表示コーディネーターの作成
    qCDebug(lcApp).noquote() << "initializeBranchNavigationClasses: m_displayCoordinator=" << (m_displayCoordinator ? "exists" : "null");
    if (m_displayCoordinator == nullptr) {
        qCDebug(lcApp).noquote() << "initializeBranchNavigationClasses: creating KifuDisplayCoordinator";
        m_displayCoordinator = new KifuDisplayCoordinator(
            m_branchTree, m_navState, m_kifuNavController, this);
        m_displayCoordinator->setRecordPane(m_recordPane);
        m_displayCoordinator->setRecordModel(m_kifuRecordModel);
        m_displayCoordinator->setBranchModel(m_kifuBranchModel);

        // 分岐ツリーウィジェットを設定（EngineAnalysisTabから取得）
        if (m_branchTreeWidget != nullptr) {
            m_displayCoordinator->setBranchTreeWidget(m_branchTreeWidget);
        }

        // EngineAnalysisTab を設定（分岐ツリーハイライト用）
        if (m_analysisTab != nullptr) {
            m_displayCoordinator->setAnalysisTab(m_analysisTab);
        }

        m_displayCoordinator->wireSignals();

        if (m_liveGameSession != nullptr) {
            m_displayCoordinator->setLiveGameSession(m_liveGameSession);
        }

        // 盤面とハイライト更新シグナルを接続（新システム用）
        connect(m_displayCoordinator, &KifuDisplayCoordinator::boardWithHighlightsRequired,
                this, &MainWindow::loadBoardWithHighlights);

        // 盤面のみ更新シグナル（通常は boardWithHighlightsRequired を使用）
        connect(m_displayCoordinator, &KifuDisplayCoordinator::boardSfenChanged,
                this, &MainWindow::loadBoardFromSfen);

        connect(m_kifuNavController, &KifuNavigationController::lineSelectionChanged,
                this, &MainWindow::onLineSelectionChanged);

        connect(m_kifuNavController, &KifuNavigationController::branchNodeHandled,
                this, &MainWindow::onBranchNodeHandled);
    }

}

// `setupRecordPane`: Record Pane をセットアップする。
void MainWindow::setupRecordPane()
{
    // モデルの用意（従来どおり）
    if (!m_kifuRecordModel) m_kifuRecordModel = new KifuRecordListModel(this);
    if (!m_kifuBranchModel) m_kifuBranchModel = new KifuBranchListModel(this);

    // Wiring の生成
    if (!m_recordPaneWiring) {
        RecordPaneWiring::Deps d;
        d.parent      = m_central;                               // 親ウィジェット
        d.ctx         = this;                                    // RecordPane::mainRowChanged の受け先
        d.recordModel = m_kifuRecordModel;
        d.branchModel = m_kifuBranchModel;

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
        d.log1          = m_lineEditModel1;  // USIログ(先手)
        d.log2          = m_lineEditModel2;  // USIログ(後手)

        m_analysisWiring = new AnalysisTabWiring(d, this);
        m_analysisWiring->buildUiAndWire();
    }

    // 配線クラスから出来上がった部品を受け取る（MainWindow の既存フィールドへ反映）
    m_analysisTab    = m_analysisWiring->analysisTab();
    m_tab            = m_analysisWiring->tab();
    m_modelThinking1 = m_analysisWiring->thinking1();
    m_modelThinking2 = m_analysisWiring->thinking2();

    Q_ASSERT(m_analysisTab && m_modelThinking1 && m_modelThinking2);

    // 分岐ツリーのアクティベートを MainWindow スロットへ（ラムダ不使用）
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
        this,          &MainWindow::onBranchNodeActivated,
        Qt::UniqueConnection);

    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::commentUpdated,
        this,          &MainWindow::onCommentUpdated,
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

    if (!m_considerationModel) {
        m_considerationModel = new ShogiEngineThinkingModel(this);
    }
    if (m_analysisTab) {
        m_analysisTab->setConsiderationThinkingModel(m_considerationModel);
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
    m_evalChartDock = m_dockCreationService->createEvalChartDock();
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
    m_recordPaneDock = m_dockCreationService->createRecordPaneDock();
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
    m_dockCreationService->setModels(m_modelThinking1, m_modelThinking2, m_lineEditModel1, m_lineEditModel2);
    m_dockCreationService->createAnalysisDocks();

    // 作成されたドックへの参照を取得
    m_gameInfoDock = m_dockCreationService->gameInfoDock();
    m_thinkingDock = m_dockCreationService->thinkingDock();
    m_considerationDock = m_dockCreationService->considerationDock();
    m_usiLogDock = m_dockCreationService->usiLogDock();
    m_csaLogDock = m_dockCreationService->csaLogDock();
    m_commentDock = m_dockCreationService->commentDock();
    m_branchTreeDock = m_dockCreationService->branchTreeDock();
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
    m_menuWindowDock = m_dockCreationService->createMenuWindowDock();
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
    m_josekiWindowDock = m_dockCreationService->createJosekiWindowDock();

    // ドックが表示されたときに定跡ウィンドウを更新する
    // （トグルアクション経由で表示された場合にも対応）
    if (m_josekiWindowDock) {
        connect(m_josekiWindowDock, &QDockWidget::visibilityChanged,
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
    m_analysisResultsDock = m_dockCreationService->createAnalysisResultsDock();
}

// MatchCoordinator 構築と配線の集約ポイント。
void MainWindow::initMatchCoordinator()
{
    // 依存が揃っていない場合は何もしない
    if (!m_gameController || !m_shogiView) return;

    // まず時計を用意（nullでも可だが、あれば渡す）
    ensureTimeController();

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    // --- MatchCoordinator::Deps を構築（UI hooks は従来どおりここで設定） ---
    MatchCoordinator::Deps d;
    d.gc    = m_gameController;
    d.clock = m_timeController ? m_timeController->clock() : nullptr;
    d.view  = m_shogiView;
    d.usi1  = m_usi1;
    d.usi2  = m_usi2;

    // （EvE 用）ログ／思考モデル
    d.comm1  = m_lineEditModel1;
    d.think1 = m_modelThinking1;
    d.comm2  = m_lineEditModel2;
    d.think2 = m_modelThinking2;

    d.sfenRecord = sfenRecord();

    // 評価値グラフ更新フック（EvaluationGraphController直接）
    ensureEvaluationGraphController();
    d.hooks.appendEvalP1       = std::bind(&EvaluationGraphController::redrawEngine1Graph, m_evalGraphController, -1);
    d.hooks.appendEvalP2       = std::bind(&EvaluationGraphController::redrawEngine2Graph, m_evalGraphController, -1);
    d.hooks.updateTurnDisplay  = [this](MatchCoordinator::Player cur) {
        const auto now = (cur == MatchCoordinator::P2)
                             ? ShogiGameController::Player2
                             : ShogiGameController::Player1;
        onTurnManagerChanged(now);
    };
    d.hooks.sendGoToEngine = [this](Usi* which, const MatchCoordinator::GoTimes& t) {
        if (m_match) m_match->sendGoToEngine(which, t);
    };
    d.hooks.sendStopToEngine = [this](Usi* which) {
        if (m_match) m_match->sendStopToEngine(which);
    };
    d.hooks.sendRawToEngine = [this](Usi* which, const QString& cmd) {
        if (m_match) m_match->sendRawToEngine(which, cmd);
    };
    d.hooks.initializeNewGame  = std::bind(&MainWindow::initializeNewGameHook, this, _1);
    d.hooks.renderBoardFromGc  = std::bind(&MainWindow::renderBoardFromGc, this);
    d.hooks.showMoveHighlights = std::bind(&MainWindow::showMoveHighlights, this, _1, _2);
    d.hooks.appendKifuLine     = std::bind(&MainWindow::appendKifuLineHook, this, _1, _2);

    d.hooks.showGameOverDialog = std::bind(&MainWindow::showGameOverMessageBox, this, _1, _2);

    d.hooks.remainingMsFor = std::bind(&MainWindow::getRemainingMsFor, this, _1);
    d.hooks.incrementMsFor = std::bind(&MainWindow::getIncrementMsFor, this, _1);
    d.hooks.byoyomiMs      = std::bind(&MainWindow::getByoyomiMs, this);

    // 対局者名の更新フック（PlayerInfoWiring経由）
    ensurePlayerInfoWiring();
    d.hooks.setPlayersNames = std::bind(&PlayerInfoWiring::onSetPlayersNames, m_playerInfoWiring, _1, _2);
    d.hooks.setEngineNames  = std::bind(&PlayerInfoWiring::onSetEngineNames, m_playerInfoWiring, _1, _2);

    d.hooks.autoSaveKifu = std::bind(&MainWindow::autoSaveKifuToFile, this, _1, _2, _3, _4, _5, _6);

    // `m_gameStartCoordinator` は MatchCoordinator の生成/配線専用。
    // 画面側の開始操作を受ける `m_gameStart` とは役割を分ける。
    // --- GameStartCoordinator の確保（1 回だけ） ---
    if (!m_gameStartCoordinator) {
        GameStartCoordinator::Deps gd;
        gd.match = nullptr;
        gd.clock = m_timeController ? m_timeController->clock() : nullptr;
        gd.gc    = m_gameController;
        gd.view  = m_shogiView;

        m_gameStartCoordinator = new GameStartCoordinator(gd, this);

        // timeUpdated
        if (m_timeConn) { QObject::disconnect(m_timeConn); m_timeConn = {}; }
        m_timeConn = connect(
            m_gameStartCoordinator, &GameStartCoordinator::timeUpdated,
            m_timePresenter, &TimeDisplayPresenter::onMatchTimeUpdated,
            Qt::UniqueConnection);

        // requestAppendGameOverMove
        connect(m_gameStartCoordinator, &GameStartCoordinator::requestAppendGameOverMove,
                this,                   &MainWindow::onRequestAppendGameOverMove,
                Qt::UniqueConnection);

        // boardFlipped
        connect(m_gameStartCoordinator, &GameStartCoordinator::boardFlipped,
                this,                   &MainWindow::onBoardFlipped,
                Qt::UniqueConnection);

        // gameOverStateChanged
        connect(m_gameStartCoordinator, &GameStartCoordinator::gameOverStateChanged,
                this,                   &MainWindow::onGameOverStateChanged,
                Qt::UniqueConnection);

        // gameEnded → onMatchGameEnded
        connect(
            m_gameStartCoordinator, &GameStartCoordinator::matchGameEnded,
            this, &MainWindow::onMatchGameEnded,
            Qt::UniqueConnection);

        // 対局終了時にUI状態を「アイドル」に遷移
        ensureUiStatePolicyManager();
        connect(m_gameStartCoordinator, &GameStartCoordinator::matchGameEnded,
                m_uiStatePolicy, &UiStatePolicyManager::transitionToIdle,
                Qt::UniqueConnection);
    }

    // --- MatchCoordinator（司令塔）の生成＆初期配線 ---
    m_match = m_gameStartCoordinator->createAndWireMatch(d, this);

    // USIコマンドコントローラへ司令塔を反映
    ensureUsiCommandController();

    // PlayMode を司令塔へ伝達（従来どおり）
    if (m_match) {
        m_match->setPlayMode(m_playMode);
    }

    if (m_evalGraphController) {
        m_evalGraphController->setMatchCoordinator(m_match);
        m_evalGraphController->setSfenRecord(sfenRecord());
    }

    if (m_match) {
        MatchCoordinator::UndoRefs u;
        u.recordModel      = m_kifuRecordModel;      // 棋譜テーブルのモデル
        u.gameMoves        = &m_gameMoves;           // 内部の着手配列
        u.positionStrList  = &m_positionStrList;     // "position ..." 履歴（あれば）
        u.sfenRecord       = sfenRecord();           // SFEN の履歴（Presenterと同一）
        u.currentMoveIndex = &m_currentMoveIndex;    // 現在手数（0起点）
        u.gc               = m_gameController;       // 盤ロジック
        u.boardCtl         = m_boardController;      // ハイライト消去など（null可）
        u.clock            = m_timeController ? m_timeController->clock() : nullptr;  // 時計（null可）
        u.view             = m_shogiView;            // クリック可否の切替

        MatchCoordinator::UndoHooks h;
        h.getMainRowGuard                 = std::bind(&MainWindow::getMainRowGuard, this);
        h.setMainRowGuard                 = std::bind(&MainWindow::setMainRowGuard, this, _1);
        h.updateHighlightsForPly          = std::bind(&MainWindow::syncBoardAndHighlightsAtRow, this, _1);
        h.updateTurnAndTimekeepingDisplay = std::bind(&MainWindow::updateTurnAndTimekeepingDisplay, this);
        // 評価値などの巻戻しが必要なら将来ここに追加：
        // h.handleUndoMove               = std::bind(&MainWindow::onUndoMove, this, _1);
        h.isHumanSide                     = std::bind(&MainWindow::isHumanSide, this, _1);
        h.isHvH                           = std::bind(&MainWindow::isHvH, this);

        m_match->setUndoBindings(u, h);

        // 検討モード終了・詰み探索終了時にUI状態を「アイドル」に遷移
        ensureUiStatePolicyManager();
        connect(m_match, &MatchCoordinator::considerationModeEnded,
                m_uiStatePolicy, &UiStatePolicyManager::transitionToIdle,
                Qt::UniqueConnection);
        connect(m_match, &MatchCoordinator::tsumeSearchModeEnded,
                m_uiStatePolicy, &UiStatePolicyManager::transitionToIdle,
                Qt::UniqueConnection);
    }

    if (m_timeController) {
        m_timeController->setMatchCoordinator(m_match);
    }

    // Clock → MainWindow の投了シグナルは従来どおり UI 側で受ける
    ShogiClock* clock = m_timeController ? m_timeController->clock() : nullptr;
    if (clock) {
        connect(clock, &ShogiClock::resignationTriggered,
                this, &MainWindow::onResignationTriggered, Qt::UniqueConnection);
    }

    // 盤クリックの配線
    connectBoardClicks();

    // 人間操作 → 合法判定＆適用の配線
    connectMoveRequested();

    // 既定モード（必要に応じて開始時に上書き）
    if (m_boardController)
        m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
}

// `ensureTimeController`: Time Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureTimeController()
{
    if (m_timeController) return;

    m_timeController = new TimeControlController(this);
    m_timeController->setTimeDisplayPresenter(m_timePresenter);
    m_timeController->ensureClock();
}

// `onMatchGameEnded`: Match Game Ended のイベント受信時処理を行う。
void MainWindow::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->onMatchGameEnded(info);
    }

    //   onRequestAppendGameOverMove の後で行う（投了手を含めるため）

    if (m_timeController) {
        m_timeController->recordGameEndTime();
        const QDateTime endTime = m_timeController->gameEndDateTime();
        if (endTime.isValid()) {
            ensurePlayerInfoWiring();
            if (m_playerInfoWiring) {
                m_playerInfoWiring->updateGameInfoWithEndTime(endTime);
            }
        }
    }

    const bool isEvE = (m_playMode == PlayMode::EvenEngineVsEngine ||
                        m_playMode == PlayMode::HandicapEngineVsEngine);
    ensureConsecutiveGamesController();
    if (isEvE && m_consecutiveGamesController && m_consecutiveGamesController->shouldStartNextGame()) {
        qCDebug(lcApp) << "EvE game ended, starting next consecutive game...";
        startNextConsecutiveGame();
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

    if (m_liveGameSession != nullptr && m_liveGameSession->isActive()) {
        qCDebug(lcApp) << "onRequestAppendGameOverMove: committing live game session";
        m_liveGameSession->commit();
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
                       << " m_playMode=" << static_cast<int>(m_playMode)
                       << " m_match=" << (m_match ? "valid" : "null")
                       << " matchMode=" << (m_match ? static_cast<int>(m_match->playMode()) : -1);
    
    ensureBoardSetupController();
    if (m_boardSetupController) {
        // 最新の状態を同期
        m_boardSetupController->setPlayMode(m_playMode);
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

// `broadcastComment`: コメント表示先（棋譜欄/解析タブ）へ同報する。
void MainWindow::broadcastComment(const QString& text, bool asHtml)
{
    ensureCommentCoordinator();
    if (m_commentCoordinator) {
        // 最新の依存を同期
        m_commentCoordinator->setAnalysisTab(m_analysisTab);
        m_commentCoordinator->setRecordPane(m_recordPane);
        m_commentCoordinator->broadcastComment(text, asHtml);
    }
}

// `onBranchNodeActivated`: Branch Node Activated のイベント受信時処理を行う。
void MainWindow::onBranchNodeActivated(int row, int ply)
{
    qCDebug(lcApp).noquote() << "onBranchNodeActivated ENTER row=" << row << "ply=" << ply;
    if (m_kifuNavController == nullptr) {
        qCDebug(lcApp).noquote() << "onBranchNodeActivated: kifuNavController is null";
        return;
    }

    // KifuNavigationControllerに委譲
    m_kifuNavController->handleBranchNodeActivated(row, ply);
    qCDebug(lcApp).noquote() << "onBranchNodeActivated LEAVE";
}

// `onBranchNodeHandled`: Branch Node Handled のイベント受信時処理を行う。
void MainWindow::onBranchNodeHandled(int ply, const QString& sfen,
                                     int previousFileTo, int previousRankTo,
                                     const QString& lastUsiMove)
{
    qCDebug(lcApp).noquote() << "onBranchNodeHandled ENTER ply=" << ply
                       << "sfen=" << sfen
                       << "fileTo=" << previousFileTo << "rankTo=" << previousRankTo
                       << "usiMove=" << lastUsiMove
                       << "playMode=" << static_cast<int>(m_playMode)
                       << "match=" << (m_match ? "valid" : "null");

    // plyインデックス変数を更新
    m_activePly = ply;
    m_currentSelectedPly = ply;
    m_currentMoveIndex = ply;

    // m_currentSfenStr を更新
    if (!sfen.isEmpty()) {
        m_currentSfenStr = sfen;
    }

    // 定跡ウィンドウを更新
    updateJosekiWindow();

    // 検討モード時はエンジンに新しい局面を送信
    if (m_playMode == PlayMode::ConsiderationMode && m_match && !sfen.isEmpty()) {
        const QString newPosition = QStringLiteral("position sfen ") + sfen;
        qCDebug(lcApp).noquote() << "onBranchNodeHandled: sending to engine:" << newPosition;
        if (m_match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo, lastUsiMove)) {
            qCDebug(lcApp).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned true";
            if (m_analysisTab) {
                m_analysisTab->startElapsedTimer();
            }
        } else {
            qCDebug(lcApp).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned false (same position or not in consideration)";
        }
    } else {
        qCDebug(lcApp).noquote() << "onBranchNodeHandled: NOT in consideration mode or match/sfen missing";
    }
    qCDebug(lcApp).noquote() << "onBranchNodeHandled LEAVE";
}

void MainWindow::onBranchTreeBuilt()
{
    qCDebug(lcApp) << "onBranchTreeBuilt: updating new navigation system";

    // 新しいナビゲーションシステムを更新
    if (m_navState != nullptr && m_branchTree != nullptr) {
        m_navState->setTree(m_branchTree);
    }

    // 分岐ツリーウィジェットを更新
    if (m_branchTreeWidget != nullptr && m_branchTree != nullptr) {
        m_branchTreeWidget->setTree(m_branchTree);
    }

    // 表示コーディネーターにツリー変更を通知
    if (m_displayCoordinator != nullptr) {
        m_displayCoordinator->onTreeChanged();
    }

    if (m_gameRecord != nullptr && m_branchTree != nullptr) {
        m_gameRecord->setBranchTree(m_branchTree);
        if (m_navState != nullptr) {
            m_gameRecord->setNavigationState(m_navState);
        }
    }
}

void MainWindow::loadBoardFromSfen(const QString& sfen)
{
    qCDebug(lcApp).noquote() << "loadBoardFromSfen: sfen=" << sfen.left(60);

    if (sfen.isEmpty()) {
        qCWarning(lcApp) << "loadBoardFromSfen: empty SFEN, skipping";
        return;
    }

    if (m_gameController && m_gameController->board() && m_shogiView) {
        // 盤面を設定
        m_gameController->board()->setSfen(sfen);
        // ビューに反映
        m_shogiView->applyBoardAndRender(m_gameController->board());
        // 手番表示を更新
        setCurrentTurn();
        // 現在のSFEN文字列を更新
        m_currentSfenStr = sfen;

        qCDebug(lcApp) << "loadBoardFromSfen: board updated successfully";
    } else {
        qCWarning(lcApp) << "loadBoardFromSfen: missing dependencies"
                   << "gc=" << m_gameController
                   << "board=" << (m_gameController ? m_gameController->board() : nullptr)
                   << "view=" << m_shogiView;
    }
}

// 新システム（KifuNavigationState）が管理
void MainWindow::onLineSelectionChanged(int newLineIndex)
{
    Q_UNUSED(newLineIndex)
    // KifuNavigationState が currentLineIndex() を管理
}

// SFENから盤面とハイライトを更新（分岐ナビゲーション用）
void MainWindow::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    // 分岐由来の局面切替では通常同期との競合を避けるため、ガードを立てる。
    m_skipBoardSyncForBranchNav = true;

    // 盤面更新とハイライト計算は BoardSyncPresenter に委譲する。
    ensureBoardSyncPresenter();
    if (m_boardSync) {
        m_boardSync->loadBoardWithHighlights(currentSfen, prevSfen);
    }

    // 手番表示を更新
    if (!currentSfen.isEmpty()) {
        setCurrentTurn();
        m_currentSfenStr = currentSfen;
    }

    // 同一イベントループ内の連鎖通知を吸収したあと、次周回でガードを解除する。
    QTimer::singleShot(0, this, [this]() {
        m_skipBoardSyncForBranchNav = false;
    });
}

// 毎手の着手確定時：ライブ分岐ツリー更新をイベントループ後段に遅延
void MainWindow::onMoveCommitted(ShogiGameController::Player mover, int ply)
{
    ensureBoardSetupController();
    if (m_boardSetupController) {
        m_boardSetupController->setPlayMode(m_playMode);
        m_boardSetupController->onMoveCommitted(mover, ply);
    }

    // USI形式の指し手を記録（詰み探索などで使用）
    // m_gameMovesの最新の指し手をUSI形式に変換して追加
    if (!m_gameMoves.isEmpty()) {
        const ShogiMove& lastMove = m_gameMoves.last();
        const QString usiMove = ShogiUtils::moveToUsi(lastMove);
        if (!usiMove.isEmpty()) {
            // m_gameUsiMovesのサイズがm_gameMoves.size()-1の場合のみ追加
            // （重複追加を防ぐ）
            if (m_gameUsiMoves.size() == m_gameMoves.size() - 1) {
                m_gameUsiMoves.append(usiMove);
                qCDebug(lcApp).noquote() << "onMoveCommitted: added USI move:" << usiMove
                                   << "m_gameUsiMoves.size()=" << m_gameUsiMoves.size();
            }
        }
    }

    // m_currentSfenStrを現在の局面に更新
    // sfenRecord()の最新のSFENを取得（plyはタイミングの問題で信頼できない）
    if (sfenRecord() && !sfenRecord()->isEmpty()) {
        // 最新のSFENを取得
        m_currentSfenStr = sfenRecord()->last();
        qCDebug(lcApp) << "onMoveCommitted: ply=" << ply
                 << "sfenRecord.size()=" << sfenRecord()->size()
                 << "using last sfen=" << m_currentSfenStr;
    }

    // 定跡ウィンドウを更新
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
            m_lastP1Ms, m_lastP2Ms, m_lastP1Turn, /*urgencyMs(未使用)*/ 0);
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
    m_bottomIsP1 = !m_bottomIsP1;

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

// `ensureDialogCoordinator`: Dialog Coordinator を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureDialogCoordinator()
{
    if (m_dialogCoordinator) return;

    m_dialogCoordinator = new DialogCoordinator(this, this);
    m_dialogCoordinator->setMatchCoordinator(m_match);
    m_dialogCoordinator->setGameController(m_gameController);

    // 検討コンテキストを設定
    DialogCoordinator::ConsiderationContext conCtx;
    conCtx.gameController = m_gameController;
    conCtx.gameMoves = &m_gameMoves;
    conCtx.currentMoveIndex = &m_currentMoveIndex;
    conCtx.kifuRecordModel = m_kifuRecordModel;
    conCtx.sfenRecord = sfenRecord();
    conCtx.startSfenStr = &m_startSfenStr;
    conCtx.currentSfenStr = &m_currentSfenStr;
    conCtx.branchTree = m_branchTree;
    conCtx.navState = m_navState;
    conCtx.considerationModel = &m_considerationModel;
    conCtx.gameUsiMoves = &m_gameUsiMoves;
    conCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    m_dialogCoordinator->setConsiderationContext(conCtx);

    // 詰み探索コンテキストを設定
    DialogCoordinator::TsumeSearchContext tsumeCtx;
    tsumeCtx.sfenRecord = sfenRecord();
    tsumeCtx.startSfenStr = &m_startSfenStr;
    tsumeCtx.positionStrList = &m_positionStrList;
    tsumeCtx.currentMoveIndex = &m_currentMoveIndex;
    tsumeCtx.gameUsiMoves = &m_gameUsiMoves;
    tsumeCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    m_dialogCoordinator->setTsumeSearchContext(tsumeCtx);

    // 棋譜解析コンテキストを設定
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.sfenRecord = sfenRecord();
    kifuCtx.moveRecords = m_moveRecords;
    kifuCtx.recordModel = m_kifuRecordModel;
    kifuCtx.activePly = &m_activePly;
    kifuCtx.gameController = m_gameController;
    kifuCtx.gameInfoController = m_gameInfoController;
    kifuCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    kifuCtx.evalChart = m_evalChart;
    kifuCtx.gameUsiMoves = &m_gameUsiMoves;
    kifuCtx.presenter = m_analysisPresenter;
    kifuCtx.getBoardFlipped = [this]() { return m_shogiView ? m_shogiView->getFlipMode() : false; };
    m_dialogCoordinator->setKifuAnalysisContext(kifuCtx);

    // 検討モード関連シグナルをConsiderationWiringに接続
    ensureConsiderationWiring();
    connect(m_dialogCoordinator, &DialogCoordinator::considerationModeStarted,
            m_considerationWiring, &ConsiderationWiring::onModeStarted);
    connect(m_dialogCoordinator, &DialogCoordinator::considerationTimeSettingsReady,
            m_considerationWiring, &ConsiderationWiring::onTimeSettingsReady);
    connect(m_dialogCoordinator, &DialogCoordinator::considerationMultiPVReady,
            m_considerationWiring, &ConsiderationWiring::onDialogMultiPVReady);

    // 解析進捗シグナルを接続
    connect(m_dialogCoordinator, &DialogCoordinator::analysisProgressReported,
            this, &MainWindow::onKifuAnalysisProgress);

    // 解析結果行選択シグナルを接続（棋譜欄・将棋盤・分岐ツリー連動用）
    connect(m_dialogCoordinator, &DialogCoordinator::analysisResultRowSelected,
            this, &MainWindow::onKifuAnalysisResultRowSelected);

    // UI状態遷移シグナルを接続
    ensureUiStatePolicyManager();
    connect(m_dialogCoordinator, &DialogCoordinator::analysisModeStarted,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToDuringAnalysis);
    connect(m_dialogCoordinator, &DialogCoordinator::analysisModeEnded,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToIdle);
    connect(m_dialogCoordinator, &DialogCoordinator::tsumeSearchModeStarted,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToDuringTsumeSearch);
    connect(m_dialogCoordinator, &DialogCoordinator::tsumeSearchModeEnded,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToIdle);
    connect(m_dialogCoordinator, &DialogCoordinator::considerationModeStarted,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToDuringConsideration);
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
    if (!m_kifuExportController) return;

    KifuExportController::Dependencies deps;
    deps.gameRecord = m_gameRecord;
    deps.kifuRecordModel = m_kifuRecordModel;
    deps.gameInfoController = m_gameInfoController;
    deps.timeController = m_timeController;
    deps.kifuLoadCoordinator = m_kifuLoadCoordinator;
    deps.recordPresenter = m_recordPresenter;
    deps.match = m_match;
    deps.replayController = m_replayController;
    deps.gameController = m_gameController;
    deps.statusBar = ui ? ui->statusbar : nullptr;
    deps.sfenRecord = sfenRecord();
    deps.usiMoves = &m_gameUsiMoves;
    deps.resolvedRows = nullptr;  // KifuBranchTree を優先使用
    deps.commentsByRow = &m_commentsByRow;
    deps.startSfenStr = m_startSfenStr;
    deps.playMode = m_playMode;
    deps.humanName1 = m_humanName1;
    deps.humanName2 = m_humanName2;
    deps.engineName1 = m_engineName1;
    deps.engineName2 = m_engineName2;
    deps.activeResolvedRow = (m_navState != nullptr) ? m_navState->currentLineIndex() : 0;
    deps.currentMoveIndex = m_currentMoveIndex;
    deps.activePly = m_activePly;
    deps.currentSelectedPly = m_currentSelectedPly;

    m_kifuExportController->setDependencies(deps);
}

// `ensureGameStateController`: Game State Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureGameStateController()
{
    if (m_gameStateController) return;

    m_gameStateController = new GameStateController(this);

    // 依存オブジェクトの設定
    m_gameStateController->setMatchCoordinator(m_match);
    m_gameStateController->setShogiView(m_shogiView);
    m_gameStateController->setRecordPane(m_recordPane);
    m_gameStateController->setReplayController(m_replayController);
    m_gameStateController->setTimeController(m_timeController);
    m_gameStateController->setKifuLoadCoordinator(m_kifuLoadCoordinator);
    m_gameStateController->setKifuRecordModel(m_kifuRecordModel);
    m_gameStateController->setPlayMode(m_playMode);

    // コールバックの設定: 対局終了時にUI状態を「アイドル」に遷移
    ensureUiStatePolicyManager();
    m_gameStateController->setEnableArrowButtonsCallback(
        std::bind(&UiStatePolicyManager::transitionToIdle, m_uiStatePolicy));
    m_gameStateController->setSetReplayModeCallback(
        std::bind(&MainWindow::setReplayMode, this, std::placeholders::_1));
    m_gameStateController->setRefreshBranchTreeCallback([this]() {
        m_currentSelectedPly = 0;  // リセット
        refreshBranchTreeLive();
    });
    m_gameStateController->setUpdatePlyStateCallback([this](int activePly, int selectedPly, int moveIndex) {
        m_activePly = activePly;
        m_currentSelectedPly = selectedPly;
        m_currentMoveIndex = moveIndex;
    });
}

// `ensurePlayerInfoController`: Player Info Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensurePlayerInfoController()
{
    if (m_playerInfoController) return;

    // PlayerInfoWiring経由でPlayerInfoControllerを取得
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoController = m_playerInfoWiring->playerInfoController();
    }

    // PlayerInfoControllerが取得できた場合、追加の依存オブジェクトを設定
    if (m_playerInfoController) {
        m_playerInfoController->setEvalGraphController(m_evalGraphController);
        m_playerInfoController->setLogModels(m_lineEditModel1, m_lineEditModel2);
        m_playerInfoController->setAnalysisTab(m_analysisTab);
    }
}

// `ensureBoardSetupController`: Board Setup Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureBoardSetupController()
{
    if (m_boardSetupController) return;

    m_boardSetupController = new BoardSetupController(this);

    // 依存オブジェクトの設定
    m_boardSetupController->setShogiView(m_shogiView);
    m_boardSetupController->setGameController(m_gameController);
    m_boardSetupController->setMatchCoordinator(m_match);
    m_boardSetupController->setTimeController(m_timeController);
    m_boardSetupController->setSfenRecord(sfenRecord());
    m_boardSetupController->setGameMoves(&m_gameMoves);
    m_boardSetupController->setCurrentMoveIndex(&m_currentMoveIndex);

    // コールバックの設定
    m_boardSetupController->setEnsurePositionEditCallback([this]() {
        ensurePositionEditController();
    });
    m_boardSetupController->setEnsureTimeControllerCallback([this]() {
        ensureTimeController();
    });
    m_boardSetupController->setUpdateGameRecordCallback([this](const QString& moveText, const QString& elapsed) {
        m_lastMove = moveText;
        updateGameRecord(elapsed);
    });
    m_boardSetupController->setRedrawEngine1GraphCallback([this](int ply) {
        ensureEvaluationGraphController();
        if (m_evalGraphController) m_evalGraphController->redrawEngine1Graph(ply);
    });
    m_boardSetupController->setRedrawEngine2GraphCallback([this](int ply) {
        ensureEvaluationGraphController();
        if (m_evalGraphController) m_evalGraphController->redrawEngine2Graph(ply);
    });
    m_boardSetupController->setRefreshBranchTreeCallback([this]() {
        refreshBranchTreeLive();
    });
}

// `ensurePvClickController`: Pv Click Controller を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensurePvClickController()
{
    if (m_pvClickController) return;

    m_pvClickController = new PvClickController(this);

    // 依存オブジェクトの設定
    m_pvClickController->setThinkingModels(m_modelThinking1, m_modelThinking2);
    m_pvClickController->setConsiderationModel(m_considerationModel);
    m_pvClickController->setLogModels(m_lineEditModel1, m_lineEditModel2);
    m_pvClickController->setSfenRecord(sfenRecord());
    m_pvClickController->setGameMoves(&m_gameMoves);
    m_pvClickController->setUsiMoves(&m_gameUsiMoves);
    QObject::connect(
        m_pvClickController, &PvClickController::pvDialogClosed,
        this,                &MainWindow::onPvDialogClosed,
        Qt::UniqueConnection);
}


// `ensurePositionEditCoordinator`: Position Edit Coordinator を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensurePositionEditCoordinator()
{
    if (m_posEditCoordinator) return;

    m_posEditCoordinator = new PositionEditCoordinator(this);

    // 依存オブジェクトの設定
    m_posEditCoordinator->setShogiView(m_shogiView);
    m_posEditCoordinator->setGameController(m_gameController);
    m_posEditCoordinator->setBoardController(m_boardController);
    m_posEditCoordinator->setPositionEditController(m_posEdit);
    m_posEditCoordinator->setMatchCoordinator(m_match);
    m_posEditCoordinator->setReplayController(m_replayController);
    m_posEditCoordinator->setSfenRecord(sfenRecord());

    // 状態参照の設定
    m_posEditCoordinator->setCurrentSelectedPly(&m_currentSelectedPly);
    m_posEditCoordinator->setActivePly(&m_activePly);
    m_posEditCoordinator->setStartSfenStr(&m_startSfenStr);
    m_posEditCoordinator->setCurrentSfenStr(&m_currentSfenStr);
    m_posEditCoordinator->setResumeSfenStr(&m_resumeSfenStr);
    m_posEditCoordinator->setOnMainRowGuard(&m_onMainRowGuard);

    // コールバックの設定：局面編集状態の遷移をUiStatePolicyManagerに委譲
    ensureUiStatePolicyManager();
    m_posEditCoordinator->setApplyEditMenuStateCallback([this](bool editing) {
        if (editing) {
            m_uiStatePolicy->transitionToDuringPositionEdit();
        } else {
            m_uiStatePolicy->transitionToIdle();
        }
    });
    m_posEditCoordinator->setEnsurePositionEditCallback([this]() {
        ensurePositionEditController();
        if (m_posEditCoordinator) {
            m_posEditCoordinator->setPositionEditController(m_posEdit);
        }
    });

    // 編集用アクションの設定
    if (ui) {
        PositionEditCoordinator::EditActions actions;
        actions.actionReturnAllPiecesToStand = ui->actionReturnAllPiecesToStand;
        actions.actionSetHiratePosition = ui->actionSetHiratePosition;
        actions.actionSetTsumePosition = ui->actionSetTsumePosition;
        actions.actionChangeTurn = ui->actionChangeTurn;
        m_posEditCoordinator->setEditActions(actions);
    }
}

// `ensureCsaGameWiring`: Csa Game Wiring を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureCsaGameWiring()
{
    if (m_csaGameWiring) return;

    CsaGameWiring::Dependencies deps;
    deps.coordinator = m_csaGameCoordinator;
    deps.gameController = m_gameController;
    deps.shogiView = m_shogiView;
    deps.kifuRecordModel = m_kifuRecordModel;
    deps.recordPane = m_recordPane;
    deps.boardController = m_boardController;
    deps.statusBar = ui ? ui->statusbar : nullptr;
    deps.sfenRecord = sfenRecord();
    // CSA対局開始用の追加依存オブジェクト
    deps.analysisTab = m_analysisTab;
    deps.boardSetupController = m_boardSetupController;
    deps.usiCommLog = m_lineEditModel1;
    deps.engineThinking = m_modelThinking1;
    deps.timeController = m_timeController;
    deps.gameMoves = &m_gameMoves;

    m_csaGameWiring = new CsaGameWiring(deps, this);

    // CsaGameWiringからのシグナルをMainWindowに接続
    // CSA対局開始/終了時のUI状態遷移
    ensureUiStatePolicyManager();
    connect(m_csaGameWiring, &CsaGameWiring::disableNavigationRequested,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToDuringCsaGame);
    connect(m_csaGameWiring, &CsaGameWiring::enableNavigationRequested,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToIdle);
    connect(m_csaGameWiring, &CsaGameWiring::appendKifuLineRequested,
            this, &MainWindow::appendKifuLine);
    connect(m_csaGameWiring, &CsaGameWiring::refreshBranchTreeRequested,
            this, &MainWindow::refreshBranchTreeLive);
    connect(m_csaGameWiring, &CsaGameWiring::playModeChanged,
            this, &MainWindow::onCsaPlayModeChanged);
    connect(m_csaGameWiring, &CsaGameWiring::showGameEndDialogRequested,
            this, &MainWindow::onCsaShowGameEndDialog);
    connect(m_csaGameWiring, &CsaGameWiring::errorMessageRequested,
            this, &MainWindow::displayErrorMessage);

    qCDebug(lcApp).noquote() << "ensureCsaGameWiring_: created and connected";
}

// `ensureJosekiWiring`: Joseki Wiring を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureJosekiWiring()
{
    if (m_josekiWiring) return;

    JosekiWindowWiring::Dependencies deps;
    deps.parentWidget = this;
    deps.gameController = m_gameController;
    deps.kifuRecordModel = m_kifuRecordModel;
    deps.sfenRecord = sfenRecord();
    deps.usiMoves = &m_gameUsiMoves;
    deps.currentSfenStr = &m_currentSfenStr;
    deps.currentMoveIndex = &m_currentMoveIndex;
    deps.currentSelectedPly = &m_currentSelectedPly;
    deps.playMode = &m_playMode;

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
    deps.playMode = &m_playMode;
    deps.humanName1 = &m_humanName1;
    deps.humanName2 = &m_humanName2;
    deps.engineName1 = &m_engineName1;
    deps.engineName2 = &m_engineName2;

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
    deps.kifuRecordModel = m_kifuRecordModel;
    deps.kifuBranchModel = m_kifuBranchModel;
    deps.lineEditModel1 = m_lineEditModel1;
    deps.lineEditModel2 = m_lineEditModel2;
    deps.timeController = m_timeController;
    deps.evalChart = m_evalChart;
    deps.evalGraphController = m_evalGraphController;
    deps.recordPane = m_recordPane;
    deps.startSfenStr = &m_startSfenStr;
    deps.currentSfenStr = &m_currentSfenStr;
    deps.activePly = &m_activePly;
    deps.currentSelectedPly = &m_currentSelectedPly;
    deps.currentMoveIndex = &m_currentMoveIndex;
    deps.liveGameSession = m_liveGameSession;
    deps.branchTree = m_branchTree;
    deps.navState = m_navState;

    m_preStartCleanupHandler = new PreStartCleanupHandler(deps, this);

    // PreStartCleanupHandlerからのシグナルをMainWindowに接続
    connect(m_preStartCleanupHandler, &PreStartCleanupHandler::broadcastCommentRequested,
            this, &MainWindow::broadcastComment);

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
    d.gameMoves  = &m_gameMoves;

    m_boardSync = new BoardSyncPresenter(d, this);

    qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: created m_boardSync*=" << static_cast<const void*>(m_boardSync);
}

// `ensureAnalysisPresenter`: Analysis Presenter を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureAnalysisPresenter()
{
    if (!m_analysisPresenter)
        m_analysisPresenter = new AnalysisResultsPresenter(this);
}

// `ensureGameStartCoordinator`: Game Start Coordinator を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureGameStartCoordinator()
{
    if (m_gameStart) return;

    GameStartCoordinator::Deps d;
    d.match = m_match;
    d.clock = m_timeController ? m_timeController->clock() : nullptr;
    d.gc    = m_gameController;
    d.view  = m_shogiView;

    m_gameStart = new GameStartCoordinator(d, this);

    // 依頼シグナルを既存メソッドへ接続（ラムダ不使用）
    connect(m_gameStart, &GameStartCoordinator::requestPreStartCleanup,
            this, &MainWindow::onPreStartCleanupRequested);

    connect(m_gameStart, &GameStartCoordinator::requestApplyTimeControl,
            this, &MainWindow::onApplyTimeControlRequested);

    // 対局者名確定シグナルを接続
    connect(m_gameStart, &GameStartCoordinator::playerNamesResolved,
            this, &MainWindow::onPlayerNamesResolved);

    // 連続対局設定シグナルを接続
    connect(m_gameStart, &GameStartCoordinator::consecutiveGamesConfigured,
            this, &MainWindow::onConsecutiveGamesConfigured);

    // 対局開始時にUI状態を「対局中」に遷移
    ensureUiStatePolicyManager();
    connect(m_gameStart, &GameStartCoordinator::started,
            m_uiStatePolicy, &UiStatePolicyManager::transitionToDuringGame);

    // 対局開始時の設定を保存（連続対局用）
    connect(m_gameStart, &GameStartCoordinator::started,
            this, &MainWindow::onGameStarted);

    // 盤面反転シグナルを接続（人を手前に表示する機能用）
    connect(m_gameStart, &GameStartCoordinator::boardFlipped,
            this, &MainWindow::onBoardFlipped,
            Qt::UniqueConnection);

    // 現在局面から開始時、対局開始後に棋譜欄の指定行を選択
    connect(m_gameStart, &GameStartCoordinator::requestSelectKifuRow,
            this, &MainWindow::onRequestSelectKifuRow,
            Qt::UniqueConnection);
}

// `onPreStartCleanupRequested`: Pre Start Cleanup Requested のイベント受信時処理を行う。
void MainWindow::onPreStartCleanupRequested()
{
    ensurePreStartCleanupHandler();
    if (m_preStartCleanupHandler) {
        m_preStartCleanupHandler->performCleanup();
    }
}

// `onApplyTimeControlRequested`: Apply Time Control Requested のイベント受信時処理を行う。
void MainWindow::onApplyTimeControlRequested(const GameStartCoordinator::TimeControl& tc)
{
    qCDebug(lcApp).noquote()
    << "onApplyTimeControlRequested_:"
    << " enabled=" << tc.enabled
    << " P1{base=" << tc.p1.baseMs << " byoyomi=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
    << " P2{base=" << tc.p2.baseMs << " byoyomi=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // 連続対局用に時間設定を保存
    m_lastTimeControl = tc;

    // 時間設定の適用をTimeControlControllerに委譲
    if (m_timeController) {
        m_timeController->applyTimeControl(tc, m_match, m_startSfenStr, m_currentSfenStr, m_shogiView);
    }

    // 対局情報ドックに持ち時間を追加
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring && tc.enabled) {
        m_playerInfoWiring->updateGameInfoWithTimeControl(
            tc.enabled,
            tc.p1.baseMs,
            tc.p1.byoyomiMs,
            tc.p1.incrementMs
        );
    }
}

// `ensureRecordPresenter`: Record Presenter を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureRecordPresenter()
{
    if (m_recordPresenter) return;

    GameRecordPresenter::Deps d;
    d.model      = m_kifuRecordModel;
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
    if (m_liveGameSession == nullptr || m_branchTree == nullptr) {
        return;
    }

    if (m_liveGameSession->isActive()) {
        return;  // 既に開始済み
    }

    // ルートノードが必要（performCleanup で作成済みのはず）
    if (m_branchTree->root() == nullptr) {
        qCWarning(lcApp).noquote() << "ensureLiveGameSessionStarted: root is null, cannot start session";
        return;
    }

    // 平手初期局面SFEN
    static const QString kHirateStartSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    // 現在の局面が途中局面かどうかを判定
    const QString currentSfen = m_currentSfenStr.trimmed();
    bool isFromMidPosition = !currentSfen.isEmpty() &&
                              currentSfen != QStringLiteral("startpos") &&
                              !currentSfen.startsWith(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));

    if (isFromMidPosition) {
        // 途中局面から開始: SFENからノードを探す
        KifuBranchNode* branchPoint = m_branchTree->findBySfen(currentSfen);
        if (branchPoint != nullptr) {
            m_liveGameSession->startFromNode(branchPoint);
            qCDebug(lcApp).noquote() << "ensureLiveGameSessionStarted: started from node, ply=" << branchPoint->ply();
        } else {
            // ノードが見つからない場合はルートから開始
            qCWarning(lcApp).noquote() << "ensureLiveGameSessionStarted: branchPoint not found for sfen, starting from root";
            m_liveGameSession->startFromRoot();
        }
    } else {
        // 新規対局: ルートから開始
        m_liveGameSession->startFromRoot();
        qCDebug(lcApp).noquote() << "ensureLiveGameSessionStarted: session started from root";
    }
}

// UIスレッド安全のため queued 呼び出しにしています
void MainWindow::initializeNewGameHook(const QString& s)
{
    // --- デバッグ：誰がこの関数を呼び出したか追跡 ---
    qCDebug(lcApp) << "MainWindow::initializeNewGameHook called with sfen:" << s;
    qCDebug(lcApp) << "  PlayMode:" << static_cast<int>(m_playMode);
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

    refreshBranchTreeLive();
}

// `onRecordRowChangedByPresenter`: Record Row Changed By Presenter のイベント受信時処理を行う。
void MainWindow::onRecordRowChangedByPresenter(int row, const QString& comment)
{
    qCDebug(lcApp) << "onRecordRowChangedByPresenter called: row=" << row
             << "comment=" << comment.left(30) << "..."
             << "playMode=" << static_cast<int>(m_playMode);


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

    // 盤面・ハイライト同期
    if (row >= 0) {
        syncBoardAndHighlightsAtRow(row);

        // 現在手数トラッキングを更新
        m_activePly = row;
        m_currentSelectedPly = row;
        m_currentMoveIndex = row;

    }

    // コメント表示
    const QString cmt = comment.trimmed();
    broadcastComment(cmt.isEmpty() ? tr("コメントなし") : cmt, true);

    // 矢印ボタンなどの活性化
    enableArrowButtons();

    // 検討モード中であれば、選択した手の局面で検討を再開
    if (m_playMode == PlayMode::ConsiderationMode) {
        const QString newPosition = buildPositionStringForIndex(row);
        ensureConsiderationWiring();
        if (m_considerationWiring) {
            m_considerationWiring->updatePositionIfNeeded(
                row, newPosition, &m_gameMoves, m_kifuRecordModel);
        }
    }
}

// `onFlowError`: Flow Error のイベント受信時処理を行う。
void MainWindow::onFlowError(const QString& msg)
{
    // 既存のエラー表示に委譲（UI 専用）
    displayErrorMessage(msg);
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

// `buildPositionStringForIndex`: Position String For Index を構築する。
QString MainWindow::buildPositionStringForIndex(int moveIndex) const
{
    // 1) m_positionStrList に該当インデックスがあればそれを使用（棋譜読み込み時）
    if (moveIndex >= 0 && moveIndex < m_positionStrList.size()) {
        return m_positionStrList.at(moveIndex);
    }

    // 2) 対局後: m_gameUsiMoves または m_kifuLoadCoordinator から構築
    const QStringList* usiMoves = nullptr;
    if (!m_gameUsiMoves.isEmpty()) {
        usiMoves = &m_gameUsiMoves;
    } else if (m_kifuLoadCoordinator) {
        usiMoves = m_kifuLoadCoordinator->kifuUsiMovesPtr();
    }

    // 開始局面コマンドを決定
    QString startCmd;
    const QString kHirateSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    if (m_startSfenStr.isEmpty() || m_startSfenStr == kHirateSfen) {
        startCmd = QStringLiteral("startpos");
    } else {
        startCmd = QStringLiteral("sfen ") + m_startSfenStr;
    }

    if (usiMoves && !usiMoves->isEmpty()) {
        // USI指し手リストから構築
        return TsumePositionUtil::buildPositionWithMoves(usiMoves, startCmd, moveIndex);
    }

    // 3) フォールバック: SFEN形式で構築
    if (sfenRecord() && moveIndex >= 0 && moveIndex < sfenRecord()->size()) {
        const QString sfen = sfenRecord()->at(moveIndex);
        return QStringLiteral("position sfen ") + sfen;
    }

    return QString();
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
    m_recordNavHandler->onMainRowChanged(row);
}

// `onBuildPositionRequired`: Build Position Required のイベント受信時処理を行う。
void MainWindow::onBuildPositionRequired(int row)
{
    qCDebug(lcApp).noquote() << "onBuildPositionRequired ENTER row=" << row
                       << "playMode=" << static_cast<int>(m_playMode)
                       << "match=" << (m_match ? "valid" : "null");
    if (m_playMode != PlayMode::ConsiderationMode || !m_match) {
        qCDebug(lcApp).noquote() << "onBuildPositionRequired: not in consideration mode or no match, returning";
        return;
    }

    const QString newPosition = buildPositionStringForIndex(row);
    qCDebug(lcApp).noquote() << "onBuildPositionRequired: newPosition=" << newPosition;
    if (newPosition.isEmpty()) {
        return;
    }

    // 選択した手の移動先を取得（「同」表記のため）
    int previousFileTo = 0;
    int previousRankTo = 0;
    if (row >= 0 && row < m_gameMoves.size()) {
        const QPoint& toSquare = m_gameMoves.at(row).toSquare;
        previousFileTo = toSquare.x();
        previousRankTo = toSquare.y();
    } else if (m_kifuRecordModel && row > 0 && row < m_kifuRecordModel->rowCount()) {
        // 棋譜読み込み時: ShogiUtilsを使用して座標を解析（「同」の場合は自動的に遡る）
        ShogiUtils::parseMoveCoordinateFromModel(m_kifuRecordModel, row, &previousFileTo, &previousRankTo);
    }

    // 開始局面に至った最後の指し手を取得（読み筋表示ウィンドウのハイライト用）
    QString lastUsiMove;
    if (row > 0) {
        // 分岐ライン上では、現在ラインのノードを優先する。
        if (m_navState && m_branchTree) {
            const int lineIndex = m_navState->currentLineIndex();
            const QVector<BranchLine> lines = m_branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                for (KifuBranchNode* node : line.nodes) {
                    if (!node) continue;
                    if (node->ply() == row && node->isActualMove()) {
                        const ShogiMove mv = node->move();
                        if (mv.movingPiece != QLatin1Char(' ')) {
                            lastUsiMove = ShogiUtils::moveToUsi(mv);
                        }
                        break;
                    }
                }
            }
        }

        const int usiIdx = row - 1;
        if (lastUsiMove.isEmpty() && usiIdx >= 0) {
            if (!m_gameUsiMoves.isEmpty() && usiIdx < m_gameUsiMoves.size()) {
                lastUsiMove = m_gameUsiMoves.at(usiIdx);
            } else if (m_kifuLoadCoordinator) {
                const QStringList& kifuUsiMoves = m_kifuLoadCoordinator->kifuUsiMoves();
                if (usiIdx < kifuUsiMoves.size()) {
                    lastUsiMove = kifuUsiMoves.at(usiIdx);
                }
            } else if (usiIdx < m_gameMoves.size()) {
                lastUsiMove = ShogiUtils::moveToUsi(m_gameMoves.at(usiIdx));
            }
        }
    }

    if (m_match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo, lastUsiMove)) {
        // ポジションが変更された場合、経過時間タイマーをリセットして再開
        if (m_analysisTab) {
            m_analysisTab->startElapsedTimer();
        }
    }
}

// ============================================================
void MainWindow::createAndWireKifuLoadCoordinator()
{
    // 既存があれば即座に破棄（多重生成対策）
    if (m_kifuLoadCoordinator) {
        delete m_kifuLoadCoordinator;
        m_kifuLoadCoordinator = nullptr;
    }

    m_kifuLoadCoordinator = new KifuLoadCoordinator(
        /* gameMoves           */ m_gameMoves,
        /* positionStrList     */ m_positionStrList,
        /* activePly           */ m_activePly,
        /* currentSelectedPly  */ m_currentSelectedPly,
        /* currentMoveIndex    */ m_currentMoveIndex,
        /* sfenRecord          */ sfenRecord(),
        /* gameInfoTable       */ m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr,
        /* gameInfoDock        */ nullptr,  // GameInfoPaneControllerに移行済み
        /* tab                 */ m_tab,
        /* recordPane          */ m_recordPane,
        /* kifuRecordModel     */ m_kifuRecordModel,
        /* kifuBranchModel     */ m_kifuBranchModel,
        /* parent              */ this
        );

    // 分岐ツリーとナビゲーション状態を設定
    if (m_branchTree != nullptr) {
        m_kifuLoadCoordinator->setBranchTree(m_branchTree);
    }
    if (m_navState != nullptr) {
        m_kifuLoadCoordinator->setNavigationState(m_navState);
    }

    // 分岐ツリー構築完了シグナルを接続
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::branchTreeBuilt,
            this, &MainWindow::onBranchTreeBuilt, Qt::UniqueConnection);

    // Analysisタブ・ShogiViewとの配線
    m_kifuLoadCoordinator->setAnalysisTab(m_analysisTab);
    m_kifuLoadCoordinator->setShogiView(m_shogiView);

    // UI更新通知
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::displayGameRecord,
            this, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
            this, &MainWindow::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::enableArrowButtons,
            this, &MainWindow::enableArrowButtons, Qt::UniqueConnection);
    // 対局情報の元データを保存（PlayerInfoWiring経由）
    ensurePlayerInfoWiring();
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::gameInfoPopulated,
            m_playerInfoWiring, &PlayerInfoWiring::setOriginalGameInfo, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::errorOccurred,
            this, &MainWindow::displayErrorMessage, Qt::UniqueConnection);
}

// ============================================================
void MainWindow::dispatchKifuLoad(const QString& filePath)
{
    if (filePath.endsWith(QLatin1String(".csa"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadCsaFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".ki2"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadKi2FromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".jkf"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadJkfFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".usen"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadUsenFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".usi"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadUsiFromFile(filePath);
    } else {
        m_kifuLoadCoordinator->loadKifuFromFile(filePath);
    }
}

// ============================================================
void MainWindow::ensureKifuLoadCoordinatorForLive()
{
    if (m_kifuLoadCoordinator) {
        return; // 既に用意済み
    }

    createAndWireKifuLoadCoordinator();
}

// ライブ対局中の分岐ツリー更新エントリポイント（互換用）。
void MainWindow::refreshBranchTreeLive()
{
    // 現行実装では LiveGameSession + KifuDisplayCoordinator が自動更新する。
    // 旧呼び出し元との互換維持のため no-op として残している。
    qCDebug(lcApp).noquote() << "refreshBranchTreeLive(): delegating to new system (no-op)";
}

// ============================================================

bool MainWindow::getMainRowGuard() const
{
    return m_onMainRowGuard;
}

// `setMainRowGuard`: Main Row Guard を設定する。
void MainWindow::setMainRowGuard(bool on)
{
    m_onMainRowGuard = on;
}

// `isHvH`: Hv H かどうかを判定する。
bool MainWindow::isHvH() const
{
    if (m_gameStateController) {
        return m_gameStateController->isHvH();
    }
    return (m_playMode == PlayMode::HumanVsHuman);
}

// `isHumanSide`: Human Side かどうかを判定する。
bool MainWindow::isHumanSide(ShogiGameController::Player p) const
{
    if (m_gameStateController) {
        return m_gameStateController->isHumanSide(p);
    }
    // フォールバック
    switch (m_playMode) {
    case PlayMode::HumanVsHuman:
        return true;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        return (p == ShogiGameController::Player1);
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        return (p == ShogiGameController::Player2);
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        return false;
    default:
        return true;
    }
}

// `updateTurnAndTimekeepingDisplay`: Turn And Timekeeping Display を更新する。
void MainWindow::updateTurnAndTimekeepingDisplay()
{
    // 手番ラベルなど
    setCurrentTurn();

    // 時計の再描画（Presenterに現在値を流し直し）
    ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr;
    if (m_timePresenter && clk) {
        const qint64 p1 = clk->getPlayer1TimeIntMs();
        const qint64 p2 = clk->getPlayer2TimeIntMs();
        const bool p1turn =
            (m_gameController ? (m_gameController->currentPlayer() == ShogiGameController::Player1) : true);
        m_timePresenter->onMatchTimeUpdated(p1, p2, p1turn, /*urgencyMs*/ 0);
    }
}

// `updateGameRecord`: Game Record を更新する。
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    const bool gameOverAppended =
        (m_match && m_match->gameOverState().isOver && m_match->gameOverState().moveAppended);
    if (gameOverAppended) return;

    ensureRecordPresenter();
    if (m_recordPresenter) {
        m_recordPresenter->appendMoveLine(m_lastMove, elapsedTime);

        if (!m_lastMove.isEmpty()) {
            m_recordPresenter->addLiveKifItem(m_lastMove, elapsedTime);
        }
    }

    if (m_liveGameSession != nullptr && !m_lastMove.isEmpty()) {
        // セッションが未開始の場合は遅延開始
        if (!m_liveGameSession->isActive()) {
            ensureLiveGameSessionStarted();
        }

        if (m_liveGameSession->isActive()) {
            //    実際の盤面から完全な SFEN を構築する必要がある
            QString sfen;
            if (m_gameController && m_gameController->board()) {
                ShogiBoard* board = m_gameController->board();
                // 完全な SFEN を構築: <盤面> <手番> <持ち駒> <手数>
                const QString boardPart = board->convertBoardToSfen();
                // 手番は GC を正とする（board->currentPlayer() は setSfen() 以外で更新されない）
                const QString turnPart =
                    (m_gameController->currentPlayer() == ShogiGameController::Player2)
                        ? QStringLiteral("w")
                        : QStringLiteral("b");
                QString standPart = board->convertStandToSfen();
                if (standPart.isEmpty()) {
                    standPart = QStringLiteral("-");
                }
                // 手数は anchorPly + 現在のセッション内の手数 + 1
                const int moveCount = m_liveGameSession->totalPly() + 1;
                sfen = QStringLiteral("%1 %2 %3 %4")
                           .arg(boardPart, turnPart, standPart, QString::number(moveCount));
                qCDebug(lcApp).noquote() << "appendMoveLineToRecordPane: constructed full SFEN for LiveGameSession"
                                   << "sfen=" << sfen.left(80);
            } else if (sfenRecord() && !sfenRecord()->isEmpty()) {
                // フォールバック: 盤面がない場合は sfenRecord() を使用
                sfen = sfenRecord()->last();
                qCWarning(lcApp) << "appendMoveLineToRecordPane: fallback to sfenRecord() (no board)";
            }

            ShogiMove move;
            if (!m_gameMoves.isEmpty()) {
                move = m_gameMoves.last();
            }

            m_liveGameSession->addMove(move, m_lastMove, sfen, elapsedTime);
        }
    }

    m_lastMove.clear();
}

// 新しい保存関数
void MainWindow::saveKifuToFile()
{
    ensureGameRecordModel();
    ensureKifuExportController();
    updateKifuExportDependencies();

    const QString path = m_kifuExportController->saveToFile();
    if (!path.isEmpty()) {
        kifuSaveFileName = path;
    }
}

// 棋譜自動保存
void MainWindow::autoSaveKifuToFile(const QString& saveDir, PlayMode playMode,
                                      const QString& humanName1, const QString& humanName2,
                                      const QString& engineName1, const QString& engineName2)
{
    Q_UNUSED(playMode);
    Q_UNUSED(humanName1);
    Q_UNUSED(humanName2);
    Q_UNUSED(engineName1);
    Q_UNUSED(engineName2);
    qCDebug(lcApp) << "autoSaveKifuToFile_ called: dir=" << saveDir
             << "mode=" << static_cast<int>(playMode);

    // GameRecordModel と KifuExportController の準備
    ensureGameRecordModel();
    ensureKifuExportController();
    updateKifuExportDependencies();

    if (!m_kifuExportController) {
        qCWarning(lcApp) << "autoSaveKifuToFile_: kifuExportController is null";
        return;
    }

    QString savedPath;
    const bool ok = m_kifuExportController->autoSaveToDir(saveDir, &savedPath);
    if (ok && !savedPath.isEmpty()) {
        kifuSaveFileName = savedPath;
    }
}

// KIF形式で棋譜をクリップボードにコピー
void MainWindow::pasteKifuFromClipboard()
{
    KifuPasteDialog* dlg = new KifuPasteDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // ダイアログの「取り込む」シグナルをKifuLoadCoordinatorに接続
    connect(dlg, &KifuPasteDialog::importRequested,
            this, &MainWindow::onKifuPasteImportRequested);

    dlg->show();
}

// `onKifuPasteImportRequested`: Kifu Paste Import Requested のイベント受信時処理を行う。
void MainWindow::onKifuPasteImportRequested(const QString& content)
{
    qCDebug(lcApp).noquote() << "onKifuPasteImportRequested_: content length =" << content.size();

    // KifuLoadCoordinator を確保
    ensureKifuLoadCoordinatorForLive();

    if (m_kifuLoadCoordinator) {
        // 文字列から棋譜を読み込み
        const bool success = m_kifuLoadCoordinator->loadKifuFromString(content);
        if (success) {
            ui->statusbar->showMessage(tr("棋譜を取り込みました"), 3000);
        } else {
            ui->statusbar->showMessage(tr("棋譜の取り込みに失敗しました"), 3000);
        }
    } else {
        qCWarning(lcApp) << "onKifuPasteImportRequested_: m_kifuLoadCoordinator is null";
        ui->statusbar->showMessage(tr("棋譜の取り込みに失敗しました（内部エラー）"), 3000);
    }
}

void MainWindow::displaySfenCollectionViewer()
{
    SfenCollectionDialog* dlg = new SfenCollectionDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SfenCollectionDialog::positionSelected,
            this, &MainWindow::onSfenCollectionPositionSelected);
    dlg->show();
}

void MainWindow::onSfenCollectionPositionSelected(const QString& sfen)
{
    ensureKifuLoadCoordinatorForLive();

    if (m_kifuLoadCoordinator) {
        const bool success = m_kifuLoadCoordinator->loadPositionFromSfen(sfen);
        if (success) {
            ui->statusbar->showMessage(tr("局面を反映しました"), 3000);
        } else {
            ui->statusbar->showMessage(tr("局面の反映に失敗しました"), 3000);
        }
    } else {
        ui->statusbar->showMessage(tr("局面の反映に失敗しました（内部エラー）"), 3000);
    }
}

// `overwriteKifuFile`: 既存保存先への上書き保存を行う（保存先未確定なら通常保存へフォールバック）。
void MainWindow::overwriteKifuFile()
{
    if (kifuSaveFileName.isEmpty()) {
        saveKifuToFile();
        return;
    }

    ensureGameRecordModel();
    ensureKifuExportController();
    updateKifuExportDependencies();
    m_kifuExportController->overwriteFile(kifuSaveFileName);
}

void MainWindow::onCommentUpdated(int moveIndex, const QString& newComment)
{
    // EngineAnalysisTab::m_currentMoveIndex は broadcastComment() 経由でしか
    // 同期されないため、信頼できる MainWindow::m_currentMoveIndex を優先する。
    const int effectiveIndex = (m_currentMoveIndex >= 0) ? m_currentMoveIndex : moveIndex;

    qCDebug(lcApp).noquote()
        << "onCommentUpdated:"
        << " signalIndex=" << moveIndex
        << " m_currentMoveIndex=" << m_currentMoveIndex
        << " effectiveIndex=" << effectiveIndex;

    ensureGameRecordModel();
    ensureCommentCoordinator();
    if (m_commentCoordinator) {
        m_commentCoordinator->setGameRecordModel(m_gameRecord);
        m_commentCoordinator->onCommentUpdated(effectiveIndex, newComment);
    }
}

void MainWindow::ensureGameRecordModel()
{
    if (m_gameRecord) return;

    m_gameRecord = new GameRecordModel(this);

    // 外部データストアをバインド
    QList<KifDisplayItem>* liveDispPtr = nullptr;
    if (m_recordPresenter) {
        // const_cast は GameRecordModel 内部での同期更新に必要
        liveDispPtr = const_cast<QList<KifDisplayItem>*>(&m_recordPresenter->liveDisp());
    }

    m_gameRecord->bind(liveDispPtr);

    if (m_branchTree != nullptr) {
        m_gameRecord->setBranchTree(m_branchTree);
    }
    if (m_navState != nullptr) {
        m_gameRecord->setNavigationState(m_navState);
    }

    // commentChanged シグナルを接続（将来の拡張用）
    connect(m_gameRecord, &GameRecordModel::commentChanged,
            this, &MainWindow::onGameRecordCommentChanged);

    // コメント更新時のコールバックを設定
    // m_commentsByRow 同期、RecordPresenter通知、UI更新を自動実行
    m_gameRecord->setCommentUpdateCallback(
        std::bind(&MainWindow::onCommentUpdateCallback, this,
                  std::placeholders::_1, std::placeholders::_2));

    // しおり更新時のコールバックを設定
    m_gameRecord->setBookmarkUpdateCallback(
        std::bind(&MainWindow::onBookmarkUpdateCallback, this,
                  std::placeholders::_1, std::placeholders::_2));

    qCDebug(lcApp).noquote() << "ensureGameRecordModel_: created and bound";
}

void MainWindow::onGameRecordCommentChanged(int ply, const QString& comment)
{
    ensureCommentCoordinator();
    if (m_commentCoordinator) {
        m_commentCoordinator->onGameRecordCommentChanged(ply, comment);
    }
}

// コメント更新コールバック（GameRecordModel から呼ばれる、CommentCoordinatorに委譲）
void MainWindow::onCommentUpdateCallback(int ply, const QString& comment)
{
    ensureCommentCoordinator();
    if (m_commentCoordinator) {
        m_commentCoordinator->setRecordPresenter(m_recordPresenter);
        m_commentCoordinator->setKifuRecordListModel(m_kifuRecordModel);
        m_commentCoordinator->onCommentUpdateCallback(ply, comment);
    }
}

// しおり更新コールバック（GameRecordModel から呼ばれる、CommentCoordinatorに委譲）
void MainWindow::onBookmarkUpdateCallback(int ply, const QString& bookmark)
{
    ensureCommentCoordinator();
    if (m_commentCoordinator) {
        m_commentCoordinator->setKifuRecordListModel(m_kifuRecordModel);
        m_commentCoordinator->onBookmarkUpdateCallback(ply, bookmark);
    }
}

// しおり編集リクエスト（RecordPaneのボタンから呼ばれる）
void MainWindow::onBookmarkEditRequested()
{
    ensureRecordPresenter();
    ensureCommentCoordinator();
    ensureGameRecordModel();
    if (m_commentCoordinator) {
        m_commentCoordinator->setRecordPresenter(m_recordPresenter);
        m_commentCoordinator->setGameRecordModel(m_gameRecord);
        m_commentCoordinator->setKifuRecordListModel(m_kifuRecordModel);
        m_commentCoordinator->onBookmarkEditRequested();
    }
}

void MainWindow::onPvRowClicked(int engineIndex, int row)
{
    ensurePvClickController();
    if (m_pvClickController) {
        // 状態を同期
        m_pvClickController->setPlayMode(m_playMode);
        m_pvClickController->setPlayerNames(m_humanName1, m_humanName2, m_engineName1, m_engineName2);
        m_pvClickController->setCurrentSfen(m_currentSfenStr);
        m_pvClickController->setStartSfen(m_startSfenStr);
        // 現在選択されている棋譜行のインデックスを設定（ハイライト用）
        m_pvClickController->setCurrentRecordIndex(m_currentMoveIndex);
        // 検討モデルを更新（検討タブから呼ばれた場合に必要）
        m_pvClickController->setConsiderationModel(m_considerationModel);
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

void MainWindow::onCsaPlayModeChanged(int mode)
{
    m_playMode = static_cast<PlayMode>(mode);
    qCDebug(lcApp).noquote() << "onCsaPlayModeChanged_: mode=" << mode;
}

// `onCsaShowGameEndDialog`: Csa Show Game End Dialog のイベント受信時処理を行う。
void MainWindow::onCsaShowGameEndDialog(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}

// `onCsaEngineScoreUpdated`: Csa Engine Score Updated のイベント受信時処理を行う。
void MainWindow::onCsaEngineScoreUpdated(int scoreCp, int ply)
{
    qCDebug(lcApp).noquote() << "onCsaEngineScoreUpdated_: scoreCp=" << scoreCp << "ply=" << ply;

    // 評価値グラフに追加
    if (!m_evalChart) {
        qCDebug(lcApp).noquote() << "onCsaEngineScoreUpdated_: m_evalChart is NULL";
        return;
    }

    // CSA対局では自分側のエンジンの評価値を表示
    // 自分が先手(黒)ならP1に、後手(白)ならP2に追加
    if (m_csaGameCoordinator) {
        bool isBlackSide = m_csaGameCoordinator->isBlackSide();
        if (isBlackSide) {
            // 先手側のエンジン評価値はP1に追加
            m_evalChart->appendScoreP1(ply, scoreCp, false);
            qCDebug(lcApp).noquote() << "onCsaEngineScoreUpdated_: appendScoreP1 called";
        } else {
            // 後手側のエンジン評価値はP2に追加
            // 後手視点の評価値なので、グラフ表示用に反転する
            m_evalChart->appendScoreP2(ply, -scoreCp, false);
            qCDebug(lcApp).noquote() << "onCsaEngineScoreUpdated_: appendScoreP2 called (inverted)";
        }
    }
}

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

// `ensureConsiderationUIController`: Consideration UIController を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureConsiderationUIController()
{
    ensureConsiderationWiring();
    if (m_considerationWiring) {
        m_considerationUIController = m_considerationWiring->uiController();
    }
}

// `ensureConsiderationWiring`: Consideration Wiring を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureConsiderationWiring()
{
    if (m_considerationWiring) return;

    ConsiderationWiring::Deps deps;
    deps.parentWidget = this;
    deps.analysisTab = m_analysisTab;
    deps.shogiView = m_shogiView;
    deps.match = m_match;
    deps.dialogCoordinator = m_dialogCoordinator;
    deps.considerationModel = m_considerationModel;
    deps.commLogModel = m_lineEditModel1;
    deps.playMode = &m_playMode;
    deps.currentSfenStr = &m_currentSfenStr;
    deps.ensureDialogCoordinator = [this]() {
        ensureDialogCoordinator();
        // 初期化後に最新の依存をConsiderationWiringへ反映
        if (m_considerationWiring) {
            ConsiderationWiring::Deps updated;
            updated.parentWidget = this;
            updated.analysisTab = m_analysisTab;
            updated.shogiView = m_shogiView;
            updated.match = m_match;
            updated.dialogCoordinator = m_dialogCoordinator;
            updated.considerationModel = m_considerationModel;
            updated.commLogModel = m_lineEditModel1;
            updated.playMode = &m_playMode;
            updated.currentSfenStr = &m_currentSfenStr;
            m_considerationWiring->updateDeps(updated);
        }
    };

    m_considerationWiring = new ConsiderationWiring(deps, this);

    // ConsiderationWiringからのシグナルを接続
    connect(m_considerationWiring, &ConsiderationWiring::stopRequested,
            this, &MainWindow::stopTsumeSearch);
}

// `ensureDockLayoutManager`: Dock Layout Manager を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureDockLayoutManager()
{
    if (m_dockLayoutManager) return;

    m_dockLayoutManager = new DockLayoutManager(this, this);

    // ドックを登録
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Menu, m_menuWindowDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Joseki, m_josekiWindowDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Record, m_recordPaneDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::GameInfo, m_gameInfoDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Thinking, m_thinkingDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Consideration, m_considerationDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::UsiLog, m_usiLogDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::CsaLog, m_csaLogDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Comment, m_commentDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::BranchTree, m_branchTreeDock);
    m_dockLayoutManager->registerDock(DockLayoutManager::DockType::EvalChart, m_evalChartDock);

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

// `ensureCommentCoordinator`: Comment Coordinator を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureCommentCoordinator()
{
    if (m_commentCoordinator) return;

    m_commentCoordinator = new CommentCoordinator(this);
    m_commentCoordinator->setAnalysisTab(m_analysisTab);
    m_commentCoordinator->setRecordPane(m_recordPane);
    m_commentCoordinator->setRecordPresenter(m_recordPresenter);
    m_commentCoordinator->setStatusBar(ui->statusbar);
    m_commentCoordinator->setCurrentMoveIndex(&m_currentMoveIndex);
    m_commentCoordinator->setCommentsByRow(&m_commentsByRow);
    m_commentCoordinator->setGameRecordModel(m_gameRecord);
    m_commentCoordinator->setKifuRecordListModel(m_kifuRecordModel);

    // GameRecordModel初期化要求時のシグナル接続
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

// `ensureRecordNavigationHandler`: Record Navigation Handler を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureRecordNavigationHandler()
{
    if (!m_recordNavHandler) {
        m_recordNavHandler = new RecordNavigationHandler(this);

        // シグナル接続
        connect(m_recordNavHandler, &RecordNavigationHandler::boardSyncRequired,
                this, &MainWindow::syncBoardAndHighlightsAtRow);
        connect(m_recordNavHandler, &RecordNavigationHandler::branchBoardSyncRequired,
                this, &MainWindow::loadBoardWithHighlights);
        connect(m_recordNavHandler, &RecordNavigationHandler::enableArrowButtonsRequired,
                this, &MainWindow::enableArrowButtons);
        connect(m_recordNavHandler, &RecordNavigationHandler::turnUpdateRequired,
                this, &MainWindow::setCurrentTurn);
        connect(m_recordNavHandler, &RecordNavigationHandler::josekiUpdateRequired,
                this, &MainWindow::updateJosekiWindow);
        connect(m_recordNavHandler, &RecordNavigationHandler::buildPositionRequired,
                this, &MainWindow::onBuildPositionRequired);
    }

    RecordNavigationHandler::Deps deps;
    deps.navState = m_navState;
    deps.branchTree = m_branchTree;
    deps.displayCoordinator = m_displayCoordinator;
    deps.kifuRecordModel = m_kifuRecordModel;
    deps.shogiView = m_shogiView;
    deps.evalGraphController = m_evalGraphController;
    deps.sfenRecord = sfenRecord();
    deps.activePly = &m_activePly;
    deps.currentSelectedPly = &m_currentSelectedPly;
    deps.currentMoveIndex = &m_currentMoveIndex;
    deps.currentSfenStr = &m_currentSfenStr;
    deps.skipBoardSyncForBranchNav = &m_skipBoardSyncForBranchNav;
    deps.csaGameCoordinator = m_csaGameCoordinator;
    deps.playMode = &m_playMode;
    deps.match = m_match;
    m_recordNavHandler->updateDeps(deps);
}

// `ensureUiStatePolicyManager`: UI State Policy Manager を必要に応じて生成し、依存関係を更新する。
void MainWindow::ensureUiStatePolicyManager()
{
    if (!m_uiStatePolicy) {
        m_uiStatePolicy = new UiStatePolicyManager(this);
    }

    UiStatePolicyManager::Deps deps;
    deps.ui = ui;
    deps.recordPane = m_recordPane;
    deps.analysisTab = m_analysisTab;
    deps.boardController = m_boardController;
    m_uiStatePolicy->updateDeps(deps);
}
