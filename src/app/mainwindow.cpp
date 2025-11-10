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
#include "considerationflowcontroller.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"
#include "tsumesearchflowcontroller.h"
#include "ui_mainwindow.h"
#include "shogiclock.h"
#include "apptooltipfilter.h"
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

using KifuIoService::makeDefaultSaveFileName;
using KifuIoService::writeKifuFile;
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
        bw.branchModel     = m_kifuBranchModel;     // 既に保持していれば渡す（null可）
        bw.variationEngine = m_varEngine.get();     // unique_ptr想定
        bw.kifuLoader      = m_kifuLoadCoordinator; // 読み込み済みなら渡す（null可）
        bw.parent          = this;

        m_branchWiring = new BranchWiringCoordinator(bw);

        // ★ 冪等なのでこの1回で十分
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

    // 盤操作・表示（★ここを直結にする）
    connect(ui->actionFlipBoard,            &QAction::triggered, this,        &MainWindow::onActionFlipBoardTriggered);
    connect(ui->actionCopyBoardToClipboard, &QAction::triggered, this,        &MainWindow::copyBoardToClipboard);
    connect(ui->actionMakeImmediateMove,    &QAction::triggered, this,        &MainWindow::movePieceImmediately);

    // ▼ 直結：MainWindow 経由ではなく ShogiView のメソッドを直接呼ぶ
    if (m_shogiView) {
        connect(ui->actionEnlargeBoard,     &QAction::triggered, m_shogiView, &ShogiView::enlargeBoard);
        connect(ui->actionShrinkBoard,      &QAction::triggered, m_shogiView, &ShogiView::reduceBoard);
    }

    connect(ui->actionUndoMove,             &QAction::triggered, this,        &MainWindow::undoLastTwoMoves);
    connect(ui->actionSaveBoardImage,       &QAction::triggered, this,        &MainWindow::saveShogiBoardImage);

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
                m_shogiView,      &ShogiView::endDrag, Qt::UniqueConnection);

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
    // ShogiView が未生成なら何もしない
    if (!m_shogiView) return;

    // GameStartCoordinator がある（= 対局開始後の通常経路）の場合は従来どおり適用
    if (m_gameStart) {
        m_gameStart->applyPlayersNamesForMode(
            m_shogiView,
            m_playMode,
            m_humanName1, m_humanName2,
            m_engineName1, m_engineName2
            );
        return;
    }

    // ★ 起動直後（m_gameStart が未設定）のデフォルト表示を補う
    //   「▲/▽」マークは ShogiView 側の refreshNameLabels() で自動付与されるため、
    //   ここでは素の名前（先手/後手）のみを渡します。
    m_shogiView->setBlackPlayerName(tr("先手"));
    m_shogiView->setWhitePlayerName(tr("後手"));
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

// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    if (m_match) m_match->handleResign();
}

void MainWindow::redrawEngine1EvaluationGraph()
{
    EvalGraphPresenter::appendPrimaryScore(m_scoreCp, m_match);
}

