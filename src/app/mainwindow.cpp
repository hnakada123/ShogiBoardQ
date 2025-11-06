#include <QMessageBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QToolButton>
#include <QWidgetAction>
#include <QMetaType>
#include <QDebug>
#include <QScrollBar>
#include <QPushButton>
#include <functional>

#include "mainwindow.h"
#include "branchwiringcoordinator.h"
#include "promotedialog.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"
#include "ui_mainwindow.h"
#include "engineregistrationdialog.h"
#include "versiondialog.h"
#include "usi.h"
#include "kifuanalysisdialog.h"
#include "shogiclock.h"
#include "apptooltipfilter.h"
#include "sfenpositiontracer.h"
#include "enginesettingsconstants.h"
#include "navigationcontroller.h"
#include "boardimageexporter.h"
#include "engineinfowidget.h"
#include "engineanalysistab.h"
#include "matchcoordinator.h"
#include "kifuvariationengine.h"
#include "branchcandidatescontroller.h"
#include "turnmanager.h"
#include "errorbus.h"
#include "kifuloadcoordinator.h"
#include "evaluationchartwidget.h"
#include "timekeepingservice.h"
#include "kifuioservice.h"
#include "positioneditcontroller.h"
#include "playernameservice.h"
#include "analysisresultspresenter.h"
#include "boardsyncpresenter.h"
#include "gamestartcoordinator.h"
#include "navigationpresenter.h"
#include "analysiscoordinator.h"
#include "kifuanalysislistmodel.h"
#include "analysiscoordinator.h"
#include "recordpresenter.h"
#include "tsumepositionutil.h"
#include "timecontrolutil.h"
#include "analysisflowcontroller.h"
#include "sfenutils.h"

using KifuIoService::makeDefaultSaveFileName;
using KifuIoService::writeKifuFile;
using namespace EngineSettingsConstants;
using GameOverCause = MatchCoordinator::Cause;
using std::placeholders::_1;
using std::placeholders::_2;

static inline GameOverCause toUiCause(MatchCoordinator::Cause c) { return c; }

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_startSfenStr(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"))
    , m_currentSfenStr(QStringLiteral("startpos"))
    , m_errorOccurred(false)
    , m_usi1(nullptr)
    , m_usi2(nullptr)
    , m_playMode(NotStarted)
    , m_lineEditModel1(new UsiCommLogModel(this))
    , m_lineEditModel2(new UsiCommLogModel(this))
    , m_gameController(nullptr)
    , m_analysisModel(nullptr)
    , m_anaCoord(nullptr)
{
    ui->setupUi(this);

    setupCentralWidgetContainer_();

    configureToolBarFromUi_();

    // コア部品（GC, View, 盤モデル etc.）は既存関数で初期化
    initializeComponents();

    if (!m_timePresenter) m_timePresenter = new TimeDisplayPresenter(m_shogiView, this);

    // 画面骨格（棋譜/分岐/レイアウト/タブ/中央表示）
    buildGamePanels_();

    // ウィンドウ設定の復元（位置/サイズなど）
    restoreWindowAndSync_();

    // メニュー/アクションのconnect（関数ポインタで統一）
    connectAllActions_();

    // コアシグナル（昇格ダイアログ・エラー・ドラッグ終了・指し手確定 等）
    connectCoreSignals_();

    // ツールチップをコンパクト表示へ
    installAppToolTips_();

    // 司令塔やUIフォント/位置編集コントローラの最終初期化
    finalizeCoordinators_();
}

void MainWindow::setupCentralWidgetContainer_()
{
    m_central = ui->centralwidget;
    m_centralLayout = new QVBoxLayout(m_central);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);
}

void MainWindow::configureToolBarFromUi_()
{
    if (QToolBar* tb = ui->toolBar) {
        tb->setIconSize(QSize(18, 18)); // お好みで 16/18/20/24
        tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
        tb->setStyleSheet(
            "QToolBar{margin:0px; padding:0px; spacing:2px;}"
            "QToolButton{margin:0px; padding:2px;}"
            );
    }
}

void MainWindow::buildGamePanels_()
{
    // 1) 記録ペイン（RecordPane）など UI 部の初期化
    setupRecordPane();

    // 2) 分岐配線をコーディネータに集約（旧: setupBranchCandidatesWiring_()）
    //    既存があれば入れ替え（多重接続を防ぐ）
    if (m_branchWiring) {
        m_branchWiring->deleteLater();
        m_branchWiring = nullptr;
    }
    if (m_recordPane) {
        BranchWiringCoordinator::Deps bw;
        bw.recordPane      = m_recordPane;
        bw.branchModel     = m_kifuBranchModel;          // 既に保持していれば渡す（null可）
        bw.variationEngine = m_varEngine.get();          // unique_ptr想定
        bw.kifuLoader      = m_kifuLoadCoordinator;      // 読み込み済みなら渡す（null可）
        bw.parent          = this;

        m_branchWiring = new BranchWiringCoordinator(bw);
        m_branchWiring->setupBranchView();
        m_branchWiring->setupBranchCandidatesWiring();
    } else {
        qWarning() << "[UI] buildGamePanels_: RecordPane is null; skip branch wiring.";
    }

    // 3) 将棋盤・駒台の初期化（従来順序を維持）
    startNewShogiGame(m_startSfenStr);

    // 4) 盤＋各パネルの横並びレイアウト構築
    setupHorizontalGameLayout();

    // 5) エンジン解析タブの構築
    setupEngineAnalysisTab();

    // 6) central へのタブ追加など表示側の初期化
    initializeCentralGameDisplay();
}


void MainWindow::restoreWindowAndSync_()
{
    loadWindowSettings();
    // ensureTurnSyncBridge_(); // ← initializeComponents() 内で済ませる方針のためここでは呼ばない
}

void MainWindow::connectAllActions_()
{
    // ファイル/アプリ
    connect(ui->actionQuit,                 &QAction::triggered, this, &MainWindow::saveSettingsAndClose);
    connect(ui->actionSaveAs,               &QAction::triggered, this, &MainWindow::saveKifuToFile);
    connect(ui->actionSave,                 &QAction::triggered, this, &MainWindow::overwriteKifuFile);
    connect(ui->actionOpenKifuFile,         &QAction::triggered, this, &MainWindow::chooseAndLoadKifuFile);
    connect(ui->actionVersionInfo,          &QAction::triggered, this, &MainWindow::displayVersionInformation);
    connect(ui->actionOpenWebsite,          &QAction::triggered, this, &MainWindow::openWebsiteInExternalBrowser);

    // 対局
    connect(ui->actionNewGame,              &QAction::triggered, this, &MainWindow::resetToInitialState);
    connect(ui->actionStartGame,            &QAction::triggered, this, &MainWindow::initializeGame);
    connect(ui->actionResign,               &QAction::triggered, this, &MainWindow::handleResignation);
    connect(ui->breakOffGame,               &QAction::triggered, this, &MainWindow::handleBreakOffGame);

    // 盤操作・表示
    connect(ui->actionFlipBoard,            &QAction::triggered, this, &MainWindow::onActionFlipBoardTriggered);
    connect(ui->actionCopyBoardToClipboard, &QAction::triggered, this, &MainWindow::copyBoardToClipboard);
    connect(ui->actionMakeImmediateMove,    &QAction::triggered, this, &MainWindow::movePieceImmediately);
    connect(ui->actionEnlargeBoard,         &QAction::triggered, this, &MainWindow::enlargeBoard);
    connect(ui->actionShrinkBoard,          &QAction::triggered, this, &MainWindow::reduceBoardSize);
    connect(ui->actionUndoMove,             &QAction::triggered, this, &MainWindow::undoLastTwoMoves);
    connect(ui->actionSaveBoardImage,       &QAction::triggered, this, &MainWindow::saveShogiBoardImage);

    // 解析/検討/詰み・エンジン設定
    connect(ui->actionToggleEngineAnalysis, &QAction::triggered, this, &MainWindow::toggleEngineAnalysisVisibility);
    connect(ui->actionEngineSettings,       &QAction::triggered, this, &MainWindow::displayEngineSettingsDialog);
    connect(ui->actionConsideration,        &QAction::triggered, this, &MainWindow::displayConsiderationDialog);
    connect(ui->actionAnalyzeKifu,          &QAction::triggered, this, &MainWindow::displayKifuAnalysisDialog);
    connect(ui->actionStartEditPosition,    &QAction::triggered, this, &MainWindow::beginPositionEditing);
    connect(ui->actionEndEditPosition,      &QAction::triggered, this, &MainWindow::finishPositionEditing);
    connect(ui->actionTsumeShogiSearch,     &QAction::triggered, this, &MainWindow::displayTsumeShogiSearchDialog);
    connect(ui->actionQuitEngine,           &QAction::triggered, this, &MainWindow::handleBreakOffConsidaration);
}

void MainWindow::connectCoreSignals_()
{
    // 将棋盤表示・昇格・ドラッグ終了・指し手確定
    if (m_gameController) {
        connect(m_gameController, &ShogiGameController::showPromotionDialog,
                this, &MainWindow::displayPromotionDialog, Qt::UniqueConnection);
        connect(m_gameController, &ShogiGameController::endDragSignal,
                this, &MainWindow::endDrag, Qt::UniqueConnection);
        connect(m_gameController, &ShogiGameController::moveCommitted,
                this, &MainWindow::onMoveCommitted, Qt::UniqueConnection);
    }
    if (m_shogiView) {
        connect(m_shogiView, &ShogiView::errorOccurred,
                this, &MainWindow::displayErrorMessage, Qt::UniqueConnection);
    }

    // ErrorBus はラムダを使わず専用スロットへ
    connect(&ErrorBus::instance(), &ErrorBus::errorOccurred,
            this, &MainWindow::onErrorBusOccurred, Qt::UniqueConnection);
}

void MainWindow::installAppToolTips_()
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

