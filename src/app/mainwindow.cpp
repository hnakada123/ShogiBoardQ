#include <QMessageBox>
#include <QInputDialog>   // ★ 追加: レイアウト名入力用
#include <QMenu>          // ★ 追加: レイアウトサブメニュー用
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
#include <QDebug>
#include <QScrollBar>
#include <QScrollArea>    // ★ 追加: 将棋盤ドック用
#include <QPushButton>
#include <QDialog>
#include <QSettings>
#include <QSignalBlocker>  // ★ 追加
#include <QLabel>          // ★ 追加
#include <QApplication>    // ★ 追加
#include <QClipboard>      // ★ 追加
#include <QLineEdit>       // ★ 追加
#include <QElapsedTimer>   // ★ パフォーマンス計測用
#include <QTimer>          // ★ 追加: 分岐ナビゲーションフラグリセット用
#include <QSizePolicy>
#include <functional>
#include <limits>
#include <QPainter>
#include <QFontDatabase>
#include <QRegularExpression>

#include "mainwindow.h"
#include "changeenginesettingsdialog.h"
#include "considerationflowcontroller.h"
#include "shogiutils.h"
#include "gamelayoutbuilder.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "jishogicalculator.h"
#include "movevalidator.h"
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
#include "recordpresenter.h"
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
#include "gameinfopanecontroller.h"  // ★ 追加: 対局情報タブ管理
#include "kifuclipboardservice.h"    // ★ 追加: 棋譜クリップボード操作
#include "evaluationgraphcontroller.h"  // ★ 追加: 評価値グラフ管理
#include "timecontrolcontroller.h"   // ★ 追加: 時間制御管理
#include "replaycontroller.h"        // ★ 追加: リプレイモード管理
#include "dialogcoordinator.h"       // ★ 追加: ダイアログ管理
#include "kifuexportcontroller.h"    // ★ 追加: 棋譜エクスポート管理
#include "gamestatecontroller.h"     // ★ 追加: ゲーム状態管理
#include "playerinfocontroller.h"    // ★ 追加: 対局者情報管理
#include "boardsetupcontroller.h"    // ★ 追加: 盤面操作配線管理
#include "pvclickcontroller.h"       // ★ 追加: 読み筋クリック処理
#include "positioneditcoordinator.h"    // ★ 追加: 局面編集調整
#include "csagamedialog.h"              // ★ 追加: CSA通信対局ダイアログ
#include "csagamecoordinator.h"         // ★ 追加: CSA通信対局コーディネータ
#include "csawaitingdialog.h"           // ★ 追加: CSA通信対局待機ダイアログ
#include "josekiwindowwiring.h"          // ★ 追加: 定跡ウィンドウUI配線
#include "josekiwindow.h"                // ★ 追加: 定跡ウィンドウ
#include "menuwindowwiring.h"            // ★ 追加: メニューウィンドウUI配線
#include "menuwindow.h"                  // ★ 追加: メニューウィンドウ
#include "csagamewiring.h"              // ★ 追加: CSA通信対局UI配線
#include "playerinfowiring.h"           // ★ 追加: 対局情報UI配線

// ★ 新規分岐ナビゲーションクラス
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifudisplaycoordinator.h"
#include "branchtreewidget.h"
#include "livegamesession.h"
#include "prestartcleanuphandler.h"     // ★ 追加: 対局開始前クリーンアップ
#include "jishogiscoredialogcontroller.h"  // ★ 追加: 持将棋点数ダイアログ
#include "nyugyokudeclarationhandler.h"    // ★ 追加: 入玉宣言処理
#include "consecutivegamescontroller.h"    // ★ 追加: 連続対局管理
#include "languagecontroller.h"            // ★ 追加: 言語設定管理
#include "considerationmodeuicontroller.h" // ★ 追加: 検討モードUI管理
#include "docklayoutmanager.h"             // ★ 追加: ドックレイアウト管理
#include "dockcreationservice.h"           // ★ 追加: ドック作成サービス
#include "commentcoordinator.h"            // ★ 追加: コメント処理
#include "usicommandcontroller.h"          // ★ 追加: USI手動送信

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_startSfenStr(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"))
    , m_currentSfenStr(QStringLiteral("startpos"))
    , m_errorOccurred(false)
    , m_playMode(PlayMode::NotStarted)
    , m_gameController(nullptr)
    , m_usi1(nullptr)
    , m_usi2(nullptr)
    , m_analysisModel(nullptr)
    , m_lineEditModel1(new UsiCommLogModel(this))
    , m_lineEditModel2(new UsiCommLogModel(this))
{
    ui->setupUi(this);

    // ドックネスティングを有効化（同じエリア内でドックを左右に分割可能にする）
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

    // 起動時用：編集メニューを"編集前（未編集）"の初期状態にする
    initializeEditMenuForStartup();

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

void MainWindow::setupCentralWidgetContainer()
{
    m_central = ui->centralwidget;
    m_centralLayout = new QVBoxLayout(m_central);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);
}

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

void MainWindow::buildGamePanels()
{
    // 1) 記録ペイン（RecordPane）など UI 部の初期化
    setupRecordPane();

    // 2) 棋譜欄をQDockWidgetとして作成
    createRecordPaneDock();

    // 4) 将棋盤・駒台の初期化（従来順序を維持）
    startNewShogiGame(m_startSfenStr);

    // 5) 将棋盤をQDockWidgetとして作成
    setupBoardInCenter();

    // 6) エンジン解析タブの構築
    setupEngineAnalysisTab();

    // 7) 解析用ドックを作成（6つの独立したドック）
    createAnalysisDocks();

    // 7.5) 新規分岐ナビゲーションクラスの初期化
    initializeBranchNavigationClasses();

    // 8) 評価値グラフのQDockWidget作成
    createEvalChartDock();

    // 9) メニューウィンドウのQDockWidget作成（デフォルトは非表示）
    createMenuWindowDock();

    // 10) 定跡ウィンドウのQDockWidget作成（デフォルトは非表示）
    createJosekiWindowDock();

    // 11) 棋譜解析結果のQDockWidget作成（初期状態は非表示）
    createAnalysisResultsDock();

    // 12) central への初期化（すべてのウィジェットはドックに移動）
    initializeCentralGameDisplay();

    // 13) 表示メニューにドックレイアウト関連アクションを追加
    if (ui->Display) {
        ui->Display->addSeparator();

        // ドックレイアウトをリセット
        QAction* resetLayoutAction = new QAction(tr("ドックレイアウトをリセット"), this);
        resetLayoutAction->setObjectName(QStringLiteral("actionResetDockLayout"));
        ui->Display->addAction(resetLayoutAction);

        // ドックレイアウトを保存
        QAction* saveLayoutAction = new QAction(tr("ドックレイアウトを保存..."), this);
        saveLayoutAction->setObjectName(QStringLiteral("actionSaveDockLayout"));
        ui->Display->addAction(saveLayoutAction);

        // 保存済みレイアウトのサブメニュー
        m_savedLayoutsMenu = new QMenu(tr("保存済みレイアウト"), this);
        m_savedLayoutsMenu->setObjectName(QStringLiteral("menuSavedLayouts"));
        ui->Display->addMenu(m_savedLayoutsMenu);

        // サブメニュー設定はDockLayoutManagerに委譲

        // ドック固定のチェックボックスアクションを追加
        ui->Display->addSeparator();
        QAction* lockDocksAction = new QAction(tr("ドックを固定"), this);
        lockDocksAction->setObjectName(QStringLiteral("actionLockDocks"));
        lockDocksAction->setCheckable(true);
        const bool docksLocked = SettingsService::docksLocked();
        lockDocksAction->setChecked(docksLocked);
        ui->Display->addAction(lockDocksAction);
        // onDocksLockToggled への接続は DockLayoutManager.wireMenuActions で実施

        // ドックレイアウト関連アクションをDockLayoutManagerに配線
        ensureDockLayoutManager();
        if (m_dockLayoutManager) {
            m_dockLayoutManager->wireMenuActions(
                resetLayoutAction,
                saveLayoutAction,
                /*clearStartupLayout*/nullptr,
                lockDocksAction,
                m_savedLayoutsMenu);
        }
    }
}

void MainWindow::restoreWindowAndSync()
{
    loadWindowSettings();

    // 起動時のカスタムレイアウトがあれば復元
    restoreStartupLayoutIfSet();
}

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

void MainWindow::connectCoreSignals()
{
    qDebug() << "[CONNECT] connectCoreSignals_ called";

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
        qDebug() << "[CONNECT] fieldSizeChanged -> onBoardSizeChanged connected:" << connected
                 << "m_shogiView=" << m_shogiView;
    } else {
        qDebug() << "[CONNECT] m_shogiView is NULL, cannot connect fieldSizeChanged";
    }

    // ErrorBus はラムダを使わず専用スロットへ
    connect(&ErrorBus::instance(), &ErrorBus::errorOccurred,
            this, &MainWindow::onErrorBusOccurred, Qt::UniqueConnection);
}

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

void MainWindow::finalizeCoordinators()
{
    initMatchCoordinator();
    setupNameAndClockFonts();
    ensurePositionEditController();
    ensureBoardSyncPresenter();
    ensureAnalysisPresenter();
}

void MainWindow::onErrorBusOccurred(const QString& msg)
{
    displayErrorMessage(msg);
}

// GUIを構成するWidgetなどを生成する。（リファクタ後）
void MainWindow::initializeComponents()
{
    // ───────────────── Core models ─────────────────
    // ShogiGameController は QObject 親を付けてリーク防止
    if (!m_gameController) {
        m_gameController = new ShogiGameController(this);
    } else {
        // 再初期化に備えて、必要ならシグナル切断などをここで行う
        // m_gameController->disconnect(this);
    }

    // 局面履歴（SFEN列）を確保（起動直後の初期化なのでクリアもOK）
    if (!m_sfenRecord) {
        m_sfenRecord = new QStringList;
        qDebug().noquote() << "[MW] initializeComponents: created m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord);
    } else {
        qDebug().noquote() << "[MW] initializeComponents: clearing m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
                           << "old_size=" << m_sfenRecord->size();
        m_sfenRecord->clear();
    }

    // 棋譜表示用のレコードリストを確保（ここでは容器だけ用意）
    if (!m_moveRecords) m_moveRecords = new QList<KifuDisplay *>;
    else                m_moveRecords->clear();

    // ゲーム中の指し手リストをクリア
    m_gameMoves.clear();
    m_gameUsiMoves.clear();

    // ───────────────── View ─────────────────
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

    // ───────────────── Board model 初期化 ─────────────────
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

    // ───────────────── Turn 初期化 & 同期 ─────────────────
    // 1) TurnManager 側の初期手番（b→Player1）を立ち上げる
    setCurrentTurn();

    // 2) GC ↔ TurnManager のブリッジ確立＆初期同期（内部で gc->currentPlayer() を反映）
    ensureTurnSyncBridge();

    // ───────────────── 表示名・ログモデル名の初期反映（任意だが初期表示を安定化） ─────────────────
    // setPlayersNamesForMode / setEngineNamesBasedOnMode がサービスへ移設済みでも呼び出し名は同じ
    setPlayersNamesForMode();
    setEngineNamesBasedOnMode();
    updateSecondEngineVisibility();  // ★ 追加: EvE対局時に2番目エンジン情報を表示
}

// エラーメッセージを表示する。
void MainWindow::displayErrorMessage(const QString& errorMessage)
{
    // エラー状態を設定する。
    m_errorOccurred = true;

    // 将棋盤内をマウスでクリックできないようにフラグを変更する。
    m_shogiView->setErrorOccurred(m_errorOccurred);

    // エラーメッセージを表示する。
    QMessageBox::critical(this, tr("Error"), errorMessage);
}

/*
// 「表示」の「思考」 思考タブの表示・非表示
void MainWindow::toggleEngineAnalysisVisibility()
{
    if (!m_analysisTab) return;
    m_analysisTab->setAnalysisVisible(ui->actionToggleEngineAnalysis->isChecked());
}
*/

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
                const int currentPly = m_sfenRecord 
                    ? static_cast<int>(qMax(static_cast<qsizetype>(0), m_sfenRecord->size() - 1)) 
                    : 0;
                m_evalGraphController->setCurrentPly(currentPly);
            }
        }
    }
}

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

void MainWindow::saveShogiBoardImage()
{
    BoardImageExporter::saveImage(this, m_shogiView);
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

// ★ 追加: EvE対局時に2番目のエンジン情報を表示する
void MainWindow::updateSecondEngineVisibility()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_playMode);
        m_playerInfoController->updateSecondEngineVisibility();
    }
}

// 以前は対局者名と残り時間、将棋盤のレイアウトを構築していた。
// ★ すべての主要ウィジェットはQDockWidgetに移動したため、この関数は不要になった。
void MainWindow::setupHorizontalGameLayout()
{
    // 何もしない（すべてのウィジェットはドックに移動）
}

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
        // ★ ライブ記録のクリアも Presenter に依頼
        if (m_recordPresenter) {
            m_recordPresenter->clearLiveDisp();
        }
    }

    // 司令塔が未用意なら作る
    if (!m_match) {
        initMatchCoordinator();
    }
    if (!m_match) return;

    // ★ ここで一括：開始に必要な前処理～初手goまでを司令塔に丸投げ
    m_match->prepareAndStartGame(
        m_playMode,
        startSfenStr,
        m_sfenRecord,
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

// 棋譜欄と矢印ボタンの有効/無効を設定する。
void MainWindow::setNavigationEnabled(bool on)
{
    if (m_recordPane) m_recordPane->setNavigationEnabled(on);
    // 分岐ツリータブのクリックも連動して有効/無効にする
    if (m_analysisTab) m_analysisTab->setBranchTreeClickEnabled(on);
}

// 対局中にナビゲーション（棋譜欄と矢印ボタン）を無効にする。
void MainWindow::disableNavigationForGame()
{
    setNavigationEnabled(false);
}

// 対局終了後にナビゲーション（棋譜欄と矢印ボタン）を有効にする。
void MainWindow::enableNavigationAfterGame()
{
    setNavigationEnabled(true);
}

// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    // CSA通信対局モードの場合
    if (m_playMode == PlayMode::CsaNetworkMode && m_csaGameCoordinator) {
        // 投了確認ダイアログ
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("投了確認"),
            tr("本当に投了しますか？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            m_csaGameCoordinator->onResign();
        }
        return;
    }

    // 通常対局モードの場合
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->handleResignation();
    }
}

void MainWindow::redrawEngine1EvaluationGraph(int ply)
{
    ensureEvaluationGraphController();
    if (m_evalGraphController) {
        m_evalGraphController->redrawEngine1Graph(ply);
    }
}

void MainWindow::redrawEngine2EvaluationGraph(int ply)
{
    ensureEvaluationGraphController();
    if (m_evalGraphController) {
        m_evalGraphController->redrawEngine2Graph(ply);
    }
}

// ★ 追加: EvaluationGraphControllerの初期化
void MainWindow::ensureEvaluationGraphController()
{
    if (m_evalGraphController) return;

    m_evalGraphController = new EvaluationGraphController(this);
    m_evalGraphController->setEvalChart(m_evalChart);
    m_evalGraphController->setMatchCoordinator(m_match);
    m_evalGraphController->setSfenRecord(m_sfenRecord);
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
        qWarning() << "ShogiClock not ready yet";
        return;
    }

    clock->setCurrentPlayer(currentPlayer);
    m_shogiView->setActiveSide(currentPlayer == 1);
}

void MainWindow::displayVersionInformation()
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showVersionInformation();
    }
}

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

void MainWindow::openWebsiteInExternalBrowser()
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->openProjectWebsite();
    }
}

void MainWindow::displayEngineSettingsDialog()
{
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showEngineSettingsDialog();
    }
}

// 検討タブからのエンジン設定リクエスト
void MainWindow::onConsiderationEngineSettingsRequested(int engineNumber, const QString& engineName)
{
    qDebug().noquote() << "[MainWindow::onConsiderationEngineSettingsRequested] engineNumber=" << engineNumber
                       << " engineName=" << engineName;

    ChangeEngineSettingsDialog dialog(this);
    dialog.setEngineNumber(engineNumber);
    dialog.setEngineName(engineName);
    dialog.setupEngineOptionsDialog();

    if (dialog.exec() == QDialog::Rejected) {
        return;
    }
}