void MainWindow::redrawEngine2EvaluationGraph()
{
    EvalGraphPresenter::appendSecondaryScore(m_scoreCp, m_match);
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

void MainWindow::displayVersionInformation()
{
    AboutCoordinator::showVersionDialog(this);
}

void MainWindow::openWebsiteInExternalBrowser()
{
    AboutCoordinator::openProjectWebsite();
}

void MainWindow::displayEngineSettingsDialog()
{
    EngineSettingsCoordinator::openDialog(this);
}

// 成る・不成の選択ダイアログを起動する。
void MainWindow::displayPromotionDialog()
{
    if (!m_gameController) return;
    const bool promote = PromotionFlow::askPromote(this);
    m_gameController->setPromote(promote);
}

// 検討ダイアログを表示する。
void MainWindow::displayConsiderationDialog()
{
    // UI 側の状態保持（従来どおり）
    m_playMode = ConsidarationMode;

    // 手番表示（必要最小限）
    if (m_gameController && m_gameMoves.size() > 0 && m_currentMoveIndex >= 0 && m_currentMoveIndex < m_gameMoves.size()) {
        if (m_gameMoves.at(m_currentMoveIndex).movingPiece.isUpper())
            m_gameController->setCurrentPlayer(ShogiGameController::Player1);
        else
            m_gameController->setCurrentPlayer(ShogiGameController::Player2);
    }

    // 送信する position（既存の m_positionStrList を利用）
    const QString position = (m_currentMoveIndex >= 0 && m_currentMoveIndex < m_positionStrList.size())
                                 ? m_positionStrList.at(m_currentMoveIndex)
                                 : QString();

    // Flow に一任（onError はラムダではなく専用スロットへバインド）
    ConsiderationFlowController* flow = new ConsiderationFlowController(this);
    ConsiderationFlowController::Deps d;
    d.match   = m_match;
    d.onError = std::bind(&MainWindow::onFlowError_, this, std::placeholders::_1);

    flow->runWithDialog(d, this, position);
}

// 詰み探索ダイアログを表示する。
void MainWindow::displayTsumeShogiSearchDialog()
{
    // 解析モード切替
    m_playMode = TsumiSearchMode;

    // Flow に一任（ダイアログ生成～exec～司令塔連携）
    TsumeSearchFlowController* flow = new TsumeSearchFlowController(this);

    TsumeSearchFlowController::Deps d;
    d.match            = m_match;
    d.sfenRecord       = m_sfenRecord;
    d.startSfenStr     = m_startSfenStr;
    d.positionStrList  = m_positionStrList;
    d.currentMoveIndex = qMax(0, m_currentMoveIndex);
    d.onError          = std::bind(&MainWindow::onFlowError_, this, std::placeholders::_1);

    flow->runWithDialog(d, this);
}

// 棋譜解析ダイアログを表示する。
void MainWindow::displayKifuAnalysisDialog()
{
    // 解析モードに遷移
    m_playMode = AnalysisMode;

    // Flow の用意
    if (!m_analysisFlow) {
        m_analysisFlow = new AnalysisFlowController(this);
    }

    // 解析モデルが未生成ならここで作成
    if (!m_analysisModel) {
        m_analysisModel = new KifuAnalysisListModel(this);
    }

    // 依存を詰めて Flow へ一任（displayError はスロットにバインド）
    AnalysisFlowController::Deps d;
    d.sfenRecord    = m_sfenRecord;
    d.moveRecords   = m_moveRecords;
    d.analysisModel = m_analysisModel;
    d.analysisTab   = m_analysisTab;
    d.usi           = m_usi1;
    d.logModel      = m_lineEditModel1;  // info/bestmove の橋渡し用
    d.activePly     = m_activePly;
    d.displayError  = std::bind(&MainWindow::onFlowError_, this, std::placeholders::_1);

    m_analysisFlow->runWithDialog(d, this);
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
    // 既存呼び出し互換のため残し、内部は司令塔フックへ集約
    onPreStartCleanupRequested_();
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

    // ★ 追加 (1): BranchWiring に loader を後から注入
    if (m_branchWiring) {
        m_branchWiring->setKifuLoader(m_kifuLoadCoordinator);
    }

    // ★ 追加 (2): KifuLoadCoordinator 側が必要時に再配線させるためのトリガ
    // （KifuLoadCoordinator::setupBranchCandidatesWiring_ シグナル → BranchWiring の配線処理）
    if (m_branchWiring) {
        connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::setupBranchCandidatesWiring_,
                m_branchWiring,       &BranchWiringCoordinator::setupBranchCandidatesWiring,
                Qt::UniqueConnection);
    }

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

    // --- 4) 読み込み実行（ロジックは Coordinator へ） ---
    m_kifuLoadCoordinator->loadKifuFromFile(filePath);
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

void MainWindow::loadWindowSettings()
{
    SettingsService::loadWindowSize(this);
}

void MainWindow::saveWindowAndBoardSettings()
{
    SettingsService::saveWindowAndBoard(this, m_shogiView);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    saveWindowAndBoardSettings();
    QMainWindow::closeEvent(e);
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

// 「すぐ指させる」
void MainWindow::movePieceImmediately()
{
    if (m_match) {
        m_match->forceImmediateMove();
    }
}

void MainWindow::onPlayer1TimeOut()
{
    if (m_match) m_match->handlePlayerTimeOut(1); // 1 = 先手
}

void MainWindow::onPlayer2TimeOut()
{
    if (m_match) m_match->handlePlayerTimeOut(2); // 2 = 後手
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

// --- INavigationContext の実装 ---
bool MainWindow::hasResolvedRows() const
{
    return !m_resolvedRows.isEmpty();
}

int MainWindow::resolvedRowCount() const
{
    return m_resolvedRows.size();
}

int MainWindow::activeResolvedRow() const
{
    return m_activeResolvedRow;
}

int MainWindow::maxPlyAtRow(int row) const
{
    if (m_resolvedRows.isEmpty()) {
        // まず SFEN 履歴があればそれを優先（開始局面を含むので -1）
        if (m_sfenRecord && !m_sfenRecord->isEmpty())
            return m_sfenRecord->size() - 1;

        // フォールバックとして KIF 行数
        const int rows = (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : 0);
        return (rows > 0) ? (rows - 1) : 0;
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

                // ★ 修正点：currentPly() のベースになるトラッキングを更新
                m_activePly          = safe;   // ← 追加
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

        connect(m_recordPane, &RecordPane::mainRowChanged,
                this, &MainWindow::onRecordPaneMainRowChanged_,
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

    // 5) 起動直後に棋譜欄へ「=== 開始局面 ===」を用意（重複挿入を避ける）
    {
        if (m_kifuRecordModel) {
            const int rows = m_kifuRecordModel->rowCount();
            bool need = true;
            if (rows > 0) {
                const QModelIndex idx0 = m_kifuRecordModel->index(0, 0);
                const QString head = m_kifuRecordModel->data(idx0, Qt::DisplayRole).toString();
                if (head == QStringLiteral("=== 開始局面 ==="))
                    need = false;
            }
            if (need) {
                if (rows == 0) {
                    m_kifuRecordModel->appendItem(
                        new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                        QStringLiteral("（１手 / 合計）")));
                } else {
                    // 念のため既存行がある場合は先頭に差し込む
                    m_kifuRecordModel->prependItem(
                        new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                        QStringLiteral("（１手 / 合計）")));
                }
            }
        }
    }

    // （初回のみで良い UI 調整があれば firstTime を使って分岐できます）
    Q_UNUSED(firstTime);
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

    Q_ASSERT(m_analysisTab && m_tab && m_modelThinking1 && m_modelThinking2);

    // 分岐ツリーのアクティベートを MainWindow スロットへ（ラムダ不使用）
    QObject::connect(
        m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
        this,          &MainWindow::onBranchNodeActivated_,
        Qt::UniqueConnection);
}

void MainWindow::initMatchCoordinator()
{
    // 依存が揃っていない場合は何もしない
    if (!m_gameController || !m_shogiView) return;

    // まず時計を用意（nullでも可だが、あれば渡す）
    ensureClockReady_();

    using std::placeholders::_1;
    using std::placeholders::_2;

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

    // ★Presenterと同じリストを渡す（Single Source of Truth）
    d.sfenRecord = m_sfenRecord;

    // ---- ここは「コメントアウト」せず、関数バインドで割り当て ----
    d.hooks.appendEvalP1      = std::bind(&MainWindow::requestRedrawEngine1Eval_, this);
    d.hooks.appendEvalP2      = std::bind(&MainWindow::requestRedrawEngine2Eval_, this);
    d.hooks.sendGoToEngine    = std::bind(&MatchCoordinator::sendGoToEngine,   m_match, _1, _2);
    d.hooks.sendStopToEngine  = std::bind(&MatchCoordinator::sendStopToEngine, m_match, _1);
    d.hooks.sendRawToEngine   = std::bind(&MatchCoordinator::sendRawToEngine,  m_match, _1, _2);
    d.hooks.initializeNewGame = std::bind(&MainWindow::initializeNewGame_, this, _1);
    d.hooks.showMoveHighlights= std::bind(&MainWindow::showMoveHighlights_, this, _1, _2);
    d.hooks.appendKifuLine    = std::bind(&MainWindow::appendKifuLineHook_, this, _1, _2);

    // ★ 追加（今回の肝）：結果ダイアログ表示フックを配線
    d.hooks.showGameOverDialog = std::bind(&MainWindow::showGameOverMessageBox_, this, _1, _2);

    // ★★ 追加：時計から「残り/増加/秒読み」を司令塔へ提供するフックを配線
    d.hooks.remainingMsFor = std::bind(&MainWindow::getRemainingMsFor_, this, _1);
    d.hooks.incrementMsFor = std::bind(&MainWindow::getIncrementMsFor_, this, _1);
    d.hooks.byoyomiMs      = std::bind(&MainWindow::getByoyomiMs_, this);

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
        connect(m_shogiClock, &ShogiClock::resignationTriggered,
                this, &MainWindow::onResignationTriggered, Qt::UniqueConnection);
    }

    // 盤クリックの配線
    connectBoardClicks_();

    // 人間操作 → 合法判定＆適用の配線
    connectMoveRequested_();

    // 既定モード（必要に応じて開始時に上書き）
    if (m_boardController)
        m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
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

void MainWindow::onActionFlipBoardTriggered(bool /*checked*/)
{
    if (m_match) m_match->flipBoard();
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

    // ▼▼▼ 通常対局 ▼▼▼
    if (!m_gameController) {
        qWarning() << "[UI][WARN] m_gameController is null";
        return;
    }

    PlayMode matchMode = (m_match ? m_match->playMode() : NotStarted);
    PlayMode modeNow   = (m_playMode != NotStarted) ? m_playMode : matchMode;

    qInfo() << "[UI] effective modeNow=" << int(modeNow)
            << "(ui m_playMode=" << int(m_playMode) << ", matchMode=" << int(matchMode) << ")";

    // 着手前の手番（HvH/HvE 後処理で使用）
    const auto moverBefore = m_gameController->currentPlayer();

    // validateAndMove は参照引数なのでローカルに退避
    QPoint hFrom = from, hTo = to;

    // ★ 次の着手番号は「記録サイズ」を信頼する（既存ロジックのまま）
    const int recSizeBefore = (m_sfenRecord ? m_sfenRecord->size() : 0);
    const int nextIdx       = qMax(1, recSizeBefore);

    // ここで合法判定＆盤面反映。m_lastMove に「▲７六歩」のような整形済み文字列がセットされる
    const bool ok = m_gameController->validateAndMove(
        hFrom, hTo, m_lastMove, modeNow, const_cast<int&>(nextIdx), m_sfenRecord, m_gameMoves);

    if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, ok);
    if (!ok) {
        qInfo() << "[UI] validateAndMove failed (human move rejected)";
        return;
    }

    // UI 側の現在カーソルは、常に「記録サイズ」に同期させる
    if (m_sfenRecord) {
        m_currentMoveIndex = m_sfenRecord->size() - 1; // 末尾（直近の局面）
    }

    // --- 対局モードごとの後処理 ---
    switch (modeNow) {
    case HumanVsHuman: {
        qInfo() << "[UI] HvH: delegate post-human-move to MatchCoordinator";
        if (m_match) {
            // 直前手の考慮msを ShogiClock へ確定（MatchCoordinator 側で行う）
            m_match->onHumanMove_HvH(moverBefore);
        }

        // ★ 追加：HvH でも「指し手＋考慮時間」を棋譜欄に追記する
        // MatchCoordinator::onHumanMove_HvH() で直前手の考慮時間を ShogiClock に確定済み。
        QString elapsed;
        if (m_shogiClock) {
            elapsed = (moverBefore == ShogiGameController::Player1)
            ? m_shogiClock->getPlayer1ConsiderationAndTotalTime()
            : m_shogiClock->getPlayer2ConsiderationAndTotalTime();
        } else {
            ensureClockReady_();
            elapsed = QStringLiteral("00:00/00:00:00"); // フォールバック
        }

        // m_lastMove（validateAndMoveで得た表示用文字列）＋ elapsed を1行追記
        updateGameRecord(elapsed);
        break;
    }

    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        if (m_match) {
            qInfo() << "[UI] HvE: forwarding to MatchCoordinator::onHumanMove_HvE";
            m_match->onHumanMove_HvE(hFrom, hTo, m_lastMove);
        }
        break;

    default:
        qInfo() << "[UI] not a live play mode; skip post-handling";
        break;
    }
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
    // 司令塔が isOver / Cause / KIF一行追記 まで面倒を見る前提
    if (!st.isOver) return;

    // UI 遷移（閲覧モードへ）
    enableArrowButtons();
    setReplayMode(true);

    // ハイライト消去
    if (m_shogiView) {
        m_shogiView->removeHighlightAllData();
    }
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
    auto* gc = m_gameController;
    auto* tm = findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, this);
}

void MainWindow::saveKifuToFile()
{
    QString err;
    const QString saved = KifuSaveCoordinator::saveViaDialog(
        this, m_kifuDataList, m_playMode,
        m_humanName1, m_humanName2, m_engineName1, m_engineName2, &err);
    if (saved.isEmpty() && !err.isEmpty())
        displayErrorMessage(tr("An error occurred in saveKifuToFile. %1").arg(err));
    else if (!saved.isEmpty())
        kifuSaveFileName = saved;
}

void MainWindow::overwriteKifuFile()
{
    if (kifuSaveFileName.isEmpty()) { saveKifuToFile(); return; }
    QString err;
    if (!KifuSaveCoordinator::overwriteExisting(kifuSaveFileName, m_kifuDataList, &err))
        displayErrorMessage(tr("An error occurred in overwriteKifuFile. %1").arg(err));
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

void MainWindow::onPreStartCleanupRequested_()
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

    // 対局開始時、最初の1手が入る前にヘッダを用意しておく
    {
        if (m_kifuRecordModel) {
            m_kifuRecordModel->appendItem(
                new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                QStringLiteral("（１手 / 合計）")));
        }
    }

    // 評価値チャートは RecordPane が所有。毎回ゲッターで取得してクリア
    if (auto* ec = (m_recordPane ? m_recordPane->evalChart() : nullptr)) {
        ec->clearAll();
        ec->update(); // 念のため描画更新
    }

    // 変数類の初期化など（既存処理）
    m_scoreCp.clear();
    m_currentMoveIndex = 0;
    m_currentSelectedPly = 0;
    m_activePly = 0;

    // 分岐モデル
    if (m_kifuBranchModel) {
        m_kifuBranchModel->clear();
    }
    m_branchDisplayPlan.clear();

    // コメント欄（Presenter管理でも、見た目は一度クリア）
    broadcastComment(QString(), /*asHtml=*/true);

    // USIログの初期化（既存処理）
    auto resetInfo = [](UsiCommLogModel* m) {
        if (!m) return;
        m->clear();
        m->setEngineName(QString());
        m->setPredictiveMove(QString());
        m->setSearchedMove(QString());
        m->setSearchDepth(QString());
        m->setNodeCount(QString());
        m->setNodesPerSecond(QString());
        m->setHashUsage(QString());
    };
    resetInfo(m_lineEditModel1);
    resetInfo(m_lineEditModel2);

    // タブの選択を先頭へ
    if (m_tab) m_tab->setCurrentIndex(0);
}