void MainWindow::finalizeCoordinators_()
{
    initMatchCoordinator();
    setupNameAndClockFonts_();
    ensurePositionEditController_();
    ensureBoardSyncPresenter_();
    ensureAnalysisPresenter_();
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
    if (!m_sfenRecord) m_sfenRecord = new QStringList;
    else               m_sfenRecord->clear();

    // 棋譜表示用のレコードリストを確保（ここでは容器だけ用意）
    if (!m_moveRecords) m_moveRecords = new QList<KifuDisplay *>;
    else                m_moveRecords->clear();

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
    ensureTurnSyncBridge_();

    // ───────────────── 表示名・ログモデル名の初期反映（任意だが初期表示を安定化） ─────────────────
    // setPlayersNamesForMode / setEngineNamesBasedOnMode がサービスへ移設済みでも呼び出し名は同じ
    setPlayersNamesForMode();
    setEngineNamesBasedOnMode();
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

// 「表示」の「思考」 思考タブの表示・非表示
void MainWindow::toggleEngineAnalysisVisibility()
{
    if (!m_analysisTab) return;
    m_analysisTab->setAnalysisVisible(ui->actionToggleEngineAnalysis->isChecked());
}

// 待ったボタンを押すと、2手戻る。
void MainWindow::undoLastTwoMoves()
{
    if (m_match) {
        m_match->undoTwoPlies();
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
    const PlayerNameMapping names =
        PlayerNameService::computePlayers(m_playMode, m_humanName1, m_humanName2, m_engineName1, m_engineName2);

    // 対局者名をセットする。
    m_shogiView->setBlackPlayerName(names.p1);
    m_shogiView->setWhitePlayerName(names.p2);
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
    const EngineNameMapping e =
        PlayerNameService::computeEngineModels(m_playMode, m_engineName1, m_engineName2);

    if (m_lineEditModel1) m_lineEditModel1->setEngineName(e.model1);
    if (m_lineEditModel2) m_lineEditModel2->setEngineName(e.model2);
}

// 対局者名と残り時間、将棋盤と棋譜、矢印ボタン、評価値グラフのグループを横に並べて表示する。
void MainWindow::setupHorizontalGameLayout()
{
    m_hsplit = new QSplitter(Qt::Horizontal);

    m_hsplit->addWidget(m_shogiView);
    m_hsplit->addWidget(m_gameRecordLayoutWidget);
}

// --- レイアウトの中身だけ入れ替えるヘルパ ---
static void clearLayout(QLayout* lay) {
    while (QLayoutItem* it = lay->takeAt(0)) {
        if (QWidget* w = it->widget()) w->setParent(nullptr);
        delete it;
    }
}

void MainWindow::initializeCentralGameDisplay()
{
    if (!m_centralLayout) return;
    clearLayout(m_centralLayout);

    Q_ASSERT(m_hsplit);
    Q_ASSERT(m_tab);                // ここで早期に気づける

    if (m_hsplit) m_centralLayout->addWidget(m_hsplit);
    if (m_tab)    m_centralLayout->addWidget(m_tab);
}

void MainWindow::startNewShogiGame(QString& startSfenStr)
{
    const bool resume = m_isResumeFromCurrent;

    // --- UI: 評価値グラフと内部スコアの初期化（再開時は消さない） ---
    if (auto ec = m_recordPane ? m_recordPane->evalChart() : nullptr) {
        if (!resume) ec->clearAll();
    }
    if (!resume) {
        m_scoreCp.clear();
    }

    // --- 司令塔を用意 ---
    if (!m_match) {
        initMatchCoordinator();    // m_match を生成・配線
    }
    ensureGameStartCoordinator_(); // m_gameStart を用意
    if (!m_match || !m_gameStart) return;

    // --- まず従来どおり UI 側で盤を即時初期化→描画（★ここが今回の表示不具合の核心） ---
    // ※ 将来的には司令塔の UI フックへ移すが、現時点ではここで確実に盤を見せる
    initializeNewGame(startSfenStr);

    if (m_shogiView && m_gameController && m_gameController->board()) {
        m_shogiView->applyBoardAndRender(m_gameController->board());
        m_shogiView->configureFixedSizing();
    }

    // 対局者名・エンジン名も現時点では UI 層で更新しておく
    setPlayersNamesForMode();
    setEngineNamesBasedOnMode();

    // --- 以降は司令塔へ ---
    GameStartCoordinator::Request req;
    req.mode        = static_cast<int>(m_playMode);
    req.startSfen   = startSfenStr;
    req.bottomIsP1  = m_bottomIsP1;
    req.startDialog = m_startGameDialog;
    req.clock       = m_shogiClock;
    m_gameStart->prepare(req);

    MatchCoordinator::StartOptions opt =
        m_match->buildStartOptions(
            m_playMode,
            startSfenStr,
            m_sfenRecord,
            m_startGameDialog);

    m_match->ensureHumanAtBottomIfApplicable(m_startGameDialog, m_bottomIsP1);

    GameStartCoordinator::StartParams p;
    p.opt                 = opt;
    p.autoStartEngineMove = true;
    m_gameStart->start(p);

    // （将来）司令塔の UI フックが整ったら、上の initializeNewGame/描画/名前更新は外します
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
    if (m_match) m_match->handleResign();
}

// エンジン1の評価値グラフの再描画を行う。
void MainWindow::redrawEngine1EvaluationGraph()
{
    // ★ 司令塔から主エンジンを取得（m_usi1 は使わない）
    Usi* eng = (m_match ? m_match->primaryEngine() : nullptr);

    // エンジンが未初期化でも落ちないようにガード
    const int scoreCp = eng ? eng->lastScoreCp() : 0;

    // 記録も更新（整合のためエンジン未初期化時は 0 を入れておく）
    m_scoreCp.append(scoreCp);
}

// エンジン2の評価値グラフの再描画を行う。
void MainWindow::redrawEngine2EvaluationGraph()
{
    // ★ 司令塔から 2nd エンジンを取得（m_usi2 は使わない）
    Usi* eng2 = (m_match ? m_match->secondaryEngine() : nullptr);

    // エンジン未初期化でも落ちないように 0 を既定値に
    const int scoreCp = eng2 ? eng2->lastScoreCp() : 0;

    // 既存仕様に合わせてそのまま記録（未初期化時は 0 を入れる）
    m_scoreCp.append(scoreCp);
}

// 将棋クロックの手番を設定する。
void MainWindow::updateTurnStatus(int currentPlayer)
{
    if (!m_shogiView) return;

    if (!m_shogiClock) { // 保険
        qWarning() << "ShogiClock not ready yet";
        ensureClockReady_();
        if (!m_shogiClock) return;
    }

    m_shogiClock->setCurrentPlayer(currentPlayer);
    m_shogiView->setActiveSide(currentPlayer == 1);
}

void MainWindow::updateTurnAndTimekeepingDisplay()
{
    TimekeepingService::updateTurnAndTimekeepingDisplay(
        m_shogiClock,
        m_match,
        m_gameController,
        m_isReplayMode,
        // appendElapsedLine:
        [this](const QString& s){ updateGameRecord(s); },
        // updateTurnStatus:
        [this](int p){ updateTurnStatus(p); }
        );
}

// GUIのバージョン情報を表示する。
void MainWindow::displayVersionInformation()
{
    // バージョン情報ダイアログを作成する。
    VersionDialog dialog(this);

    // バージョン情報ダイアログを実行する。
    dialog.exec();
}

// エンジン設定のダイアログを起動する。
void MainWindow::displayEngineSettingsDialog()
{
    // エンジン設定ダイアログを作成する。
    EngineRegistrationDialog dialog(this);

    // エンジン設定ダイアログを実行する。
    dialog.exec();
}

// 成る・不成の選択ダイアログを起動する。
void MainWindow::displayPromotionDialog()
{
    // 成る・不成の選択ダイアログを作成する。
    PromoteDialog dialog(this);

    // 成る・不成の選択ダイアログを実行し、成るを選択した場合
    if (dialog.exec() == QDialog::Accepted) {
        // 成るのフラグをセットする。
        m_gameController->setPromote(true);
    }
    // 不成を選択した場合
    else {
        // 不成のフラグをセットする。
        m_gameController->setPromote(false);
    }
}

// ドラッグを終了する。駒を移動してカーソルを戻す。
void MainWindow::endDrag()
{
    // ドラッグを終了する。
    m_shogiView->endDrag();
}

// Webサイトをブラウザで表示する。
void MainWindow::openWebsiteInExternalBrowser()
{
    QDesktopServices::openUrl(QUrl("https://github.com/hnakada123/ShogiBoardQ"));
}

void MainWindow::displayAnalysisResults()
{
    if (m_analysisModel) {
        delete m_analysisModel;
        m_analysisModel = nullptr;
    }
    m_analysisModel = new KifuAnalysisListModel(this);

    ensureAnalysisPresenter_();
    m_analysisPresenter->showWithModel(m_analysisModel);

    // 互換性のために保持（スクロール追従等）
    m_analysisResultsView = m_analysisPresenter->view();
}

// 検討ダイアログを表示する。
void MainWindow::displayConsiderationDialog()
{
    // 検討ダイアログを生成する。
    m_considerationDialog = new ConsiderationDialog(this);

    // 検討ダイアログを表示後にユーザーがOKボタンを押した場合
    if (m_considerationDialog->exec() == QDialog::Accepted) {
        // 検討を開始する。
        startConsidaration();
    }

    // 検討ダイアログを削除する。
    delete m_considerationDialog;
}

// 詰み探索ダイアログを表示する。
void MainWindow::displayTsumeShogiSearchDialog()
{
    // 詰み探索ダイアログを生成する。
    m_tsumeShogiSearchDialog = new TsumeShogiSearchDialog(this);

    // 詰み探索ダイアログを表示後にユーザーがOKボタンを押した場合
    if (m_tsumeShogiSearchDialog->exec() == QDialog::Accepted) {
        // 詰み探索を開始する。
        startTsumiSearch();
    }

    // 詰み探索ダイアログを削除する。
    delete m_tsumeShogiSearchDialog;
}

// 棋譜解析ダイアログを表示する。
void MainWindow::displayKifuAnalysisDialog()
{
    m_analyzeGameRecordDialog = new KifuAnalysisDialog(this);

    const int result = m_analyzeGameRecordDialog->exec();
    if (result == QDialog::Accepted) {
        // 解析結果の受け皿（モデル/ビュー）を準備
        displayAnalysisResults();

        // 実行（以降の“回し方／投入”は AnalysisCoordinator 側）
        analyzeGameRecord();
    }

    delete m_analyzeGameRecordDialog;
    m_analyzeGameRecordDialog = nullptr;
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

// 対局を開始する。
void MainWindow::initializeGame()
{
    ensureGameStartCoordinator_();

    GameStartCoordinator::Ctx c;
    c.view            = m_shogiView;
    c.gc              = m_gameController;
    c.clock           = m_shogiClock;
    c.sfenRecord      = m_sfenRecord;           // QStringList*
    c.currentSfenStr  = &m_currentSfenStr;       // 現局面の SFEN
    c.startSfenStr    = &m_startSfenStr;         // 既定開始 SFEN（空でも可）
    c.selectedPly     = m_currentSelectedPly;   // 必要なら GameStart 側で利用
    c.isReplayMode    = m_isReplayMode;
    c.bottomIsP1 = m_bottomIsP1;

    // 実処理は GameStartCoordinator 側で完結
    m_gameStart->initializeGame(c);
}

// 設定ファイルにGUI全体のウィンドウサイズを書き込む。
// また、将棋盤のマスサイズも書き込む。その後、GUIを終了する。
void MainWindow::saveSettingsAndClose()
{
    // 設定ファイルにGUIQ全体のウィンドウサイズを書き込む。
    // また、将棋盤のマスサイズも書き込む。
    saveWindowAndBoardSettings();

    // GUIを終了する。
    QCoreApplication::quit();
}

// GUIを初期画面表示に戻す。
void MainWindow::resetToInitialState()
{
    // ハイライト消去はコントローラ経由に統一
    if (m_boardController)
        m_boardController->clearAllHighlights();

    if (m_shogiView) {
        m_shogiView->blackClockLabel()->setText("00:00:00");
        m_shogiView->whiteClockLabel()->setText("00:00:00");
    }

    // 棋譜と評価値
    if (m_kifuRecordModel) m_kifuRecordModel->clearAllItems();

    // 評価値チャートは RecordPane が所有。毎回ゲッターで取得してクリア
    if (auto* ec = (m_recordPane ? m_recordPane->evalChart() : nullptr)) {
        ec->clearAll();
    }

    // 盤面リセット
    startNewShogiGame(m_startSfenStr);

    // 思考モデルはモデル側をクリア（ビューは EngineAnalysisTab 内）
    if (m_modelThinking1) m_modelThinking1->clearAllItems();
    if (m_modelThinking2) m_modelThinking2->clearAllItems();

    // EngineInfoWidget に反映されるプロパティもモデル側で空に
    auto resetInfo = [](UsiCommLogModel* m){
        if (!m) return;
        m->setEngineName(QString());
        m->setPredictiveMove(QString());
        m->setSearchedMove(QString());
        m->setSearchDepth(QString());
        m->setNodeCount(QString());
        m->setNodesPerSecond(QString());
        m->setHashUsage(QString());
        // ※ USIログ表示だけを消したいなら EngineAnalysisTab に clearUsiLog() を追加するのが綺麗
    };
    resetInfo(m_lineEditModel1);
    resetInfo(m_lineEditModel2);

    // タブの選択を先頭へ戻す（任意）
    if (m_tab) m_tab->setCurrentIndex(0);
}

// 棋譜ファイルをダイアログから選択し、そのファイルを開く。
void MainWindow::chooseAndLoadKifuFile()
{
    // --- 1) ファイル選択（UI層に残す） ---
    const QString filePath =
        QFileDialog::getOpenFileName(this, tr("KIFファイルを開く"), QString(), tr("KIF Files (*.kif *.kifu)"));
    if (filePath.isEmpty()) return;

    setReplayMode(true);
    ensureGameInfoTable();

    // 既存があれば破棄予約（多重生成対策）
    if (m_kifuLoadCoordinator) {
        m_kifuLoadCoordinator->deleteLater();
        m_kifuLoadCoordinator = nullptr;
    }

    // --- 2) 読み込み系の配線と依存は Coordinator に集約 ---
    m_kifuLoadCoordinator = new KifuLoadCoordinator(
        /* gameMoves           */ m_gameMoves,
        /* resolvedRows        */ m_resolvedRows,
        /* positionStrList     */ m_positionStrList,
        /* activeResolvedRow   */ m_activeResolvedRow,
        /* activePly           */ m_activePly,
        /* currentSelectedPly  */ m_currentSelectedPly,
        /* currentMoveIndex    */ m_currentMoveIndex,
        /* sfenRecord          */ m_sfenRecord,
        /* gameInfoTable       */ m_gameInfoTable,
        /* gameInfoDock        */ m_gameInfoDock,
        /* analysisTab         */ m_analysisTab,
        /* tab                 */ m_tab,
        /* shogiView           */ m_shogiView,
        /* recordPane          */ m_recordPane,
        /* kifuRecordModel     */ m_kifuRecordModel,
        /* kifuBranchModel     */ m_kifuBranchModel,
        /* branchCtl           */ m_branchCtl,
        /* kifuBranchView      */ m_kifuBranchView,
        /* branchDisplayPlan   */ m_branchDisplayPlan,
        /* parent              */ this
        );

    // ★ MainWindow 側でやっていた branchNode 配線は setAnalysisTab() に委譲
    //   （内部で disconnect / connect を一貫管理）
    m_kifuLoadCoordinator->setAnalysisTab(m_analysisTab);

    // --- 3) Coordinator -> MainWindow の通知（UI更新）は従来どおり受ける ---
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::displayGameRecord,
            this, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
            this, &MainWindow::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::enableArrowButtons,
            this, &MainWindow::enableArrowButtons, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::setupBranchCandidatesWiring_,
            this, &MainWindow::setupBranchCandidatesWiring_, Qt::UniqueConnection);

    // --- 4) 読み込み実行（ロジックは Coordinator へ） ---
    m_kifuLoadCoordinator->loadKifuFromFile(filePath);
}

void MainWindow::rebuildSfenRecord(const QString& initialSfen,
                                   const QStringList& usiMoves,
                                   bool hasTerminal)
{
    const QStringList list = SfenPositionTracer::buildSfenRecord(initialSfen, usiMoves, hasTerminal);
    if (!m_sfenRecord) m_sfenRecord = new QStringList;
    *m_sfenRecord = list; // COW
}

void MainWindow::rebuildGameMoves(const QString& initialSfen,
                                  const QStringList& usiMoves)
{
    m_gameMoves = SfenPositionTracer::buildGameMoves(initialSfen, usiMoves);
}

void MainWindow::displayGameRecord(const QList<KifDisplayItem> disp)
{
    if (!m_kifuRecordModel) return;

    ensureRecordPresenter_();
    if (!m_recordPresenter) return;

    const int moveCount = disp.size();
    const int rowCount  = (m_sfenRecord && !m_sfenRecord->isEmpty())
                             ? m_sfenRecord->size()
                             : (moveCount + 1);

    // ← まとめて Presenter 側に委譲
    m_recordPresenter->displayAndWire(disp, rowCount, m_recordPane);
}

// 置き換え：1手追記も Presenter に一本化
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    const bool gameOverAppended =
        (m_match && m_match->gameOverState().isOver && m_match->gameOverState().moveAppended);
    if (gameOverAppended) return;

    ensureRecordPresenter_();
    if (m_recordPresenter) {
        m_recordPresenter->appendMoveLine(m_lastMove, elapsedTime);
    }

    // 二重追記防止（従来どおり）
    m_lastMove.clear();
}

// 検討を開始する（司令塔へ依頼する形に集約）
void MainWindow::startConsidaration()
{
    // 1) 対局モードを検討モードへ（UI の状態保持用）
    m_playMode = ConsidarationMode;

    // 2) 盤面の手番表示用に現在手番を更新（従来コードを踏襲）
    //    ※ positionStr に手番情報は含まれるが、UI の手番表示は GameController にも反映しておく
    if (m_gameMoves.at(m_currentMoveIndex).movingPiece.isUpper()) {
        m_gameController->setCurrentPlayer(ShogiGameController::Player1);
    } else {
        m_gameController->setCurrentPlayer(ShogiGameController::Player2);
    }

    // 3) 送信用 position 構築（従来のリストをそのまま利用）
    m_positionStr1 = m_positionStrList.at(m_currentMoveIndex);

    // 4) ダイアログからエンジン/時間設定を取得
    const int engineNumber = m_considerationDialog->getEngineNumber();
    const auto engine      = m_considerationDialog->getEngineList().at(engineNumber);

    int byoyomiMs = 0;
    if (!m_considerationDialog->unlimitedTimeFlag()) {
        byoyomiMs = m_considerationDialog->getByoyomiSec() * 1000; // 秒→ms
    }

    // 5) 司令塔に検討を依頼
    if (m_match) {
        MatchCoordinator::AnalysisOptions opt;
        opt.enginePath  = engine.path;
        opt.engineName  = m_considerationDialog->getEngineName();
        opt.positionStr = m_positionStr1;
        opt.byoyomiMs   = byoyomiMs;
        opt.mode        = ConsidarationMode;

        m_match->startAnalysis(opt);
    }
}

// 関数差し替え
void MainWindow::startTsumiSearch()
{
    m_playMode = TsumiSearchMode;
    if (!m_tsumeShogiSearchDialog) return;

    // 局面の決定（ユーティリティへ委譲）
    const QString pos = TsumePositionUtil::buildPositionForMate(
        m_sfenRecord, m_startSfenStr, m_positionStrList, qMax(0, m_currentMoveIndex));
    if (pos.isEmpty()) {
        displayErrorMessage(u8"詰み探索用の局面（SFEN）が取得できません。棋譜を読み込むか局面を指定してください。");
        return;
    }
    m_positionStr1 = pos; // 既存のフィールド互換

    // エンジンと byoyomi
    const auto engines = m_tsumeShogiSearchDialog->getEngineList();
    const int engineIndex = m_tsumeShogiSearchDialog->getEngineNumber();
    if (engineIndex < 0 || engineIndex >= engines.size()) {
        displayErrorMessage(u8"エンジン選択が不正です。");
        return;
    }
    const auto engine   = engines.at(engineIndex);
    const int  byoyomiMs = m_tsumeShogiSearchDialog->getByoyomiSec() * 1000;

    // 司令塔へ（go mate）
    if (m_match) {
        MatchCoordinator::AnalysisOptions opt;
        opt.enginePath  = engine.path;
        opt.engineName  = m_tsumeShogiSearchDialog->getEngineName();
        opt.positionStr = m_positionStr1;
        opt.byoyomiMs   = byoyomiMs;
        opt.mode        = TsumiSearchMode;
        m_match->startAnalysis(opt);
    }
}

void MainWindow::analyzeGameRecord()
{
    m_playMode = AnalysisMode;
    if (!m_analyzeGameRecordDialog) return;

    if (!m_analysisFlow) {
        m_analysisFlow = new AnalysisFlowController(this);
    }

    AnalysisFlowController::Deps d;
    d.sfenRecord    = m_sfenRecord;
    d.moveRecords   = m_moveRecords;
    d.analysisModel = m_analysisModel;
    d.analysisTab   = m_analysisTab;
    d.usi           = m_usi1;
    d.logModel      = m_lineEditModel1;     // ← 使うUSIに合わせて適切なログモデルを
    d.activePly     = m_activePly;

    // ラムダでもOKですが、方針に寄せるなら std::bind でも可
    d.displayError  = std::bind(&MainWindow::displayErrorMessage, this, std::placeholders::_1);

    m_analysisFlow->start(d, m_analyzeGameRecordDialog);
}

// 設定ファイルにGUI全体のウィンドウサイズを書き込む。
// また、将棋盤のマスサイズも書き込む。
void MainWindow::saveWindowAndBoardSettings()
{
    // 実行パスをプログラムと同じディレクトリに設定する。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 書き込む設定ファイルの指定
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // 書き込むグループをSizeRelatedに指定
    settings.beginGroup("SizeRelated");

    // ウィンドウサイズを書き込む。
    settings.setValue("mainWindowSize", this->size());

    // 将棋盤のマスサイズ
    int squareSize = m_shogiView->squareSize();

    // 将棋盤のマスサイズを書き込む。
    settings.setValue("squareSize", squareSize);

    // グループの終了
    settings.endGroup();
}

// ウィンドウを閉じる処理を行う。
void MainWindow::closeEvent(QCloseEvent* event)
{
    // 設定ファイルにGUI全体のウィンドウサイズを書き込む。
    // また、将棋盤のマスサイズも書き込む。
    saveWindowAndBoardSettings();

    // ウィンドウを閉じる。
    QMainWindow::closeEvent(event);
}

// GUI全体のウィンドウサイズを読み込む。
// 前回起動したウィンドウサイズに設定する。
void MainWindow::loadWindowSettings()
{
    // 実行パスをShogiBoardQと同じディレクトリに設定する。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルShogiBoardQ.iniを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // メインウィンドウのリサイズを行う。  
    const QSize sz = settings.value("SizeRelated/mainWindowSize", QSize(1100, 720)).toSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100) resize(sz);
}