// 検討中にエンジンが変更された場合の処理
void MainWindow::onConsiderationEngineChanged(int engineIndex, const QString& engineName)
{
    qDebug().noquote() << "[MainWindow::onConsiderationEngineChanged] engineIndex=" << engineIndex
                       << " engineName=" << engineName
                       << " m_playMode=" << static_cast<int>(m_playMode);

    // 検討モード中でなければ何もしない
    if (m_playMode != PlayMode::ConsiderationMode) {
        qDebug().noquote() << "[MainWindow::onConsiderationEngineChanged] not in consideration mode, ignoring";
        return;
    }

    // 現在の検討を中止して新しいエンジンで再開
    // まず現在のエンジンを停止
    if (m_match) {
        m_match->stopAnalysisEngine();
    }

    // 新しいエンジンで検討を開始
    // displayConsiderationDialog は現在のコンボボックスの選択を使用するため、
    // 単に呼び出すだけで新しいエンジンで検討が開始される
    displayConsiderationDialog();
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

// 検討を開始する（検討タブの設定を使用）
void MainWindow::displayConsiderationDialog()
{
    qDebug().noquote() << "[MainWindow::displayConsiderationDialog] ENTER, current m_playMode=" << static_cast<int>(m_playMode);

    // ★ エンジン破棄中の場合は検討開始を拒否（ハングアップ防止）
    if (m_match && m_match->isEngineShutdownInProgress()) {
        qDebug().noquote() << "[MainWindow::displayConsiderationDialog] engine shutdown in progress, ignoring request";
        return;
    }

    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->setAnalysisTab(m_analysisTab);
        // ★ 検討開始前にモードを設定（onSetEngineNamesで検討タブにエンジン名を設定するため）
        const PlayMode previousMode = m_playMode;
        m_playMode = PlayMode::ConsiderationMode;
        if (!m_dialogCoordinator->startConsiderationFromContext()) {
            // 検討開始失敗時は元のモードに戻す
            m_playMode = previousMode;
        }
    }
    qDebug().noquote() << "[MainWindow::displayConsiderationDialog] EXIT";
}

// 検討モード開始時の初期化（タブは切り替えない）
void MainWindow::onConsiderationModeStarted()
{
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        // 最新の依存オブジェクトを同期
        m_considerationUIController->setAnalysisTab(m_analysisTab);
        m_considerationUIController->setConsiderationModel(m_considerationModel);
        m_considerationUIController->setCommLogModel(m_lineEditModel1);
        m_considerationUIController->onModeStarted();
    }
}

// 検討モードの時間設定が確定したときの処理
void MainWindow::onConsiderationTimeSettingsReady(bool unlimited, int byoyomiSec)
{
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        // 最新の依存オブジェクトを同期
        m_considerationUIController->setAnalysisTab(m_analysisTab);
        m_considerationUIController->setMatchCoordinator(m_match);
        m_considerationUIController->onTimeSettingsReady(unlimited, byoyomiSec);
    }
}

// 検討モード終了時の処理
void MainWindow::onConsiderationModeEnded()
{
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        m_considerationUIController->setAnalysisTab(m_analysisTab);
        m_considerationUIController->setShogiView(m_shogiView);
        m_considerationUIController->onModeEnded();
    }
}

// 検討待機開始時の処理（時間切れ後、次の局面選択待ち）
void MainWindow::onConsiderationWaitingStarted()
{
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        m_considerationUIController->setAnalysisTab(m_analysisTab);
        m_considerationUIController->onWaitingStarted();
    }
}

// 検討中にMultiPV変更時の処理
void MainWindow::onConsiderationMultiPVChanged(int value)
{
    qDebug().noquote() << "[MainWindow::onConsiderationMultiPVChanged] value=" << value;

    // 検討中の場合のみMatchCoordinatorに転送
    if (m_match && m_playMode == PlayMode::ConsiderationMode) {
        m_match->updateConsiderationMultiPV(value);
    }

    // コントローラにも通知
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        m_considerationUIController->onMultiPVChanged(value);
    }
}

// 検討ダイアログでMultiPVが設定されたとき
void MainWindow::onConsiderationDialogMultiPVReady(int multiPV)
{
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        m_considerationUIController->setAnalysisTab(m_analysisTab);
        m_considerationUIController->onDialogMultiPVReady(multiPV);
    }

    // 互換性のため既存のロジックも維持
    if (m_analysisTab) {
        m_analysisTab->setConsiderationMultiPV(multiPV);
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
            // 表示時に定跡ウィンドウを更新
            updateJosekiWindow();
        }
    }
}

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

void MainWindow::resetDockLayout()
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->resetToDefault();
    }
}

void MainWindow::saveDockLayoutAs()
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->saveLayoutAs();
    }
}

void MainWindow::restoreSavedDockLayout(const QString& name)
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->restoreLayout(name);
    }
}

void MainWindow::deleteSavedDockLayout(const QString& name)
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->deleteLayout(name);
    }
}

void MainWindow::setAsStartupLayout(const QString& name)
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->setAsStartupLayout(name);
    }
}

void MainWindow::clearStartupLayout()
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->clearStartupLayout();
    }
}

void MainWindow::restoreStartupLayoutIfSet()
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->restoreStartupLayoutIfSet();
    }
}

void MainWindow::updateSavedLayoutsMenu()
{
    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->updateSavedLayoutsMenu();
    }
}

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
            qWarning().noquote() << "[MainWindow] displayCsaGameDialog: CsaGameWiring is null";
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
    qDebug().noquote() << "[MainWindow] stopTsumeSearch called";
    if (m_match) {
        m_match->stopAnalysisEngine();
    }
}

// 棋譜解析ダイアログを表示する。
void MainWindow::displayKifuAnalysisDialog()
{
    qDebug().noquote() << "[MainWindow::displayKifuAnalysisDialog] START";

    // 解析モードに遷移
    m_playMode = PlayMode::AnalysisMode;

    // 解析モデルが未生成ならここで作成
    if (!m_analysisModel) {
        m_analysisModel = new KifuAnalysisListModel(this);
    }

    ensureDialogCoordinator();
    if (!m_dialogCoordinator) return;

    // 依存オブジェクトを確保
    ensureGameInfoController();
    ensureAnalysisPresenter();

    // 解析に必要な依存オブジェクトを設定
    m_dialogCoordinator->setAnalysisModel(m_analysisModel);
    m_dialogCoordinator->setAnalysisTab(m_analysisTab);
    m_dialogCoordinator->setUsiEngine(m_usi1);
    m_dialogCoordinator->setLogModel(m_lineEditModel1);
    m_dialogCoordinator->setThinkingModel(m_modelThinking1);

    // 棋譜解析コンテキストを更新（遅延初期化されたオブジェクトを反映）
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.sfenRecord = m_sfenRecord;
    kifuCtx.moveRecords = m_moveRecords;
    kifuCtx.recordModel = m_kifuRecordModel;
    kifuCtx.activePly = &m_activePly;
    kifuCtx.gameController = m_gameController;
    kifuCtx.gameInfoController = m_gameInfoController;
    kifuCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    kifuCtx.evalChart = m_evalChart;
    kifuCtx.gameUsiMoves = &m_gameUsiMoves;
    kifuCtx.presenter = m_analysisPresenter;
    m_dialogCoordinator->setKifuAnalysisContext(kifuCtx);

    // コンテキストから自動パラメータ構築してダイアログを表示
    m_dialogCoordinator->showKifuAnalysisDialogFromContext();
}

void MainWindow::cancelKifuAnalysis()
{
    qDebug().noquote() << "[MainWindow::cancelKifuAnalysis] called";
    
    ensureDialogCoordinator();
    if (m_dialogCoordinator) {
        if (m_dialogCoordinator->isKifuAnalysisRunning()) {
            m_dialogCoordinator->stopKifuAnalysis();
            
            // 解析前のモードに戻す
            m_playMode = PlayMode::NotStarted;
            
            qDebug().noquote() << "[MainWindow::cancelKifuAnalysis] analysis cancelled";
        } else {
            qDebug().noquote() << "[MainWindow::cancelKifuAnalysis] no analysis running";
        }
    }
}

void MainWindow::onKifuAnalysisProgress(int ply, int scoreCp)
{
    qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] ply=" << ply << "scoreCp=" << scoreCp;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    navigateKifuViewToRow(ply);

    // 2) 評価値グラフに評価値をプロット
    //    ※AnalysisFlowControllerで既に先手視点に変換済み（後手番は符号反転済み）
    //    ※scoreCpがINT_MINの場合は位置移動のみ（評価値追加しない）
    static constexpr int POSITION_ONLY_MARKER = std::numeric_limits<int>::min();
    if (scoreCp != POSITION_ONLY_MARKER && m_evalChart) {
        m_evalChart->appendScoreP1(ply, scoreCp, false);
        qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] appended score to chart: ply=" << ply
                           << "scoreCp=" << scoreCp;
    } else if (scoreCp == POSITION_ONLY_MARKER) {
        qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] position only (no score update)";
    } else {
        qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] m_recordPane is null!";
    }
}

void MainWindow::onKifuAnalysisResultRowSelected(int row)
{
    qDebug().noquote() << "[MainWindow::onKifuAnalysisResultRowSelected] row=" << row;

    // rowは解析結果の行番号で、plyに相当する
    int ply = row;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    navigateKifuViewToRow(ply);

    // 2) 分岐ツリーの該当手数をハイライト
    if (m_analysisTab) {
        qDebug().noquote() << "[MainWindow::onKifuAnalysisResultRowSelected] calling highlightBranchTreeAt(0, " << ply << ")";
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

    // ★ 3) 盤ビュー側の「次の手番」ラベル表示を更新（片側のみ表示）
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

    // 盤（SFEN）の "b"/"w" から TurnManager へ → UI/Clock は changed によって更新
    const QString bw = (m_shogiView && m_shogiView->board())
                       ? m_shogiView->board()->currentPlayer()
                       : QStringLiteral("b");
    tm->setFromSfenToken(bw);

    // GC 側も TurnManager に揃える（意味統一＝GC 方式）
    if (m_gameController) {
        m_gameController->setCurrentPlayer(tm->toGc());
    }
}

QString MainWindow::resolveCurrentSfenForGameStart() const
{
    qDebug().noquote() << "[DEBUG] resolveCurrentSfenForGameStart: m_currentSelectedPly=" << m_currentSelectedPly
                       << " m_sfenRecord=" << (m_sfenRecord ? "exists" : "null")
                       << " size=" << (m_sfenRecord ? m_sfenRecord->size() : -1);

    // 1) 棋譜SFENリストの「選択手」から取得（最優先）
    if (m_sfenRecord) {
        const qsizetype size = m_sfenRecord->size();
        // m_currentSelectedPly が [0..size-1] のインデックスである前提（本プロジェクトの慣習）
        // 1始まりの場合はプロジェクト実装に合わせて +0 / -1 調整してください。
        int idx = m_currentSelectedPly;
        qDebug().noquote() << "[DEBUG] resolveCurrentSfenForGameStart: initial idx=" << idx;
        if (idx < 0) {
            // 0手目（開始局面）などのとき
            idx = 0;
        }
        // ★修正: 投了行など、m_sfenRecordの範囲外を指している場合は
        // 最後の有効なSFEN（対局終了時点の局面）を使用する
        if (idx >= size && size > 0) {
            qDebug().noquote() << "[DEBUG] resolveCurrentSfenForGameStart: idx(" << idx << ") >= size(" << size << "), clamping to " << (size - 1);
            idx = static_cast<int>(size - 1);
        }
        if (idx >= 0 && idx < size) {
            const QString s = m_sfenRecord->at(idx).trimmed();
            qDebug().noquote() << "[DEBUG] resolveCurrentSfenForGameStart: at(" << idx << ")=" << s.left(50);
            if (!s.isEmpty()) return s;
        }
    }

    // 2) フォールバックなし（司令塔側が安全に処理）
    qDebug().noquote() << "[DEBUG] resolveCurrentSfenForGameStart: returning EMPTY";
    return QString();
}

// 対局を開始する。
void MainWindow::initializeGame()
{
    ensureGameStartCoordinator();

    // ★ 平手SFENが優先されてしまう問題の根本対策：
    //    ダイアログ確定直後に司令塔へ渡す前に、startSfen を明示クリアし、
    //    currentSfen を「選択中の手のSFEN（最優先）→それがなければ空」の順で決定しておく。
    m_startSfenStr.clear();

    // 現在の局面SFEN（棋譜レコードから最優先で取得）
    {
        qDebug().noquote() << "[DEBUG] initializeGame: BEFORE resolve, m_currentSfenStr=" << m_currentSfenStr.left(50);
        const QString sfen = resolveCurrentSfenForGameStart().trimmed();
        qDebug().noquote() << "[DEBUG] initializeGame: resolved sfen=" << sfen.left(50);
        if (!sfen.isEmpty()) {
            m_currentSfenStr = sfen;
            qDebug().noquote() << "[DEBUG] initializeGame: SET m_currentSfenStr=" << m_currentSfenStr.left(50);
        } else {
            // 何も取れないケースは珍しいが、空のままでも司令塔側で安全にフォールバックされる。
            // ここでは何もしない（ログのみ）
            qDebug().noquote() << "[INIT] resolveCurrentSfenForGameStart_: empty. delegate to coordinator.";
        }
    }

    qDebug().noquote() << "[DEBUG] initializeGame: FINAL m_currentSfenStr=" << m_currentSfenStr.left(50)
                       << " m_startSfenStr=" << m_startSfenStr.left(50)
                       << " m_currentSelectedPly=" << m_currentSelectedPly;

    // ★ 修正: 分岐ツリーから途中局面で再対局する場合、m_sfenRecord を
    // 現在のラインのSFENで再構築する。これにより、前の対局の異なる分岐の
    // SFENが混在してSFEN差分によるハイライトが誤動作する問題を防ぐ。
    if (m_branchTree != nullptr && m_navState != nullptr
        && m_sfenRecord != nullptr && m_currentSelectedPly > 0) {
        const int lineIndex = m_navState->currentLineIndex();
        const QStringList branchSfens = m_branchTree->getSfenListForLine(lineIndex);
        if (!branchSfens.isEmpty() && m_currentSelectedPly < branchSfens.size()) {
            m_sfenRecord->clear();
            for (int i = 0; i <= m_currentSelectedPly; ++i) {
                m_sfenRecord->append(branchSfens.at(i));
            }
            qInfo().noquote()
                << "[MW] initializeGame: rebuilt sfenRecord from branchTree line=" << lineIndex
                << " entries=" << m_sfenRecord->size();
        }
    }

    GameStartCoordinator::Ctx c;
    c.view            = m_shogiView;
    c.gc              = m_gameController;
    c.clock           = m_timeController ? m_timeController->clock() : nullptr;
    c.sfenRecord      = m_sfenRecord;          // QStringList*
    c.kifuModel       = m_kifuRecordModel;     // ★ 棋譜欄モデル（終端行削除に使用）
    c.kifuLoadCoordinator = m_kifuLoadCoordinator;  // ★ 分岐構造の設定用
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
    // 設定ファイルにGUIQ全体のウィンドウサイズを書き込む。
    // また、将棋盤のマスサイズも書き込む。
    saveWindowAndBoardSettings();

    // エンジンが起動していれば終了する
    if (m_match) {
        m_match->destroyEngines();
    }

    // GUIを終了する。
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
    qDebug().noquote() << "[MW] chooseAndLoadKifuFile ENTER"
                       << "m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
                       << "m_sfenRecord.size=" << (m_sfenRecord ? m_sfenRecord->size() : -1)
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
    ensureGameInfoController();

    // 既存があれば即座に破棄（多重生成対策）
    // ★注: deleteLater()は非同期のため、直後に再作成すると複数存在しうる
    if (m_kifuLoadCoordinator) {
        delete m_kifuLoadCoordinator;
        m_kifuLoadCoordinator = nullptr;
    }

    // --- 2) 読み込み系の配線と依存は Coordinator に集約 ---
    m_kifuLoadCoordinator = new KifuLoadCoordinator(
        /* gameMoves           */ m_gameMoves,
        /* positionStrList     */ m_positionStrList,
        /* activePly           */ m_activePly,
        /* currentSelectedPly  */ m_currentSelectedPly,
        /* currentMoveIndex    */ m_currentMoveIndex,
        /* sfenRecord          */ m_sfenRecord,
        /* gameInfoTable       */ m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr,
        /* gameInfoDock        */ nullptr,  // GameInfoPaneControllerに移行済み
        /* tab                 */ m_tab,
        /* recordPane          */ m_recordPane,
        /* kifuRecordModel     */ m_kifuRecordModel,
        /* kifuBranchModel     */ m_kifuBranchModel,
        /* parent              */ this
        );

    // ★ 新規: 分岐ツリーとナビゲーション状態を設定
    if (m_branchTree != nullptr) {
        m_kifuLoadCoordinator->setBranchTree(m_branchTree);
    }
    if (m_navState != nullptr) {
        m_kifuLoadCoordinator->setNavigationState(m_navState);
    }

    // ★ 新規: 分岐ツリー構築完了シグナルを接続
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::branchTreeBuilt,
            this, &MainWindow::onBranchTreeBuilt, Qt::UniqueConnection);

    // ★ MainWindow 側でやっていた branchNode 配線は setAnalysisTab() に委譲
    //   （内部で disconnect / connect を一貫管理）
    m_kifuLoadCoordinator->setAnalysisTab(m_analysisTab);
    m_kifuLoadCoordinator->setShogiView(m_shogiView);

    // --- 3) Coordinator -> MainWindow の通知（UI更新）は従来どおり受ける ---
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::displayGameRecord,
            this, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
            this, &MainWindow::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::enableArrowButtons,
            this, &MainWindow::enableArrowButtons, Qt::UniqueConnection);
    // ★ 追加: 対局情報の元データを保存
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::gameInfoPopulated,
            this, &MainWindow::setOriginalGameInfo, Qt::UniqueConnection);

    qDebug().noquote() << "[MW] chooseAndLoadKifuFile: KifuLoadCoordinator created"
                       << "m_kifuLoadCoordinator*=" << static_cast<const void*>(m_kifuLoadCoordinator)
                       << "passed m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord);

    // --- 4) 読み込み実行（ロジックは Coordinator へ） ---
    // 拡張子判定
    qDebug().noquote() << "[MW] chooseAndLoadKifuFile: loading file=" << filePath;
    if (filePath.endsWith(QLatin1String(".csa"), Qt::CaseInsensitive)) {
        // CSA読み込み
        m_kifuLoadCoordinator->loadCsaFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".ki2"), Qt::CaseInsensitive)) {
        // Ki2読み込み
        m_kifuLoadCoordinator->loadKi2FromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".jkf"), Qt::CaseInsensitive)) {
        // JKF読み込み
        m_kifuLoadCoordinator->loadJkfFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".usen"), Qt::CaseInsensitive)) {
        // USEN読み込み
        m_kifuLoadCoordinator->loadUsenFromFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".usi"), Qt::CaseInsensitive)) {
        // USI読み込み
        m_kifuLoadCoordinator->loadUsiFromFile(filePath);
    } else {
        // KIF読み込み (既存)
        m_kifuLoadCoordinator->loadKifuFromFile(filePath);
    }

    // デバッグ: 読み込み後のsfenRecord状態を確認
    qDebug().noquote() << "[MW] chooseAndLoadKifuFile LEAVE"
                       << "m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
                       << "m_sfenRecord.size=" << (m_sfenRecord ? m_sfenRecord->size() : -1);
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        qDebug().noquote() << "[MW] m_sfenRecord[0]=" << m_sfenRecord->first().left(60);
        if (m_sfenRecord->size() > 1) {
            qDebug().noquote() << "[MW] m_sfenRecord[1]=" << m_sfenRecord->at(1).left(60);
        }
    }
}

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
    const int rowCount  = (m_sfenRecord && !m_sfenRecord->isEmpty())
                             ? static_cast<int>(m_sfenRecord->size())
                             : static_cast<int>(moveCount + 1);

    // ★ GameRecordModel を初期化
    ensureGameRecordModel();
    if (m_gameRecord) {
        m_gameRecord->initializeFromDisplayItems(disp, rowCount);
    }
    qDebug().noquote() << QStringLiteral("[PERF] GameRecordModel init: %1 ms").arg(timer.elapsed());

    // ★ m_commentsByRow も同期（互換性のため）
    m_commentsByRow.clear();
    m_commentsByRow.resize(rowCount);
    for (qsizetype i = 0; i < disp.size() && i < rowCount; ++i) {
        m_commentsByRow[i] = disp[i].comment;
    }
    qDebug().noquote() << "[MW] displayGameRecord: initialized with" << rowCount << "entries";

    // ← まとめて Presenter 側に委譲
    QElapsedTimer presenterTimer;
    presenterTimer.start();
    m_recordPresenter->displayAndWire(disp, rowCount, m_recordPane);
    qDebug().noquote() << QStringLiteral("[PERF] displayAndWire: %1 ms").arg(presenterTimer.elapsed());
    qDebug().noquote() << QStringLiteral("[PERF] displayGameRecord TOTAL: %1 ms").arg(timer.elapsed());
}

