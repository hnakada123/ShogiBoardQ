#include <QMessageBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QToolButton>
#include <QWidgetAction>
#include <QMetaType>
#include <QDebug>
#include <QScrollBar>
#include <QPushButton>
#include <QSignalBlocker>  // ★ 追加
#include <QLabel>          // ★ 追加
#include <QApplication>    // ★ 追加
#include <QClipboard>      // ★ 追加
#include <QLineEdit>       // ★ 追加
#include <QWheelEvent>     // ★ 追加: Ctrl+ホイール処理用
#include <functional>
#include <limits>
#include <QPainter>
#include <QFontDatabase>

#include "mainwindow.h"
#include "branchwiringcoordinator.h"
#include "considerationflowcontroller.h"
#include "gamelayoutbuilder.h"
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
#include "navigationcontroller.h"
#include "uiactionswiring.h"
#include "kifucontentbuilder.h"
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
#include "recordnavigationcontroller.h" // ★ 追加: 棋譜ナビゲーション管理
#include "positioneditcoordinator.h"    // ★ 追加: 局面編集調整
#include "csagamedialog.h"              // ★ 追加: CSA通信対局ダイアログ
#include "csagamecoordinator.h"         // ★ 追加: CSA通信対局コーディネータ
#include "csawaitingdialog.h"           // ★ 追加: CSA通信対局待機ダイアログ
#include "josekiwindowwiring.h"          // ★ 追加: 定跡ウィンドウUI配線
#include "csagamewiring.h"              // ★ 追加: CSA通信対局UI配線
#include "playerinfowiring.h"           // ★ 追加: 対局情報UI配線
#include "prestartcleanuphandler.h"     // ★ 追加: 対局開始前クリーンアップ

// ★ コメント整形ヘルパ：KifuContentBuilderへ委譲
namespace {
static QString toRichHtmlWithStarBreaksAndLinks(const QString& raw)
{
    return KifuContentBuilder::toRichHtmlWithStarBreaksAndLinks(raw);
}
} // anonymous namespace

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

    setupCentralWidgetContainer_();

    configureToolBarFromUi_();

    // コア部品（GC, View, 盤モデル etc.）は既存関数で初期化
    initializeComponents();

    if (!m_timePresenter) m_timePresenter = new TimeDisplayPresenter(m_shogiView, this);

    // TimeControlControllerを初期化してTimeDisplayPresenterに設定
    ensureTimeController_();
    if (m_timePresenter && m_timeController) {
        m_timePresenter->setClock(m_timeController->clock());
    }

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

    // 起動時用：編集メニューを“編集前（未編集）”の初期状態にする
    initializeEditMenuForStartup();
    
    // 評価値グラフ高さ調整用タイマーを初期化（デバウンス処理用）
    m_evalChartResizeTimer = new QTimer(this);
    m_evalChartResizeTimer->setSingleShot(true);
    connect(m_evalChartResizeTimer, &QTimer::timeout,
            this, &MainWindow::performDeferredEvalChartResize);
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

void MainWindow::connectCoreSignals_()
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
        // Ctrl+ホイールイベントを横取りして、MainWindow側で同期的に処理する
        // これにより盤サイズ変更時のちらつきを防止
        m_shogiView->installEventFilter(this);
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
        // 2手戻しを実行し、成功した場合は分岐ツリーを更新する
        if (m_match->undoTwoPlies()) {
            // 短くなった棋譜データに基づいて分岐ツリー（長方形と罫線）を再描画・同期する
            refreshBranchTreeLive_();

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
    ensurePlayerInfoController_();
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
    ensurePlayerInfoController_();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_playMode);
        m_playerInfoController->setEngineNames(m_engineName1, m_engineName2);
        m_playerInfoController->applyEngineNamesToLogModels();
    }
}

// ★ 追加: EvE対局時に2番目のエンジン情報を表示する
void MainWindow::updateSecondEngineVisibility()
{
    ensurePlayerInfoController_();
    if (m_playerInfoController) {
        m_playerInfoController->setPlayMode(m_playMode);
        m_playerInfoController->updateSecondEngineVisibility();
    }
}

// 対局者名と残り時間、将棋盤と棋譜、矢印ボタン、評価値グラフのグループを横に並べて表示する。
void MainWindow::setupHorizontalGameLayout()
{
    if (!m_layoutBuilder) {
        GameLayoutBuilder::Deps d;
        d.shogiView          = m_shogiView;
        d.recordPaneOrWidget = m_gameRecordLayoutWidget;
        d.analysisTabWidget  = m_tab; // 下段タブ
        m_layoutBuilder = new GameLayoutBuilder(d, this);
    }
    m_hsplit = m_layoutBuilder->buildHorizontalSplit();
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

    // レイアウトを空にする（既存のウィジェットはレイアウトから外すだけ）
    clearLayout(m_centralLayout);

    // 必須部品の存在チェック（無ければ何もしない）
    if (!m_shogiView || !m_gameRecordLayoutWidget || !m_tab) {
        qWarning().noquote()
        << "[initializeCentralGameDisplay] missing widget(s)."
        << " shogiView=" << (m_shogiView!=nullptr)
        << " recordPaneOrWidget=" << (m_gameRecordLayoutWidget!=nullptr)
        << " tab=" << (m_tab!=nullptr);
        return;
    }

    // ★ 重要：
    // setupHorizontalGameLayout() は setupEngineAnalysisTab() より先に呼ばれるため、
    // 起動順によっては m_tab 未設定のまま m_layoutBuilder が生成されている。
    // ここで一度 Builder を作り直し、最新の依存を確実に反映させる。
    if (m_layoutBuilder) {
        m_layoutBuilder->deleteLater();
        m_layoutBuilder = nullptr;
    }
    {
        GameLayoutBuilder::Deps d;
        d.shogiView          = m_shogiView;
        d.recordPaneOrWidget = m_gameRecordLayoutWidget; // 右ペイン（RecordPane）
        d.analysisTabWidget  = m_tab;                    // 下段タブ（EngineAnalysisTab を内包）
        m_layoutBuilder = new GameLayoutBuilder(d, this);
    }

    // 左右スプリッタを生成して保持（以後も m_hsplit を参照する箇所があるため）
    m_hsplit = m_layoutBuilder->buildHorizontalSplit();
    Q_ASSERT(m_hsplit);
    Q_ASSERT(m_tab);

    // 中央レイアウトへ追加：上=スプリッタ、下=タブ
    m_centralLayout->addWidget(m_hsplit);
    m_centralLayout->addWidget(m_tab);

    // 見た目調整：上を広く、下のタブは控えめ
    m_centralLayout->setStretch(0, 1); // splitter
    m_centralLayout->setStretch(1, 0); // analysis tab
}

void MainWindow::startNewShogiGame(QString& startSfenStr)
{
    ensureReplayController_();
    const bool resume = m_replayController ? m_replayController->isResumeFromCurrent() : false;

    // 対局終了時のスタイルロックを解除
    if (m_shogiView) m_shogiView->setGameOverStyleLock(false);

    // 評価値グラフ等の初期化
    if (auto ec = m_recordPane ? m_recordPane->evalChart() : nullptr) {
        if (!resume) ec->clearAll();
    }
    if (!resume) {
        ensureEvaluationGraphController_();
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
    ensureGameStateController_();
    if (m_gameStateController) {
        m_gameStateController->handleResignation();
    }
}

void MainWindow::redrawEngine1EvaluationGraph(int ply)
{
    ensureEvaluationGraphController_();
    if (m_evalGraphController) {
        m_evalGraphController->redrawEngine1Graph(ply);
    }
}

void MainWindow::redrawEngine2EvaluationGraph(int ply)
{
    ensureEvaluationGraphController_();
    if (m_evalGraphController) {
        m_evalGraphController->redrawEngine2Graph(ply);
    }
}

// ★ 追加: EvaluationGraphControllerの初期化
void MainWindow::ensureEvaluationGraphController_()
{
    if (m_evalGraphController) return;

    m_evalGraphController = new EvaluationGraphController(this);
    m_evalGraphController->setRecordPane(m_recordPane);
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

    ensureTimeController_();
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
    ensureDialogCoordinator_();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showVersionInformation();
    }
}