void MainWindow::onReverseTriggered()
{
    if (m_match) m_match->flipBoard();
}

void MainWindow::beginPositionEditing()
{
    ensurePositionEditController_();
    if (!m_posEdit || !m_shogiView || !m_gameController) return;

    PositionEditController::BeginEditContext ctx;
    ctx.view       = m_shogiView;
    ctx.gc         = m_gameController;
    ctx.bic        = m_boardController;
    ctx.sfenRecord = m_sfenRecord ? m_sfenRecord : nullptr;

    ctx.selectedPly = m_currentSelectedPly;
    ctx.activePly   = m_activePly;
    ctx.gameOver    = (m_match ? m_match->gameOverState().isOver : false);

    ctx.startSfenStr    = &m_startSfenStr;
    ctx.currentSfenStr  = &m_currentSfenStr;
    ctx.resumeSfenStr   = &m_resumeSfenStr;

    // メニュー表示（Controller → callback）
    ctx.onEnterEditMenu = [this]() {
        if (!ui) return;
        ui->actionStartEditPosition->setVisible(false);
        ui->actionEndEditPosition->setVisible(true);
        ui->flatHandInitialPosition->setVisible(true);
        ui->returnAllPiecesOnStand->setVisible(true);
        ui->reversal->setVisible(true);
        ui->shogiProblemInitialPosition->setVisible(true);
        ui->turnaround->setVisible(true);
    };

    // 「編集終了」ボタン表示（Controller API 経由）
    ctx.onShowEditExitButton = [this]() {
        if (m_posEdit && m_shogiView) {
            m_posEdit->showEditExitButtonOnBoard(m_shogiView, this, SLOT(finishPositionEditing()));
        }
    };

    // 実行（盤と文字列状態の同期等は Controller が担当）
    m_posEdit->beginPositionEditing(ctx);

    // ── 編集用アクション配線（ラムダ無し・重複防止） ─────────────
    if (ui) {
        // ★ Controller のスロットに接続するよう変更（MainWindow側の同名スロットは削除）
        connect(ui->returnAllPiecesOnStand,      &QAction::triggered,
                m_posEdit, &PositionEditController::onReturnAllPiecesOnStandTriggered,
                Qt::UniqueConnection);

        connect(ui->flatHandInitialPosition,     &QAction::triggered,
                m_posEdit, &PositionEditController::onFlatHandInitialPositionTriggered,
                Qt::UniqueConnection);

        connect(ui->shogiProblemInitialPosition, &QAction::triggered,
                m_posEdit, &PositionEditController::onShogiProblemInitialPositionTriggered,
                Qt::UniqueConnection);

        connect(ui->turnaround,                  &QAction::triggered,
                m_posEdit, &PositionEditController::onToggleSideToMoveTriggered,
                Qt::UniqueConnection);

        // 先後反転は司令塔の責務のまま（MatchCoordinator操作）
        connect(ui->reversal,                    &QAction::triggered,
                this,     &MainWindow::onReverseTriggered,
                Qt::UniqueConnection);
    }
}