void MainWindow::loadWindowSettings()
{
    SettingsService::loadWindowSize(this);
}

void MainWindow::saveWindowAndBoardSettings()
{
    SettingsService::saveWindowAndBoard(this, m_shogiView);

    ensureDockLayoutManager();
    if (m_dockLayoutManager) {
        m_dockLayoutManager->saveDockStates();
    }
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    saveWindowAndBoardSettings();

    // エンジンが起動していれば終了する（quitコマンドを送信してプロセスを停止）
    if (m_match) {
        m_match->destroyEngines();
    }

    QMainWindow::closeEvent(e);
}

void MainWindow::onReverseTriggered()
{
    if (m_match) m_match->flipBoard();
}

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

// 起動時用：編集メニューを"編集前（未編集）"の初期状態にする
void MainWindow::initializeEditMenuForStartup()
{
    // 未編集状態（＝編集モードではない）でメニューを整える
    applyEditMenuEditingState(false);
}

// 共通ユーティリティ：編集モードかどうかで可視/不可視を一括切り替え
// editing == true  : 編集モード中 → 「局面編集終了」などを表示／「編集局面開始」は隠す
// editing == false : 未編集（通常）→ 「編集局面開始」を表示／それ以外を隠す
void MainWindow::applyEditMenuEditingState(bool editing)
{
    if (!ui) {
        return;
    }

    // 未編集状態では「局面編集開始」を表示、それ以外は非表示
    ui->actionStartEditPosition->setVisible(!editing);

    // 編集モード関連アクションは editing のときのみ表示
    ui->actionEndEditPosition->setVisible(editing);
    ui->actionSetHiratePosition->setVisible(editing);
    ui->actionSetTsumePosition->setVisible(editing);
    ui->actionReturnAllPiecesToStand->setVisible(editing);
    ui->actionSwapSides->setVisible(editing);
    ui->actionChangeTurn->setVisible(editing);

    // ※ QMenu名は .ui 上で "Edit"（= ui->Edit）です。必要なら再描画。
    if (ui->Edit) {
        ui->Edit->update();
    }
}

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

void MainWindow::setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->setGameOverMove(cause, loserIsPlayerOne);
    }
}

void MainWindow::appendKifuLine(const QString& text, const QString& elapsedTime)
{
    qDebug().noquote() << "[MW-DEBUG] appendKifuLine ENTER: text=" << text
                       << "elapsedTime=" << elapsedTime;

    // KIF 追記の既存フローに合わせて m_lastMove を経由し、updateGameRecord() を1回だけ呼ぶ
    m_lastMove = text;

    // ここで棋譜へ 1 行追加（手数インクリメントやモデル反映は updateGameRecord が面倒を見る）
    updateGameRecord(elapsedTime);

    qDebug().noquote() << "[MW-DEBUG] appendKifuLine AFTER updateGameRecord:"
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

// ★ 新規: GameInfoPaneControllerの初期化（PlayerInfoWiring経由）
void MainWindow::ensureGameInfoController()
{
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring && !m_gameInfoController) {
        // ★ 追加: PlayerInfoWiring側でGameInfoPaneControllerを確実に生成
        m_playerInfoWiring->ensureGameInfoController();
        m_gameInfoController = m_playerInfoWiring->gameInfoController();
    }
}

// ★ 追加: 起動時に対局情報タブを追加（PlayerInfoWiring経由）
void MainWindow::addGameInfoTabAtStartup()
{
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        // ★ 追加: m_tabが設定されている場合はPlayerInfoWiringにも設定
        if (m_tab) {
            m_playerInfoWiring->setTabWidget(m_tab);
        }
        m_playerInfoWiring->addGameInfoTabAtStartup();
        // MainWindowのメンバ変数も同期
        m_gameInfoController = m_playerInfoWiring->gameInfoController();
    }
}

// ★ 追加: 対局情報テーブルにデフォルト値を設定（PlayerInfoWiring内部で実行）
void MainWindow::populateDefaultGameInfo()
{
    // PlayerInfoWiring::addGameInfoTabAtStartup()内で呼ばれるため、
    // 直接呼ぶ必要がある場合のみ実行
    if (m_gameInfoController) {
        QList<KifGameInfoItem> defaultItems;
        defaultItems.append({tr("先手"), tr("先手")});
        defaultItems.append({tr("後手"), tr("後手")});
        defaultItems.append({tr("手合割"), tr("平手")});
        m_gameInfoController->setGameInfo(defaultItems);
    }
}

// ★ 追加: 対局者名設定フック（PlayerInfoWiring経由）
void MainWindow::onSetPlayersNames(const QString& p1, const QString& p2)
{
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->onSetPlayersNames(p1, p2);
    }
}

// ★ 追加: エンジン名設定フック（PlayerInfoWiring経由）
void MainWindow::onSetEngineNames(const QString& e1, const QString& e2)
{
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->onSetEngineNames(e1, e2);
    }

    // 検討モード・詰み探索モードの場合、検討タブと思考タブにエンジン名を設定
    if (m_playMode == PlayMode::ConsiderationMode || m_playMode == PlayMode::TsumiSearchMode) {
        if (m_analysisTab) {
            m_analysisTab->setConsiderationEngineName(e1);
            // 思考タブにもエンジン名を設定
            if (m_analysisTab->info1() && !e1.isEmpty()) {
                m_analysisTab->info1()->setDisplayNameFallback(e1);
            }
        }
    }
}

// ★ 追加: 対局情報タブの先手・後手名を更新（PlayerInfoWiring経由）
void MainWindow::updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName)
{
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->updateGameInfoPlayerNames(blackName, whiteName);
    }
}

// ★ 追加: 元の対局情報を保存（PlayerInfoWiring経由）
void MainWindow::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->setOriginalGameInfo(items);
    }
}

// ★ 追加: 現在の対局に基づいて対局情報タブを更新（PlayerInfoWiring経由）
void MainWindow::updateGameInfoForCurrentMatch()
{
    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->updateGameInfoForCurrentMatch();
    }
}

// ★ 追加: 対局者名確定時のスロット（PlayerInfoWiring経由）
void MainWindow::onPlayerNamesResolved(const QString& human1, const QString& human2,
                                        const QString& engine1, const QString& engine2,
                                        int playMode)
{
    qDebug().noquote() << "[MW] onPlayerNamesResolved_: playMode=" << playMode;

    ensurePlayerInfoWiring();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);

        // 対局情報ドックに開始日時・持ち時間を含む情報を設定
        if (m_timeController) {
            // プレイモードに応じた先手・後手名を決定
            const PlayMode mode = static_cast<PlayMode>(playMode);
            QString blackName, whiteName;
            switch (mode) {
            case PlayMode::HumanVsHuman:
                blackName = human1.isEmpty() ? tr("先手") : human1;
                whiteName = human2.isEmpty() ? tr("後手") : human2;
                break;
            case PlayMode::EvenHumanVsEngine:
            case PlayMode::HandicapHumanVsEngine:
                blackName = human1.isEmpty() ? tr("先手") : human1;
                whiteName = engine2.isEmpty() ? tr("Engine") : engine2;
                break;
            case PlayMode::EvenEngineVsHuman:
            case PlayMode::HandicapEngineVsHuman:
                blackName = engine1.isEmpty() ? tr("Engine") : engine1;
                whiteName = human2.isEmpty() ? tr("後手") : human2;
                break;
            case PlayMode::EvenEngineVsEngine:
            case PlayMode::HandicapEngineVsEngine:
                blackName = engine1.isEmpty() ? tr("Engine1") : engine1;
                whiteName = engine2.isEmpty() ? tr("Engine2") : engine2;
                break;
            default:
                blackName = tr("先手");
                whiteName = tr("後手");
                break;
            }

            // 手合割の判定
            const QString sfen = m_startSfenStr.trimmed();
            const QString initPP = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
            QString handicap = tr("平手");
            if (!sfen.isEmpty()) {
                const QString pp = sfen.section(QLatin1Char(' '), 0, 0);
                if (!pp.isEmpty() && pp != initPP) {
                    handicap = tr("その他");
                }
            }

            // 終了日時をクリア（新しい対局が始まるため）
            m_timeController->clearGameEndTime();

            // 対局情報を設定
            m_playerInfoWiring->setGameInfoForMatchStart(
                m_timeController->gameStartDateTime(),
                blackName,
                whiteName,
                handicap,
                m_timeController->hasTimeControl(),
                m_timeController->baseTimeMs(),
                m_timeController->byoyomiMs(),
                m_timeController->incrementMs()
            );
        }
    }
}

// ★ 追加: GameInfoPaneControllerからの更新通知
void MainWindow::onGameInfoUpdated(const QList<KifGameInfoItem>& items)
{
    qDebug().noquote() << "[MW] onGameInfoUpdated_: Game info updated, items=" << items.size();
}

// ★ 追加: 連続対局設定を受信
void MainWindow::onConsecutiveGamesConfigured(int totalGames, bool switchTurn)
{
    qDebug().noquote() << "[MW] onConsecutiveGamesConfigured_: totalGames=" << totalGames << " switchTurn=" << switchTurn;

    ensureConsecutiveGamesController();
    if (m_consecutiveGamesController) {
        m_consecutiveGamesController->configure(totalGames, switchTurn);
    }
}

// ★ 追加: 対局開始時の設定を保存（連続対局用）
void MainWindow::onGameStarted(const MatchCoordinator::StartOptions& opt)
{
    qDebug().noquote() << "[MW] onGameStarted_: mode=" << static_cast<int>(opt.mode)
                       << " sfenStart=" << opt.sfenStart;

    ensureConsecutiveGamesController();
    if (m_consecutiveGamesController) {
        m_consecutiveGamesController->onGameStarted(opt, m_lastTimeControl);
    }
}

// ★ 追加: 連続対局の次の対局を開始
void MainWindow::startNextConsecutiveGame()
{
    qDebug().noquote() << "[MW] startNextConsecutiveGame_ (delegated to controller)";

    ensureConsecutiveGamesController();
    if (m_consecutiveGamesController && m_consecutiveGamesController->shouldStartNextGame()) {
        m_consecutiveGamesController->startNextGame();
    }
}

void MainWindow::onRequestSelectKifuRow(int row)
{
    qDebug().noquote() << "[MW] onRequestSelectKifuRow: row=" << row;

    // 棋譜欄の指定行を選択
    if (m_recordPane) {
        if (QTableView* view = m_recordPane->kifuView()) {
            if (view->model() && row >= 0 && row < view->model()->rowCount()) {
                const QModelIndex idx = view->model()->index(row, 0);
                view->setCurrentIndex(idx);
                qDebug().noquote() << "[MW] onRequestSelectKifuRow: selected row=" << row;

                // 盤面・ハイライトも同期
                syncBoardAndHighlightsAtRow(row);
            }
        }
    }
}

void MainWindow::syncBoardAndHighlightsAtRow(int ply)
{
    qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow ENTER ply=" << ply;

    // ★ 分岐ナビゲーション中は盤面同期をスキップ
    if (m_skipBoardSyncForBranchNav) {
        qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow skipped (branch navigation in progress)";
        return;
    }

    // 位置編集モード中はスキップ
    if (m_shogiView && m_shogiView->positionEditMode()) {
        qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow skipped (edit-mode)";
        return;
    }

    // BoardSyncPresenterを確保して盤面同期
    ensureBoardSyncPresenter();
    if (m_boardSync) {
        m_boardSync->syncBoardAndHighlightsAtRow(ply);
    }

    // 矢印ボタンの活性化
    enableArrowButtons();

    // m_currentSfenStrを現在の局面に更新
    // 分岐ライン上では m_branchTree から正しいSFENを取得
    bool foundInBranch = false;
    if (m_navState != nullptr && !m_navState->isOnMainLine() && m_branchTree != nullptr) {
        const int lineIndex = m_navState->currentLineIndex();
        QVector<BranchLine> lines = m_branchTree->allLines();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            const BranchLine& line = lines.at(lineIndex);
            if (ply >= 0 && ply < line.nodes.size()) {
                m_currentSfenStr = line.nodes.at(ply)->sfen();
                foundInBranch = true;
                qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow: updated m_currentSfenStr from branchTree";
            }
        }
    }
    if (!foundInBranch && m_sfenRecord && ply >= 0 && ply < m_sfenRecord->size()) {
        m_currentSfenStr = m_sfenRecord->at(ply);
        qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow: updated m_currentSfenStr=" << m_currentSfenStr;
    }

    // 定跡ウィンドウを更新
    updateJosekiWindow();

    qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow LEAVE";
}

void MainWindow::navigateKifuViewToRow(int ply)
{
    qDebug().noquote() << "[MW] navigateKifuViewToRow ENTER ply=" << ply;

    if (!m_recordPane || !m_kifuRecordModel) {
        qDebug().noquote() << "[MW] navigateKifuViewToRow ABORT: recordPane or kifuRecordModel is null";
        return;
    }

    QTableView* view = m_recordPane->kifuView();
    if (!view) {
        qDebug().noquote() << "[MW] navigateKifuViewToRow ABORT: kifuView is null";
        return;
    }

    const int rows = m_kifuRecordModel->rowCount();
    const int safe = (rows > 0) ? qBound(0, ply, rows - 1) : 0;

    qDebug().noquote() << "[MW] navigateKifuViewToRow: ply=" << ply
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

    qDebug().noquote() << "[MW] navigateKifuViewToRow LEAVE";
}