void MainWindow::onApplyTimeControlRequested_(const GameStartCoordinator::TimeControl& tc)
{
    qDebug().noquote()
    << "[MW] onApplyTimeControlRequested_:"
    << " enabled=" << tc.enabled
    << " P1{base=" << tc.p1.baseMs << " byoyomi=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
    << " P2{base=" << tc.p2.baseMs << " byoyomi=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // 1) まず時計に適用
    TimeControlUtil::applyToClock(m_shogiClock, tc, m_startSfenStr, m_currentSfenStr);

    // 2) 司令塔へも必ず反映（これが無いと computeGoTimes_ が byoyomi を使いません）
    if (m_match) {
        const bool useByoyomi = (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0);

        // byoyomi を使う場合は inc は無視されます（司令塔側ロジックに合わせる）
        const qint64 byo1 = tc.p1.byoyomiMs;
        const qint64 byo2 = tc.p2.byoyomiMs;
        const qint64 inc1 = tc.p1.incrementMs;
        const qint64 inc2 = tc.p2.incrementMs;

        // 負け扱い（秒読みのみ運用でも true 推奨）
        const bool loseOnTimeout = true;

        qDebug().noquote()
            << "[MW] setTimeControlConfig to MatchCoordinator:"
            << " useByoyomi=" << useByoyomi
            << " byo1=" << byo1 << " byo2=" << byo2
            << " inc1=" << inc1 << " inc2=" << inc2
            << " loseOnTimeout=" << loseOnTimeout;

        m_match->setTimeControlConfig(useByoyomi, byo1, byo2, inc1, inc2, loseOnTimeout);
        // 反映直後に現在の go 用数値を再計算しておくと安全
        m_match->refreshGoTimes();
    }

    // 3) 表示更新
    if (m_shogiClock) {
        qDebug() << "[MW] clock after apply:"
                 << "P1=" << m_shogiClock->getPlayer1TimeIntMs()
                 << "P2=" << m_shogiClock->getPlayer2TimeIntMs()
                 << "byo=" << m_shogiClock->getCommonByoyomiMs()
                 << "binc=" << m_shogiClock->getBincMs()
                 << "winc=" << m_shogiClock->getWincMs();
        m_shogiClock->updateClock();
    }
    if (m_shogiView) m_shogiView->update();
}