void MainWindow::openWebsiteInExternalBrowser()
{
    ensureDialogCoordinator_();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->openProjectWebsite();
    }
}

void MainWindow::displayEngineSettingsDialog()
{
    ensureDialogCoordinator_();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showEngineSettingsDialog();
    }
}

// 成る・不成の選択ダイアログを起動する。
void MainWindow::displayPromotionDialog()
{
    if (!m_gameController) return;
    ensureDialogCoordinator_();
    if (m_dialogCoordinator) {
        const bool promote = m_dialogCoordinator->showPromotionDialog();
        m_gameController->setPromote(promote);
    }
}

// 検討ダイアログを表示する。
void MainWindow::displayConsiderationDialog()
{
    // UI 側の状態保持（従来どおり）
    m_playMode = PlayMode::ConsiderationMode;

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

    ensureDialogCoordinator_();
    if (m_dialogCoordinator) {
        DialogCoordinator::ConsiderationParams params;
        params.position = position;
        m_dialogCoordinator->showConsiderationDialog(params);
    }
}

// 詰み探索ダイアログを表示する。
// CSA通信対局ダイアログを表示する。
void MainWindow::displayJosekiWindow()
{
    ensureJosekiWiring_();
    if (m_josekiWiring) {
        m_josekiWiring->displayJosekiWindow();
    }
}

void MainWindow::updateJosekiWindow()
{
    if (m_josekiWiring) {
        m_josekiWiring->updateJosekiWindow();
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
        ensureCsaGameWiring_();
        if (!m_csaGameWiring) {
            qWarning().noquote() << "[MainWindow] displayCsaGameDialog: CsaGameWiring is null";
            return;
        }

        // BoardSetupControllerを確保して設定
        ensureBoardSetupController_();
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
                    this, &MainWindow::onCsaEngineScoreUpdated_,
                    Qt::UniqueConnection);
        }
    }
}

void MainWindow::displayTsumeShogiSearchDialog()
{
    // 解析モード切替
    m_playMode = PlayMode::TsumiSearchMode;

    ensureDialogCoordinator_();
    if (m_dialogCoordinator) {
        DialogCoordinator::TsumeSearchParams params;
        params.sfenRecord = m_sfenRecord;
        params.startSfenStr = m_startSfenStr;
        params.positionStrList = m_positionStrList;
        params.currentMoveIndex = qMax(0, m_currentMoveIndex);
        m_dialogCoordinator->showTsumeSearchDialog(params);
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

    ensureDialogCoordinator_();
    if (!m_dialogCoordinator) return;

    // 解析モデルとタブを設定
    m_dialogCoordinator->setAnalysisModel(m_analysisModel);
    m_dialogCoordinator->setAnalysisTab(m_analysisTab);
    m_dialogCoordinator->setUsiEngine(m_usi1);
    m_dialogCoordinator->setLogModel(m_lineEditModel1);
    m_dialogCoordinator->setThinkingModel(m_modelThinking1);

    // 解析進捗シグナルを接続
    QObject::connect(m_dialogCoordinator, &DialogCoordinator::analysisProgressReported,
                     this, &MainWindow::onKifuAnalysisProgress, Qt::UniqueConnection);
    
    // 解析結果行選択シグナルを接続（棋譜欄・将棋盤・分岐ツリー連動用）
    QObject::connect(m_dialogCoordinator, &DialogCoordinator::analysisResultRowSelected,
                     this, &MainWindow::onKifuAnalysisResultRowSelected, Qt::UniqueConnection);

    // コンテキストを設定
    ensureGameInfoController_();
    DialogCoordinator::KifuAnalysisContext ctx;
    ctx.sfenRecord = m_sfenRecord;
    ctx.moveRecords = m_moveRecords;
    ctx.recordModel = m_kifuRecordModel;
    ctx.activePly = &m_activePly;
    ctx.gameController = m_gameController;
    ctx.gameInfoController = m_gameInfoController;
    ctx.kifuLoadCoordinator = m_kifuLoadCoordinator;
    ctx.recordPane = m_recordPane;
    ctx.gameUsiMoves = &m_gameUsiMoves;  // 対局時のUSI形式指し手リスト
    m_dialogCoordinator->setKifuAnalysisContext(ctx);

    // コンテキストから自動パラメータ構築してダイアログを表示
    m_dialogCoordinator->showKifuAnalysisDialogFromContext();
}

void MainWindow::cancelKifuAnalysis()
{
    qDebug().noquote() << "[MainWindow::cancelKifuAnalysis] called";
    
    ensureDialogCoordinator_();
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
    ensureRecordNavigationController_();
    if (m_recordNavController) {
        qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] calling navigateKifuViewToRow(" << ply << ")";
        m_recordNavController->navigateKifuViewToRow(ply);
    } else {
        qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] m_recordNavController is null!";
    }
    
    // 2) 評価値グラフに評価値をプロット
    //    ※AnalysisFlowControllerで既に先手視点に変換済み（後手番は符号反転済み）
    //    ※scoreCpがINT_MINの場合は位置移動のみ（評価値追加しない）
    static constexpr int POSITION_ONLY_MARKER = std::numeric_limits<int>::min();
    if (scoreCp != POSITION_ONLY_MARKER && m_recordPane) {
        EvaluationChartWidget* ec = m_recordPane->evalChart();
        if (ec) {
            ec->appendScoreP1(ply, scoreCp, false);
            qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] appended score to chart: ply=" << ply 
                               << "scoreCp=" << scoreCp;
        } else {
            qDebug().noquote() << "[MainWindow::onKifuAnalysisProgress] evalChart is null!";
        }
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
    ensureRecordNavigationController_();
    if (m_recordNavController) {
        qDebug().noquote() << "[MainWindow::onKifuAnalysisResultRowSelected] calling navigateKifuViewToRow(" << ply << ")";
        m_recordNavController->navigateKifuViewToRow(ply);
    }
    
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

QString MainWindow::resolveCurrentSfenForGameStart_() const
{
    // 1) 棋譜SFENリストの「選択手」から取得（最優先）
    if (m_sfenRecord) {
        const qsizetype size = m_sfenRecord->size();
        // m_currentSelectedPly が [0..size-1] のインデックスである前提（本プロジェクトの慣習）
        // 1始まりの場合はプロジェクト実装に合わせて +0 / -1 調整してください。
        int idx = m_currentSelectedPly;
        if (idx < 0) {
            // 0手目（開始局面）などのとき
            idx = 0;
        }
        if (idx >= 0 && idx < size) {
            const QString s = m_sfenRecord->at(idx).trimmed();
            if (!s.isEmpty()) return s;
        }
    }

    // 2) フォールバックなし（司令塔側が安全に処理）
    return QString();
}

// 対局を開始する。
void MainWindow::initializeGame()
{
    ensureGameStartCoordinator_();

    // ★ 平手SFENが優先されてしまう問題の根本対策：
    //    ダイアログ確定直後に司令塔へ渡す前に、startSfen を明示クリアし、
    //    currentSfen を「選択中の手のSFEN（最優先）→それがなければ空」の順で決定しておく。
    m_startSfenStr.clear();

    // 現在の局面SFEN（棋譜レコードから最優先で取得）
    {
        const QString sfen = resolveCurrentSfenForGameStart_().trimmed();
        if (!sfen.isEmpty()) {
            m_currentSfenStr = sfen;
        } else {
            // 何も取れないケースは珍しいが、空のままでも司令塔側で安全にフォールバックされる。
            // ここでは何もしない（ログのみ）
            qDebug().noquote() << "[INIT] resolveCurrentSfenForGameStart_: empty. delegate to coordinator.";
        }
    }

    GameStartCoordinator::Ctx c;
    c.view            = m_shogiView;
    c.gc              = m_gameController;
    c.clock           = m_timeController ? m_timeController->clock() : nullptr;
    c.sfenRecord      = m_sfenRecord;          // QStringList*
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
    ensureGameInfoController_();

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
        /* gameInfoTable       */ m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr,
        /* gameInfoDock        */ nullptr,  // GameInfoPaneControllerに移行済み
        /* tab                 */ m_tab,
        /* recordPane          */ m_recordPane,
        /* kifuRecordModel     */ m_kifuRecordModel,
        /* kifuBranchModel     */ m_kifuBranchModel,
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

    // --- 4) 読み込み実行（ロジックは Coordinator へ） ---
    // 拡張子判定
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
}