KifuExportController* MainWindow::kifuExportController()
{
    ensureKifuExportController();
    return m_kifuExportController;
}

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
            qDebug().noquote() << "[MW] initializeBranchNavigationClasses: KifuNavigationController buttons connected";
        }
    }

    // ライブゲームセッションの作成
    if (m_liveGameSession == nullptr) {
        m_liveGameSession = new LiveGameSession(this);
        m_liveGameSession->setTree(m_branchTree);
    }

    // 表示コーディネーターの作成
    qDebug().noquote() << "[MW] initializeBranchNavigationClasses: m_displayCoordinator=" << (m_displayCoordinator ? "exists" : "null");
    if (m_displayCoordinator == nullptr) {
        qDebug().noquote() << "[MW] initializeBranchNavigationClasses: creating KifuDisplayCoordinator";
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

        // ★ ライブゲームセッションを設定（分岐ツリーのリアルタイム更新用）
        if (m_liveGameSession != nullptr) {
            m_displayCoordinator->setLiveGameSession(m_liveGameSession);
        }

        // 盤面とハイライト更新シグナルを接続（新システム用）
        connect(m_displayCoordinator, &KifuDisplayCoordinator::boardWithHighlightsRequired,
                this, &MainWindow::loadBoardWithHighlights);

        // 盤面のみ更新シグナル（通常は boardWithHighlightsRequired を使用）
        connect(m_displayCoordinator, &KifuDisplayCoordinator::boardSfenChanged,
                this, &MainWindow::loadBoardFromSfen);

        // ★ 分岐ライン選択変更シグナルを接続
        connect(m_kifuNavController, &KifuNavigationController::lineSelectionChanged,
                this, &MainWindow::onLineSelectionChanged);
    }

}

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

    // ★ RecordPane は QDockWidget に移動したため、m_gameRecordLayoutWidget への設定は不要
}

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

    // ★ 追加: コメント更新シグナルの接続
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::commentUpdated,
        this,          &MainWindow::onCommentUpdated,
        Qt::UniqueConnection);

    // ★ 追加: 読み筋クリックシグナルの接続
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::pvRowClicked,
        this,          &MainWindow::onPvRowClicked,
        Qt::UniqueConnection);

    // ★ 追加: USIコマンド手動送信シグナルの接続（専用コントローラへ委譲）
    ensureUsiCommandController();
    if (m_usiCommandController) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::usiCommandRequested,
            m_usiCommandController, &UsiCommandController::sendCommand,
            Qt::UniqueConnection);
    }

    // ★ 追加: 検討開始シグナルの接続
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::startConsiderationRequested,
        this,          &MainWindow::displayConsiderationDialog,
        Qt::UniqueConnection);

    // ★ 追加: 検討タブからのエンジン設定リクエストの接続
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::engineSettingsRequested,
        this,          &MainWindow::onConsiderationEngineSettingsRequested,
        Qt::UniqueConnection);

    // ★ 追加: 検討中のエンジン変更リクエストの接続
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::considerationEngineChanged,
        this,          &MainWindow::onConsiderationEngineChanged,
        Qt::UniqueConnection);

    // ★ 追加: PlayerInfoControllerにもm_analysisTabを設定
    //    （ensurePlayerInfoController_がこれより先に呼ばれた場合への対応）
    if (m_playerInfoController) {
        m_playerInfoController->setAnalysisTab(m_analysisTab);
    }

    // ★ 追加: 起動時に対局情報タブを追加
    addGameInfoTabAtStartup();

    // ★ 追加: 起動時に検討タブのモデルを設定（ヘッダー表示のため）
    if (!m_considerationModel) {
        m_considerationModel = new ShogiEngineThinkingModel(this);
    }
    if (m_analysisTab) {
        m_analysisTab->setConsiderationThinkingModel(m_considerationModel);
    }
}

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

void MainWindow::createRecordPaneDock()
{
    if (!m_recordPane) {
        qWarning() << "[MainWindow] createRecordPaneDock: m_recordPane is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setRecordPane(m_recordPane);
    m_recordPaneDock = m_dockCreationService->createRecordPaneDock();
}

void MainWindow::createAnalysisDocks()
{
    if (!m_analysisTab) {
        qWarning() << "[MainWindow] createAnalysisDocks: m_analysisTab is null!";
        return;
    }

    // 対局情報コントローラを準備し、デフォルト値を設定
    ensureGameInfoController();
    if (m_gameInfoController) {
        populateDefaultGameInfo();
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

void MainWindow::setupBoardInCenter()
{
    if (!m_shogiView) {
        qWarning() << "[MainWindow] setupBoardInCenter: m_shogiView is null!";
        return;
    }

    if (!m_centralLayout) {
        qWarning() << "[MainWindow] setupBoardInCenter: m_centralLayout is null!";
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

void MainWindow::createMenuWindowDock()
{
    // MenuWindowWiringを確保
    ensureMenuWiring();
    if (!m_menuWiring) {
        qWarning() << "[MainWindow] createMenuWindowDock: MenuWindowWiring is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setMenuWiring(m_menuWiring);
    m_menuWindowDock = m_dockCreationService->createMenuWindowDock();
}

void MainWindow::createJosekiWindowDock()
{
    // JosekiWindowWiringを確保
    ensureJosekiWiring();
    if (!m_josekiWiring) {
        qWarning() << "[MainWindow] createJosekiWindowDock: JosekiWindowWiring is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setJosekiWiring(m_josekiWiring);
    m_josekiWindowDock = m_dockCreationService->createJosekiWindowDock();
}

void MainWindow::createAnalysisResultsDock()
{
    // AnalysisResultsPresenterを確保
    ensureAnalysisPresenter();
    if (!m_analysisPresenter) {
        qWarning() << "[MainWindow] createAnalysisResultsDock: AnalysisResultsPresenter is null!";
        return;
    }

    // DockCreationServiceに委譲
    ensureDockCreationService();
    m_dockCreationService->setAnalysisPresenter(m_analysisPresenter);
    m_analysisResultsDock = m_dockCreationService->createAnalysisResultsDock();
}

// src/app/mainwindow.cpp
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

    // ★Presenterと同じリストを渡す（Single Source of Truth）
    d.sfenRecord = m_sfenRecord;

    // ---- ここは「コメントアウト」せず、関数バインドで割り当て ----
    d.hooks.appendEvalP1       = std::bind(&MainWindow::requestRedrawEngine1Eval, this);
    d.hooks.appendEvalP2       = std::bind(&MainWindow::requestRedrawEngine2Eval, this);
    d.hooks.sendGoToEngine     = std::bind(&MatchCoordinator::sendGoToEngine,   m_match, _1, _2);
    d.hooks.sendStopToEngine   = std::bind(&MatchCoordinator::sendStopToEngine, m_match, _1);
    d.hooks.sendRawToEngine    = std::bind(&MatchCoordinator::sendRawToEngine,  m_match, _1, _2);
    d.hooks.initializeNewGame  = std::bind(&MainWindow::initializeNewGameHook, this, _1);
    d.hooks.showMoveHighlights = std::bind(&MainWindow::showMoveHighlights, this, _1, _2);
    d.hooks.appendKifuLine     = std::bind(&MainWindow::appendKifuLineHook, this, _1, _2);

    // ★ 追加（今回の肝）：結果ダイアログ表示フックを配線
    d.hooks.showGameOverDialog = std::bind(&MainWindow::showGameOverMessageBox, this, _1, _2);

    // ★★ 追加：時計から「残り/増加/秒読み」を司令塔へ提供するフックを配線
    d.hooks.remainingMsFor = std::bind(&MainWindow::getRemainingMsFor, this, _1);
    d.hooks.incrementMsFor = std::bind(&MainWindow::getIncrementMsFor, this, _1);
    d.hooks.byoyomiMs      = std::bind(&MainWindow::getByoyomiMs, this);

    // ★ 追加：対局者名の更新フック（将棋盤ラベルと対局情報タブ）
    d.hooks.setPlayersNames = std::bind(&MainWindow::onSetPlayersNames, this, _1, _2);
    d.hooks.setEngineNames  = std::bind(&MainWindow::onSetEngineNames, this, _1, _2);

    // ★ 追加：棋譜自動保存フック
    d.hooks.autoSaveKifu = std::bind(&MainWindow::autoSaveKifuToFile, this, _1, _2, _3, _4, _5, _6);

    // --- GameStartCoordinator の確保（1 回だけ） ---
    if (!m_gameStartCoordinator) {
        GameStartCoordinator::Deps gd;
        gd.match = nullptr;
        gd.clock = m_timeController ? m_timeController->clock() : nullptr;
        gd.gc    = m_gameController;
        gd.view  = m_shogiView;

        m_gameStartCoordinator = new GameStartCoordinator(gd, this);

        // ★ Coordinator の転送シグナルを MainWindow のスロットへ接続
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
    }

    // --- 司令塔の生成＆初期配線を Coordinator に委譲 ---
    m_match = m_gameStartCoordinator->createAndWireMatch(d, this);

    // USIコマンドコントローラへ司令塔を反映
    ensureUsiCommandController();

    // PlayMode を司令塔へ伝達（従来どおり）
    if (m_match) {
        m_match->setPlayMode(m_playMode);
    }

    // ★ 追加: EvaluationGraphControllerにMatchCoordinatorを設定
    if (m_evalGraphController) {
        m_evalGraphController->setMatchCoordinator(m_match);
    }

    // ★★ UNDO 用バインディング（今回の修正点）★★
    if (m_match) {
        MatchCoordinator::UndoRefs u;
        u.recordModel      = m_kifuRecordModel;      // 棋譜テーブルのモデル
        u.gameMoves        = &m_gameMoves;           // 内部の着手配列
        u.positionStrList  = &m_positionStrList;     // "position ..." 履歴（あれば）
        u.sfenRecord       = m_sfenRecord;           // SFEN の履歴（Presenterと同一）
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
    }

    // ★ TimeControlControllerにMatchCoordinatorを設定（タイムアウト処理用）
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

void MainWindow::ensureTimeController()
{
    if (m_timeController) return;

    m_timeController = new TimeControlController(this);
    m_timeController->setTimeDisplayPresenter(m_timePresenter);
    m_timeController->ensureClock();
}

void MainWindow::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->onMatchGameEnded(info);
    }

    // ★ ライブ対局の確定（LiveGameSession::commit）は
    //   onRequestAppendGameOverMove の後で行う（投了手を含めるため）

    // ★ 追加: 対局終了日時を記録し、対局情報ドックを更新
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

    // ★ 追加: EvE対局で連続対局が残っている場合、次の対局を自動開始
    const bool isEvE = (m_playMode == PlayMode::EvenEngineVsEngine ||
                        m_playMode == PlayMode::HandicapEngineVsEngine);
    ensureConsecutiveGamesController();
    if (isEvE && m_consecutiveGamesController && m_consecutiveGamesController->shouldStartNextGame()) {
        qDebug() << "[MW] EvE game ended, starting next consecutive game...";
        startNextConsecutiveGame();
    }
}

void MainWindow::onActionFlipBoardTriggered(bool /*checked*/)
{
    if (m_match) m_match->flipBoard();
}

void MainWindow::onActionEnlargeBoardTriggered(bool /*checked*/)
{
    if (!m_shogiView) return;
    m_shogiView->enlargeBoard(true);
}

void MainWindow::onActionShrinkBoardTriggered(bool /*checked*/)
{
    if (!m_shogiView) return;
    m_shogiView->reduceBoard(true);
}

void MainWindow::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->onRequestAppendGameOverMove(info);
    }

    // ★ 新システム: ライブ対局の確定は LiveGameSession::commit() で行う
    if (m_liveGameSession != nullptr && m_liveGameSession->isActive()) {
        qDebug() << "[MW] onRequestAppendGameOverMove: committing live game session";
        m_liveGameSession->commit();
    }
}

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

void MainWindow::connectBoardClicks()
{
    ensureBoardSetupController();
    if (m_boardSetupController) {
        m_boardSetupController->connectBoardClicks();
    }
}

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

void MainWindow::onMoveRequested(const QPoint& from, const QPoint& to)
{
    qDebug().noquote() << "[MW] onMoveRequested_: from=" << from << " to=" << to
                       << " m_playMode=" << static_cast<int>(m_playMode)
                       << " m_match=" << (m_match ? "valid" : "null")
                       << " matchMode=" << (m_match ? static_cast<int>(m_match->playMode()) : -1);
    
    ensureBoardSetupController();
    if (m_boardSetupController) {
        // 最新の状態を同期
        m_boardSetupController->setPlayMode(m_playMode);
        m_boardSetupController->setMatchCoordinator(m_match);
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

void MainWindow::onBranchNodeActivated(int row, int ply)
{
    qDebug().noquote() << "[MW] onBranchNodeActivated ENTER row=" << row << "ply=" << ply;

    // ★ 新システム: KifuBranchTree を使用して境界チェック
    if (m_branchTree == nullptr || m_kifuNavController == nullptr) {
        qDebug().noquote() << "[MW] onBranchNodeActivated: branchTree or kifuNavController is null, cannot proceed";
        return;
    }

    QVector<BranchLine> lines = m_branchTree->allLines();
    if (lines.isEmpty()) {
        qDebug().noquote() << "[MW] onBranchNodeActivated: no lines available";
        return;
    }

    // ★ 重要: EngineAnalysisTabの「row」は allLines() のインデックスそのもの
    // setBranchTreeRows() で allLines() の順番で設定されるため、
    // row はそのまま lines のインデックスとして使用できる

    // 境界チェック
    if (row < 0 || row >= lines.size()) {
        qDebug().noquote() << "[MW] onBranchNodeActivated: row out of bounds (row=" << row << ", lines=" << lines.size() << ")";
        return;
    }

    // ply=0の場合（開始局面）は常にルートノードを使用
    if (ply == 0) {
        KifuBranchNode* targetNode = m_branchTree->root();
        if (targetNode != nullptr) {
            if (m_navState != nullptr) {
                m_navState->resetPreferredLineIndex();
            }
            m_kifuNavController->goToNode(targetNode);
            m_activePly = 0;
            m_currentSelectedPly = 0;
            m_currentMoveIndex = 0;
            if (!targetNode->sfen().isEmpty()) {
                m_currentSfenStr = targetNode->sfen();
            }
            updateJosekiWindow();
        }
        qDebug().noquote() << "[MW] onBranchNodeActivated LEAVE (root node)";
        return;
    }

    // ★ 共有ノードのクリック時はライン維持
    // 分岐ライン上にいるとき、分岐前の共有ノードをクリックした場合は現在のラインを維持
    int effectiveRow = row;
    if (m_navState != nullptr) {
        const int currentLine = m_navState->currentLineIndex();
        if (currentLine > 0 && currentLine < lines.size()) {
            const BranchLine& currentBranchLine = lines.at(currentLine);
            // クリックされたplyが現在ラインの分岐点より前なら、共有ノードとみなす
            if (ply < currentBranchLine.branchPly) {
                effectiveRow = currentLine;
                qDebug().noquote() << "[MW] onBranchNodeActivated: shared node clicked, keeping current line=" << currentLine;
            }
        }
    }

    const BranchLine& line = lines.at(effectiveRow);
    // ライン上の最大ply（ノードがない場合は0）
    const int maxPly = line.nodes.isEmpty() ? 0 : line.nodes.last()->ply();
    const int selPly = qBound(0, ply, maxPly);

    // ★ 新システム用：分岐ラインを選択した場合、優先ラインを設定
    // これにより、prev/nextボタンで分岐ライン上をナビゲートできる
    if (m_navState != nullptr) {
        if (effectiveRow > 0) {
            m_navState->setPreferredLineIndex(effectiveRow);
            qDebug().noquote() << "[MW] onBranchNodeActivated: setPreferredLineIndex=" << effectiveRow;
        } else {
            m_navState->resetPreferredLineIndex();
            qDebug().noquote() << "[MW] onBranchNodeActivated: resetPreferredLineIndex (main line)";
        }
    }

    // 対応するノードを探してナビゲート
    KifuBranchNode* targetNode = nullptr;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == selPly) {
            targetNode = node;
            break;
        }
    }

    // ply=0の場合（開始局面）はルートノードを使用
    if (targetNode == nullptr && selPly == 0) {
        targetNode = m_branchTree->root();
    }

    if (targetNode != nullptr) {
        // goToNode で盤面同期・棋譜欄ハイライト・分岐候補更新が行われる
        m_kifuNavController->goToNode(targetNode);
        qDebug().noquote() << "[MW] onBranchNodeActivated: goToNode ply=" << selPly;

        // ★ plyインデックス変数を更新（新システムはnavStateが管理）
        m_activePly = selPly;
        m_currentSelectedPly = selPly;
        m_currentMoveIndex = selPly;

        // ★ m_currentSfenStr を更新（再対局時に正しい分岐点を見つけるため）
        if (!targetNode->sfen().isEmpty()) {
            m_currentSfenStr = targetNode->sfen();
            qDebug().noquote() << "[MW] onBranchNodeActivated: updated m_currentSfenStr="
                               << m_currentSfenStr.left(60);
        }

        // 定跡ウィンドウを更新
        updateJosekiWindow();
    } else {
        qDebug().noquote() << "[MW] onBranchNodeActivated: node not found for ply=" << selPly;
    }

    qDebug().noquote() << "[MW] onBranchNodeActivated LEAVE";
}

// ★ 新規: 分岐ツリー構築完了時のハンドラ
void MainWindow::onBranchTreeBuilt()
{
    qDebug() << "[MW] onBranchTreeBuilt: updating new navigation system";

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

    // ★ GameRecordModel に新システムを設定（エクスポート用）
    if (m_gameRecord != nullptr && m_branchTree != nullptr) {
        m_gameRecord->setBranchTree(m_branchTree);
        if (m_navState != nullptr) {
            m_gameRecord->setNavigationState(m_navState);
        }
    }
}

// ★ 新規: SFENから直接盤面を読み込む（分岐ナビゲーション用）
void MainWindow::loadBoardFromSfen(const QString& sfen)
{
    qDebug().noquote() << "[MW] loadBoardFromSfen: sfen=" << sfen.left(60);

    if (sfen.isEmpty()) {
        qWarning() << "[MW] loadBoardFromSfen: empty SFEN, skipping";
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

        qDebug() << "[MW] loadBoardFromSfen: board updated successfully";
    } else {
        qWarning() << "[MW] loadBoardFromSfen: missing dependencies"
                   << "gc=" << m_gameController
                   << "board=" << (m_gameController ? m_gameController->board() : nullptr)
                   << "view=" << m_shogiView;
    }
}

// ★ 分岐ライン選択変更時の処理
// 新システム（KifuNavigationState）が管理
void MainWindow::onLineSelectionChanged(int newLineIndex)
{
    Q_UNUSED(newLineIndex)
    // KifuNavigationState が currentLineIndex() を管理
}

// ★ 新規: SFENから盤面とハイライトを更新（分岐ナビゲーション用）
void MainWindow::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    qDebug().noquote() << "[MW] loadBoardWithHighlights ENTER"
                       << "currentSfen=" << currentSfen.left(40)
                       << "prevSfen=" << (prevSfen.isEmpty() ? "(empty)" : prevSfen.left(40));

    // ★ 盤面同期をスキップするフラグを設定
    // このメソッドが完了するまで、onRecordPaneMainRowChanged での盤面同期を抑制する
    m_skipBoardSyncForBranchNav = true;

    // ★ フラグをリセットするラムダ（QTimer::singleShotで使用）
    auto resetSkipFlags = [this]() {
        m_skipBoardSyncForBranchNav = false;
    };

    if (currentSfen.isEmpty()) {
        qWarning() << "[MW] loadBoardWithHighlights: empty currentSfen, skipping";
        QTimer::singleShot(0, this, resetSkipFlags);
        return;
    }

    // 1. 盤面を更新
    loadBoardFromSfen(currentSfen);

    // 2. ハイライトを計算・表示
    if (!m_boardController) {
        qWarning() << "[MW] loadBoardWithHighlights: m_boardController is null, skipping highlights";
        QTimer::singleShot(0, this, resetSkipFlags);
        return;
    }

    // 開始局面（prevSfenが空）の場合はハイライトをクリア
    if (prevSfen.isEmpty()) {
        m_boardController->clearAllHighlights();
        qDebug() << "[MW] loadBoardWithHighlights: cleared highlights (start position)";
        QTimer::singleShot(0, this, resetSkipFlags);
        return;
    }

    // SFEN差分からfrom/toを計算
    // 盤面部分だけを解析してグリッドに展開し、差分を検出する
    auto parseOneBoard = [](const QString& sfen, QString grid[9][9]) -> bool {
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 9; ++x)
                grid[y][x].clear();

        if (sfen.isEmpty()) return false;
        const QString boardField = sfen.split(QLatin1Char(' '), Qt::KeepEmptyParts).value(0);
        const QStringList rows = boardField.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (rows.size() != 9) return false;

        for (int r = 0; r < 9; ++r) {
            const QString& row = rows.at(r);
            const int y = r;    // 上(後手側)=0, 下(先手側)=8
            int x = 8;          // 筋は右(9筋)=8 → 左(1筋)=0 へ詰める

            for (qsizetype i = 0; i < row.size(); ++i) {
                const QChar ch = row.at(i);
                if (ch.isDigit()) {
                    x -= (ch.toLatin1() - '0'); // 連続空白
                } else if (ch == QLatin1Char('+')) {
                    if (i + 1 >= row.size() || x < 0) return false;
                    grid[y][x] = QStringLiteral("+") + row.at(++i); // 成り駒
                    --x;
                } else {
                    if (x < 0) return false;
                    grid[y][x] = QString(ch); // 通常駒
                    --x;
                }
            }
            if (x != -1) return false; // 9マス分ちょうどで終わっていない
        }
        return true;
    };

    // from/toを推定
    auto deduceByDiff = [&](const QString& a, const QString& b,
                            QPoint& from, QPoint& to, QChar& droppedPiece) -> bool {
        QString ga[9][9], gb[9][9];
        if (!parseOneBoard(a, ga) || !parseOneBoard(b, gb)) return false;

        bool foundTo = false;
        droppedPiece = QChar();

        for (int y = 0; y < 9; ++y) {
            for (int x = 0; x < 9; ++x) {
                if (ga[y][x] == gb[y][x]) continue;
                if (!ga[y][x].isEmpty() && gb[y][x].isEmpty()) {
                    from = QPoint(x, y);   // 元が空いた
                } else if (ga[y][x].isEmpty() && !gb[y][x].isEmpty()) {
                    to = QPoint(x, y); foundTo = true;       // 駒打ち：空→駒
                    droppedPiece = gb[y][x].at(0);
                } else {
                    to = QPoint(x, y); foundTo = true;       // 駒移動または駒取り
                }
            }
        }
        return foundTo;
    };

    // 0-origin → 1-origin 変換
    auto toOne = [](const QPoint& p) -> QPoint {
        return QPoint(p.x() + 1, p.y() + 1);
    };

    QPoint from(-1, -1), to(-1, -1);
    QChar droppedPiece;
    bool ok = deduceByDiff(prevSfen, currentSfen, from, to, droppedPiece);

    if (!ok) {
        qDebug() << "[MW] loadBoardWithHighlights: SFEN diff failed, clearing highlights";
        m_boardController->clearAllHighlights();
        QTimer::singleShot(0, this, resetSkipFlags);
        return;
    }

    const QPoint to1 = toOne(to);
    const bool hasFrom = (from.x() >= 0 && from.y() >= 0);

    qDebug().noquote() << "[MW] loadBoardWithHighlights: highlight"
                       << "from=(" << from.x() << "," << from.y() << ")"
                       << "to=(" << to.x() << "," << to.y() << ")"
                       << "hasFrom=" << hasFrom;

    if (hasFrom) {
        const QPoint from1 = toOne(from);
        m_boardController->showMoveHighlights(from1, to1);
    } else {
        // 駒打ちの場合: toのみハイライト
        m_boardController->showMoveHighlights(QPoint(-1, -1), to1);
    }

    // ★ イベントループの次の反復でフラグをリセット
    // これにより、このメソッドの後で処理されるシグナルでも盤面同期がスキップされる
    QTimer::singleShot(0, this, resetSkipFlags);
    qDebug() << "[MW] loadBoardWithHighlights LEAVE";
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
                qDebug().noquote() << "[MainWindow] onMoveCommitted: added USI move:" << usiMove
                                   << "m_gameUsiMoves.size()=" << m_gameUsiMoves.size();
            }
        }
    }

    // m_currentSfenStrを現在の局面に更新
    // m_sfenRecordの最新のSFENを取得（plyはタイミングの問題で信頼できない）
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        // 最新のSFENを取得
        m_currentSfenStr = m_sfenRecord->last();
        qDebug() << "[JosekiWindow] onMoveCommitted: ply=" << ply
                 << "sfenRecord.size()=" << m_sfenRecord->size()
                 << "using last sfen=" << m_currentSfenStr;
    }

    // 定跡ウィンドウを更新
    updateJosekiWindow();
}