void MainWindow::ensureRecordPresenter_()
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

// UIスレッド安全のため queued 呼び出しにしています
void MainWindow::requestRedrawEngine1Eval_()
{
    QMetaObject::invokeMethod(this, &MainWindow::redrawEngine1EvaluationGraph, Qt::QueuedConnection);
}

void MainWindow::requestRedrawEngine2Eval_()
{
    QMetaObject::invokeMethod(this, &MainWindow::redrawEngine2EvaluationGraph, Qt::QueuedConnection);
}

void MainWindow::initializeNewGame_(const QString& s)
{
    // --- 司令塔からのコールバック：UI側の初期化のみ行う ---
    QString startSfenStr = s;              // initializeNewGame(QString&) が参照で受けるため可変にコピー

    // 盤モデルの初期化（従来の UI 側初期化）
    initializeNewGame(startSfenStr);

    // 盤の再描画・サイズ調整
    if (m_shogiView && m_gameController && m_gameController->board()) {
        m_shogiView->applyBoardAndRender(m_gameController->board());
        m_shogiView->configureFixedSizing();
    }

    // 表示名の更新（必要に応じて）
    setPlayersNamesForMode();
    setEngineNamesBasedOnMode();
}

void MainWindow::showMoveHighlights_(const QPoint& from, const QPoint& to)
{
    if (m_boardController) m_boardController->showMoveHighlights(from, to);
}