void MainWindow::displayGameRecord(const QList<KifDisplayItem> disp)
{
    if (!m_kifuRecordModel) return;

    ensureRecordPresenter_();
    if (!m_recordPresenter) return;

    const qsizetype moveCount = disp.size();
    const int rowCount  = (m_sfenRecord && !m_sfenRecord->isEmpty())
                             ? static_cast<int>(m_sfenRecord->size())
                             : static_cast<int>(moveCount + 1);

    // ★ GameRecordModel を初期化
    ensureGameRecordModel_();
    if (m_gameRecord) {
        m_gameRecord->initializeFromDisplayItems(disp, rowCount);
    }

    // ★ m_commentsByRow も同期（互換性のため）
    m_commentsByRow.clear();
    m_commentsByRow.resize(rowCount);
    for (qsizetype i = 0; i < disp.size() && i < rowCount; ++i) {
        m_commentsByRow[i] = disp[i].comment;
    }
    qDebug().noquote() << "[MW] displayGameRecord: initialized with" << rowCount << "entries";

    // ← まとめて Presenter 側に委譲
    m_recordPresenter->displayAndWire(disp, rowCount, m_recordPane);
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

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // ShogiViewのCtrl+ホイールイベントを横取りして処理
    // これにより盤サイズ変更とラベル更新を同期的に行い、ちらつきを防止
    if (watched == m_shogiView && event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            const int delta = wheelEvent->angleDelta().y();
            if (delta > 0) {
                // 上方向スクロール → 拡大
                // シグナル発火を抑制して同期的に処理
                m_shogiView->enlargeBoard(false);
            } else if (delta < 0) {
                // 下方向スクロール → 縮小
                // シグナル発火を抑制して同期的に処理
                m_shogiView->reduceBoard(false);
            }
            // 評価値グラフの高さ自動調整は無効（QSplitterのリサイズがちらつきの原因となるため）
            return true;  // イベントを消費（ShogiViewには渡さない）
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onReverseTriggered()
{
    if (m_match) m_match->flipBoard();
}

// 起動時用：編集メニューを“編集前（未編集）”の初期状態にする
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
    ui->flatHandInitialPosition->setVisible(editing);
    ui->shogiProblemInitialPosition->setVisible(editing);
    ui->returnAllPiecesOnStand->setVisible(editing);
    ui->reversal->setVisible(editing);
    ui->turnaround->setVisible(editing);

    // ※ QMenu名は .ui 上で "Edit"（= ui->Edit）です。必要なら再描画。
    if (ui->Edit) {
        ui->Edit->update();
    }
}

void MainWindow::beginPositionEditing()
{
    ensurePositionEditCoordinator_();
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
    ensurePositionEditCoordinator_();
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
    ensureGameStateController_();
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
bool MainWindow::isHumanTurnNow_() const
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
void MainWindow::ensureGameInfoController_()
{
    ensurePlayerInfoWiring_();
    if (m_playerInfoWiring && !m_gameInfoController) {
        // ★ 追加: PlayerInfoWiring側でGameInfoPaneControllerを確実に生成
        m_playerInfoWiring->ensureGameInfoController();
        m_gameInfoController = m_playerInfoWiring->gameInfoController();
    }
}

// ★ 追加: 起動時に対局情報タブを追加（PlayerInfoWiring経由）
void MainWindow::addGameInfoTabAtStartup_()
{
    ensurePlayerInfoWiring_();
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
void MainWindow::populateDefaultGameInfo_()
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
void MainWindow::onSetPlayersNames_(const QString& p1, const QString& p2)
{
    ensurePlayerInfoWiring_();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->onSetPlayersNames(p1, p2);
    }
}

// ★ 追加: エンジン名設定フック（PlayerInfoWiring経由）
void MainWindow::onSetEngineNames_(const QString& e1, const QString& e2)
{
    ensurePlayerInfoWiring_();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->onSetEngineNames(e1, e2);
    }
}

// ★ 追加: 対局情報タブの先手・後手名を更新（PlayerInfoWiring経由）
void MainWindow::updateGameInfoPlayerNames_(const QString& blackName, const QString& whiteName)
{
    ensurePlayerInfoWiring_();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->updateGameInfoPlayerNames(blackName, whiteName);
    }
}

// ★ 追加: 元の対局情報を保存（PlayerInfoWiring経由）
void MainWindow::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    ensurePlayerInfoWiring_();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->setOriginalGameInfo(items);
    }
}

// ★ 追加: 現在の対局に基づいて対局情報タブを更新（PlayerInfoWiring経由）
void MainWindow::updateGameInfoForCurrentMatch_()
{
    ensurePlayerInfoWiring_();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->updateGameInfoForCurrentMatch();
    }
}

// ★ 追加: 対局者名確定時のスロット（PlayerInfoWiring経由）
void MainWindow::onPlayerNamesResolved_(const QString& human1, const QString& human2,
                                        const QString& engine1, const QString& engine2,
                                        int playMode)
{
    qDebug().noquote() << "[MW] onPlayerNamesResolved_: playMode=" << playMode;

    ensurePlayerInfoWiring_();
    if (m_playerInfoWiring) {
        m_playerInfoWiring->onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);
    }
}

// ★ 追加: GameInfoPaneControllerからの更新通知
void MainWindow::onGameInfoUpdated_(const QList<KifGameInfoItem>& items)
{
    qDebug().noquote() << "[MW] onGameInfoUpdated_: Game info updated, items=" << items.size();
}

void MainWindow::syncBoardAndHighlightsAtRow(int ply)
{
    qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow ENTER ply=" << ply;
    ensureRecordNavigationController_();
    if (m_recordNavController) {
        m_recordNavController->syncBoardAndHighlightsAtRow(ply);
    }
    
    // m_currentSfenStrを現在の局面に更新
    if (m_sfenRecord && ply >= 0 && ply < m_sfenRecord->size()) {
        m_currentSfenStr = m_sfenRecord->at(ply);
        qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow: updated m_currentSfenStr=" << m_currentSfenStr;
    }
    
    // 定跡ウィンドウを更新
    updateJosekiWindow();
    
    qDebug() << "[MW-DEBUG] syncBoardAndHighlightsAtRow LEAVE";
}

void MainWindow::applyResolvedRowAndSelect(int row, int selPly)
{
    if (!m_kifuLoadCoordinator) return;

    // 状態の差し替え（disp/sfen/gm）と Presenter 更新は Coordinator 側の責務
    m_kifuLoadCoordinator->applyResolvedRowAndSelect(row, selPly);

    // ★ 追加：盤面適用後に手番表示を更新
    setCurrentTurn();
    
    // m_currentSfenStrを選択した局面に更新
    if (m_sfenRecord && selPly >= 0 && selPly < m_sfenRecord->size()) {
        m_currentSfenStr = m_sfenRecord->at(selPly);
        qDebug() << "[JosekiWindow] applyResolvedRowAndSelect: row=" << row << "selPly=" << selPly << "sfen=" << m_currentSfenStr;
    }
    
    // 定跡ウィンドウを更新
    updateJosekiWindow();
}

// --- INavigationContext の実装 ---
bool MainWindow::hasResolvedRows() const
{
    return !m_resolvedRows.isEmpty();
}

int MainWindow::resolvedRowCount() const
{
    return static_cast<int>(m_resolvedRows.size());
}

int MainWindow::activeResolvedRow() const
{
    return m_activeResolvedRow;
}