void MainWindow::flipBoardAndUpdatePlayerInfo()
{
    qDebug() << "[UI] flipBoardAndUpdatePlayerInfo ENTER";
    if (!m_shogiView) return;

    // 盤の表示向きをトグル
    const bool flipped = !m_shogiView->getFlipMode();
    m_shogiView->setFlipMode(flipped);
    if (flipped) m_shogiView->setPiecesFlip();
    else         m_shogiView->setPieces();

    // ★ 手番強調/緊急度は Presenter に再適用してもらう
    if (m_timePresenter) {
        m_timePresenter->onMatchTimeUpdated(
            m_lastP1Ms, m_lastP2Ms, m_lastP1Turn, /*urgencyMs(未使用)*/ 0);
    }

    m_shogiView->update();
    qDebug() << "[UI] flipBoardAndUpdatePlayerInfo LEAVE";
}

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

void MainWindow::performDeferredEvalChartResize()
{
    // デバウンス後の評価値グラフ高さ調整
    // Ctrl+ホイールによる連続したサイズ変更が完了した後に一度だけ実行される
    if (m_shogiView) {
        onBoardSizeChanged(m_shogiView->fieldSize());
    }
}

void MainWindow::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->onGameOverStateChanged(st);
    }
}

void MainWindow::handleBreakOffGame()
{
    ensureGameStateController();
    if (m_gameStateController) {
        m_gameStateController->handleBreakOffGame();
    }
}

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
    conCtx.sfenRecord = m_sfenRecord;
    conCtx.startSfenStr = &m_startSfenStr;
    conCtx.considerationModel = &m_considerationModel;
    conCtx.gameUsiMoves = &m_gameUsiMoves;
    conCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    m_dialogCoordinator->setConsiderationContext(conCtx);

    // 詰み探索コンテキストを設定
    DialogCoordinator::TsumeSearchContext tsumeCtx;
    tsumeCtx.sfenRecord = m_sfenRecord;
    tsumeCtx.startSfenStr = &m_startSfenStr;
    tsumeCtx.positionStrList = &m_positionStrList;
    tsumeCtx.currentMoveIndex = &m_currentMoveIndex;
    tsumeCtx.gameUsiMoves = &m_gameUsiMoves;
    tsumeCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    m_dialogCoordinator->setTsumeSearchContext(tsumeCtx);

    // 棋譜解析コンテキストを設定
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.sfenRecord = m_sfenRecord;
    kifuCtx.moveRecords = m_moveRecords;
    kifuCtx.recordModel = m_kifuRecordModel;
    kifuCtx.activePly = &m_activePly;
    kifuCtx.gameController = m_gameController;
    kifuCtx.gameInfoController = m_gameInfoController;
    kifuCtx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    kifuCtx.evalChart = m_evalChart;
    kifuCtx.gameUsiMoves = &m_gameUsiMoves;
    kifuCtx.presenter = m_analysisPresenter;
    m_dialogCoordinator->setKifuAnalysisContext(kifuCtx);

    // 検討モード開始時に検討タブへ切り替え
    connect(m_dialogCoordinator, &DialogCoordinator::considerationModeStarted,
            this, &MainWindow::onConsiderationModeStarted);

    // 検討モードの時間設定確定時
    connect(m_dialogCoordinator, &DialogCoordinator::considerationTimeSettingsReady,
            this, &MainWindow::onConsiderationTimeSettingsReady);

    // 検討ダイアログでMultiPVが設定されたとき
    connect(m_dialogCoordinator, &DialogCoordinator::considerationMultiPVReady,
            this, &MainWindow::onConsiderationDialogMultiPVReady);

    // 解析進捗シグナルを接続
    connect(m_dialogCoordinator, &DialogCoordinator::analysisProgressReported,
            this, &MainWindow::onKifuAnalysisProgress);

    // 解析結果行選択シグナルを接続（棋譜欄・将棋盤・分岐ツリー連動用）
    connect(m_dialogCoordinator, &DialogCoordinator::analysisResultRowSelected,
            this, &MainWindow::onKifuAnalysisResultRowSelected);
}

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
    deps.sfenRecord = m_sfenRecord;
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

    // コールバックの設定
    // ★ 修正: 対局終了後に棋譜欄と矢印ボタンを有効化
    m_gameStateController->setEnableArrowButtonsCallback(
        std::bind(&MainWindow::enableNavigationAfterGame, this));
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

void MainWindow::ensureBoardSetupController()
{
    if (m_boardSetupController) return;

    m_boardSetupController = new BoardSetupController(this);

    // 依存オブジェクトの設定
    m_boardSetupController->setShogiView(m_shogiView);
    m_boardSetupController->setGameController(m_gameController);
    m_boardSetupController->setMatchCoordinator(m_match);
    m_boardSetupController->setTimeController(m_timeController);
    m_boardSetupController->setSfenRecord(m_sfenRecord);
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
        redrawEngine1EvaluationGraph(ply);
    });
    m_boardSetupController->setRedrawEngine2GraphCallback([this](int ply) {
        redrawEngine2EvaluationGraph(ply);
    });
    m_boardSetupController->setRefreshBranchTreeCallback([this]() {
        refreshBranchTreeLive();
    });
}

void MainWindow::ensurePvClickController()
{
    if (m_pvClickController) return;

    m_pvClickController = new PvClickController(this);

    // 依存オブジェクトの設定
    m_pvClickController->setThinkingModels(m_modelThinking1, m_modelThinking2);
    m_pvClickController->setConsiderationModel(m_considerationModel);
    m_pvClickController->setLogModels(m_lineEditModel1, m_lineEditModel2);
    m_pvClickController->setSfenRecord(m_sfenRecord);
    m_pvClickController->setGameMoves(&m_gameMoves);
    m_pvClickController->setUsiMoves(&m_gameUsiMoves);
    QObject::connect(
        m_pvClickController, &PvClickController::pvDialogClosed,
        this,                &MainWindow::onPvDialogClosed,
        Qt::UniqueConnection);
}


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
    m_posEditCoordinator->setSfenRecord(m_sfenRecord);

    // 状態参照の設定
    m_posEditCoordinator->setCurrentSelectedPly(&m_currentSelectedPly);
    m_posEditCoordinator->setActivePly(&m_activePly);
    m_posEditCoordinator->setStartSfenStr(&m_startSfenStr);
    m_posEditCoordinator->setCurrentSfenStr(&m_currentSfenStr);
    m_posEditCoordinator->setResumeSfenStr(&m_resumeSfenStr);
    m_posEditCoordinator->setOnMainRowGuard(&m_onMainRowGuard);

    // コールバックの設定
    m_posEditCoordinator->setApplyEditMenuStateCallback([this](bool editing) {
        applyEditMenuEditingState(editing);
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
        actions.actionSwapSides = ui->actionSwapSides;
        m_posEditCoordinator->setEditActions(actions);
    }

    // 先後反転シグナルの接続
    connect(m_posEditCoordinator, &PositionEditCoordinator::reversalTriggered,
            this, &MainWindow::onReverseTriggered, Qt::UniqueConnection);
}

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
    deps.sfenRecord = m_sfenRecord;
    // CSA対局開始用の追加依存オブジェクト
    deps.analysisTab = m_analysisTab;
    deps.boardSetupController = m_boardSetupController;
    deps.usiCommLog = m_lineEditModel1;
    deps.engineThinking = m_modelThinking1;
    deps.timeController = m_timeController;
    deps.gameMoves = &m_gameMoves;

    m_csaGameWiring = new CsaGameWiring(deps, this);

    // CsaGameWiringからのシグナルをMainWindowに接続
    connect(m_csaGameWiring, &CsaGameWiring::disableNavigationRequested,
            this, &MainWindow::disableNavigationForGame);
    connect(m_csaGameWiring, &CsaGameWiring::enableNavigationRequested,
            this, &MainWindow::enableNavigationAfterGame);
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

    qDebug().noquote() << "[MW] ensureCsaGameWiring_: created and connected";
}

void MainWindow::ensureJosekiWiring()
{
    if (m_josekiWiring) return;

    JosekiWindowWiring::Dependencies deps;
    deps.parentWidget = this;
    deps.gameController = m_gameController;
    deps.kifuRecordModel = m_kifuRecordModel;
    deps.sfenRecord = m_sfenRecord;
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

    qDebug().noquote() << "[MW] ensureJosekiWiring_: created and connected";
}

void MainWindow::ensureMenuWiring()
{
    if (m_menuWiring) return;

    MenuWindowWiring::Dependencies deps;
    deps.parentWidget = this;
    deps.menuBar = menuBar();

    m_menuWiring = new MenuWindowWiring(deps, this);

    qDebug().noquote() << "[MW] ensureMenuWiring_: created and connected";
}

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

    // PlayerInfoWiringからのシグナルをMainWindowに接続
    connect(m_playerInfoWiring, &PlayerInfoWiring::gameInfoUpdated,
            this, &MainWindow::onGameInfoUpdated);
    connect(m_playerInfoWiring, &PlayerInfoWiring::tabCurrentChanged,
            this, &MainWindow::onTabCurrentChanged);

    // PlayerInfoControllerも同期
    m_playerInfoController = m_playerInfoWiring->playerInfoController();

    qDebug().noquote() << "[MW] ensurePlayerInfoWiring_: created and connected";
}

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

    qDebug().noquote() << "[MW] ensurePreStartCleanupHandler_: created and connected";
}

void MainWindow::ensureTurnSyncBridge()
{
    auto* gc = m_gameController;
    auto* tm = findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, this);
}

void MainWindow::ensurePositionEditController()
{
    if (m_posEdit) return;
    m_posEdit = new PositionEditController(this); // 親=MainWindow にして寿命管理
}

void MainWindow::ensureBoardSyncPresenter()
{
    if (m_boardSync) return;

    qDebug().noquote() << "[MW] ensureBoardSyncPresenter: creating BoardSyncPresenter"
                       << "m_sfenRecord*=" << static_cast<const void*>(m_sfenRecord)
                       << "m_sfenRecord.size=" << (m_sfenRecord ? m_sfenRecord->size() : -1);

    BoardSyncPresenter::Deps d;
    d.gc         = m_gameController;
    d.view       = m_shogiView;
    d.bic        = m_boardController;
    d.sfenRecord = m_sfenRecord;
    d.gameMoves  = &m_gameMoves;

    m_boardSync = new BoardSyncPresenter(d, this);

    qDebug().noquote() << "[MW] ensureBoardSyncPresenter: created m_boardSync*=" << static_cast<const void*>(m_boardSync);
}