void MainWindow::finishPositionEditing()
{
    // --- A) 自動同期を一時停止（ここが肝） ---
    const bool prevGuard = m_onMainRowGuard;
    m_onMainRowGuard = true;

    ensurePositionEditController_();
    if (!m_posEdit || !m_shogiView || !m_gameController) {
        m_onMainRowGuard = prevGuard;
        return;
    }

    // Controller に委譲して SFEN の確定・sfenRecord/開始SFEN 更新・UI 後片付けを実施
    PositionEditController::FinishEditContext ctx;
    ctx.view       = m_shogiView;
    ctx.gc         = m_gameController;
    ctx.bic        = m_boardController;
    ctx.sfenRecord = m_sfenRecord ? m_sfenRecord : nullptr;
    ctx.startSfenStr       = &m_startSfenStr;
    ctx.isResumeFromCurrent = &m_isResumeFromCurrent;

    // 「編集終了」ボタンの後片付け（Controller → callback）
    ctx.onHideEditExitButton = [this]() {
        if (m_posEdit && m_shogiView) {
            m_posEdit->hideEditExitButtonOnBoard(m_shogiView);
        }
    };

    // メニューを元に戻す（Controller → callback）
    ctx.onLeaveEditMenu = [this]() {
        if (!ui) return;
        ui->actionStartEditPosition->setVisible(true);
        ui->actionEndEditPosition->setVisible(false);
        ui->flatHandInitialPosition->setVisible(false);
        ui->shogiProblemInitialPosition->setVisible(false);
        ui->returnAllPiecesOnStand->setVisible(false);
        ui->reversal->setVisible(false);
        ui->turnaround->setVisible(false);
    };

    // 実行
    m_posEdit->finishPositionEditing(ctx);

    // ★ 冗長な編集モード解除は Controller 側で完了済みなので、ここでは行わない

    // --- D) 自動同期を再開 ---
    m_onMainRowGuard = prevGuard;

    qDebug() << "[EDIT-END] flags: editMode="
             << (m_shogiView ? m_shogiView->positionEditMode() : false)
             << " guard=" << m_onMainRowGuard
             << " m_startSfenStr=" << m_startSfenStr;
}

// 盤面のサイズを拡大する。
void MainWindow::enlargeBoard()
{
    m_shogiView->enlargeBoard();
}

// 盤面のサイズを縮小する。
void MainWindow::reduceBoardSize()
{
    m_shogiView->reduceBoard();
}

// 「すぐ指させる」
void MainWindow::movePieceImmediately()
{
    if (m_match) {
        m_match->forceImmediateMove();
    }
}

// 先手が時間切れ → 先手敗北
void MainWindow::onPlayer1TimeOut()
{
    if (m_match) m_match->notifyTimeout(MatchCoordinator::P1);
}

// 後手が時間切れ → 後手敗北
void MainWindow::onPlayer2TimeOut()
{
    if (m_match) m_match->notifyTimeout(MatchCoordinator::P2);
}

// 表示用の▲/△を返す（既存 setResignationMove の分岐をそのまま一般化）
QChar MainWindow::glyphForPlayer(bool isPlayerOne) const
{
    // 棋譜用は絶対座席で固定（UIの flip は無視）
    return isPlayerOne ? QChar(u'▲') : QChar(u'△');
}

void MainWindow::setGameOverMove(GameOverCause cause, bool loserIsPlayerOne)
{
    if (!m_match || !m_match->gameOverState().isOver) return;

    m_match->appendGameOverLineAndMark(
        (cause == GameOverCause::Resignation) ? MatchCoordinator::Cause::Resignation
                                              : MatchCoordinator::Cause::Timeout,
        loserIsPlayerOne ? MatchCoordinator::P1 : MatchCoordinator::P2);

    // UI 後処理は従来通り
    if (m_shogiView) m_shogiView->update();
    if (m_recordPane) {
        if (auto* view = m_recordPane->kifuView()) {
            view->setSelectionMode(QAbstractItemView::SingleSelection);
        }
    }
    setReplayMode(true);
    exitLiveAppendMode_();
}

void MainWindow::appendKifuLine(const QString& text, const QString& elapsedTime)
{
    // KIF 追記の既存フローに合わせて m_lastMove を経由し、updateGameRecord() を1回だけ呼ぶ
    m_lastMove = text;

    // ここで棋譜へ 1 行追加（手数インクリメントやモデル反映は updateGameRecord が面倒を見る）
    updateGameRecord(elapsedTime);

    // 二重追記防止のためクリア
    m_lastMove.clear();
}

void MainWindow::ensureGameInfoTable()
{
    if (m_gameInfoTable) return;

    m_gameInfoTable = new QTableWidget(m_central);
    m_gameInfoTable->setColumnCount(2);
    m_gameInfoTable->setHorizontalHeaderLabels({ tr("項目"), tr("内容") });
    m_gameInfoTable->horizontalHeader()->setStretchLastSection(true);
    m_gameInfoTable->verticalHeader()->setVisible(false);
    m_gameInfoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gameInfoTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_gameInfoTable->setWordWrap(true);
    m_gameInfoTable->setShowGrid(false);
}