int MainWindow::maxPlyAtRow(int row) const
{
    if (m_resolvedRows.isEmpty()) {
        // ライブ（解決済み行なし）のとき：
        // - SFEN: 「開始局面 + 実手数」なので終局行（投了/時間切れ）は含まれない → size()-1
        // - 棋譜欄: 「実手 + 終局行（あれば）」が入る → rowCount()-1
        // 末尾へ進める上限は「どちらか大きい方」を採用する。
        const int sfenMax = (m_sfenRecord && !m_sfenRecord->isEmpty())
                                ? static_cast<int>(m_sfenRecord->size() - 1)
                                : 0;
        const int kifuMax = (m_kifuRecordModel && m_kifuRecordModel->rowCount() > 0)
                                ? (m_kifuRecordModel->rowCount() - 1)
                                : 0;
        const int result = qMax(sfenMax, kifuMax);
        qDebug().noquote() << "[MW-DEBUG] maxPlyAtRow (live): row=" << row
                           << "sfenMax=" << sfenMax << "kifuMax=" << kifuMax
                           << "result=" << result;
        return result;
    }

    // 既に解決済み行がある（棋譜ファイル読み込み後など）のとき：
    // その行に表示するエントリ数（disp.size()）が末尾。
    const int clamped = static_cast<int>(qBound(qsizetype(0), qsizetype(row), m_resolvedRows.size() - 1));
    const int result = static_cast<int>(m_resolvedRows[clamped].disp.size());
    qDebug().noquote() << "[MW-DEBUG] maxPlyAtRow (resolved): row=" << row
                       << "clamped=" << clamped << "result=" << result;
    return result;
}

int MainWindow::currentPly() const
{
    // ★ リプレイ／再開（ライブ追記）中は UI 側のトラッキング値を優先
    const bool liveAppend = m_replayController ? m_replayController->isLiveAppendMode() : false;

    qDebug().noquote() << "[MW-DEBUG] currentPly(): liveAppend=" << liveAppend
                       << "m_currentSelectedPly=" << m_currentSelectedPly
                       << "m_activePly=" << m_activePly;

    if (liveAppend) {
        if (m_currentSelectedPly >= 0) {
            qDebug().noquote() << "[MW-DEBUG] currentPly() returning m_currentSelectedPly=" << m_currentSelectedPly;
            return m_currentSelectedPly;
        }

        // 念のためビューの currentIndex もフォールバックに
        const QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
        if (view) {
            const QModelIndex cur = view->currentIndex();
            if (cur.isValid()) {
                int result = qMax(0, cur.row());
                qDebug().noquote() << "[MW-DEBUG] currentPly() (liveAppend) returning view.currentIndex.row=" << result;
                return result;
            }
        }
        qDebug().noquote() << "[MW-DEBUG] currentPly() (liveAppend) returning 0 (fallback)";
        return 0;
    }

    // 通常時は従来通り m_activePly を優先
    if (m_activePly >= 0) {
        qDebug().noquote() << "[MW-DEBUG] currentPly() returning m_activePly=" << m_activePly;
        return m_activePly;
    }

    const QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (view) {
        const QModelIndex cur = view->currentIndex();
        if (cur.isValid()) {
            int result = qMax(0, cur.row());
            qDebug().noquote() << "[MW-DEBUG] currentPly() returning view.currentIndex.row=" << result;
            return result;
        }
    }
    qDebug().noquote() << "[MW-DEBUG] currentPly() returning 0 (final fallback)";
    return 0;
}

void MainWindow::applySelect(int row, int ply)
{
    ensureRecordNavigationController_();
    if (m_recordNavController) {
        m_recordNavController->applySelect(row, ply);
    }
    
    // m_currentSfenStrを選択した局面に更新
    if (m_sfenRecord && ply >= 0 && ply < m_sfenRecord->size()) {
        m_currentSfenStr = m_sfenRecord->at(ply);
        qDebug() << "[JosekiWindow] applySelect: row=" << row << "ply=" << ply << "sfen=" << m_currentSfenStr;
    }
    
    // 定跡ウィンドウを更新
    updateJosekiWindow();
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
        d.navCtx      = dynamic_cast<INavigationContext*>(this); // NavigationController 用
        d.recordModel = m_kifuRecordModel;
        d.branchModel = m_kifuBranchModel;

        m_recordPaneWiring = new RecordPaneWiring(d, this);
    }

    // RecordPane の構築と配線
    m_recordPaneWiring->buildUiAndWire();

    // 生成物の取得
    m_recordPane = m_recordPaneWiring->pane();
    m_nav        = m_recordPaneWiring->nav(); // 既存コードで m_nav を使う場合に備えて保持

    // レイアウト用（右側ペインとして使用）
    m_gameRecordLayoutWidget = m_recordPane;
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

    // ★ 追加: PlayerInfoControllerにもm_analysisTabを設定
    //    （ensurePlayerInfoController_がこれより先に呼ばれた場合への対応）
    if (m_playerInfoController) {
        m_playerInfoController->setAnalysisTab(m_analysisTab);
    }

    // ★ 追加: 起動時に対局情報タブを追加
    addGameInfoTabAtStartup_();
}

// src/app/mainwindow.cpp
void MainWindow::initMatchCoordinator()
{
    // 依存が揃っていない場合は何もしない
    if (!m_gameController || !m_shogiView) return;

    // まず時計を用意（nullでも可だが、あれば渡す）
    ensureTimeController_();

    using std::placeholders::_1;
    using std::placeholders::_2;

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
    d.hooks.appendEvalP1       = std::bind(&MainWindow::requestRedrawEngine1Eval_, this);
    d.hooks.appendEvalP2       = std::bind(&MainWindow::requestRedrawEngine2Eval_, this);
    d.hooks.sendGoToEngine     = std::bind(&MatchCoordinator::sendGoToEngine,   m_match, _1, _2);
    d.hooks.sendStopToEngine   = std::bind(&MatchCoordinator::sendStopToEngine, m_match, _1);
    d.hooks.sendRawToEngine    = std::bind(&MatchCoordinator::sendRawToEngine,  m_match, _1, _2);
    d.hooks.initializeNewGame  = std::bind(&MainWindow::initializeNewGame_, this, _1);
    d.hooks.showMoveHighlights = std::bind(&MainWindow::showMoveHighlights_, this, _1, _2);
    d.hooks.appendKifuLine     = std::bind(&MainWindow::appendKifuLineHook_, this, _1, _2);

    // ★ 追加（今回の肝）：結果ダイアログ表示フックを配線
    d.hooks.showGameOverDialog = std::bind(&MainWindow::showGameOverMessageBox_, this, _1, _2);

    // ★★ 追加：時計から「残り/増加/秒読み」を司令塔へ提供するフックを配線
    d.hooks.remainingMsFor = std::bind(&MainWindow::getRemainingMsFor_, this, _1);
    d.hooks.incrementMsFor = std::bind(&MainWindow::getIncrementMsFor_, this, _1);
    d.hooks.byoyomiMs      = std::bind(&MainWindow::getByoyomiMs_, this);

    // ★ 追加：対局者名の更新フック（将棋盤ラベルと対局情報タブ）
    d.hooks.setPlayersNames = std::bind(&MainWindow::onSetPlayersNames_, this, _1, _2);
    d.hooks.setEngineNames  = std::bind(&MainWindow::onSetEngineNames_, this, _1, _2);

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
        h.getMainRowGuard                 = std::bind(&MainWindow::getMainRowGuard_, this);
        h.setMainRowGuard                 = std::bind(&MainWindow::setMainRowGuard_, this, _1);
        h.updateHighlightsForPly          = std::bind(&MainWindow::syncBoardAndHighlightsAtRow, this, _1);
        h.updateTurnAndTimekeepingDisplay = std::bind(&MainWindow::updateTurnAndTimekeepingDisplay_, this);
        // 評価値などの巻戻しが必要なら将来ここに追加：
        // h.handleUndoMove               = std::bind(&MainWindow::onUndoMove_, this, _1);
        h.isHumanSide                     = std::bind(&MainWindow::isHumanSide_, this, _1);
        h.isHvH                           = std::bind(&MainWindow::isHvH_, this);

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
    connectBoardClicks_();

    // 人間操作 → 合法判定＆適用の配線
    connectMoveRequested_();

    // 既定モード（必要に応じて開始時に上書き）
    if (m_boardController)
        m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
}

void MainWindow::ensureTimeController_()
{
    if (m_timeController) return;

    m_timeController = new TimeControlController(this);
    m_timeController->setTimeDisplayPresenter(m_timePresenter);
    m_timeController->ensureClock();
}

void MainWindow::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    ensureGameStateController_();
    if (m_gameStateController) {
        m_gameStateController->onMatchGameEnded(info);
    }
}

void MainWindow::onActionFlipBoardTriggered(bool /*checked*/)
{
    if (m_match) m_match->flipBoard();
}