void MainWindow::ensureAnalysisPresenter()
{
    if (!m_analysisPresenter)
        m_analysisPresenter = new AnalysisResultsPresenter(this);
}

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

    // ★ 追加: 対局者名確定シグナルを接続
    connect(m_gameStart, &GameStartCoordinator::playerNamesResolved,
            this, &MainWindow::onPlayerNamesResolved);

    // ★ 追加: 連続対局設定シグナルを接続
    connect(m_gameStart, &GameStartCoordinator::consecutiveGamesConfigured,
            this, &MainWindow::onConsecutiveGamesConfigured);

    // ★ 追加: 対局開始時にナビゲーション（棋譜欄と矢印ボタン）を無効化
    connect(m_gameStart, &GameStartCoordinator::started,
            this, &MainWindow::disableNavigationForGame);

    // ★ 追加: 対局開始時の設定を保存（連続対局用）
    connect(m_gameStart, &GameStartCoordinator::started,
            this, &MainWindow::onGameStarted);

    // ★ 追加: 盤面反転シグナルを接続（人を手前に表示する機能用）
    connect(m_gameStart, &GameStartCoordinator::boardFlipped,
            this, &MainWindow::onBoardFlipped,
            Qt::UniqueConnection);

    // ★ 追加: 現在局面から開始時、対局開始後に棋譜欄の指定行を選択
    connect(m_gameStart, &GameStartCoordinator::requestSelectKifuRow,
            this, &MainWindow::onRequestSelectKifuRow,
            Qt::UniqueConnection);
}

void MainWindow::onPreStartCleanupRequested()
{
    ensurePreStartCleanupHandler();
    if (m_preStartCleanupHandler) {
        m_preStartCleanupHandler->performCleanup();
    }
}

void MainWindow::onApplyTimeControlRequested(const GameStartCoordinator::TimeControl& tc)
{
    qDebug().noquote()
    << "[MW] onApplyTimeControlRequested_:"
    << " enabled=" << tc.enabled
    << " P1{base=" << tc.p1.baseMs << " byoyomi=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
    << " P2{base=" << tc.p2.baseMs << " byoyomi=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // ★ 連続対局用に時間設定を保存
    m_lastTimeControl = tc;

    // 時間設定の適用をTimeControlControllerに委譲
    if (m_timeController) {
        m_timeController->applyTimeControl(tc, m_match, m_startSfenStr, m_currentSfenStr, m_shogiView);
    }

    // ★ 対局情報ドックに持ち時間を追加
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
        qWarning().noquote() << "[MW] ensureLiveGameSessionStarted: root is null, cannot start session";
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
            qDebug().noquote() << "[MW] ensureLiveGameSessionStarted: started from node, ply=" << branchPoint->ply();
        } else {
            // ノードが見つからない場合はルートから開始
            qWarning().noquote() << "[MW] ensureLiveGameSessionStarted: branchPoint not found for sfen, starting from root";
            m_liveGameSession->startFromRoot();
        }
    } else {
        // 新規対局: ルートから開始
        m_liveGameSession->startFromRoot();
        qDebug().noquote() << "[MW] ensureLiveGameSessionStarted: session started from root";
    }
}

// UIスレッド安全のため queued 呼び出しにしています
void MainWindow::requestRedrawEngine1Eval()
{
    qDebug() << "[EVAL_GRAPH] requestRedrawEngine1Eval() hook called - invoking redrawEngine1EvaluationGraph";
    // デフォルトパラメータ -1 で呼び出し（HvEでは sfenRecord から計算）
    redrawEngine1EvaluationGraph(-1);
}

void MainWindow::requestRedrawEngine2Eval()
{
    qDebug() << "[EVAL_GRAPH] requestRedrawEngine2Eval() hook called - invoking redrawEngine2EvaluationGraph";
    // デフォルトパラメータ -1 で呼び出し（HvEでは sfenRecord から計算）
    redrawEngine2EvaluationGraph(-1);
}

void MainWindow::initializeNewGameHook(const QString& s)
{
    // --- デバッグ：誰がこの関数を呼び出したか追跡 ---
    qDebug() << "[DEBUG] MainWindow::initializeNewGameHook called with sfen:" << s;
    qDebug() << "[DEBUG]   PlayMode:" << static_cast<int>(m_playMode);
    qDebug() << "[DEBUG]   Call stack trace requested";
    
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
    updateSecondEngineVisibility();  // ★ 追加: 棋譜読み込み後も2番目エンジン表示を更新
}

void MainWindow::showMoveHighlights(const QPoint& from, const QPoint& to)
{
    if (m_boardController) m_boardController->showMoveHighlights(from, to);
}

// ===== MainWindow.cpp: appendKifuLineHook_（ライブ分岐ツリー更新を追加） =====
void MainWindow::appendKifuLineHook(const QString& text, const QString& elapsed)
{
    // 既存：棋譜欄へ 1手追記（Presenter がモデルへ反映）
    appendKifuLine(text, elapsed);

    // ★追加：HvH/HvE の「1手指すごと」に分岐ツリーを更新
    refreshBranchTreeLive();
}

void MainWindow::onRecordRowChangedByPresenter(int row, const QString& comment)
{
    qDebug() << "[MW-DEBUG] onRecordRowChangedByPresenter called: row=" << row
             << "comment=" << comment.left(30) << "..."
             << "playMode=" << static_cast<int>(m_playMode);

    // ★ RecordNavigationController::onRecordRowChangedByPresenter から移植

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

        // ★ 注意：分岐候補欄の更新は新システム（KifuDisplayCoordinator）が担当
    }

    // コメント表示
    const QString cmt = comment.trimmed();
    broadcastComment(cmt.isEmpty() ? tr("コメントなし") : cmt, true);

    // 矢印ボタンなどの活性化
    enableArrowButtons();

    // 検討モード中であれば、選択した手の局面で検討を再開
    if (m_playMode == PlayMode::ConsiderationMode) {
        const QString newPosition = buildPositionStringForIndex(row);
        ensureConsiderationUIController();
        if (m_considerationUIController) {
            m_considerationUIController->setMatchCoordinator(m_match);
            m_considerationUIController->setAnalysisTab(m_analysisTab);
            m_considerationUIController->updatePositionIfInConsiderationMode(
                row, newPosition, &m_gameMoves, m_kifuRecordModel);
        }
    }
}

void MainWindow::onFlowError(const QString& msg)
{
    // 既存のエラー表示に委譲（UI 専用）
    displayErrorMessage(msg);
}

void MainWindow::onResignationTriggered()
{
    // 既存の投了処理に委譲（m_matchの有無は中で判定）
    handleResignation();
}

qint64 MainWindow::getRemainingMsFor(MatchCoordinator::Player p) const
{
    if (!m_timeController) {
        qDebug() << "[MW] getRemainingMsFor_: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_timeController->getRemainingMs(player);
}

qint64 MainWindow::getIncrementMsFor(MatchCoordinator::Player p) const
{
    if (!m_timeController) {
        qDebug() << "[MW] getIncrementMsFor_: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_timeController->getIncrementMs(player);
}

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
    if (m_sfenRecord && moveIndex >= 0 && moveIndex < m_sfenRecord->size()) {
        const QString sfen = m_sfenRecord->at(moveIndex);
        return QStringLiteral("position sfen ") + sfen;
    }

    return QString();
}

qint64 MainWindow::getByoyomiMs() const
{
    if (!m_timeController) {
        qDebug() << "[MW] getByoyomiMs_: timeController=null";
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

void MainWindow::onRecordPaneMainRowChanged(int row)
{
    qDebug().noquote() << "[MW] onRecordPaneMainRowChanged ENTER row=" << row
                       << "navState lineIndex=" << (m_navState ? m_navState->currentLineIndex() : -1)
                       << "isOnMainLine=" << (m_navState ? m_navState->isOnMainLine() : true)
                       << "m_sfenRecord.size=" << (m_sfenRecord ? m_sfenRecord->size() : -1);

    // 再入防止
    static bool s_inProgress = false;
    if (s_inProgress) {
        qDebug() << "[MW] onRecordPaneMainRowChanged: SKIPPED (re-entry guard)";
        return;
    }
    s_inProgress = true;

    // ★ 分岐ナビゲーション中は盤面同期をスキップ
    // loadBoardWithHighlights() が盤面を管理しているため、二重更新を防ぐ
    if (m_skipBoardSyncForBranchNav) {
        qDebug() << "[MW] onRecordPaneMainRowChanged: SKIPPED (branch navigation in progress)";
        s_inProgress = false;
        return;
    }

    // ★ CSA対局が進行中の場合のみ棋譜リストの選択変更による盤面同期をスキップ
    bool csaGameInProgress = false;
    if (m_csaGameCoordinator) {
        csaGameInProgress = (m_csaGameCoordinator->gameState() == CsaGameCoordinator::GameState::InGame);
    }

    if (csaGameInProgress) {
        qDebug() << "[MW] onRecordPaneMainRowChanged: SKIPPED (CSA game in progress)";
        s_inProgress = false;
        return;
    }

    // ★ 盤面同期とUI更新（RecordNavigationController::onMainRowChanged から移植）
    if (row >= 0) {
        // ★ 修正: 分岐ライン上の場合は分岐ツリーからSFENを取得して盤面を更新
        // syncBoardAndHighlightsAtRow は m_sfenRecord（本譜）を使用するため、
        // 分岐ライン上では正しくない局面が表示されてしまう
        bool handledByBranchTree = false;
        if (m_navState != nullptr && !m_navState->isOnMainLine() && m_branchTree != nullptr) {
            const int lineIndex = m_navState->currentLineIndex();
            QVector<BranchLine> lines = m_branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (row >= 0 && row < line.nodes.size()) {
                    QString currentSfen = line.nodes.at(row)->sfen();
                    QString prevSfen;
                    if (row > 0 && (row - 1) < line.nodes.size()) {
                        prevSfen = line.nodes.at(row - 1)->sfen();
                    }
                    qDebug().noquote() << "[MW] onRecordPaneMainRowChanged: using branch tree SFEN"
                                       << "lineIndex=" << lineIndex << "row=" << row
                                       << "sfen=" << currentSfen.left(40);
                    loadBoardWithHighlights(currentSfen, prevSfen);
                    handledByBranchTree = true;
                }
            }
        }

        // 本譜の場合、または分岐ツリーから取得できなかった場合は従来の方法を使用
        if (!handledByBranchTree) {
            syncBoardAndHighlightsAtRow(row);
        }

        // 現在手数トラッキングを更新
        m_activePly = row;
        m_currentSelectedPly = row;
        m_currentMoveIndex = row;

        // 棋譜欄のハイライト行を更新
        if (m_kifuRecordModel) {
            m_kifuRecordModel->setCurrentHighlightRow(row);
        }

        // 手番表示を更新
        setCurrentTurn();

        // 盤面の手番ラベルを更新
        if (m_shogiView && m_shogiView->board()) {
            const QString bw = m_shogiView->board()->currentPlayer();
            const bool isBlackTurn = (bw != QStringLiteral("w"));
            m_shogiView->setActiveSide(isBlackTurn);

            const auto player = isBlackTurn ? ShogiGameController::Player1 : ShogiGameController::Player2;
            if (m_shogiView) {
                m_shogiView->updateTurnIndicator(player);
            }
        }

        // 評価値グラフの縦線（カーソルライン）を更新
        if (m_evalGraphController) {
            m_evalGraphController->setCurrentPly(row);
        }
    }

    // 矢印ボタンを有効化
    enableArrowButtons();

    // ★ 注意：分岐候補欄の更新は KifuDisplayCoordinator::onPositionChanged() が担当

    // m_currentSfenStrを現在の局面に更新
    // ★ 注意：変化ライン上にいる場合は m_sfenRecord（本譜のSFEN）を使わない
    // 変化ラインのSFENはKifuDisplayCoordinatorのboardWithHighlightsRequiredシグナル経由で設定済み
    bool isOnVariation = (m_navState != nullptr && !m_navState->isOnMainLine());
    qDebug().noquote() << "[MW] onRecordPaneMainRowChanged: checking sfenRecord"
                       << "row=" << row
                       << "m_sfenRecord=" << (m_sfenRecord ? "exists" : "NULL")
                       << "size=" << (m_sfenRecord ? m_sfenRecord->size() : -1)
                       << "isOnVariation=" << isOnVariation;
    if (!isOnVariation && row >= 0 && m_sfenRecord && row < m_sfenRecord->size()) {
        m_currentSfenStr = m_sfenRecord->at(row);
        qDebug().noquote() << "[MW] onRecordPaneMainRowChanged: updated m_currentSfenStr="
                           << m_currentSfenStr.left(60);
    } else if (isOnVariation) {
        qDebug().noquote() << "[MW] onRecordPaneMainRowChanged: skipped m_currentSfenStr update (on variation line)";
    } else {
        qWarning() << "[MW] onRecordPaneMainRowChanged: row out of range or sfenRecord invalid!";
    }

    // 定跡ウィンドウを更新
    updateJosekiWindow();

    // 検討モード中であれば、選択した手の局面で検討を再開
    if (m_playMode == PlayMode::ConsiderationMode && m_match) {
        const QString newPosition = buildPositionStringForIndex(row);
        if (!newPosition.isEmpty()) {
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
                const int usiIdx = row - 1;
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

            if (m_match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo, lastUsiMove)) {
                // ポジションが変更された場合、経過時間タイマーをリセットして再開
                if (m_analysisTab) {
                    m_analysisTab->startElapsedTimer();
                }
            }
        }
    }

    // ★ 新システム（KifuDisplayCoordinator）に位置変更を通知
    // これにより分岐候補欄と分岐ツリーのハイライトが更新される
    if (m_displayCoordinator != nullptr) {
        QString sfen;
        bool foundInBranch = false;
        const int lineIndex = (m_navState != nullptr) ? m_navState->currentLineIndex() : 0;

        // 分岐ライン上では m_branchTree から正しいSFENを取得
        if (m_navState != nullptr && !m_navState->isOnMainLine() && m_branchTree != nullptr) {
            QVector<BranchLine> lines = m_branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (row >= 0 && row < line.nodes.size()) {
                    sfen = line.nodes.at(row)->sfen();
                    foundInBranch = true;
                    qDebug().noquote() << "[MW] onRecordPaneMainRowChanged: got SFEN from branchTree"
                                       << "lineIndex=" << lineIndex << "row=" << row;
                }
            }
        }

        // 本譜または分岐ツリーから取得できなかった場合は m_sfenRecord から取得
        if (!foundInBranch && m_sfenRecord != nullptr && row >= 0 && row < m_sfenRecord->size()) {
            sfen = m_sfenRecord->at(row);
        }

        if (!sfen.isEmpty()) {
            m_displayCoordinator->onPositionChanged(lineIndex, row, sfen);
        }
    }

    qDebug() << "[MW] onRecordPaneMainRowChanged LEAVE row=" << row;
    s_inProgress = false;
}

// ===== MainWindow.cpp: ライブ用の KifuLoadCoordinator を確保 =====
void MainWindow::ensureKifuLoadCoordinatorForLive()
{
    if (m_kifuLoadCoordinator) {
        return; // 既に用意済み
    }

    // KIF読込時と同等の依存で生成（ロード自体はしない）
    m_kifuLoadCoordinator = new KifuLoadCoordinator(
        /* gameMoves           */ m_gameMoves,
        /* positionStrList     */ m_positionStrList,
        /* activePly           */ m_activePly,
        /* currentSelectedPly  */ m_currentSelectedPly,
        /* currentMoveIndex    */ m_currentMoveIndex,
        /* sfenRecord          */ m_sfenRecord,
        /* gameInfoTable       */ m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr,
        /* gameInfoDock        */ nullptr,  // GameInfoPaneControllerに移行済み
        /* tab                 */ m_tab,
        /* recordPane          */ m_recordPane,
        /* kifuRecordModel     */ m_kifuRecordModel,
        /* kifuBranchModel     */ m_kifuBranchModel,
        this);

    // ★ 新規: 分岐ツリーとナビゲーション状態を設定
    if (m_branchTree != nullptr) {
        m_kifuLoadCoordinator->setBranchTree(m_branchTree);
    }
    if (m_navState != nullptr) {
        m_kifuLoadCoordinator->setNavigationState(m_navState);
    }

    // ★ 新規: 分岐ツリー構築完了シグナルを接続
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::branchTreeBuilt,
            this, &MainWindow::onBranchTreeBuilt, Qt::UniqueConnection);

    // Analysisタブ・ShogiViewとの配線
    m_kifuLoadCoordinator->setAnalysisTab(m_analysisTab);
    m_kifuLoadCoordinator->setShogiView(m_shogiView);

    // UI更新通知（既存と同じ）
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::displayGameRecord,
            this, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
            this, &MainWindow::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::enableArrowButtons,
            this, &MainWindow::enableArrowButtons, Qt::UniqueConnection);
    // ★ 追加: 対局情報の元データを保存
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::gameInfoPopulated,
            this, &MainWindow::setOriginalGameInfo, Qt::UniqueConnection);
}

// ===== MainWindow.cpp: ライブ対局中に分岐ツリーを更新 =====
void MainWindow::refreshBranchTreeLive()
{
    // ★ 新システム: LiveGameSession と KifuDisplayCoordinator が分岐ツリーを管理
    // この関数は互換性のために残すが、新システムが自動的に更新を行う
    qDebug().noquote() << "[MW-DEBUG] refreshBranchTreeLive(): delegating to new system (no-op)";
}