void MainWindow::onMainMoveRowChanged(int selPly)
{
    qDebug().noquote() << "[ROW] onMainMoveRowChanged selPly=" << selPly
                       << " resolvedRows=" << m_resolvedRows.size()
                       << " isLiveAppend=" << m_isLiveAppendMode;

    // 再入防止（applyResolvedRowAndSelect 内で選択を動かすと再度シグナルが来るため）
    if (m_onMainRowGuard) return;
    m_onMainRowGuard = true;

    const int safePly = qMax(0, selPly);

    // ライブ中は常にライブ経路で同期（分岐再生を強制無効化）
    if (m_isLiveAppendMode || m_resolvedRows.isEmpty()) {
        // ← 投了後の矢印ナビはここに入る。局面＋ハイライトを一括同期
        syncBoardAndHighlightsAtRow(safePly);
        m_onMainRowGuard = false;
        return;
    }

    // いまアクティブな“本譜 or 分岐”行
    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);

    // 行に対応する disp/sfen/gm を差し替え（Presenter更新は Coordinator 側で実施）
    applyResolvedRowAndSelect(row, safePly);

    // その手数の局面を反映し、最後の一手をハイライト
    syncBoardAndHighlightsAtRow(safePly);

    m_onMainRowGuard = false;
}

void MainWindow::populateBranchListForPly(int ply)
{
    // Coordinator 主導へ一本化（表示のみ更新）
    if (!m_kifuLoadCoordinator) return;

    // いまアクティブな行を安全化
    const int row = (m_resolvedRows.isEmpty()
                         ? 0
                         : qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1));

    const int safePly = qMax(0, ply);

    // 分岐候補欄の更新は Coordinator へ委譲
    m_kifuLoadCoordinator->showBranchCandidates(row, safePly);
}

// MainWindow::syncBoardAndHighlightsAtRow を置き換え
void MainWindow::syncBoardAndHighlightsAtRow(int ply)
{
    // 位置編集モード中は従来どおりスキップ
    if (m_shogiView && m_shogiView->positionEditMode()) {
        qDebug() << "[UI] syncBoardAndHighlightsAtRow skipped (edit-mode). ply=" << ply;
        return;
    }

    ensureBoardSyncPresenter_();
    if (m_boardSync) {
        m_boardSync->syncBoardAndHighlightsAtRow(ply);
    }

    // 旧コードが行っていた「矢印ボタンの活性化」等の軽いUIは残す
    enableArrowButtons();
}

void MainWindow::onBranchCandidateActivated(const QModelIndex& index)
{
    if (!index.isValid()) return;
    if (!m_kifuBranchModel) return;

    // 末尾の「本譜へ戻る」行？
    if (m_kifuBranchModel->isBackToMainRow(index.row())) {
        // 本譜に戻す：既存スナップショットから再表示
        displayGameRecord(m_dispMain);
        rebuildSfenRecord(m_startSfenStr, m_usiMoves, /*hasTerminal=*/false);
        rebuildGameMoves(m_startSfenStr, m_usiMoves);
    } else {
        // 行→variationId を引いて確定ラインを取得
        if (index.row() < 0 || index.row() >= m_branchVarIds.size()) return;
        const int variationId = m_branchVarIds.at(index.row());
        if (!m_varEngine) return;

        const auto line = m_varEngine->resolveAfterWins(variationId);

        // 表示列の差し替え（既存関数）
        displayGameRecord(line.disp);

        // USI/SFEN は既存の安全経路で再構築
        rebuildSfenRecord(m_startSfenStr, line.usi, /*hasTerminal=*/false);
        rebuildGameMoves(m_startSfenStr, line.usi);
    }

    // 以後：ハイライト・分岐ツリー同期など既存処理へ（最小限）
    enableArrowButtons();
    if (m_analysisTab) {
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }
}

void MainWindow::applyResolvedRowAndSelect(int row, int selPly)
{
    if (!m_kifuLoadCoordinator) return;

    // 状態の差し替え（disp/sfen/gm）と Presenter 更新は Coordinator 側の責務
    m_kifuLoadCoordinator->applyResolvedRowAndSelect(row, selPly);
}

void MainWindow::BranchRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    QStyledItemDelegate::initStyleOption(&opt, index);

    const bool isBranchable = (m_marks && m_marks->contains(index.row()));

    // 選択時のハイライトと衝突しないように、未選択時のみオレンジ背景
    if (isBranchable && !(opt.state & QStyle::State_Selected)) {
        painter->save();
        painter->fillRect(opt.rect, QColor(255, 220, 160));
        painter->restore();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

inline void pumpUi() {
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5); // 最大5ms程度
}

// --- INavigationContext の実装 ---
bool MainWindow::hasResolvedRows() const {
    return !m_resolvedRows.isEmpty();
}

int MainWindow::resolvedRowCount() const {
    return m_resolvedRows.size();
}

int MainWindow::activeResolvedRow() const {
    return m_activeResolvedRow;
}

int MainWindow::maxPlyAtRow(int row) const
{
    // 解決済み行が無い → 棋譜モデルの最終行を上限に使う
    if (m_resolvedRows.isEmpty()) {
        const int rows = (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : 0);
        return (rows > 0) ? (rows - 1) : 0;  // 0=開始局面のみ
    }

    const int clamped = qBound(0, row, m_resolvedRows.size() - 1);
    return m_resolvedRows[clamped].disp.size();
}

int MainWindow::currentPly() const
{
    // ★ リプレイ／再開（ライブ追記）中は UI 側のトラッキング値を優先
    if (m_isLiveAppendMode) {
        if (m_currentSelectedPly >= 0) return m_currentSelectedPly;

        // 念のためビューの currentIndex もフォールバックに
        const QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
        if (view) {
            const QModelIndex cur = view->currentIndex();
            if (cur.isValid()) return qMax(0, cur.row());
        }
        return 0;
    }

    // 通常時は従来通り m_activePly を優先
    if (m_activePly >= 0) return m_activePly;

    const QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (view) {
        const QModelIndex cur = view->currentIndex();
        if (cur.isValid()) return qMax(0, cur.row());
    }
    return 0;
}

void MainWindow::applySelect(int row, int ply)
{
    // ライブ append 中 or 解決済み行が未構築のときは
    // → 表の選択を直接動かして局面・ハイライトを同期
    if (m_isLiveAppendMode || m_resolvedRows.isEmpty()) {
        if (m_recordPane && m_kifuRecordModel) {
            if (QTableView* view = m_recordPane->kifuView()) {
                const int rows = m_kifuRecordModel->rowCount();
                const int safe = (rows > 0) ? qBound(0, ply, rows - 1) : 0;

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

                // 盤・ハイライト即時同期（従来の onMainMoveRowChanged と同じ流れ）
                syncBoardAndHighlightsAtRow(safe);
                m_currentSelectedPly = safe;
                m_currentMoveIndex   = safe;
            }
        }
        return;
    }

    // 通常（KIF再生/分岐再生）ルート
    applyResolvedRowAndSelect(row, ply);
}

void MainWindow::setupRecordPane()
{
    // 1) モデルが無ければ生成（既にあれば再利用）
    if (!m_kifuRecordModel)
        m_kifuRecordModel = new KifuRecordListModel(this);
    if (!m_kifuBranchModel)
        m_kifuBranchModel = new KifuBranchListModel(this);

    // 2) RecordPane 生成（既にあれば使い回し）
    const bool firstTime = (m_recordPane == nullptr);
    if (!m_recordPane) {
        m_recordPane = new RecordPane(m_central);

        // RecordPane → MainWindow 通知
        connect(m_recordPane, &RecordPane::mainRowChanged,
                this, &MainWindow::onMainMoveRowChanged,
                Qt::UniqueConnection);

        connect(m_recordPane, &RecordPane::branchActivated,
                this, &MainWindow::onBranchCandidateActivated,
                Qt::UniqueConnection);

        // 旧: setupRecordAndEvaluationLayout() が返していた root の置き換え
        m_gameRecordLayoutWidget = m_recordPane;
    }

    // 3) モデルをビューにセット
    m_recordPane->setModels(m_kifuRecordModel, m_kifuBranchModel);

    // 4) NavigationController を RecordPane のボタンで構築（再実行に強いよう毎回作り直し）
    if (m_nav) {
        m_nav->deleteLater();
        m_nav = nullptr;
    }

    NavigationController::Buttons btns{
        m_recordPane->firstButton(),
        m_recordPane->back10Button(),
        m_recordPane->prevButton(),
        m_recordPane->nextButton(),
        m_recordPane->fwd10Button(),
        m_recordPane->lastButton(),
    };

    // すべてのボタンが存在する前提。デバッグ時に早期に気付けるようアサート
    Q_ASSERT(btns.first && btns.back10 && btns.prev &&
             btns.next && btns.fwd10 && btns.last);

    m_nav = new NavigationController(btns, /*ctx=*/this, /*parent=*/this);

    // （初回のみで良い UI 調整があれば firstTime を使って分岐できます）
    Q_UNUSED(firstTime);

    setupBranchView_();
}

void MainWindow::setupEngineAnalysisTab()
{
    // すでに作成済みなら、防御的に m_tab だけ拾って終了
    if (m_analysisTab) {
        if (!m_tab) {
            if (auto* tw = m_analysisTab->tab()) m_tab = tw;  // ← EngineAnalysisTab 側の accessor を利用
        }
        return;
    }

    // EngineAnalysisTab を 1個だけ生成
    m_analysisTab = new EngineAnalysisTab(m_central);

    // ★ 先に UI を構築して内部の m_tab / m_view1 / m_view2 を生成
    m_analysisTab->buildUi();

    // 例：MainWindow::MainWindow(...) の本体先頭あたり
    m_modelThinking1 = new ShogiEngineThinkingModel(this);
    m_modelThinking2 = new ShogiEngineThinkingModel(this);

    // 生成後にモデルを渡す（UI ができてから）
    m_analysisTab->setModels(m_modelThinking1, m_modelThinking2,
                             m_lineEditModel1, m_lineEditModel2);

    // 初期状態は単機表示（EvE 開始時に必要なら true にする）
    m_analysisTab->setDualEngineVisible(false);

    // ★ EngineAnalysisTab が作成した同じ QTabWidget を受け取って中央に貼る
    m_tab = m_analysisTab->tab();
    Q_ASSERT(m_tab);

    // 分岐ツリークリック → MainWindow へ（重複接続防止）
    connect(m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
            this, &MainWindow::onBranchNodeActivated_, Qt::UniqueConnection);
}