void MainWindow::appendKifuLineHook_(const QString& text, const QString& elapsed)
{
    appendKifuLine(text, elapsed);
}

void MainWindow::onRecordRowChangedByPresenter(int row, const QString& comment)
{
    const int modelRowsBefore =
        (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);
    const int presenterCurBefore =
        (m_recordPresenter ? m_recordPresenter->currentRow() : -1);

    qDebug().noquote()
        << "[MW] onRecordRowChangedByPresenter ENTER"
        << " row=" << row
        << " comment.len=" << comment.size()
        << " modelRows(before)=" << modelRowsBefore
        << " presenter.cur(before)=" << presenterCurBefore
        << " tracking.before{ activePly=" << m_activePly
        << ", currentSelectedPly=" << m_currentSelectedPly
        << ", currentMoveIndex=" << m_currentMoveIndex << " }";

    // 盤面・ハイライト同期
    if (row >= 0) {
        qDebug().noquote() << "[MW] syncBoardAndHighlightsAtRow CALL row=" << row;
        syncBoardAndHighlightsAtRow(row);
        qDebug().noquote() << "[MW] syncBoardAndHighlightsAtRow DONE row=" << row;

        // ▼ 現在手数トラッキングを更新（NavigationController::next/prev 用）
        m_activePly          = row;   // ← これが無いと currentPly() が 0 のまま
        m_currentSelectedPly = row;
        m_currentMoveIndex   = row;

        qDebug().noquote()
            << "[MW] tracking UPDATED"
            << " activePly=" << m_activePly
            << " currentSelectedPly=" << m_currentSelectedPly
            << " currentMoveIndex=" << m_currentMoveIndex;

        // ▼ 分岐候補欄の更新は Coordinator へ直接委譲
        if (m_kifuLoadCoordinator) {
            const int rows        = m_resolvedRows.size();
            const int resolvedRow = (rows <= 0) ? 0 : qBound(0, m_activeResolvedRow, rows - 1);
            const int safePly     = (row < 0) ? 0 : row;

            qDebug().noquote()
                << "[MW] showBranchCandidates CALL"
                << " resolvedRows.size=" << rows
                << " activeResolvedRow=" << m_activeResolvedRow
                << " resolvedRow=" << resolvedRow
                << " safePly=" << safePly;

            m_kifuLoadCoordinator->showBranchCandidates(resolvedRow, safePly);

            qDebug().noquote() << "[MW] showBranchCandidates DONE";
        } else {
            qWarning().noquote() << "[MW] m_kifuLoadCoordinator is nullptr; skip showBranchCandidates";
        }
    } else {
        qWarning().noquote() << "[MW] row < 0; skip sync/tracking";
    }

    // コメント表示は既存の一括関数に統一
    const QString cmt = comment.trimmed();
    qDebug().noquote()
        << "[MW] broadcastComment"
        << " empty?=" << cmt.isEmpty()
        << " len=" << cmt.size();
    broadcastComment(cmt.isEmpty() ? tr("コメントなし") : cmt, /*asHtml=*/true);

    // 矢印ボタンなどの活性化（直前の状態を可視化）
    const int modelRowsAfter =
        (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);
    const int presenterCurAfter =
        (m_recordPresenter ? m_recordPresenter->currentRow() : -1);
    const bool canPrevComputed = (presenterCurAfter > 0);
    const bool canNextComputed = (modelRowsAfter >= 0) ? (presenterCurAfter < modelRowsAfter - 1) : false;

    qDebug().noquote()
        << "[MW] enableArrowButtons BEFORE"
        << " presenter.cur(after)=" << presenterCurAfter
        << " modelRows(after)=" << modelRowsAfter
        << " canPrev(computed)=" << canPrevComputed
        << " canNext(computed)=" << canNextComputed
        << " atLastRow?=" << (presenterCurAfter >= 0 && modelRowsAfter >= 0 && presenterCurAfter == modelRowsAfter - 1);

    enableArrowButtons();

    // ここでは UI の有効/無効を直接読めないため、計算値を再掲
    qDebug().noquote()
        << "[MW] enableArrowButtons AFTER (restate)"
        << " presenter.cur=" << presenterCurAfter
        << " modelRows=" << modelRowsAfter
        << " canPrev(computed)=" << canPrevComputed
        << " canNext(computed)=" << canNextComputed;

    qDebug().noquote() << "[MW] onRecordRowChangedByPresenter LEAVE";
}