void MainWindow::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    ensureGameStateController_();
    if (m_gameStateController) {
        m_gameStateController->onRequestAppendGameOverMove(info);
    }
}

void MainWindow::setupBoardInteractionController()
{
    ensureBoardSetupController_();
    if (m_boardSetupController) {
        m_boardSetupController->setupBoardInteractionController();
        m_boardController = m_boardSetupController->boardController();

        // 人間の手番かどうかを判定するコールバックを設定
        if (m_boardController) {
            m_boardController->setIsHumanTurnCallback([this]() -> bool {
                return isHumanTurnNow_();
            });
        }
    }
}

void MainWindow::connectBoardClicks_()
{
    ensureBoardSetupController_();
    if (m_boardSetupController) {
        m_boardSetupController->connectBoardClicks();
    }
}

void MainWindow::connectMoveRequested_()
{
    // BoardInteractionControllerからのシグナルをMainWindow経由で処理
    if (m_boardController) {
        QObject::connect(
            m_boardController, &BoardInteractionController::moveRequested,
            this,              &MainWindow::onMoveRequested_,
            Qt::UniqueConnection);
    }
}

void MainWindow::onMoveRequested_(const QPoint& from, const QPoint& to)
{
    qDebug().noquote() << "[MW] onMoveRequested_: from=" << from << " to=" << to
                       << " m_playMode=" << static_cast<int>(m_playMode)
                       << " m_match=" << (m_match ? "valid" : "null")
                       << " matchMode=" << (m_match ? static_cast<int>(m_match->playMode()) : -1);
    
    ensureBoardSetupController_();
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
    ensureReplayController_();
    if (m_replayController) {
        m_replayController->setReplayMode(on);
    }
}

void MainWindow::broadcastComment(const QString& text, bool asHtml)
{
    // ★ 追加: 現在の手数インデックスをEngineAnalysisTabに設定
    if (m_analysisTab) m_analysisTab->setCurrentMoveIndex(m_currentMoveIndex);

    if (asHtml) {
        // ★ 「*の手前で改行」＋「URLリンク化」付きのHTMLに整形して配信
        const QString html = toRichHtmlWithStarBreaksAndLinks(text);
        if (m_analysisTab) m_analysisTab->setCommentHtml(html);
        if (m_recordPane)  m_recordPane->setBranchCommentHtml(html);
    } else {
        // プレーンテキスト経路は従来通り
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
    const qsizetype maxPly = m_resolvedRows[row].disp.size();
    const int selPly = static_cast<int>(qBound(qsizetype(0), qsizetype(ply), maxPly));

    // これだけで：局面更新 / 棋譜欄差し替え＆選択 / 分岐候補欄更新 / ツリーハイライト同期
    applyResolvedRowAndSelect(row, selPly);
}

// 毎手の着手確定時：ライブ分岐ツリー更新をイベントループ後段に遅延
void MainWindow::onMoveCommitted(ShogiGameController::Player mover, int ply)
{
    ensureBoardSetupController_();
    if (m_boardSetupController) {
        m_boardSetupController->setPlayMode(m_playMode);
        m_boardSetupController->onMoveCommitted(mover, ply);
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

void MainWindow::onBoardSizeChanged(QSize fieldSize)
{
    // 将棋盤のマスサイズに連動して評価値グラフの高さを調整
    // fieldSizeは1マスのサイズなので、盤全体の高さは fieldSize.height() * 9
    // 評価値グラフは盤の高さの約1/3程度が見やすい

    qDebug() << "[BOARD_SIZE] onBoardSizeChanged called, fieldSize=" << fieldSize;

    if (!m_recordPane) {
        qDebug() << "[BOARD_SIZE] m_recordPane is NULL, returning";
        return;
    }

    const int squareSize = fieldSize.height();
    // 基準高さ250に対して、マスサイズの変化に応じてスケール
    // デフォルトのマスサイズを40として計算
    const int baseHeight = 250;
    const int baseSquareSize = 40;
    const int newHeight = baseHeight * squareSize / baseSquareSize;

    // 最小220、最大500に制限
    const int clampedHeight = qBound(220, newHeight, 500);

    qDebug() << "[BOARD_SIZE] squareSize=" << squareSize
             << "baseHeight=" << baseHeight
             << "baseSquareSize=" << baseSquareSize
             << "newHeight=" << newHeight
             << "clampedHeight=" << clampedHeight;

    m_recordPane->setEvalChartHeight(clampedHeight);
    qDebug() << "[BOARD_SIZE] setEvalChartHeight called with" << clampedHeight;
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
    ensureGameStateController_();
    if (m_gameStateController) {
        m_gameStateController->onGameOverStateChanged(st);
    }
}

void MainWindow::handleBreakOffGame()
{
    ensureGameStateController_();
    if (m_gameStateController) {
        m_gameStateController->handleBreakOffGame();
    }
}

void MainWindow::ensureReplayController_()
{
    if (m_replayController) return;

    m_replayController = new ReplayController(this);
    m_replayController->setClock(m_timeController ? m_timeController->clock() : nullptr);
    m_replayController->setShogiView(m_shogiView);
    m_replayController->setGameController(m_gameController);
    m_replayController->setMatchCoordinator(m_match);
    m_replayController->setRecordPane(m_recordPane);
}

void MainWindow::ensureDialogCoordinator_()
{
    if (m_dialogCoordinator) return;

    m_dialogCoordinator = new DialogCoordinator(this, this);
    m_dialogCoordinator->setMatchCoordinator(m_match);
    m_dialogCoordinator->setGameController(m_gameController);
}

void MainWindow::ensureKifuExportController_()
{
    if (m_kifuExportController) return;

    m_kifuExportController = new KifuExportController(this, this);

    // ステータスバーへのメッセージ転送
    connect(m_kifuExportController, &KifuExportController::statusMessage,
            this, [this](const QString& msg, int timeout) {
                if (ui && ui->statusbar) {
                    ui->statusbar->showMessage(msg, timeout);
                }
            });
}

// KifuExportControllerに依存を設定するヘルパー
void MainWindow::updateKifuExportDependencies_()
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
    deps.resolvedRows = &m_resolvedRows;
    deps.commentsByRow = &m_commentsByRow;
    deps.startSfenStr = m_startSfenStr;
    deps.playMode = m_playMode;
    deps.humanName1 = m_humanName1;
    deps.humanName2 = m_humanName2;
    deps.engineName1 = m_engineName1;
    deps.engineName2 = m_engineName2;
    deps.activeResolvedRow = m_activeResolvedRow;
    deps.currentMoveIndex = m_currentMoveIndex;
    deps.activePly = m_activePly;
    deps.currentSelectedPly = m_currentSelectedPly;

    m_kifuExportController->setDependencies(deps);
}

void MainWindow::ensureGameStateController_()
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
        refreshBranchTreeLive_();
    });
    m_gameStateController->setUpdatePlyStateCallback([this](int activePly, int selectedPly, int moveIndex) {
        m_activePly = activePly;
        m_currentSelectedPly = selectedPly;
        m_currentMoveIndex = moveIndex;
    });
}

void MainWindow::ensurePlayerInfoController_()
{
    if (m_playerInfoController) return;

    // PlayerInfoWiring経由でPlayerInfoControllerを取得
    ensurePlayerInfoWiring_();
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

void MainWindow::ensureBoardSetupController_()
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
        ensurePositionEditController_();
    });
    m_boardSetupController->setEnsureTimeControllerCallback([this]() {
        ensureTimeController_();
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
        refreshBranchTreeLive_();
    });
}

void MainWindow::ensurePvClickController_()
{
    if (m_pvClickController) return;

    m_pvClickController = new PvClickController(this);

    // 依存オブジェクトの設定
    m_pvClickController->setThinkingModels(m_modelThinking1, m_modelThinking2);
    m_pvClickController->setLogModels(m_lineEditModel1, m_lineEditModel2);
    m_pvClickController->setSfenRecord(m_sfenRecord);
    m_pvClickController->setGameMoves(&m_gameMoves);
    m_pvClickController->setUsiMoves(&m_gameUsiMoves);
}