void MainWindow::onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex&)
{
    const int row = cur.isValid() ? cur.row() : 0;

    // コメントは従来どおり
    QString text;
    if (row >= 0 && row < m_commentsByRow.size())
        text = m_commentsByRow[row].trimmed();
    broadcastComment(text.isEmpty() ? tr("コメントなし") : text, /*asHtml=*/true);

    // ★ 分岐候補は SFEN 基準の自前インデックスで更新
    populateBranchListForPly(row);
}

void MainWindow::initMatchCoordinator()
{
    // 依存が揃っていない場合は何もしない
    if (!m_gameController || !m_shogiView) return;

    // まず時計を用意（nullでも可だが、あれば渡す）
    ensureClockReady_();

    // --- MatchCoordinator::Deps を構築（UI hooks は従来どおりここで設定） ---
    MatchCoordinator::Deps d;
    d.gc    = m_gameController;
    d.clock = m_shogiClock;
    d.view  = m_shogiView;
    d.usi1  = m_usi1;
    d.usi2  = m_usi2;

    // （EvE 用）ログ／思考モデル
    d.comm1  = m_lineEditModel1;
    d.think1 = m_modelThinking1;
    d.comm2  = m_lineEditModel2;
    d.think2 = m_modelThinking2;

    // ---- ここは「コメントアウト」せず、関数バインドで割り当て ----
    d.hooks.appendEvalP1     = std::bind(&MainWindow::requestRedrawEngine1Eval_, this);
    d.hooks.appendEvalP2     = std::bind(&MainWindow::requestRedrawEngine2Eval_, this); 
    d.hooks.sendGoToEngine   = std::bind(&MatchCoordinator::sendGoToEngine,   m_match, _1, _2);
    d.hooks.sendStopToEngine = std::bind(&MatchCoordinator::sendStopToEngine, m_match, _1);
    d.hooks.sendRawToEngine  = std::bind(&MatchCoordinator::sendRawToEngine,  m_match, _1, _2);
    d.hooks.initializeNewGame= std::bind(&MainWindow::initializeNewGame_, this, _1);
    d.hooks.showMoveHighlights= std::bind(&MainWindow::showMoveHighlights_, this, _1, _2);
    d.hooks.appendKifuLine   = std::bind(&MainWindow::appendKifuLineHook_, this, _1, _2);

    // --- GameStartCoordinator の確保（1 回だけ） ---
    if (!m_gameStartCoordinator) {
        GameStartCoordinator::Deps gd;
        gd.match = nullptr;
        gd.clock = m_shogiClock;
        gd.gc    = m_gameController;
        gd.view  = m_shogiView;

        m_gameStartCoordinator = new GameStartCoordinator(gd, this);

        // ★ Coordinator の転送シグナルを MainWindow のスロットへ接続
        // timeUpdated
        if (m_timeConn) { QObject::disconnect(m_timeConn); m_timeConn = {}; }
        m_timeConn = connect(
            m_gameStartCoordinator,
            static_cast<void (GameStartCoordinator::*)(qint64,qint64,bool,qint64)>(&GameStartCoordinator::timeUpdated),
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
            m_gameStartCoordinator,
            static_cast<void (GameStartCoordinator::*)(const MatchCoordinator::GameEndInfo&)>(&GameStartCoordinator::matchGameEnded),
            this,
            static_cast<void (MainWindow::*)(const MatchCoordinator::GameEndInfo&)>(&MainWindow::onMatchGameEnded),
            Qt::UniqueConnection);
    }

    // --- 司令塔の生成＆初期配線を Coordinator に委譲 ---
    m_match = m_gameStartCoordinator->createAndWireMatch(d, this);

    // PlayMode を司令塔へ伝達（従来どおり）
    if (m_match) {
        m_match->setPlayMode(m_playMode);
    }

    // Clock → MainWindow のタイムアウト系は従来どおり UI 側で受ける
    if (m_shogiClock) {
        connect(m_shogiClock, &ShogiClock::player1TimeOut,
                this, &MainWindow::onPlayer1TimeOut, Qt::UniqueConnection);
        connect(m_shogiClock, &ShogiClock::player2TimeOut,
                this, &MainWindow::onPlayer2TimeOut, Qt::UniqueConnection);
    }

    // ※ 旧：wireMatchSignals_() は Coordinator で転送するようにしたため不要です
}

void MainWindow::ensureClockReady_()
{
    if (m_shogiClock) return;

    m_shogiClock = new ShogiClock(this);

    // ★ ここでは timeUpdated を m_match に接続しない
    //    （接続は wireMatchSignals_() に一本化）

    connect(m_shogiClock, &ShogiClock::player1TimeOut,
            this, &MainWindow::onPlayer1TimeOut, Qt::UniqueConnection);
    connect(m_shogiClock, &ShogiClock::player2TimeOut,
            this, &MainWindow::onPlayer2TimeOut, Qt::UniqueConnection);
}

void MainWindow::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    qDebug().nospace()
    << "[UI] onMatchGameEnded ENTER cause="
    << ((info.cause==MatchCoordinator::Cause::Timeout)?"Timeout":"Resign")
    << " loser=" << ((info.loser==MatchCoordinator::P1)?"P1":"P2");

    // --- UI 後始末（UI層に残す） ---
    if (m_match)      m_match->disarmHumanTimerIfNeeded();
    if (m_shogiClock) m_shogiClock->stopClock();
    if (m_shogiView)  m_shogiView->setMouseClickMode(false);

    // --- 棋譜追記＋時間確定は司令塔へ一括委譲 ---
    const bool loserIsP1 = (info.loser == MatchCoordinator::P1);
    setGameOverMove(toUiCause(info.cause), /*loserIsPlayerOne=*/loserIsP1);

    // --- UIの後処理（矢印ボタン・選択モード整え） ---
    enableArrowButtons();
    if (m_recordPane && m_recordPane->kifuView()) {
        m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::SingleSelection);
    }

    qDebug() << "[UI] onMatchGameEnded LEAVE";
}

void MainWindow::setGameInProgressActions(bool inProgress)
{
    if (inProgress) {
        // ここに旧 setGameInProgressActions() の本体をそのまま貼る
        // 例）対局中メニューのON、操作の有効化/無効化 など
        // 「投了」を表示する。
        ui->actionResign->setVisible(true);

        // 「待った」を表示する。
        ui->actionUndoMove->setVisible(true);

        // 「すぐ指させる」を表示する。
        ui->actionMakeImmediateMove->setVisible(true);

        // 「中断」を表示する。
        ui->breakOffGame->setVisible(true);

        // 「盤面の回転」の表示
        ui->actionFlipBoard->setVisible(true);

        // 「検討」を隠す。
        ui->actionConsideration->setVisible(true);

        // 「棋譜解析」を隠す。
        ui->actionAnalyzeKifu->setVisible(true);
    } else {
        //hideGameActions(); // 既存の終了時UIへ戻す処理
    }
}

void MainWindow::onActionFlipBoardTriggered(bool /*checked*/)
{
    if (m_match) m_match->flipBoard();
}

void MainWindow::wireMatchSignals_()
{
    if (!m_match) return;
    if (m_timeConn) { QObject::disconnect(m_timeConn); m_timeConn = {}; }

    auto sig = static_cast<void (MatchCoordinator::*)(qint64,qint64,bool,qint64)>
        (&MatchCoordinator::timeUpdated);

    m_timeConn = connect(m_match, sig,
                         m_timePresenter, &TimeDisplayPresenter::onMatchTimeUpdated,
                         Qt::UniqueConnection);
    Q_ASSERT(m_timeConn);

    connect(m_match, &MatchCoordinator::requestAppendGameOverMove,
            this, &MainWindow::onRequestAppendGameOverMove,
            Qt::UniqueConnection);
}

void MainWindow::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    const bool loserIsP1 = (info.loser == MatchCoordinator::P1);
    setGameOverMove(toUiCause(info.cause), loserIsP1);
}

void MainWindow::setupBoardInteractionController()
{
    // 既存があれば入れ替え
    if (m_boardController) {
        m_boardController->deleteLater();
        m_boardController = nullptr;
    }

    // コントローラ生成
    m_boardController = new BoardInteractionController(m_shogiView, m_gameController, this);

    // 盤クリックの配線
    connectBoardClicks_();

    // 人間操作 → 合法判定＆適用の配線
    connectMoveRequested_();

    // 既定モード（必要に応じて開始時に上書き）
    m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
}

void MainWindow::connectBoardClicks_()
{
    Q_ASSERT(m_shogiView && m_boardController);

    QObject::connect(m_shogiView, &ShogiView::clicked,
                     m_boardController, &BoardInteractionController::onLeftClick,
                     Qt::UniqueConnection);

    QObject::connect(m_shogiView, &ShogiView::rightClicked,
                     m_boardController, &BoardInteractionController::onRightClick,
                     Qt::UniqueConnection);
}

void MainWindow::connectMoveRequested_()
{
    Q_ASSERT(m_boardController && m_gameController);

    QObject::connect(
        m_boardController, &BoardInteractionController::moveRequested,
        this,              &MainWindow::onMoveRequested_,
        Qt::UniqueConnection);
}