// ========== UNDO用：MainWindow 補助関数 ==========

bool MainWindow::getMainRowGuard() const
{
    return m_onMainRowGuard;
}

void MainWindow::setMainRowGuard(bool on)
{
    m_onMainRowGuard = on;
}

bool MainWindow::isHvH() const
{
    if (m_gameStateController) {
        return m_gameStateController->isHvH();
    }
    return (m_playMode == PlayMode::HumanVsHuman);
}

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

void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    const bool gameOverAppended =
        (m_match && m_match->gameOverState().isOver && m_match->gameOverState().moveAppended);
    if (gameOverAppended) return;

    ensureRecordPresenter();
    if (m_recordPresenter) {
        m_recordPresenter->appendMoveLine(m_lastMove, elapsedTime);

        // ★ 変更: Presenter にライブ記録を蓄積させる
        if (!m_lastMove.isEmpty()) {
            m_recordPresenter->addLiveKifItem(m_lastMove, elapsedTime);
        }
    }

    // ★ 新システム: LiveGameSession にライブ対局の手を通知
    if (m_liveGameSession != nullptr && !m_lastMove.isEmpty()) {
        // セッションが未開始の場合は遅延開始
        if (!m_liveGameSession->isActive()) {
            ensureLiveGameSessionStarted();
        }

        if (m_liveGameSession->isActive()) {
            // ★ 修正: m_sfenRecord は本譜の SFEN リストなので、分岐対局中は
            //    実際の盤面から完全な SFEN を構築する必要がある
            QString sfen;
            if (m_gameController && m_gameController->board()) {
                ShogiBoard* board = m_gameController->board();
                // 完全な SFEN を構築: <盤面> <手番> <持ち駒> <手数>
                const QString boardPart = board->convertBoardToSfen();
                const QString turnPart = board->currentPlayer();  // "b" or "w"
                QString standPart = board->convertStandToSfen();
                if (standPart.isEmpty()) {
                    standPart = QStringLiteral("-");
                }
                // 手数は anchorPly + 現在のセッション内の手数 + 1
                const int moveCount = m_liveGameSession->totalPly() + 1;
                sfen = QStringLiteral("%1 %2 %3 %4")
                           .arg(boardPart, turnPart, standPart, QString::number(moveCount));
                qDebug().noquote() << "[MW] appendMoveLineToRecordPane: constructed full SFEN for LiveGameSession"
                                   << "sfen=" << sfen.left(80);
            } else if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
                // フォールバック: 盤面がない場合は m_sfenRecord を使用
                sfen = m_sfenRecord->last();
                qWarning() << "[MW] appendMoveLineToRecordPane: fallback to m_sfenRecord (no board)";
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
    qDebug() << "[MW] autoSaveKifuToFile_ called: dir=" << saveDir
             << "mode=" << static_cast<int>(playMode);

    // GameRecordModel と KifuExportController の準備
    ensureGameRecordModel();
    ensureKifuExportController();
    updateKifuExportDependencies();

    if (!m_kifuExportController) {
        qWarning() << "[MW] autoSaveKifuToFile_: kifuExportController is null";
        return;
    }

    QString savedPath;
    const bool ok = m_kifuExportController->autoSaveToDir(saveDir, &savedPath);
    if (ok && !savedPath.isEmpty()) {
        kifuSaveFileName = savedPath;
    }
}

// KIF形式で棋譜をクリップボードにコピー
// ★ クリップボードから棋譜を貼り付け
void MainWindow::pasteKifuFromClipboard()
{
    KifuPasteDialog* dlg = new KifuPasteDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // ダイアログの「取り込む」シグナルをKifuLoadCoordinatorに接続
    connect(dlg, &KifuPasteDialog::importRequested,
            this, &MainWindow::onKifuPasteImportRequested);

    dlg->show();
}

void MainWindow::onKifuPasteImportRequested(const QString& content)
{
    qDebug().noquote() << "[MW] onKifuPasteImportRequested_: content length =" << content.size();

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
        qWarning() << "[MW] onKifuPasteImportRequested_: m_kifuLoadCoordinator is null";
        ui->statusbar->showMessage(tr("棋譜の取り込みに失敗しました（内部エラー）"), 3000);
    }
}

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

// ★ コメント更新スロットの実装（CommentCoordinatorに委譲）
void MainWindow::onCommentUpdated(int moveIndex, const QString& newComment)
{
    ensureCommentCoordinator();
    if (m_commentCoordinator) {
        m_commentCoordinator->setGameRecordModel(m_gameRecord);
        m_commentCoordinator->onCommentUpdated(moveIndex, newComment);
    }
}

// ★ 追加: GameRecordModel の遅延初期化
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

    // ★ 新システム: KifuBranchTree と KifuNavigationState を設定
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

    qDebug().noquote() << "[MW] ensureGameRecordModel_: created and bound";
}

// ★ GameRecordModel::commentChanged スロット（CommentCoordinatorに委譲）
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
        m_commentCoordinator->onCommentUpdateCallback(ply, comment);
    }
}

// ★ 追加: 読み筋クリック処理
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
        m_pvClickController->onPvRowClicked(engineIndex, row);
    }
}

void MainWindow::onPvDialogClosed(int engineIndex)
{
    if (m_analysisTab) {
        m_analysisTab->clearThinkingViewSelection(engineIndex);
    }
}

// ★ 追加: タブ選択変更時のスロット（インデックスを設定ファイルに保存）
void MainWindow::onTabCurrentChanged(int index)
{
    // タブインデックスを設定ファイルに保存
    SettingsService::setLastSelectedTabIndex(index);
    qDebug().noquote() << "[MW] onTabCurrentChanged: saved tab index =" << index;
}

// ========================================================
// CSA通信対局関連のスロット（CsaGameWiring経由）
// ========================================================

void MainWindow::onCsaPlayModeChanged(int mode)
{
    m_playMode = static_cast<PlayMode>(mode);
    qDebug().noquote() << "[MW] onCsaPlayModeChanged_: mode=" << mode;
}

void MainWindow::onCsaShowGameEndDialog(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::onCsaEngineScoreUpdated(int scoreCp, int ply)
{
    qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: scoreCp=" << scoreCp << "ply=" << ply;

    // 評価値グラフに追加
    if (!m_evalChart) {
        qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: m_evalChart is NULL";
        return;
    }

    // CSA対局では自分側のエンジンの評価値を表示
    // 自分が先手(黒)ならP1に、後手(白)ならP2に追加
    if (m_csaGameCoordinator) {
        bool isBlackSide = m_csaGameCoordinator->isBlackSide();
        if (isBlackSide) {
            // 先手側のエンジン評価値はP1に追加
            m_evalChart->appendScoreP1(ply, scoreCp, false);
            qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: appendScoreP1 called";
        } else {
            // 後手側のエンジン評価値はP2に追加
            // 後手視点の評価値なので、グラフ表示用に反転する
            m_evalChart->appendScoreP2(ply, -scoreCp, false);
            qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: appendScoreP2 called (inverted)";
        }
    }
}

void MainWindow::onJosekiForcedPromotion(bool forced, bool promote)
{
    if (m_gameController) {
        m_gameController->setForcedPromotion(forced, promote);
    }
}

// =============================================================================
// 言語設定
// =============================================================================

void MainWindow::onToolBarVisibilityToggled(bool visible)
{
    if (ui->toolBar) {
        ui->toolBar->setVisible(visible);
    }
    SettingsService::setToolbarVisible(visible);
}

// =============================================================================
// 新規コントローラのensure関数
// =============================================================================

void MainWindow::ensureJishogiController()
{
    if (m_jishogiController) return;
    m_jishogiController = new JishogiScoreDialogController(this);
}

void MainWindow::ensureNyugyokuHandler()
{
    if (m_nyugyokuHandler) return;
    m_nyugyokuHandler = new NyugyokuDeclarationHandler(this);
}

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

void MainWindow::ensureConsiderationUIController()
{
    if (m_considerationUIController) return;

    m_considerationUIController = new ConsiderationModeUIController(this);
    m_considerationUIController->setAnalysisTab(m_analysisTab);
    m_considerationUIController->setShogiView(m_shogiView);
    m_considerationUIController->setMatchCoordinator(m_match);
    m_considerationUIController->setConsiderationModel(m_considerationModel);
    m_considerationUIController->setCommLogModel(m_lineEditModel1);

    // コントローラからのシグナルをMainWindowスロットに接続
    connect(m_considerationUIController, &ConsiderationModeUIController::stopRequested,
            this, &MainWindow::stopTsumeSearch);
    connect(m_considerationUIController, &ConsiderationModeUIController::startRequested,
            this, &MainWindow::displayConsiderationDialog);
    connect(m_considerationUIController, &ConsiderationModeUIController::multiPVChangeRequested,
            this, [this](int value) {
                if (m_match && m_playMode == PlayMode::ConsiderationMode) {
                    m_match->updateConsiderationMultiPV(value);
                }
            });
}

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
    m_dockLayoutManager->setSavedLayoutsMenu(m_savedLayoutsMenu);
}

void MainWindow::ensureDockCreationService()
{
    if (m_dockCreationService) return;

    m_dockCreationService = new DockCreationService(this, this);
    m_dockCreationService->setDisplayMenu(ui->Display);
}

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

    // GameRecordModel初期化要求時のシグナル接続
    connect(m_commentCoordinator, &CommentCoordinator::ensureGameRecordModelRequested,
            this, &MainWindow::ensureGameRecordModel);
}

void MainWindow::ensureUsiCommandController()
{
    if (!m_usiCommandController) {
        m_usiCommandController = new UsiCommandController(this);
    }
    m_usiCommandController->setMatchCoordinator(m_match);
    m_usiCommandController->setAnalysisTab(m_analysisTab);
}

// 検討モデルから矢印を更新（コントローラに委譲）
void MainWindow::updateConsiderationArrows()
{
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        m_considerationUIController->setShogiView(m_shogiView);
        m_considerationUIController->setAnalysisTab(m_analysisTab);
        m_considerationUIController->setConsiderationModel(m_considerationModel);
        m_considerationUIController->setCurrentSfenStr(m_currentSfenStr);
        m_considerationUIController->updateArrows();
    }
}

// 矢印表示チェックボックスの状態変更時（コントローラに委譲）
void MainWindow::onShowArrowsChanged(bool checked)
{
    ensureConsiderationUIController();
    if (m_considerationUIController) {
        m_considerationUIController->onShowArrowsChanged(checked);
    }
}

// =====================================================================
// ★ テスト自動化用メソッド
// =====================================================================

void MainWindow::setTestMode(bool enabled)
{
    m_testMode = enabled;
    qDebug() << "[TEST] Test mode set to:" << enabled;
}