void MainWindow::ensureRecordNavigationController_()
{
    if (m_recordNavController) return;

    m_recordNavController = new RecordNavigationController(this);

    // 依存オブジェクトの設定
    m_recordNavController->setShogiView(m_shogiView);
    m_recordNavController->setBoardSyncPresenter(m_boardSync);
    m_recordNavController->setKifuRecordModel(m_kifuRecordModel);
    m_recordNavController->setKifuLoadCoordinator(m_kifuLoadCoordinator);
    m_recordNavController->setReplayController(m_replayController);
    m_recordNavController->setAnalysisTab(m_analysisTab);
    m_recordNavController->setRecordPane(m_recordPane);
    m_recordNavController->setRecordPresenter(m_recordPresenter);
    m_recordNavController->setCsaGameCoordinator(m_csaGameCoordinator);
    m_recordNavController->setEvalGraphController(m_evalGraphController);

    // 状態参照の設定
    m_recordNavController->setResolvedRows(&m_resolvedRows);
    m_recordNavController->setActiveResolvedRow(&m_activeResolvedRow);

    // コールバックの設定
    m_recordNavController->setEnsureBoardSyncCallback([this]() {
        ensureBoardSyncPresenter_();
        if (m_recordNavController) {
            m_recordNavController->setBoardSyncPresenter(m_boardSync);
        }
    });
    m_recordNavController->setEnableArrowButtonsCallback([this]() {
        enableArrowButtons();
    });
    m_recordNavController->setSetCurrentTurnCallback([this]() {
        setCurrentTurn();
    });
    m_recordNavController->setBroadcastCommentCallback([this](const QString& text, bool asHtml) {
        broadcastComment(text, asHtml);
    });
    m_recordNavController->setApplyResolvedRowAndSelectCallback([this](int row, int ply) {
        applyResolvedRowAndSelect(row, ply);
    });
    m_recordNavController->setUpdatePlyStateCallback([this](int activePly, int selectedPly, int moveIndex) {
        m_activePly = activePly;
        m_currentSelectedPly = selectedPly;
        m_currentMoveIndex = moveIndex;
    });
    m_recordNavController->setUpdateTurnIndicatorCallback([this](ShogiGameController::Player player) {
        if (m_shogiView) {
            m_shogiView->updateTurnIndicator(player);
        }
    });
}

void MainWindow::ensurePositionEditCoordinator_()
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
        ensurePositionEditController_();
        if (m_posEditCoordinator) {
            m_posEditCoordinator->setPositionEditController(m_posEdit);
        }
    });

    // 編集用アクションの設定
    if (ui) {
        PositionEditCoordinator::EditActions actions;
        actions.returnAllPiecesOnStand = ui->returnAllPiecesOnStand;
        actions.flatHandInitialPosition = ui->flatHandInitialPosition;
        actions.shogiProblemInitialPosition = ui->shogiProblemInitialPosition;
        actions.turnaround = ui->turnaround;
        actions.reversal = ui->reversal;
        m_posEditCoordinator->setEditActions(actions);
    }

    // 先後反転シグナルの接続
    connect(m_posEditCoordinator, &PositionEditCoordinator::reversalTriggered,
            this, &MainWindow::onReverseTriggered, Qt::UniqueConnection);
}

void MainWindow::ensureCsaGameWiring_()
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
            this, &MainWindow::refreshBranchTreeLive_);
    connect(m_csaGameWiring, &CsaGameWiring::playModeChanged,
            this, &MainWindow::onCsaPlayModeChanged_);
    connect(m_csaGameWiring, &CsaGameWiring::showGameEndDialogRequested,
            this, &MainWindow::onCsaShowGameEndDialog_);
    connect(m_csaGameWiring, &CsaGameWiring::errorMessageRequested,
            this, &MainWindow::displayErrorMessage);

    qDebug().noquote() << "[MW] ensureCsaGameWiring_: created and connected";
}

void MainWindow::ensureJosekiWiring_()
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
            this, &MainWindow::onMoveRequested_);
    connect(m_josekiWiring, &JosekiWindowWiring::forcedPromotionRequested,
            this, &MainWindow::onJosekiForcedPromotion_);

    qDebug().noquote() << "[MW] ensureJosekiWiring_: created and connected";
}

void MainWindow::ensurePlayerInfoWiring_()
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
            this, &MainWindow::onGameInfoUpdated_);
    connect(m_playerInfoWiring, &PlayerInfoWiring::tabCurrentChanged,
            this, &MainWindow::onTabCurrentChanged);

    // PlayerInfoControllerも同期
    m_playerInfoController = m_playerInfoWiring->playerInfoController();

    qDebug().noquote() << "[MW] ensurePlayerInfoWiring_: created and connected";
}

void MainWindow::ensurePreStartCleanupHandler_()
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
    deps.startSfenStr = &m_startSfenStr;
    deps.currentSfenStr = &m_currentSfenStr;
    deps.activePly = &m_activePly;
    deps.currentSelectedPly = &m_currentSelectedPly;
    deps.currentMoveIndex = &m_currentMoveIndex;

    m_preStartCleanupHandler = new PreStartCleanupHandler(deps, this);

    // 分岐表示プランへの参照を設定
    m_preStartCleanupHandler->setBranchDisplayPlan(&m_branchDisplayPlan);

    // PreStartCleanupHandlerからのシグナルをMainWindowに接続
    connect(m_preStartCleanupHandler, &PreStartCleanupHandler::broadcastCommentRequested,
            this, &MainWindow::broadcastComment);

    qDebug().noquote() << "[MW] ensurePreStartCleanupHandler_: created and connected";
}

// 「検討を終了」アクション用：エンジンに quit を送り検討セッションを終了
void MainWindow::handleBreakOffConsidaration()
{
    if (!m_match) return;

    // 司令塔に依頼（内部で quit 送信→プロセス/Usi 破棄→モード PlayMode::NotStarted）
    m_match->handleBreakOffConsidaration();

    // UI の後始末（任意）——検討のハイライトなどをクリアしておく
    if (m_shogiView) m_shogiView->removeHighlightAllData();

    // MainWindow 側のモードも念のため合わせる（UI 表示に依存がある場合）
    m_playMode = PlayMode::NotStarted;

    // 盤下のエンジン名表示などを通常状態へ（関数がある場合）
    setEngineNamesBasedOnMode();
    updateSecondEngineVisibility();  // ★ 追加: 検討終了時も2番目エンジン表示を更新

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
    d.clock = m_timeController ? m_timeController->clock() : nullptr;
    d.gc    = m_gameController;
    d.view  = m_shogiView;

    m_gameStart = new GameStartCoordinator(d, this);

    // 依頼シグナルを既存メソッドへ接続（ラムダ不使用）
    connect(m_gameStart, &GameStartCoordinator::requestPreStartCleanup,
            this, &MainWindow::onPreStartCleanupRequested_);

    connect(m_gameStart, &GameStartCoordinator::requestApplyTimeControl,
            this, &MainWindow::onApplyTimeControlRequested_);

    // ★ 追加: 対局者名確定シグナルを接続
    connect(m_gameStart, &GameStartCoordinator::playerNamesResolved,
            this, &MainWindow::onPlayerNamesResolved_);

    // ★ 追加: 対局開始時にナビゲーション（棋譜欄と矢印ボタン）を無効化
    connect(m_gameStart, &GameStartCoordinator::started,
            this, &MainWindow::disableNavigationForGame);

    // ★ 追加: 盤面反転シグナルを接続（人を手前に表示する機能用）
    connect(m_gameStart, &GameStartCoordinator::boardFlipped,
            this, &MainWindow::onBoardFlipped,
            Qt::UniqueConnection);
}

void MainWindow::onPreStartCleanupRequested_()
{
    ensurePreStartCleanupHandler_();
    if (m_preStartCleanupHandler) {
        m_preStartCleanupHandler->performCleanup();
    }
}