void MainWindow::onMoveRequested_(const QPoint& from, const QPoint& to)
{
    qInfo() << "[UI] onMoveRequested_ from=" << from << " to=" << to
            << " m_playMode=" << int(m_playMode);

    // --- 編集モードは Controller へ丸投げ ---
    if (m_boardController && m_boardController->mode() == BoardInteractionController::Mode::Edit) {
        ensurePositionEditController_();
        if (!m_posEdit || !m_shogiView || !m_gameController) return;

        const bool ok = m_posEdit->applyEditMove(from, to, m_shogiView, m_gameController, m_boardController);
        if (!ok) qInfo() << "[UI] editPosition failed (edit-mode move rejected)";
        return;
    }

    // ▼▼▼ ここから通常対局（有効な PlayMode を決定） ▼▼▼
    if (!m_gameController) {
        qWarning() << "[UI][WARN] m_gameController is null";
        return;
    }

    PlayMode matchMode = (m_match ? m_match->playMode() : NotStarted);
    PlayMode modeNow   = (m_playMode != NotStarted) ? m_playMode : matchMode;

    qInfo() << "[UI] effective modeNow=" << int(modeNow)
            << "(ui m_playMode=" << int(m_playMode) << ", matchMode=" << int(matchMode) << ")";

    // 着手前の手番（HvH 後処理で使う）
    const auto moverBefore = m_gameController->currentPlayer();

    // validateAndMove は参照引数なのでローカルに退避
    QPoint hFrom = from, hTo = to;

    const bool ok = m_gameController->validateAndMove(
        hFrom, hTo, m_lastMove, modeNow, m_currentMoveIndex, m_sfenRecord, m_gameMoves);

    if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, ok);
    if (!ok) {
        qInfo() << "[UI] validateAndMove failed (human move rejected)";
        return;
    }

    // --- 対局モードごとの後処理 ---
    switch (modeNow) {
    case HumanVsHuman:
        qInfo() << "[UI] HvH: delegate post-human-move to MatchCoordinator";
        if (m_match) {
            m_match->onHumanMove_HvH(moverBefore);
        }
        break;

    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        if (m_match) {
            qInfo() << "[UI] HvE: forwarding to MatchCoordinator::onHumanMove_HvE";
            m_match->onHumanMove_HvE(hFrom, hTo, m_lastMove);
        } else {
            qWarning() << "[UI][WARN] m_match is null; fallback to handleMove_HvE_ (legacy path)";
            handleMove_HvE_(hFrom, hTo);
        }
        break;

    default:
        qInfo() << "[UI] not a live play mode; skip post-handling";
        break;
    }
}

// 人間 vs エンジン：人間が指した直後の処理（棋譜追記＋1手返しを司令塔へ一本化）
void MainWindow::handleMove_HvE_(const QPoint& humanFrom, const QPoint& humanTo)
{
    if (!m_match) return;
    // validateAndMove() でセット済みの m_lastMove（「▲７六歩」等の整形文字列）を司令塔へ渡す
    m_match->onHumanMove_HvE(humanFrom, humanTo, m_lastMove);
}

// 再生モードの切替を MainWindow 内で一元管理
void MainWindow::setReplayMode(bool on)
{
    m_isReplayMode = on;

    // 再生中は時計を止め、表示だけ整える
    if (m_shogiClock) {
        m_shogiClock->stopClock();
        m_shogiClock->updateClock(); // 表示だけは最新化
    }
    if (m_match) {
        m_match->pokeTimeUpdateNow(); // 残時間ラベル等の静的更新だけ反映
    }

    // ★ 再生モードの入/出でハイライト方針を切替
    if (m_shogiView) {
        m_shogiView->setUiMuted(on);
        if (on) {
            m_shogiView->clearTurnHighlight();   // 中立に
        } else {
            // 対局に戻る: 現手番・残時間から再適用
            const bool p1turn = (m_gameController &&
                                 m_gameController->currentPlayer() == ShogiGameController::Player1);
            // ★ enum ではなく bool を渡す（true = 先手手番）
            m_shogiView->setActiveSide(p1turn);  // or setBlackActive(p1turn); ※ヘッダに合わせて

            // ★ Urgency は時計側の更新イベントで再適用させる
            if (m_shogiClock) {
                m_shogiClock->updateClock();   // timeUpdated が飛び、既存の結線で applyClockUrgency が呼ばれる想定
            }
        }
    }
}

void MainWindow::broadcastComment(const QString& text, bool asHtml)
{
    if (asHtml) {
        if (m_analysisTab) m_analysisTab->setCommentHtml(text);
        if (m_recordPane)  m_recordPane->setBranchCommentHtml(text);
    } else {
        if (m_analysisTab) m_analysisTab->setCommentText(text);
        if (m_recordPane)  m_recordPane->setBranchCommentText(text);
    }
}

void MainWindow::setupBranchCandidatesWiring_()
{
    qDebug() << "[WIRE] setupBranchCandidatesWiring_ ENTER";

    if (!m_recordPane) {
        qWarning() << "[WIRE] no RecordPane; skip setupBranchCandidatesWiring_";
        return;
    }

    if (!m_branchCtl) {
        m_branchCtl = new BranchCandidatesController(
            m_varEngine.get(),
            m_kifuBranchModel,
            this
            );
        qDebug() << "[WIRE] BranchCandidatesController created ve=" << static_cast<void*>(m_varEngine.get())
                 << " model=" << static_cast<void*>(m_kifuBranchModel);
    }

    // Controller 自身に RecordPane→Controller の配線をやらせる
    m_branchCtl->attachRecordPane(m_recordPane);

    // 「Plan クリック → 行/手へジャンプ」は MainWindow 受けでOK（役割：画面遷移）
    const bool okA = connect(m_branchCtl,
                             &BranchCandidatesController::planActivated,
                             this,
                             &MainWindow::onBranchPlanActivated_,
                             Qt::UniqueConnection);
    qDebug() << "[WIRE] connect planActivated -> onBranchPlanActivated_ :" << okA;
}

void MainWindow::setupBranchView_()
{
    if (!m_recordPane) return;

    // モデルがまだなら用意しておく
    if (!m_kifuBranchModel) {
        m_kifuBranchModel = new KifuBranchListModel(this);
        qDebug() << "[WIRE] created KifuBranchListModel =" << m_kifuBranchModel;
    }

    QTableView* view = m_recordPane->branchView();
    if (!view) return;

    if (view->model() != m_kifuBranchModel) {
        view->setModel(m_kifuBranchModel);
        qDebug() << "[WIRE] branchView.setModel done model=" << m_kifuBranchModel;
    }

    // 初期は隠しておく（候補が入ったら populateBranchCandidates_ で制御）
    view->setVisible(false);
}

void MainWindow::onBranchPlanActivated_(int row, int ply1)
{
    qDebug() << "[BRANCH] planActivated -> applyResolvedRowAndSelect row=" << row << " ply=" << ply1;
    applyResolvedRowAndSelect(row, ply1);
}

void MainWindow::onRecordPaneBranchActivated_(const QModelIndex& index)
{
    if (!index.isValid()) return;
    if (!m_branchCtl)     return;
    m_branchCtl->activateCandidate(index.row());
}

std::pair<int,int> MainWindow::resolveBranchHighlightTarget(int row, int ply) const
{
    if (!m_varEngine) return {-1, -1};
    if (row < 0 || row >= m_resolvedRows.size() || ply < 0) return {-1, -1};

    const ResolvedRow& rr = m_resolvedRows[row];

    // Main 行（varIndex < 0）は常に vid=0（本譜）
    if (rr.varIndex < 0) {
        return { 0, ply };
    }

    // 親行（無ければ Main=0）
    const int parentRow =
        (rr.parent >= 0 && rr.parent < m_resolvedRows.size()) ? rr.parent : 0;

    // 分岐前の手は親行の vid でハイライト
    if (ply < rr.startPly) {
        return resolveBranchHighlightTarget(parentRow, ply);
    }

    // 分岐以降は当該分岐の vid（sourceIndex -> variationId）
    const int vid = m_varEngine->variationIdFromSourceIndex(rr.varIndex);
    if (vid < 0) {
        // 念のためフォールバック
        return resolveBranchHighlightTarget(parentRow, ply);
    }
    return { vid, ply };
}

void MainWindow::onBranchNodeActivated_(int row, int ply)
{
    if (row < 0 || row >= m_resolvedRows.size()) return;

    // その行の手数内にクランプ（0=開始局面, 1..N）
    const int maxPly = m_resolvedRows[row].disp.size();
    const int selPly = qBound(0, ply, maxPly);

    // これだけで：局面更新 / 棋譜欄差し替え＆選択 / 分岐候補欄更新 / ツリーハイライト同期
    applyResolvedRowAndSelect(row, selPly);
}

void MainWindow::onMoveCommitted(ShogiGameController::Player mover, int /*ply*/)
{
    const bool isEvE =
        (m_playMode == EvenEngineVsEngine) ||
        (m_playMode == HandicapEngineVsEngine);
    if (!isEvE) return;

    if (mover == ShogiGameController::Player1) {
        redrawEngine1EvaluationGraph();
    } else if (mover == ShogiGameController::Player2) {
        redrawEngine2EvaluationGraph();
    }
}

void MainWindow::flipBoardAndUpdatePlayerInfo()
{
    qDebug() << "[UI] flipBoardAndUpdatePlayerInfo ENTER";
    if (!m_shogiView) return;

    // ★回転前にハイライトを消さない（removeHighlightAllData / clearTurnHighlight を呼ばない）
    //   ハイライトは論理マス保持のまま、描画側が flipMode を見て追従させます。

    // 盤の表示向きをトグル（UI反転）
    const bool flipped = !m_shogiView->getFlipMode();
    m_shogiView->setFlipMode(flipped);

    // 駒画像セットは向きに合わせる
    if (flipped) m_shogiView->setPiecesFlip();
    else         m_shogiView->setPieces();

    // ★手番ハイライト（名前・時計の強調）を回転直後に再適用
    applyTurnHighlights_(m_lastP1Turn);

    m_shogiView->update();
    qDebug() << "[UI] flipBoardAndUpdatePlayerInfo LEAVE";
}

void MainWindow::applyTurnHighlights_(bool p1turn)
{
    // ここでは「誰が手番か」だけ渡して、実際の見た目組み立ては下へ委譲
    updateUrgencyStyles_(p1turn);
}

void MainWindow::updateUrgencyStyles_(bool p1turn)
{
    if (!m_shogiView) return;

    // アクティブ側（先手=黒か）をビューへ通知
    // ※ セッターが無ければ下の「補足：ShogiView側の追加」を実装してください
    m_shogiView->setActiveIsBlack(p1turn);

    // 残り時間から緊急度を決定
    const qint64 ms = p1turn ? m_lastP1Ms : m_lastP2Ms;
    ShogiView::Urgency u;
    if (ms <= ShogiView::kWarn5Ms)        u = ShogiView::Urgency::Warn5;
    else if (ms <= ShogiView::kWarn10Ms)  u = ShogiView::Urgency::Warn10;
    else                                   u = ShogiView::Urgency::Normal;

    // 見た目の適用は ShogiView に一元化
    m_shogiView->setUrgencyVisuals(u);
}

void MainWindow::setupNameAndClockFonts_()
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