void MainWindow::onFlowError_(const QString& msg)
{
    // 既存のエラー表示に委譲（UI 専用）
    displayErrorMessage(msg);
}

void MainWindow::onResignationTriggered()
{
    // 既存の投了処理に委譲（m_matchの有無は中で判定）
    handleResignation();
}

qint64 MainWindow::getRemainingMsFor_(MatchCoordinator::Player p) const
{
    if (!m_shogiClock) {
        qDebug() << "[MW] getRemainingMsFor_: clock=null";
        return 0;
    }
    const qint64 p1 = m_shogiClock->getPlayer1TimeIntMs();
    const qint64 p2 = m_shogiClock->getPlayer2TimeIntMs();
    qDebug() << "[MW] getRemainingMsFor_: P1=" << p1 << " P2=" << p2
             << " req=" << (p==MatchCoordinator::P1?"P1":"P2");
    return (p == MatchCoordinator::P1) ? p1 : p2;
}

qint64 MainWindow::getIncrementMsFor_(MatchCoordinator::Player p) const
{
    if (!m_shogiClock) {
        qDebug() << "[MW] getIncrementMsFor_: clock=null";
        return 0;
    }
    const qint64 inc1 = m_shogiClock->getBincMs();
    const qint64 inc2 = m_shogiClock->getWincMs();
    qDebug() << "[MW] getIncrementMsFor_: binc=" << inc1 << " winc=" << inc2
             << " req=" << (p==MatchCoordinator::P1?"P1":"P2");
    return (p == MatchCoordinator::P1) ? inc1 : inc2;
}

qint64 MainWindow::getByoyomiMs_() const
{
    const qint64 byo = m_shogiClock ? m_shogiClock->getCommonByoyomiMs() : 0;
    qDebug() << "[MW] getByoyomiMs_: ms=" << byo;
    return byo;
}

// 対局終了時のタイトルと本文を受け取り、情報ダイアログを表示するだけのヘルパ
void MainWindow::showGameOverMessageBox_(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::onRecordPaneMainRowChanged_(int row)
{
    // KifuLoadCoordinator が用意できていれば正式ルートへ委譲
    if (m_kifuLoadCoordinator) {
        m_kifuLoadCoordinator->onMainMoveRowChanged(row);
        return;
    }

    // フォールバック：起動直後など Loader 未生成時でも UI が動くように最低限の同期を行う
    if (row >= 0) {
        syncBoardAndHighlightsAtRow(row);

        // ★ 修正点：currentPly() のベースになるトラッキングを更新
        m_activePly          = row;  // ← 追加
        m_currentSelectedPly = row;
        m_currentMoveIndex   = row;
    }
    enableArrowButtons();
}