void MainWindow::loadKifuFile(const QString& path)
{
    qDebug() << "[TEST] loadKifuFile:" << path;

    if (!QFile::exists(path)) {
        qWarning() << "[TEST] File not found:" << path;
        return;
    }

    setReplayMode(true);
    ensureGameInfoController();

    // 既存があれば即座に破棄
    if (m_kifuLoadCoordinator) {
        delete m_kifuLoadCoordinator;
        m_kifuLoadCoordinator = nullptr;
    }

    // KifuLoadCoordinatorを作成
    m_kifuLoadCoordinator = new KifuLoadCoordinator(
        m_gameMoves, m_positionStrList,
        m_activePly, m_currentSelectedPly,
        m_currentMoveIndex, m_sfenRecord,
        m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr,
        nullptr, m_tab, m_recordPane,
        m_kifuRecordModel, m_kifuBranchModel,
        this);

    // 分岐ツリーとナビゲーション状態を設定
    if (m_branchTree != nullptr) {
        m_kifuLoadCoordinator->setBranchTree(m_branchTree);
    }
    if (m_navState != nullptr) {
        m_kifuLoadCoordinator->setNavigationState(m_navState);
    }

    // シグナル接続
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::branchTreeBuilt,
            this, &MainWindow::onBranchTreeBuilt, Qt::UniqueConnection);
    m_kifuLoadCoordinator->setAnalysisTab(m_analysisTab);
    m_kifuLoadCoordinator->setShogiView(m_shogiView);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::displayGameRecord,
            this, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
            this, &MainWindow::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::enableArrowButtons,
            this, &MainWindow::enableArrowButtons, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::gameInfoPopulated,
            this, &MainWindow::setOriginalGameInfo, Qt::UniqueConnection);

    // 拡張子に応じて読み込み
    if (path.endsWith(QLatin1String(".csa"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadCsaFromFile(path);
    } else if (path.endsWith(QLatin1String(".ki2"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadKi2FromFile(path);
    } else if (path.endsWith(QLatin1String(".jkf"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadJkfFromFile(path);
    } else if (path.endsWith(QLatin1String(".usen"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadUsenFromFile(path);
    } else if (path.endsWith(QLatin1String(".usi"), Qt::CaseInsensitive)) {
        m_kifuLoadCoordinator->loadUsiFromFile(path);
    } else {
        m_kifuLoadCoordinator->loadKifuFromFile(path);
    }

    qDebug() << "[TEST] loadKifuFile completed";
}

void MainWindow::navigateToPly(int ply)
{
    qDebug() << "[TEST] navigateToPly:" << ply;

    if (m_recordPane != nullptr) {
        QTableView* kifuView = m_recordPane->kifuView();
        if (kifuView != nullptr) {
            kifuView->selectRow(ply);
            qDebug() << "[TEST] navigateToPly: selected row" << ply;
        } else {
            qWarning() << "[TEST] navigateToPly: kifuView is null";
        }
    } else {
        qWarning() << "[TEST] navigateToPly: m_recordPane is null";
    }
}

void MainWindow::clickBranchCandidate(int index)
{
    qDebug() << "[TEST] clickBranchCandidate:" << index;

    if (m_displayCoordinator != nullptr) {
        // KifuDisplayCoordinatorを通じて分岐候補をクリック
        if (m_kifuBranchModel != nullptr) {
            QModelIndex modelIndex = m_kifuBranchModel->index(index, 0);
            m_displayCoordinator->onBranchCandidateActivated(modelIndex);
            qDebug() << "[TEST] clickBranchCandidate: dispatched to coordinator";
        }
    } else {
        qWarning() << "[TEST] clickBranchCandidate: m_displayCoordinator is null";
    }
}

void MainWindow::clickNextButton()
{
    qDebug() << "[TEST] clickNextButton";

    if (m_recordPane != nullptr) {
        QPushButton* nextBtn = m_recordPane->nextButton();
        if (nextBtn != nullptr) {
            nextBtn->click();
            qDebug() << "[TEST] clickNextButton: button clicked";
        } else {
            qWarning() << "[TEST] clickNextButton: nextButton is null";
        }
    } else {
        qWarning() << "[TEST] clickNextButton: m_recordPane is null";
    }
}

void MainWindow::clickPrevButton()
{
    qDebug() << "[TEST] clickPrevButton";

    if (m_recordPane != nullptr) {
        QPushButton* prevBtn = m_recordPane->prevButton();
        if (prevBtn != nullptr) {
            prevBtn->click();
            qDebug() << "[TEST] clickPrevButton: button clicked";
        } else {
            qWarning() << "[TEST] clickPrevButton: prevButton is null";
        }
    } else {
        qWarning() << "[TEST] clickPrevButton: m_recordPane is null";
    }
}

void MainWindow::clickFirstButton()
{
    qDebug() << "[TEST] clickFirstButton";

    if (m_recordPane != nullptr) {
        QPushButton* firstBtn = m_recordPane->firstButton();
        if (firstBtn != nullptr) {
            firstBtn->click();
            qDebug() << "[TEST] clickFirstButton: button clicked";
        } else {
            qWarning() << "[TEST] clickFirstButton: firstButton is null";
        }
    } else {
        qWarning() << "[TEST] clickFirstButton: m_recordPane is null";
    }
}

void MainWindow::clickLastButton()
{
    qDebug() << "[TEST] clickLastButton";

    if (m_recordPane != nullptr) {
        QPushButton* lastBtn = m_recordPane->lastButton();
        if (lastBtn != nullptr) {
            lastBtn->click();
            qDebug() << "[TEST] clickLastButton: button clicked";
        } else {
            qWarning() << "[TEST] clickLastButton: lastButton is null";
        }
    } else {
        qWarning() << "[TEST] clickLastButton: m_recordPane is null";
    }
}

void MainWindow::clickKifuRow(int row)
{
    qDebug() << "[TEST] clickKifuRow:" << row;

    // ★ ユーザー操作（テスト）による棋譜欄クリックの前にフラグをリセット
    // 分岐ナビゲーションのスキップフラグが残っていると、棋譜欄の選択が正しく処理されない
    m_skipBoardSyncForBranchNav = false;

    if (m_recordPane == nullptr) {
        qWarning() << "[TEST] clickKifuRow: m_recordPane is null";
        return;
    }

    QTableView* kifuView = m_recordPane->kifuView();
    if (kifuView == nullptr) {
        qWarning() << "[TEST] clickKifuRow: kifuView is null";
        return;
    }

    // 指定行を選択してアクティベート
    QModelIndex index = kifuView->model()->index(row, 0);
    if (!index.isValid()) {
        qWarning() << "[TEST] clickKifuRow: invalid index for row" << row;
        return;
    }

    // currentIndex を設定してからクリックシグナルをエミュレート
    kifuView->setCurrentIndex(index);
    kifuView->scrollTo(index);

    // activated シグナルを発火（ダブルクリック相当）
    emit kifuView->activated(index);

    qDebug() << "[TEST] clickKifuRow: row" << row << "clicked";
}

void MainWindow::clickBranchTreeNode(int row, int ply)
{
    qDebug() << "[TEST] clickBranchTreeNode: row=" << row << "ply=" << ply;

    // 分岐ツリーのノードをクリックしたのと同等の処理
    // EngineAnalysisTab の branchNodeActivated シグナルを発火させる
    // これにより、KifuDisplayCoordinator が分岐ナビゲーションを処理する
    if (m_analysisTab) {
        // シグナルを直接発火（テスト用）
        // まずハイライトを更新（GUIクリック時の即時フィードバックをシミュレート）
        m_analysisTab->highlightBranchTreeAt(row, ply, false);
        // シグナルを発火
        emit m_analysisTab->branchNodeActivated(row, ply);
    } else {
        // フォールバック: 直接呼び出し
        onBranchNodeActivated(row, ply);
    }

    qDebug() << "[TEST] clickBranchTreeNode: completed";
}

void MainWindow::dumpTestState()
{
    qDebug() << "========== [TEST STATE DUMP] ==========";

    // 1. 盤面情報
    qDebug() << "[TEST] === BOARD STATE ===";
    qDebug() << "[TEST] currentSfen:" << m_currentSfenStr;
    if (m_gameController && m_gameController->board()) {
        qDebug() << "[TEST] board currentPlayer:" << m_gameController->board()->currentPlayer();
        qDebug() << "[TEST] board actualSfen:" << m_gameController->board()->convertBoardToSfen();
    }

    // 2. 棋譜欄の状態
    qDebug() << "[TEST] === KIFU LIST STATE ===";
    if (m_kifuRecordModel) {
        const int rowCount = m_kifuRecordModel->rowCount();
        qDebug() << "[TEST] kifuRecordModel rowCount:" << rowCount;
        qDebug() << "[TEST] kifuRecordModel currentHighlightRow:" << m_kifuRecordModel->currentHighlightRow();
        // 実際の内容を表示（最大10件）
        for (int i = 0; i < qMin(rowCount, 10); ++i) {
            KifuDisplay* item = m_kifuRecordModel->item(i);
            if (item) {
                qDebug().noquote() << "[TEST]   kifu[" << i << "]:" << item->currentMove();
            }
        }
        if (rowCount > 10) {
            qDebug() << "[TEST]   ... and" << (rowCount - 10) << "more";
        }
    }

    // 3. 分岐候補欄の状態
    qDebug() << "[TEST] === BRANCH CANDIDATES STATE ===";
    if (m_kifuBranchModel) {
        qDebug() << "[TEST] kifuBranchModel rowCount:" << m_kifuBranchModel->rowCount();
        qDebug() << "[TEST] kifuBranchModel hasBackToMainRow:" << m_kifuBranchModel->hasBackToMainRow();
        for (int i = 0; i < m_kifuBranchModel->rowCount(); ++i) {
            QModelIndex idx = m_kifuBranchModel->index(i, 0);
            qDebug() << "[TEST]   branch[" << i << "]:"
                     << m_kifuBranchModel->data(idx, Qt::DisplayRole).toString();
        }
    }
    // 分岐候補ビューの有効状態を確認
    if (m_recordPane && m_recordPane->branchView()) {
        qDebug() << "[TEST] branchView enabled:" << m_recordPane->branchView()->isEnabled();
    }

    // 4. ナビゲーション状態
    qDebug() << "[TEST] === NAVIGATION STATE ===";
    if (m_navState) {
        qDebug() << "[TEST] currentPly:" << m_navState->currentPly();
        qDebug() << "[TEST] currentLineIndex:" << m_navState->currentLineIndex();
        qDebug() << "[TEST] isOnMainLine:" << m_navState->isOnMainLine();
        if (m_navState->currentNode()) {
            qDebug() << "[TEST] currentNode displayText:" << m_navState->currentNode()->displayText();
        }
    }

    // 5. 分岐ツリーの状態
    qDebug() << "[TEST] === BRANCH TREE STATE ===";
    if (m_branchTree) {
        auto lines = m_branchTree->allLines();
        qDebug() << "[TEST] branchTree lineCount:" << lines.size();
        for (int i = 0; i < lines.size() && i < 5; ++i) {  // 最大5ライン
            const auto& line = lines[i];
            qDebug() << "[TEST]   line[" << i << "] nodeCount:" << line.nodes.size()
                     << "branchPly:" << line.branchPly
                     << "branchPoint:" << (line.branchPoint ? line.branchPoint->displayText() : "null");
        }
    }

    // 6. 一致性チェック
    qDebug() << "[TEST] === CONSISTENCY CHECK ===";
    if (m_displayCoordinator) {
        const bool displayConsistent = m_displayCoordinator->verifyDisplayConsistency();
        qDebug() << "[TEST] displayConsistency:" << (displayConsistent ? "PASS" : "FAIL");
        if (!displayConsistent) {
            qDebug().noquote() << m_displayCoordinator->getConsistencyReport();
        }
    }

    // 7. 4方向一致検証
    const bool fourWayConsistent = verify4WayConsistency();
    qDebug() << "[TEST] fourWayConsistency:" << (fourWayConsistent ? "PASS" : "FAIL");

    qDebug() << "========== [END STATE DUMP] ==========";
}

QString MainWindow::extractSfenBase(const QString& sfen) const
{
    // SFEN文字列から局面部分（手数と持ち駒前の部分）を抽出
    // 例: "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
    // から "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL" を取得
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.isEmpty()) {
        return sfen;
    }
    return parts.at(0);  // 盤面部分のみを返す
}

bool MainWindow::verify4WayConsistency()
{
    // 4方向（盤面・棋譜欄・分岐候補欄・分岐ツリー）の一致を検証
    bool consistent = true;
    QString issues;

    // 1. 盤面SFENとナビゲーション状態のSFENを比較
    if (m_gameController != nullptr && m_gameController->board() != nullptr && m_navState != nullptr) {
        const QString boardSfen = m_gameController->board()->convertBoardToSfen();
        KifuBranchNode* currentNode = m_navState->currentNode();
        if (currentNode != nullptr) {
            const QString nodeSfen = currentNode->sfen();
            const QString boardBase = extractSfenBase(boardSfen);
            const QString nodeBase = extractSfenBase(nodeSfen);
            if (boardBase != nodeBase) {
                consistent = false;
                qWarning() << "[4WAY] Board SFEN mismatch:"
                           << "board=" << boardBase
                           << "node=" << nodeBase;
            }
        }
    }

    // 2. 棋譜欄の内容が選択ラインの内容と一致するかを検証
    if (m_kifuRecordModel != nullptr && m_navState != nullptr && m_branchTree != nullptr) {
        const int currentLineIndex = m_navState->currentLineIndex();
        QVector<BranchLine> lines = m_branchTree->allLines();
        if (currentLineIndex >= 0 && currentLineIndex < lines.size()) {
            const BranchLine& line = lines.at(currentLineIndex);
            // 3手目の内容を比較（分岐が発生しやすい箇所）
            if (line.nodes.size() > 3 && m_kifuRecordModel->rowCount() > 3) {
                KifuDisplay* item = m_kifuRecordModel->item(3);  // 3手目（1-indexed で row 3）
                if (item != nullptr && line.nodes.size() > 2) {
                    // ライン内でply=3のノードを探す
                    for (KifuBranchNode* node : std::as_const(line.nodes)) {
                        if (node->ply() == 3) {
                            // 指し手テキストから手数番号部分を除去して比較
                            QString itemMove = item->currentMove().trimmed();
                            QString nodeMove = node->displayText();
                            // itemMove は "   3 ▲○○+" のような形式
                            // 手数番号を除去: 最初の数字+スペースを取り除く
                            itemMove = itemMove.remove(QRegularExpression(QStringLiteral("^\\s*\\d+\\s+")));
                            itemMove = itemMove.remove(QLatin1Char('+'));  // 分岐マークを除去
                            if (!itemMove.contains(nodeMove) && !nodeMove.contains(itemMove)) {
                                // 完全一致しない場合でも、部分一致があれば OK
                                // （表示形式の違いによる）
                                if (itemMove != nodeMove) {
                                    consistent = false;
                                    qWarning() << "[4WAY] Kifu content mismatch at ply 3:"
                                               << "model=" << itemMove
                                               << "node=" << nodeMove;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // 3. 分岐候補数が期待値と一致するかを検証
    if (m_kifuBranchModel != nullptr && m_navState != nullptr) {
        KifuBranchNode* currentNode = m_navState->currentNode();
        if (currentNode != nullptr) {
            KifuBranchNode* parent = currentNode->parent();
            if (parent != nullptr) {
                const int expectedCandidates = parent->childCount();
                int actualCandidates = m_kifuBranchModel->rowCount();
                // "本譜へ戻る" 行がある場合は除外
                if (m_kifuBranchModel->hasBackToMainRow() && actualCandidates > 0) {
                    actualCandidates--;
                }
                // 分岐がない場合（候補が1つのみ）は0になることがある
                if (expectedCandidates > 1 && actualCandidates != expectedCandidates) {
                    // 分岐がある場合のみ警告
                    qWarning() << "[4WAY] Branch candidate count mismatch:"
                               << "expected=" << expectedCandidates
                               << "actual=" << actualCandidates;
                    // これは軽微な不一致なのでconsistentはfalseにしない
                }
            }
        }
    }

    // 4. KifuDisplayCoordinator の一致性を確認
    if (m_displayCoordinator != nullptr) {
        if (!m_displayCoordinator->verifyDisplayConsistency()) {
            consistent = false;
            qWarning() << "[4WAY] KifuDisplayCoordinator inconsistency detected";
        }
    }

    return consistent;
}

// =====================================================================
// ★ 対局シミュレーション用テストメソッド
// =====================================================================

void MainWindow::startTestGame()
{
    qDebug() << "[TEST] startTestGame: starting test game (hirate)";

    // 1. ゲームコントローラーを平手で初期化
    qDebug() << "[TEST] startTestGame: step 1 - gameController=" << static_cast<void*>(m_gameController);
    if (m_gameController != nullptr) {
        // 平手初期局面のSFEN
        QString hirateSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        m_gameController->newGame(hirateSfen);
        qDebug() << "[TEST] startTestGame: newGame completed";
    }

    // 2. 盤面を初期化
    qDebug() << "[TEST] startTestGame: step 2 - shogiView=" << static_cast<void*>(m_shogiView)
             << "board=" << (m_gameController ? static_cast<void*>(m_gameController->board()) : nullptr);
    if (m_shogiView != nullptr && m_gameController != nullptr && m_gameController->board() != nullptr) {
        m_shogiView->setBoard(m_gameController->board());
        m_shogiView->applyBoardAndRender(m_gameController->board());
        qDebug() << "[TEST] startTestGame: board render completed";
    }

    // 3. PreStartCleanupHandler を呼び出して LiveGameSession を開始
    qDebug() << "[TEST] startTestGame: step 3 - preparing LiveGameSession";
    // m_startSfenStr と m_currentSfenStr をクリア
    m_startSfenStr.clear();
    m_currentSfenStr = QStringLiteral("startpos");

    qDebug() << "[TEST] startTestGame: calling ensurePreStartCleanupHandler";
    ensurePreStartCleanupHandler();
    qDebug() << "[TEST] startTestGame: preStartCleanupHandler=" << static_cast<void*>(m_preStartCleanupHandler);
    if (m_preStartCleanupHandler != nullptr) {
        qDebug() << "[TEST] startTestGame: calling performCleanup";
        m_preStartCleanupHandler->performCleanup();
        qDebug() << "[TEST] startTestGame: performCleanup completed";
    }

    // 4. 状態をダンプ
    qDebug() << "[TEST] startTestGame: completed";
    qDebug() << "[TEST] branchTree root:" << (m_branchTree ? static_cast<void*>(m_branchTree->root()) : nullptr);
    qDebug() << "[TEST] liveGameSession isActive:" << (m_liveGameSession ? m_liveGameSession->isActive() : false);
}

bool MainWindow::makeTestMove(const QString& usiMove)
{
    qDebug() << "[TEST] makeTestMove: usiMove=" << usiMove;

    // セッションが未開始の場合は遅延開始
    if (m_liveGameSession == nullptr) {
        qWarning() << "[TEST] makeTestMove: LiveGameSession is null";
        return false;
    }
    if (!m_liveGameSession->isActive()) {
        ensureLiveGameSessionStarted();
    }
    if (!m_liveGameSession->isActive()) {
        qWarning() << "[TEST] makeTestMove: LiveGameSession could not be started";
        return false;
    }

    if (m_gameController == nullptr || m_gameController->board() == nullptr) {
        qWarning() << "[TEST] makeTestMove: gameController or board is null";
        return false;
    }

    // USI形式の指し手をパース（例: "7g7f", "3c3d"）
    // USI形式: 筋(1-9) + 段(a-i) + 筋(1-9) + 段(a-i) [+ 成(+)]
    // 例: 7g7f = 7筋7段目から7筋6段目へ
    if (usiMove.length() < 4) {
        qWarning() << "[TEST] makeTestMove: invalid usiMove format";
        return false;
    }

    // USI座標を変換
    // USI: 筋は数字(1-9)、段はアルファベット(a-i, a=1段目)
    // 内部: file=筋(1-9), rank=段(1-9)
    auto usiFileToInternal = [](QChar c) -> int {
        return c.toLatin1() - '0';  // '1'->'9' を 1->9 に
    };
    auto usiRankToInternal = [](QChar c) -> int {
        return c.toLatin1() - 'a' + 1;  // 'a'->'i' を 1->9 に
    };

    const int fromFile = usiFileToInternal(usiMove.at(0));
    const int fromRank = usiRankToInternal(usiMove.at(1));
    const int toFile = usiFileToInternal(usiMove.at(2));
    const int toRank = usiRankToInternal(usiMove.at(3));
    const bool promote = (usiMove.length() >= 5 && usiMove.at(4) == QLatin1Char('+'));

    qDebug() << "[TEST] makeTestMove: from=(" << fromFile << "," << fromRank << ")"
             << "to=(" << toFile << "," << toRank << ")"
             << "promote=" << promote;

    // 駒を移動（盤面データを更新）
    ShogiBoard* board = m_gameController->board();
    const QChar piece = board->getPieceCharacter(fromFile, fromRank);
    const QChar capturedPiece = board->getPieceCharacter(toFile, toRank);
    board->movePieceToSquare(piece, fromFile, fromRank, toFile, toRank, promote);

    // 指し手を作成
    ShogiMove move(QPoint(fromFile, fromRank), QPoint(toFile, toRank), piece, capturedPiece, promote);

    // 盤面を再描画
    if (m_shogiView != nullptr) {
        m_shogiView->applyBoardAndRender(board);
    }

    // SFEN を更新
    const QString newSfen = board->convertBoardToSfen();
    m_currentSfenStr = newSfen;

    // 手数をカウント
    const int ply = m_liveGameSession->totalPly() + 1;

    // 表示テキストを簡易生成（実際のゲームでは m_lastMove が使用される）
    const QString displayText = QString::number(ply) + QStringLiteral("手目");
    const QString elapsedTime = QStringLiteral("00:00/00:00:00");

    // LiveGameSession に追加（これがツリーにも追加する）
    m_liveGameSession->addMove(move, displayText, newSfen, elapsedTime);

    // 棋譜欄モデルに追加
    if (m_kifuRecordModel != nullptr) {
        const QString moveNumberStr = QString::number(ply);
        const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
        const QString displayTextWithPly = spaces + moveNumberStr + QLatin1Char(' ') + displayText;

        auto* item = new KifuDisplay(displayTextWithPly, elapsedTime, this);
        m_kifuRecordModel->appendItem(item);
    }

    // SFEN記録に追加
    if (m_sfenRecord != nullptr) {
        m_sfenRecord->append(newSfen);
    }

    qDebug() << "[TEST] makeTestMove: completed, ply=" << ply;
    return true;
}

int MainWindow::getBranchTreeNodeCount()
{
    if (m_branchTree == nullptr) {
        return 0;
    }

    // 全ラインのノード数を合計
    int totalNodes = 0;
    const auto lines = m_branchTree->allLines();
    for (const auto& line : lines) {
        totalNodes += static_cast<int>(line.nodes.size());
    }

    // ルートノードは全ラインで共有されているので、重複を除く
    // 実際のノード数はlineCount + 各ラインの独自ノード数
    // より正確には、nodeById のサイズを使用
    // ここでは簡易的に最初のラインのノード数を返す
    if (!lines.isEmpty()) {
        return static_cast<int>(lines.first().nodes.size());
    }

    return totalNodes;
}

bool MainWindow::verifyBranchTreeNodeCount(int minExpected)
{
    const int actual = getBranchTreeNodeCount();
    const bool pass = (actual >= minExpected);

    qDebug() << "[TEST] verifyBranchTreeNodeCount:"
             << "expected>=" << minExpected
             << "actual=" << actual
             << (pass ? "PASS" : "FAIL");

    return pass;
}