void MainWindow::onApplyTimeControlRequested_(const GameStartCoordinator::TimeControl& tc)
{
    qDebug().noquote()
    << "[MW] onApplyTimeControlRequested_:"
    << " enabled=" << tc.enabled
    << " P1{base=" << tc.p1.baseMs << " byoyomi=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
    << " P2{base=" << tc.p2.baseMs << " byoyomi=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // ★ TimeControlControllerに時間制御情報を保存
    if (m_timeController) {
        m_timeController->saveTimeControlSettings(tc.enabled, tc.p1.baseMs, tc.p1.byoyomiMs, tc.p1.incrementMs);
        m_timeController->recordGameStartTime();
    }

    // 1) まず時計に適用
    ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr;
    TimeControlUtil::applyToClock(clk, tc, m_startSfenStr, m_currentSfenStr);

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

        m_match->setTimeControlConfig(useByoyomi,
                                       static_cast<int>(byo1), static_cast<int>(byo2),
                                       static_cast<int>(inc1), static_cast<int>(inc2),
                                       loseOnTimeout);
        // 反映直後に現在の go 用数値を再計算しておくと安全
        m_match->refreshGoTimes();
    }

    // 3) 表示更新
    if (clk) {
        qDebug() << "[MW] clock after apply:"
                 << "P1=" << clk->getPlayer1TimeIntMs()
                 << "P2=" << clk->getPlayer2TimeIntMs()
                 << "byo=" << clk->getCommonByoyomiMs()
                 << "binc=" << clk->getBincMs()
                 << "winc=" << clk->getWincMs();
        clk->updateClock();
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
    qDebug() << "[EVAL_GRAPH] requestRedrawEngine1Eval_() hook called - invoking redrawEngine1EvaluationGraph";
    // デフォルトパラメータ -1 で呼び出し（HvEでは sfenRecord から計算）
    redrawEngine1EvaluationGraph(-1);
}

void MainWindow::requestRedrawEngine2Eval_()
{
    qDebug() << "[EVAL_GRAPH] requestRedrawEngine2Eval_() hook called - invoking redrawEngine2EvaluationGraph";
    // デフォルトパラメータ -1 で呼び出し（HvEでは sfenRecord から計算）
    redrawEngine2EvaluationGraph(-1);
}

void MainWindow::initializeNewGame_(const QString& s)
{
    // --- デバッグ：誰がこの関数を呼び出したか追跡 ---
    qDebug() << "[DEBUG] MainWindow::initializeNewGame_ called with sfen:" << s;
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

void MainWindow::showMoveHighlights_(const QPoint& from, const QPoint& to)
{
    if (m_boardController) m_boardController->showMoveHighlights(from, to);
}

// ===== MainWindow.cpp: appendKifuLineHook_（ライブ分岐ツリー更新を追加） =====
void MainWindow::appendKifuLineHook_(const QString& text, const QString& elapsed)
{
    // 既存：棋譜欄へ 1手追記（Presenter がモデルへ反映）
    appendKifuLine(text, elapsed);

    // ★追加：HvH/HvE の「1手指すごと」に分岐ツリーを更新
    refreshBranchTreeLive_();
}

void MainWindow::onRecordRowChangedByPresenter(int row, const QString& comment)
{
    qDebug() << "[MW-DEBUG] onRecordRowChangedByPresenter called: row=" << row
             << "comment=" << comment.left(30) << "..."
             << "playMode=" << static_cast<int>(m_playMode);
    
    ensureRecordNavigationController_();
    if (m_recordNavController) {
        m_recordNavController->onRecordRowChangedByPresenter(row, comment);
    }
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
    if (!m_timeController) {
        qDebug() << "[MW] getRemainingMsFor_: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_timeController->getRemainingMs(player);
}

qint64 MainWindow::getIncrementMsFor_(MatchCoordinator::Player p) const
{
    if (!m_timeController) {
        qDebug() << "[MW] getIncrementMsFor_: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_timeController->getIncrementMs(player);
}

qint64 MainWindow::getByoyomiMs_() const
{
    if (!m_timeController) {
        qDebug() << "[MW] getByoyomiMs_: timeController=null";
        return 0;
    }
    return m_timeController->getByoyomiMs();
}

// 対局終了時のタイトルと本文を受け取り、情報ダイアログを表示するだけのヘルパ
void MainWindow::showGameOverMessageBox_(const QString& title, const QString& message)
{
    ensureDialogCoordinator_();
    if (m_dialogCoordinator) {
        m_dialogCoordinator->showGameOverMessage(title, message);
    }
}

void MainWindow::onRecordPaneMainRowChanged_(int row)
{
    qDebug() << "[MW-DEBUG] onRecordPaneMainRowChanged_ ENTER row=" << row;

    // RecordNavigationControllerに委譲
    ensureRecordNavigationController_();
    if (m_recordNavController) {
        // 最新の依存を同期
        m_recordNavController->setCsaGameCoordinator(m_csaGameCoordinator);
        m_recordNavController->setEvalGraphController(m_evalGraphController);
        m_recordNavController->onMainRowChanged(row);
    }

    // m_currentSfenStrを現在の局面に更新
    if (row >= 0 && m_sfenRecord && row < m_sfenRecord->size()) {
        m_currentSfenStr = m_sfenRecord->at(row);
        qDebug() << "[MW-DEBUG] onRecordPaneMainRowChanged_: updated m_currentSfenStr=" << m_currentSfenStr;
    }

    // 定跡ウィンドウを更新
    updateJosekiWindow();

    qDebug() << "[MW-DEBUG] onRecordPaneMainRowChanged_ LEAVE";
}

// ===== MainWindow.cpp: ライブ用の KifuLoadCoordinator を確保 =====
void MainWindow::ensureKifuLoadCoordinatorForLive_()
{
    if (m_kifuLoadCoordinator) {
        return; // 既に用意済み
    }

    // KIF読込時と同等の依存で生成（ロード自体はしない）
    m_kifuLoadCoordinator = new KifuLoadCoordinator(
        /* gameMoves           */ m_gameMoves,
        /* resolvedRows        */ m_resolvedRows,
        /* positionStrList     */ m_positionStrList,
        /* activeResolvedRow   */ m_activeResolvedRow,
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
        /* branchDisplayPlan   */ m_branchDisplayPlan,
        this);

    // 分岐配線（既存のやり方に合わせる）
    // BranchCandidatesController は BranchWiringCoordinator が生成し、
    // setBranchCandidatesController() 経由で注入される
    if (m_branchWiring) {
        m_branchWiring->setKifuLoader(m_kifuLoadCoordinator);
        connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::setupBranchCandidatesWiring_,
                m_branchWiring,       &BranchWiringCoordinator::setupBranchCandidatesWiring,
                Qt::UniqueConnection);

        // ★追加修正: 生成した Loader に Controller を確実に注入するため、配線を即時実行する
        m_branchWiring->setupBranchCandidatesWiring();
    }

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
void MainWindow::refreshBranchTreeLive_()
{
    qDebug().noquote() << "[MW-DEBUG] refreshBranchTreeLive_() ENTER";

    ensureKifuLoadCoordinatorForLive_();
    if (!m_kifuLoadCoordinator) {
        qDebug().noquote() << "[MW-DEBUG] refreshBranchTreeLive_(): m_kifuLoadCoordinator is null";
        return;
    }

    int ply = 0;
    if (m_kifuRecordModel) {
        ply = qMax(0, m_kifuRecordModel->rowCount() - 1);
    }

    qDebug().noquote() << "[MW-DEBUG] refreshBranchTreeLive_(): ply=" << ply
                       << "kifuModel.rowCount=" << (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);

    // ツリーの再構築（既存）
    m_kifuLoadCoordinator->updateBranchTreeFromLive(ply);

    // ★ 文脈を固定したまま、＋/オレンジ/候補欄を更新
    m_kifuLoadCoordinator->rebuildBranchPlanAndMarksForLive(ply);

    qDebug().noquote() << "[MW-DEBUG] refreshBranchTreeLive_() LEAVE";
}

// ========== UNDO用：MainWindow 補助関数 ==========

bool MainWindow::getMainRowGuard_() const
{
    return m_onMainRowGuard;
}

void MainWindow::setMainRowGuard_(bool on)
{
    m_onMainRowGuard = on;
}

bool MainWindow::isHvH_() const
{
    if (m_gameStateController) {
        return m_gameStateController->isHvH();
    }
    return (m_playMode == PlayMode::HumanVsHuman);
}

bool MainWindow::isHumanSide_(ShogiGameController::Player p) const
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

void MainWindow::updateTurnAndTimekeepingDisplay_()
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

    ensureRecordPresenter_();
    if (m_recordPresenter) {
        m_recordPresenter->appendMoveLine(m_lastMove, elapsedTime);

        // ★ 変更: Presenter にライブ記録を蓄積させる
        if (!m_lastMove.isEmpty()) {
            m_recordPresenter->addLiveKifItem(m_lastMove, elapsedTime);
        }
    }

    m_lastMove.clear();
}

// 新しい保存関数
void MainWindow::saveKifuToFile()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();

    const QString path = m_kifuExportController->saveToFile();
    if (!path.isEmpty()) {
        kifuSaveFileName = path;
    }
}

// KIF形式で棋譜をクリップボードにコピー
void MainWindow::copyKifToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyKifToClipboard();
}