void MainWindow::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    if (!st.isOver) return;
    if (st.lastInfo.cause != MatchCoordinator::Cause::BreakOff) return;
    if (!m_gameController || !m_shogiClock) return;

    // 中断時の手番（これから指す側）
    const bool curIsP1 =
        (m_gameController->currentPlayer() == ShogiGameController::Player1);

    // 「▲/△中断」
    const QString line = curIsP1 ? QStringLiteral("▲中断")
                                 : QStringLiteral("△中断");

    // フォーマッタ
    auto mmss = [](qint64 ms)->QString {
        if (ms < 0) ms = 0;
        const qint64 s = ms / 1000;
        const int mm = int((s / 60) % 60);
        const int ss = int(s % 60);
        return QStringLiteral("%1:%2")
               .arg(mm, 2, 10, QLatin1Char('0'))
               .arg(ss, 2, 10, QLatin1Char('0'));
    };
    auto hhmmss = [](qint64 ms)->QString {
        if (ms < 0) ms = 0;
        const qint64 s = ms / 1000;
        const int hh = int(s / 3600);
        const int mm = int((s / 60) % 60);
        const int ss = int(s % 60);
        return QStringLiteral("%1:%2:%3")
               .arg(hh, 2, 10, QLatin1Char('0'))
               .arg(mm, 2, 10, QLatin1Char('0'))
               .arg(ss, 2, 10, QLatin1Char('0'));
    };

    // この手の考慮時間：ターン開始エポックから算出
    qint64 considerMs = 0;
    if (m_match) {
        const qint64 epoch = m_match->turnEpochFor(
            curIsP1 ? MatchCoordinator::P1 : MatchCoordinator::P2);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (epoch > 0 && now >= epoch) considerMs = now - epoch;
    }

    // 合計消費 = 初期持ち時間 - 現在残り時間（初期未設定なら0）
    const qint64 remainMs = curIsP1
        ? m_shogiClock->getPlayer1TimeIntMs()
        : m_shogiClock->getPlayer2TimeIntMs();

    const qint64 initMs = curIsP1 ? m_initialTimeP1Ms : m_initialTimeP2Ms;
    const qint64 totalUsedMs = (initMs > 0) ? (initMs - remainMs) : 0;

    const QString elapsed = QStringLiteral("%1/%2")
                                .arg(mmss(considerMs),
                                     hhmmss(totalUsedMs));

    // 棋譜欄に「N 中断 (mm:ss/HH:MM:SS)」を一度だけ追記
    appendKifuLine(line, elapsed);

    // 閲覧（リプレイ）モードへ
    enableArrowButtons();
    setReplayMode(true);

    m_shogiView->removeHighlightAllData();
}

void MainWindow::handleBreakOffGame()
{
    if (!m_match || m_match->gameOverState().isOver) return;
    m_match->handleBreakOff();
}

void MainWindow::exitLiveAppendMode_()
{
    m_isLiveAppendMode = false;
    if (!m_recordPane) return;
    if (auto* view = m_recordPane->kifuView()) {
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setFocusPolicy(Qt::StrongFocus);
    }
}

// 「検討を終了」アクション用：エンジンに quit を送り検討セッションを終了
void MainWindow::handleBreakOffConsidaration()
{
    if (!m_match) return;

    // 司令塔に依頼（内部で quit 送信→プロセス/Usi 破棄→モード NotStarted）
    m_match->handleBreakOffConsidaration();

    // UI の後始末（任意）——検討のハイライトなどをクリアしておく
    if (m_shogiView) m_shogiView->removeHighlightAllData();

    // MainWindow 側のモードも念のため合わせる（UI 表示に依存がある場合）
    m_playMode = NotStarted;

    // 盤下のエンジン名表示などを通常状態へ（関数がある場合）
    setEngineNamesBasedOnMode();

    // ここでは UI の大規模リセットは行わず、検討終了の状態だけ示す
    if (statusBar()) {
        statusBar()->showMessage(tr("検討を中断しました（エンジンに quit を送信）。"), 3000);
    }
}

void MainWindow::ensureTurnSyncBridge_()
{
    // ShogiGameController と TurnManager を取得
    auto* gc = m_gameController;                          // 既存メンバ想定
    auto* tm = findChild<TurnManager*>("TurnManager");    // 既存オブジェクト名想定

    if (!gc || !tm) return;

    // 1) GCの手番変更 → TurnManagerへミラー（重複接続は抑止）
    QObject::connect(gc, &ShogiGameController::currentPlayerChanged,
                     tm, &TurnManager::setFromGc,
                     Qt::UniqueConnection);

    // 2) TurnManagerの変更 → MainWindowのUI更新（未配線なら接続）
    QObject::connect(tm, &TurnManager::changed,
                     this, &MainWindow::onTurnManagerChanged,
                     Qt::UniqueConnection);

    // 3) 初期同期：現時点の手番を即時反映（時計/ラベルのズレ防止）
    tm->setFromGc(gc->currentPlayer());
}

// 置き換え：makeDefaultSaveFileName()
void MainWindow::makeDefaultSaveFileName()
{
    defaultSaveFileName = KifuIoService::makeDefaultSaveFileName(
        m_playMode, m_humanName1, m_humanName2, m_engineName1, m_engineName2,
        QDateTime::currentDateTime());

    if (defaultSaveFileName == "_vs.kifu") {
        defaultSaveFileName = "untitled.kifu";
    }
}

// 置き換え：saveKifuToFile()
void MainWindow::saveKifuToFile()
{
    QDir::setCurrent(QDir::homePath());

    makeDefaultSaveFileName();
    if (defaultSaveFileName == "_vs.kifu") defaultSaveFileName = "untitled.kifu";

    kifuSaveFileName = QFileDialog::getSaveFileName(
        this, tr("Save File"), defaultSaveFileName, tr("Kif(*.kifu)"));

    if (!kifuSaveFileName.isEmpty()) {
        QString err;
        if (!writeKifuFile(kifuSaveFileName, m_kifuDataList, &err)) {
            displayErrorMessage(tr("An error occurred in MainWindow::saveKifuFile. %1").arg(err));
        }
    }
    QDir::setCurrent(QApplication::applicationDirPath());
}

// 置き換え：overwriteKifuFile()
void MainWindow::overwriteKifuFile()
{
    if (kifuSaveFileName.isEmpty()) {
        saveKifuToFile();
        return;
    }
    QString err;
    if (!writeKifuFile(kifuSaveFileName, m_kifuDataList, &err)) {
        displayErrorMessage(tr("An error occurred in MainWindow::overWriteKifuFile. %1").arg(err));
    }
}

void MainWindow::ensurePositionEditController_()
{
    if (m_posEdit) return;
    m_posEdit = new PositionEditController(this); // 親=MainWindow にして寿命管理
}

void MainWindow::ensureBoardSyncPresenter_()
{
    if (m_boardSync) return;

    BoardSyncPresenter::Deps d;
    d.gc         = m_gameController;
    d.view       = m_shogiView;
    d.bic        = m_boardController;
    d.sfenRecord = m_sfenRecord;
    d.gameMoves  = &m_gameMoves;

    m_boardSync = new BoardSyncPresenter(d, this);
}

void MainWindow::ensureAnalysisPresenter_()
{
    if (!m_analysisPresenter)
        m_analysisPresenter = new AnalysisResultsPresenter(this);
}

void MainWindow::ensureGameStartCoordinator_()
{
    if (m_gameStart) return;

    GameStartCoordinator::Deps d;
    d.match = m_match;
    d.clock = m_shogiClock;
    d.gc    = m_gameController;
    d.view  = m_shogiView;

    m_gameStart = new GameStartCoordinator(d, this);

    // 依頼シグナルを既存メソッドへ接続（ラムダ不使用）
    connect(m_gameStart, &GameStartCoordinator::requestPreStartCleanup,
            this, &MainWindow::onPreStartCleanupRequested_);

    connect(m_gameStart, &GameStartCoordinator::requestApplyTimeControl,
            this, &MainWindow::onApplyTimeControlRequested_);
}

// 対局開始前の UI/状態初期化（ハイライトや選択をクリア等）
void MainWindow::onPreStartCleanupRequested_()
{
    if (m_boardController) m_boardController->clearAllHighlights();
    // 棋譜欄や解析UIのリセット、各種フラグ初期化など
    // setReplayMode(false); 等、既存処理をここへ集約
}

// 関数差し替え
void MainWindow::onApplyTimeControlRequested_(const GameStartCoordinator::TimeControl& tc)
{
    TimeControlUtil::applyToClock(m_shogiClock, tc, m_startSfenStr, m_currentSfenStr);
    if (m_shogiView) m_shogiView->update(); // 任意
}

void MainWindow::ensureRecordPresenter_()
{
    if (m_recordPresenter) return;

    GameRecordPresenter::Deps d;
    d.model = m_kifuRecordModel; // 既存の棋譜リストモデル
    d.recordPane = m_recordPane; // 既存のRecordPane（任意）

    m_recordPresenter = new GameRecordPresenter(d, this);
}

void MainWindow::onRecordRowChangedByPresenter(int row, const QString& comment)
{
    // 行番号ベースで従来処理を呼び出し
    // 例：盤面とハイライトの同期、コメント欄の反映など
    if (row >= 0) {
        syncBoardAndHighlightsAtRow(row);
    }

    // コメントの表示（適宜 UI 側のラベルに反映）
    if (m_recordPane) {
        if (auto* info = m_recordPane->commentLabel()) { // 例: コメント表示の QLabel を持っている想定
            info->setText(comment);
        }
    }

    // 必要であれば他UI（矢印ボタン活性/非活性やゲーム情報）の更新もここで
    enableArrowButtons();
}

// UIスレッド安全のため queued 呼び出しにしています
void MainWindow::requestRedrawEngine1Eval_() {
    QMetaObject::invokeMethod(this, &MainWindow::redrawEngine1EvaluationGraph, Qt::QueuedConnection);
}

void MainWindow::requestRedrawEngine2Eval_() {
    QMetaObject::invokeMethod(this, &MainWindow::redrawEngine2EvaluationGraph, Qt::QueuedConnection);
}

void MainWindow::initializeNewGame_(const QString& s)
{
    QString s2 = s;
    startNewShogiGame(s2);
}

void MainWindow::showMoveHighlights_(const QPoint& from, const QPoint& to)
{
    if (m_boardController) m_boardController->showMoveHighlights(from, to);
}

void MainWindow::appendKifuLineHook_(const QString& text, const QString& elapsed)
{
    appendKifuLine(text, elapsed);
}