// KI2形式で棋譜をクリップボードにコピー
void MainWindow::copyKi2ToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyKi2ToClipboard();
}

// CSA形式で棋譜をクリップボードにコピー
void MainWindow::copyCsaToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyCsaToClipboard();
}

// USI形式（全て）で棋譜をクリップボードにコピー
void MainWindow::copyUsiToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyUsiToClipboard();
}

// USI形式（現在の指し手まで）で棋譜をクリップボードにコピー
void MainWindow::copyUsiCurrentToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyUsiCurrentToClipboard();
}

// JKF形式で棋譜をクリップボードにコピー
void MainWindow::copyJkfToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyJkfToClipboard();
}

// USEN形式で棋譜をクリップボードにコピー
void MainWindow::copyUsenToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyUsenToClipboard();
}

// SFEN形式で局面をクリップボードにコピー
void MainWindow::copySfenToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copySfenToClipboard();
}

// BOD形式で局面をクリップボードにコピー
void MainWindow::copyBodToClipboard()
{
    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->copyBodToClipboard();
}


// ★ 追加: クリップボードから棋譜を貼り付け
void MainWindow::pasteKifuFromClipboard()
{
    KifuPasteDialog* dlg = new KifuPasteDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // ダイアログの「取り込む」シグナルをKifuLoadCoordinatorに接続
    connect(dlg, &KifuPasteDialog::importRequested,
            this, &MainWindow::onKifuPasteImportRequested_);

    dlg->show();
}

void MainWindow::onKifuPasteImportRequested_(const QString& content)
{
    qDebug().noquote() << "[MW] onKifuPasteImportRequested_: content length =" << content.size();

    // KifuLoadCoordinator を確保
    ensureKifuLoadCoordinatorForLive_();

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

    ensureGameRecordModel_();
    ensureKifuExportController_();
    updateKifuExportDependencies_();
    m_kifuExportController->overwriteFile(kifuSaveFileName);
}

// ★ 追加: コメント更新スロットの実装
void MainWindow::onCommentUpdated(int moveIndex, const QString& newComment)
{
    qDebug().noquote()
        << "[MW] onCommentUpdated"
        << " moveIndex=" << moveIndex
        << " newComment.len=" << newComment.size();

    // 有効な手数インデックスかチェック
    if (moveIndex < 0) {
        qWarning().noquote() << "[MW] onCommentUpdated: invalid moveIndex";
        return;
    }

    // GameRecordModel を使ってコメントを更新
    // （通知処理はコールバック経由で自動実行される）
    ensureGameRecordModel_();
    if (m_gameRecord) {
        m_gameRecord->setComment(moveIndex, newComment);
    }

    // ステータスバーに通知
    ui->statusbar->showMessage(tr("コメントを更新しました（手数: %1）").arg(moveIndex), 3000);
}

// ★ 追加: GameRecordModel の遅延初期化
void MainWindow::ensureGameRecordModel_()
{
    if (m_gameRecord) return;

    m_gameRecord = new GameRecordModel(this);

    // 外部データストアをバインド
    QList<KifDisplayItem>* liveDispPtr = nullptr;
    if (m_recordPresenter) {
        // const_cast は GameRecordModel 内部での同期更新に必要
        liveDispPtr = const_cast<QList<KifDisplayItem>*>(&m_recordPresenter->liveDisp());
    }

    m_gameRecord->bind(&m_resolvedRows, &m_activeResolvedRow, liveDispPtr);

    // commentChanged シグナルを接続（将来の拡張用）
    connect(m_gameRecord, &GameRecordModel::commentChanged,
            this, &MainWindow::onGameRecordCommentChanged);

    // コメント更新時のコールバックを設定
    // m_commentsByRow 同期、RecordPresenter通知、UI更新を自動実行
    m_gameRecord->setCommentUpdateCallback(
        std::bind(&MainWindow::onCommentUpdateCallback_, this,
                  std::placeholders::_1, std::placeholders::_2));

    qDebug().noquote() << "[MW] ensureGameRecordModel_: created and bound";
}

// ★ 追加: GameRecordModel::commentChanged スロット
void MainWindow::onGameRecordCommentChanged(int ply, const QString& /*comment*/)
{
    qDebug().noquote() << "[MW] GameRecordModel::commentChanged ply=" << ply;
}

// コメント更新コールバック（GameRecordModel から呼ばれる）
void MainWindow::onCommentUpdateCallback_(int ply, const QString& comment)
{
    qDebug().noquote() << "[MW] onCommentUpdateCallback_ ply=" << ply;

    // m_commentsByRow への同期（互換性・RecordPresenterへの供給用）
    while (m_commentsByRow.size() <= ply) {
        m_commentsByRow.append(QString());
    }
    m_commentsByRow[ply] = comment;

    // RecordPresenter のコメント配列も更新（行選択時に正しいコメントを表示するため）
    if (m_recordPresenter) {
        QStringList updatedComments;
        updatedComments.reserve(m_commentsByRow.size());
        for (const QString& c : std::as_const(m_commentsByRow)) {
            updatedComments.append(c);
        }
        m_recordPresenter->setCommentsByRow(updatedComments);
        qDebug().noquote() << "[MW] Updated RecordPresenter commentsByRow";
    }

    // 現在表示中のコメントを更新（両方のコメント欄に反映）
    const QString displayComment = comment.trimmed().isEmpty() ? tr("コメントなし") : comment;
    broadcastComment(displayComment, /*asHtml=*/true);
}

// ★ 追加: 読み筋クリック処理
void MainWindow::onPvRowClicked(int engineIndex, int row)
{
    ensurePvClickController_();
    if (m_pvClickController) {
        // 状態を同期
        m_pvClickController->setPlayMode(m_playMode);
        m_pvClickController->setPlayerNames(m_humanName1, m_humanName2, m_engineName1, m_engineName2);
        m_pvClickController->setCurrentSfen(m_currentSfenStr);
        m_pvClickController->setStartSfen(m_startSfenStr);
        m_pvClickController->onPvRowClicked(engineIndex, row);
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

void MainWindow::onCsaPlayModeChanged_(int mode)
{
    m_playMode = static_cast<PlayMode>(mode);
    qDebug().noquote() << "[MW] onCsaPlayModeChanged_: mode=" << mode;
}

void MainWindow::onCsaShowGameEndDialog_(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::onCsaEngineScoreUpdated_(int scoreCp, int ply)
{
    qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: scoreCp=" << scoreCp << "ply=" << ply;

    // 評価値グラフに追加
    if (!m_recordPane) {
        qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: m_recordPane is NULL";
        return;
    }

    EvaluationChartWidget* ec = m_recordPane->evalChart();
    if (!ec) {
        qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: evalChart() returned NULL";
        return;
    }

    // CSA対局では自分側のエンジンの評価値を表示
    // 自分が先手(黒)ならP1に、後手(白)ならP2に追加
    if (m_csaGameCoordinator) {
        bool isBlackSide = m_csaGameCoordinator->isBlackSide();
        if (isBlackSide) {
            // 先手側のエンジン評価値はP1に追加
            ec->appendScoreP1(ply, scoreCp, false);
            qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: appendScoreP1 called";
        } else {
            // 後手側のエンジン評価値はP2に追加
            // 後手視点の評価値なので、グラフ表示用に反転する
            ec->appendScoreP2(ply, -scoreCp, false);
            qDebug().noquote() << "[MW] onCsaEngineScoreUpdated_: appendScoreP2 called (inverted)";
        }
    }
}

void MainWindow::onJosekiForcedPromotion_(bool forced, bool promote)
{
    if (m_gameController) {
        m_gameController->setForcedPromotion(forced, promote);
    }
}
