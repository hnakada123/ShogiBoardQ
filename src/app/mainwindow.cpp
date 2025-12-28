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
#include <functional>
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

using GameOverCause = MatchCoordinator::Cause;
using std::placeholders::_1;
using std::placeholders::_2;

static inline GameOverCause toUiCause(MatchCoordinator::Cause c) { return c; }

// ★ コメント整形ヘルパ：
//   1) '*' の直前で改行
//   2) URL( http(s)://... ) を <a href="...">...</a> にリンク化
//   3) 改行を <br/> に変換（安全のため非URL部は HTML エスケープ）
namespace {
static QString toRichHtmlWithStarBreaksAndLinks(const QString& raw)
{
    // 改行正規化
    QString s = raw;
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QChar('\r'), QChar('\n'));

    // '*' が行頭でない場合、その直前に改行を入れる（直前の余分な空白は削る）
    QString withBreaks;
    withBreaks.reserve(s.size() + 16);
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('*') && i > 0 && s.at(i - 1) != QLatin1Char('\n')) {
            while (!withBreaks.isEmpty()) {
                const QChar tail = withBreaks.at(withBreaks.size() - 1);
                if (tail == QLatin1Char('\n')) break;
                if (!tail.isSpace()) break;
                withBreaks.chop(1);
            }
            withBreaks.append(QLatin1Char('\n'));
        }
        withBreaks.append(ch);
    }

    // URL をリンク化（非URL部分は都度エスケープ）
    static const QRegularExpression urlRe(
        QStringLiteral(R"((https?://[^\s<>"']+))"),
        QRegularExpression::CaseInsensitiveOption);

    QString html;
    html.reserve(withBreaks.size() + 64);

    int last = 0;
    QRegularExpressionMatchIterator it = urlRe.globalMatch(withBreaks);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const qsizetype start = m.capturedStart();
        const qsizetype end   = m.capturedEnd();

        // 非URL部分をエスケープして追加
        html += QString(withBreaks.mid(last, start - last)).toHtmlEscaped();

        // URL部分を <a href="...">...</a>
        const QString url   = m.captured(0);
        const QString href  = url.toHtmlEscaped();   // 属性用の最低限エスケープ
        const QString label = url.toHtmlEscaped();   // 表示用
        html += QStringLiteral("<a href=\"%1\">%2</a>").arg(href, label);

        last = static_cast<int>(end);
    }
    // 末尾の非URL部分
    html += QString(withBreaks.mid(last)).toHtmlEscaped();

    // 改行 → <br/>
    html.replace(QChar('\n'), QStringLiteral("<br/>"));
    return html;
}
} // anonymous namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_startSfenStr(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"))
    , m_currentSfenStr(QStringLiteral("startpos"))
    , m_errorOccurred(false)
    , m_playMode(NotStarted)
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
    qDebug().noquote() << "[MW] ★ setEngineNamesBasedOnMode: m_playMode=" << static_cast<int>(m_playMode)
                       << " m_engineName1=" << m_engineName1 << " m_engineName2=" << m_engineName2;
    
    const EngineNameMapping e =
        PlayerNameService::computeEngineModels(m_playMode, m_engineName1, m_engineName2);

    qDebug().noquote() << "[MW] ★ setEngineNamesBasedOnMode: computed model1=" << e.model1 << " model2=" << e.model2;

    if (m_lineEditModel1) {
        qDebug().noquote() << "[MW] ★ setEngineNamesBasedOnMode: setting m_lineEditModel1 to" << e.model1;
        m_lineEditModel1->setEngineName(e.model1);
    }
    if (m_lineEditModel2) {
        qDebug().noquote() << "[MW] ★ setEngineNamesBasedOnMode: setting m_lineEditModel2 to" << e.model2;
        m_lineEditModel2->setEngineName(e.model2);
    }
}

// ★ 追加: EvE対局時に2番目のエンジン情報を表示する
void MainWindow::updateSecondEngineVisibility()
{
    if (!m_analysisTab) return;

    // EvE対局の場合のみ2番目のエンジン情報を表示
    const bool isEvE =
        (m_playMode == EvenEngineVsEngine) ||
        (m_playMode == HandicapEngineVsEngine);

    m_analysisTab->setSecondEngineVisible(isEvE);
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
    const bool resume = m_isResumeFromCurrent;

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

// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    if (m_match) m_match->handleResign();
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
    c.isReplayMode    = m_isReplayMode;
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

    // ── メニュー表示（Controller → callback）: 共通ヘルパで編集メニューに遷移 ──
    ctx.onEnterEditMenu = [this]() {
        applyEditMenuEditingState(true);
    };

    // ── 「編集終了」ボタン表示（Controller API 経由） ──
    ctx.onShowEditExitButton = [this]() {
        if (m_posEdit && m_shogiView) {
            m_posEdit->showEditExitButtonOnBoard(m_shogiView, this, SLOT(finishPositionEditing()));
        }
    };

    // 実行（盤と文字列状態の同期等は Controller が担当）
    m_posEdit->beginPositionEditing(ctx);

    // ── 編集用アクション配線（ラムダ無し・重複防止） ─────────────
    if (ui) {
        // Controller のスロットへ直接接続
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
    ctx.startSfenStr        = &m_startSfenStr;
    ctx.isResumeFromCurrent = &m_isResumeFromCurrent;

    // 「編集終了」ボタンの後片付け（Controller → callback）
    ctx.onHideEditExitButton = [this]() {
        if (m_posEdit && m_shogiView) {
            m_posEdit->hideEditExitButtonOnBoard(m_shogiView);
        }
    };

    // メニューを元に戻す（Controller → callback）: 共通ヘルパで通常メニューに復帰
    ctx.onLeaveEditMenu = [this]() {
        applyEditMenuEditingState(false);
    };

    // 実行
    m_posEdit->finishPositionEditing(ctx);

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

// ★ 新規: GameInfoPaneControllerの初期化
void MainWindow::ensureGameInfoController_()
{
    if (m_gameInfoController) return;

    m_gameInfoController = new GameInfoPaneController(this);

    // 対局情報更新時のシグナル接続
    connect(m_gameInfoController, &GameInfoPaneController::gameInfoUpdated,
            this, &MainWindow::onGameInfoUpdated_);
}

// ★ 追加: 起動時に対局情報タブを追加
void MainWindow::addGameInfoTabAtStartup_()
{
    if (!m_tab) return;

    // GameInfoPaneControllerを確保
    ensureGameInfoController_();

    if (!m_gameInfoController) return;

    // 既に「対局情報」タブがあるか確認
    for (int i = 0; i < m_tab->count(); ++i) {
        if (m_tab->tabText(i) == tr("対局情報")) {
            m_tab->removeTab(i);
            break;
        }
    }

    // 初期データを設定
    populateDefaultGameInfo_();

    // タブを最初（インデックス0）に挿入
    m_tab->insertTab(0, m_gameInfoController->containerWidget(), tr("対局情報"));

    // 保存されたタブインデックスを復元
    int savedIndex = SettingsService::lastSelectedTabIndex();
    if (savedIndex >= 0 && savedIndex < m_tab->count()) {
        m_tab->setCurrentIndex(savedIndex);
    } else {
        m_tab->setCurrentIndex(0);
    }

    // タブ変更時にインデックスを保存するシグナルを接続
    connect(m_tab, &QTabWidget::currentChanged,
            this, &MainWindow::onTabCurrentChanged,
            Qt::UniqueConnection);
}

// ★ 追加: 対局情報テーブルにデフォルト値を設定
void MainWindow::populateDefaultGameInfo_()
{
    if (!m_gameInfoController) return;

    // デフォルトの対局情報を設定
    QList<KifGameInfoItem> defaultItems;
    defaultItems.append({tr("先手"), tr("先手")});
    defaultItems.append({tr("後手"), tr("後手")});
    defaultItems.append({tr("手合割"), tr("平手")});

    m_gameInfoController->setGameInfo(defaultItems);
}

// ★ 追加: 対局者名設定フック（将棋盤ラベル更新）
void MainWindow::onSetPlayersNames_(const QString& p1, const QString& p2)
{
    qDebug().noquote() << "[MW] onSetPlayersNames_: p1=" << p1 << " p2=" << p2;

    // 将棋盤の対局者名ラベルを更新
    if (m_shogiView) {
        m_shogiView->setBlackPlayerName(p1);
        m_shogiView->setWhitePlayerName(p2);
    }

    // 対局情報タブの先手・後手を更新
    updateGameInfoPlayerNames_(p1, p2);
}

// ★ 追加: エンジン名設定フック
void MainWindow::onSetEngineNames_(const QString& e1, const QString& e2)
{
    qDebug().noquote() << "[MW] ★★★ onSetEngineNames_ CALLED ★★★";
    qDebug().noquote() << "[MW] onSetEngineNames_: e1=" << e1 << " e2=" << e2;
    qDebug().noquote() << "[MW] onSetEngineNames_: current m_engineName1=" << m_engineName1 << " m_engineName2=" << m_engineName2;
    qDebug().noquote() << "[MW] onSetEngineNames_: current m_playMode=" << static_cast<int>(m_playMode);

    // メンバ変数に保存
    m_engineName1 = e1;
    m_engineName2 = e2;

    qDebug().noquote() << "[MW] onSetEngineNames_: after save m_engineName1=" << m_engineName1 << " m_engineName2=" << m_engineName2;

    // ログモデル名を更新
    setEngineNamesBasedOnMode();

    // ★ 追加: EvE対局時に2番目のエンジン情報を表示
    updateSecondEngineVisibility();

    // 将棋盤の対局者名ラベルを更新（PlayModeに応じて）
    setPlayersNamesForMode();

    // 対局情報タブも更新
    updateGameInfoForCurrentMatch_();
    
    // ★ 追加: 評価値グラフコントローラにもエンジン名を設定
    if (m_evalGraphController) {
        m_evalGraphController->setEngine1Name(m_engineName1);
        m_evalGraphController->setEngine2Name(m_engineName2);
    }
    
    qDebug().noquote() << "[MW] ★★★ onSetEngineNames_ END ★★★";
}

// ★ 追加: 対局情報タブの先手・後手名を更新
void MainWindow::updateGameInfoPlayerNames_(const QString& blackName, const QString& whiteName)
{
    if (m_gameInfoController) {
        m_gameInfoController->updatePlayerNames(blackName, whiteName);
    }
}

// ★ 追加: 元の対局情報を保存（棋譜読み込み時に呼ばれる）
void MainWindow::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    if (m_gameInfoController) {
        m_gameInfoController->setGameInfo(items);
    }
}

// ★ 追加: 現在の対局に基づいて対局情報タブを更新
void MainWindow::updateGameInfoForCurrentMatch_()
{
    if (!m_gameInfoController) return;

    // 現在のPlayModeと対局者名に基づいて先手・後手を決定
    QString blackName;
    QString whiteName;

    switch (m_playMode) {
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    case EvenEngineVsEngine:
    case HandicapEngineVsEngine:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case HumanVsHuman:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    default:
        blackName = tr("先手");
        whiteName = tr("後手");
        break;
    }

    updateGameInfoPlayerNames_(blackName, whiteName);
}

// ★ 追加: 対局者名確定時のスロット
void MainWindow::onPlayerNamesResolved_(const QString& human1, const QString& human2,
                                        const QString& engine1, const QString& engine2,
                                        int playMode)
{
    qDebug().noquote() << "[MW] onPlayerNamesResolved_:"
                       << " human1=" << human1
                       << " human2=" << human2
                       << " engine1=" << engine1
                       << " engine2=" << engine2
                       << " playMode=" << playMode;

    // メンバ変数に保存
    m_humanName1 = human1;
    m_humanName2 = human2;
    m_engineName1 = engine1;
    m_engineName2 = engine2;
    m_playMode = static_cast<PlayMode>(playMode);

    // 将棋盤の対局者名ラベルを更新
    setPlayersNamesForMode();

    // エンジン名をログモデルに反映
    setEngineNamesBasedOnMode();

    // ★ 追加: EvE対局時に2番目のエンジン情報を表示
    updateSecondEngineVisibility();

    // 対局情報タブを更新
    updateGameInfoForCurrentMatch_();
}

// ★ 追加: GameInfoPaneControllerからの更新通知
void MainWindow::onGameInfoUpdated_(const QList<KifGameInfoItem>& items)
{
    Q_UNUSED(items);
    qDebug().noquote() << "[MW] onGameInfoUpdated_: Game info updated, items=" << items.size();
    // 必要に応じて他のコンポーネントに通知
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

    // ★ 追加：盤面適用後に手番表示を更新
    setCurrentTurn();
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
        return qMax(sfenMax, kifuMax);
    }

    // 既に解決済み行がある（棋譜ファイル読み込み後など）のとき：
    // その行に表示するエントリ数（disp.size()）が末尾。
    const int clamped = static_cast<int>(qBound(qsizetype(0), qsizetype(row), m_resolvedRows.size() - 1));
    return static_cast<int>(m_resolvedRows[clamped].disp.size());
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
    // ★ 追加: 未保存のコメント編集がある場合、警告を表示
    if (m_analysisTab && m_analysisTab->hasUnsavedComment()) {
        const int editingRow = m_analysisTab->currentMoveIndex();
        // 同じ行への移動でなければ警告
        if (ply != editingRow) {
            qDebug().noquote()
                << "[MW] applySelect: UNSAVED_COMMENT_CHECK"
                << " targetPly=" << ply
                << " editingRow=" << editingRow
                << " hasUnsavedComment=true";
            if (!m_analysisTab->confirmDiscardUnsavedComment()) {
                qDebug().noquote() << "[MW] applySelect: User cancelled, aborting navigation";
                return;  // ナビゲーションを中断
            }
        }
    }

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

                // ★ 追加：棋譜欄のハイライト行を更新
                m_kifuRecordModel->setCurrentHighlightRow(safe);

                // 盤・ハイライト即時同期（従来の onMainMoveRowChanged と同じ流れ）
                syncBoardAndHighlightsAtRow(safe);

                // ★ 修正点：currentPly() のベースになるトラッキングを更新
                m_activePly          = safe;   // ← 追加
                m_currentSelectedPly = safe;
                m_currentMoveIndex   = safe;

                // ★ 追加：盤面適用後に手番表示を更新
                setCurrentTurn();
            }
        }
        return;
    }

    // 通常（KIF再生/分岐再生）ルート
    applyResolvedRowAndSelect(row, ply);
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
    qDebug().nospace()
    << "[UI] onMatchGameEnded ENTER cause="
    << ((info.cause==MatchCoordinator::Cause::Timeout)?"Timeout":"Resign")
    << " loser=" << ((info.loser==MatchCoordinator::P1)?"P1":"P2");

    // 対局終了時のスタイル維持を有効化（ダイアログ表示中もスタイルを維持）
    if (m_shogiView) m_shogiView->setGameOverStyleLock(true);

    // --- UI 後始末（UI層に残す） ---
    if (m_match)      m_match->disarmHumanTimerIfNeeded();
    if (ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr) {
        clk->stopClock();
    }
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
    const int recSizeBefore = (m_sfenRecord ? static_cast<int>(m_sfenRecord->size()) : 0);
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
        m_currentMoveIndex = static_cast<int>(m_sfenRecord->size() - 1); // 末尾（直近の局面）
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
        ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr;
        if (clk) {
            elapsed = (moverBefore == ShogiGameController::Player1)
                ? clk->getPlayer1ConsiderationAndTotalTime()
                : clk->getPlayer2ConsiderationAndTotalTime();
        } else {
            ensureTimeController_();
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
    ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr;
    if (clk) {
        clk->stopClock();
        clk->updateClock(); // 表示だけは最新化
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
            if (clk) {
                clk->updateClock();   // timeUpdated が飛び、既存の結線で applyClockUrgency が呼ばれる想定
            }
        }
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
    // 1) いまは即時呼び出しを行わず、0ms 遅延で呼ぶ
    QTimer::singleShot(0, this, &MainWindow::refreshBranchTreeLive_);

    // 2) （従来通り）EvE の評価グラフだけは維持
    const bool isEvE =
        (m_playMode == EvenEngineVsEngine) ||
        (m_playMode == HandicapEngineVsEngine);

    if (isEvE) {
        if (mover == ShogiGameController::Player1) {
            redrawEngine1EvaluationGraph(ply);
        } else if (mover == ShogiGameController::Player2) {
            redrawEngine2EvaluationGraph(ply);
        }
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

void MainWindow::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    // 司令塔が isOver / Cause / KIF一行追記 まで面倒を見る前提
    if (!st.isOver) return;

    // ★ 重要：投了行がまだ追加されていない場合は何もしない
    // 投了行追加後に再度このシグナルが発火するので、その時に処理する
    if (!st.moveAppended) {
        return;
    }

    // ★ 追加：ライブ追記モードを終了
    exitLiveAppendMode_();

    // ★ 追加：分岐コンテキストをリセット（分岐として処理されないように）
    if (m_kifuLoadCoordinator) {
        m_kifuLoadCoordinator->resetBranchContext();
    }

    // ★ 追加：m_currentSelectedPlyを先にリセット（refreshBranchTreeLive_で使われるため）
    m_currentSelectedPly = 0;

    // ★ 追加：対局終了時にresolvedRowsを構築（ナビゲーション用）
    refreshBranchTreeLive_();

    // ★ 追加：対局終了時に現在の手数（最終手）を正しく設定
    // これにより、終了直後のナビゲーションボタン操作が正常に動作する
    if (m_kifuRecordModel) {
        const int rows = m_kifuRecordModel->rowCount();
        if (rows > 0) {
            const int lastRow = rows - 1;  // 最終手の行（0始まり）
            m_activePly          = lastRow;
            m_currentSelectedPly = lastRow;
            m_currentMoveIndex   = lastRow;
        }
    }

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
}

void MainWindow::onPreStartCleanupRequested_()
{
    // --- 盤/ハイライト等のビジュアル初期化 ---
    if (m_boardController) {
        m_boardController->clearAllHighlights();
    }
    if (m_shogiView) {
        m_shogiView->blackClockLabel()->setText(QStringLiteral("00:00:00"));
        m_shogiView->whiteClockLabel()->setText(QStringLiteral("00:00:00"));
    }

    // --- 「現在の局面」から開始かどうかを判定 ---
    // 既存実装では 2局目開始直前に m_startSfenStr を明示クリア、
    // m_currentSfenStr に選択行の SFEN を入れているため、
    // これをトリガとして判定する。
    const bool startFromCurrentPos =
        m_startSfenStr.trimmed().isEmpty() && !m_currentSfenStr.trimmed().isEmpty();

    // 安全な keepRow（保持したい最終行＝選択中の行）を算出
    int keepRow = qMax(0, m_currentSelectedPly);

    // --- 棋譜モデルの扱い（ここが今回の修正ポイント） ---
    if (m_kifuRecordModel) {
        if (startFromCurrentPos) {
            // 1) 1局目の途中までを残して、末尾だけを削除
            const int rows = m_kifuRecordModel->rowCount();
            if (rows <= 0) {
                // 空なら見出しだけ用意
                m_kifuRecordModel->appendItem(
                    new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                    QStringLiteral("（１手 / 合計）")));
                keepRow = 0;
            } else {
                // keepRow をモデル範囲にクランプし、末尾の余剰行を一括削除
                if (keepRow > rows - 1) keepRow = rows - 1;
                const int toRemove = rows - (keepRow + 1);
                if (toRemove > 0) {
                    // QList の detach を避けつつ末尾からまとめて削除できる既存APIを使用
                    m_kifuRecordModel->removeLastItems(toRemove);
                }
            }
        } else {
            // 2) 平手/手合割など「新規初期局面から開始」のときは従来通り全消去
            m_kifuRecordModel->clearAllItems();
            // 見出し行を重複なく先頭へ
            m_kifuRecordModel->appendItem(
                new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                QStringLiteral("（１手 / 合計）")));
            keepRow = 0;
        }
    }

    // --- 手数トラッキングの更新 ---
    if (startFromCurrentPos) {
        m_activePly          = keepRow;
        m_currentSelectedPly = keepRow;
        m_currentMoveIndex   = keepRow;
    } else {
        m_activePly          = 0;
        m_currentSelectedPly = 0;
        m_currentMoveIndex   = 0;
    }

    // --- 分岐モデルは新規対局としてクリア ---
    if (m_kifuBranchModel) {
        m_kifuBranchModel->clear();
    }
    m_branchDisplayPlan.clear();

    // --- コメント欄は見た目リセット（Presenter管理でも表示は空に）
    broadcastComment(QString(), /*asHtml=*/true);

    // --- USI ログの初期化（既存内容を踏襲） ---
    qDebug().noquote() << "[MW] ★★★ onPreStartCleanupRequested_: RESETTING ENGINE NAMES ★★★";
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
    qDebug().noquote() << "[MW] ★★★ onPreStartCleanupRequested_: ENGINE NAMES RESET DONE ★★★";

    // ★ 修正: タブ選択は変更しない（ユーザーの選択を保持）
    // 以前は先頭（対局情報タブ）へ戻していたが、ユーザーの選択を尊重する
    // if (m_tab) {
    //     m_tab->setCurrentIndex(0);
    // }
    
    // ★ 追加: 新しい対局開始に備えて開始時刻をリセット
    if (m_timeController) {
        m_timeController->clearGameStartTime();
    }

    // デバッグログ
    qDebug().noquote()
        << "[MW] onPreStartCleanupRequested_: startFromCurrentPos=" << startFromCurrentPos
        << " keepRow=" << keepRow
        << " rows(after)=" << (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);
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

    // ★ デバッグ: 未保存コメント警告チェックの状態を出力
    // EngineAnalysisTab::m_currentMoveIndexが正しい「編集中の行」を保持している
    const int editingRow = (m_analysisTab ? m_analysisTab->currentMoveIndex() : -1);
    qDebug().noquote()
        << "[MW] UNSAVED_COMMENT_CHECK:"
        << " m_analysisTab=" << (m_analysisTab ? "valid" : "null")
        << " hasUnsavedComment=" << (m_analysisTab ? m_analysisTab->hasUnsavedComment() : false)
        << " row=" << row
        << " editingRow(from EngineAnalysisTab)=" << editingRow
        << " m_currentMoveIndex(MainWindow)=" << m_currentMoveIndex
        << " rowDiffers=" << (row != editingRow);

    // ★ 追加: 未保存のコメント編集がある場合、警告を表示
    if (m_analysisTab && m_analysisTab->hasUnsavedComment()) {
        qDebug().noquote() << "[MW] UNSAVED_COMMENT_CHECK: hasUnsavedComment=TRUE, checking row difference...";
        // 同じ行への「移動」は無視（初期化時など）
        // ★ 修正: MainWindow::m_currentMoveIndexではなくEngineAnalysisTab::currentMoveIndex()を使用
        if (row != editingRow) {
            qDebug().noquote() << "[MW] UNSAVED_COMMENT_CHECK: row differs, showing confirm dialog...";
            if (!m_analysisTab->confirmDiscardUnsavedComment()) {
                // キャンセルされた場合、元の行に戻す
                qDebug().noquote() << "[MW] User cancelled row change, reverting to row=" << editingRow;
                if (m_recordPane && m_recordPane->kifuView()) {
                    QTableView* kifuView = m_recordPane->kifuView();
                    if (kifuView->model() && editingRow >= 0 && editingRow < kifuView->model()->rowCount()) {
                        // シグナルをブロックして元の行に戻す
                        QSignalBlocker blocker(kifuView->selectionModel());
                        kifuView->setCurrentIndex(kifuView->model()->index(editingRow, 0));
                    }
                }
                return;  // 処理を中断
            } else {
                qDebug().noquote() << "[MW] UNSAVED_COMMENT_CHECK: User confirmed discard, proceeding...";
            }
        } else {
            qDebug().noquote() << "[MW] UNSAVED_COMMENT_CHECK: same row, skipping confirm dialog";
        }
    } else {
        qDebug().noquote() << "[MW] UNSAVED_COMMENT_CHECK: no unsaved comment or analysisTab is null";
    }

    // 盤面・ハイライト同期
    if (row >= 0) {
        qDebug().noquote() << "[MW] syncBoardAndHighlightsAtRow CALL row=" << row;
        syncBoardAndHighlightsAtRow(row);
        qDebug().noquote() << "[MW] syncBoardAndHighlightsAtRow DONE row=" << row;

        // ▼ 現在手数トラッキングを更新（NavigationController::next/prev 用）
        int oldActivePly = m_activePly;
        int oldCurrentSelectedPly = m_currentSelectedPly;
        int oldCurrentMoveIndex = m_currentMoveIndex;
        
        m_activePly          = row;   // ← これが無いと currentPly() が 0 のまま
        m_currentSelectedPly = row;
        m_currentMoveIndex   = row;

        qDebug().noquote()
            << "[MW] tracking UPDATED"
            << " activePly=" << oldActivePly << "->" << m_activePly
            << " currentSelectedPly=" << oldCurrentSelectedPly << "->" << m_currentSelectedPly
            << " currentMoveIndex=" << oldCurrentMoveIndex << "->" << m_currentMoveIndex;

        // ▼ 分岐候補欄の更新は Coordinator へ直接委譲
        if (m_kifuLoadCoordinator) {
            const qsizetype rows        = m_resolvedRows.size();
            const int resolvedRow = (rows <= 0) ? 0 : static_cast<int>(qBound(qsizetype(0), qsizetype(m_activeResolvedRow), rows - 1));
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
    QMessageBox::information(this, title, message);
}

void MainWindow::onRecordPaneMainRowChanged_(int row)
{
    // フォールバック：起動直後など Loader 未生成時でも UI が動くように最低限の同期を行う
    if (row >= 0) {
        syncBoardAndHighlightsAtRow(row);

        // ★ 修正点：currentPly() のベースになるトラッキングを更新
        m_activePly          = row;  // ← 追加
        m_currentSelectedPly = row;
        m_currentMoveIndex   = row;

        // ★ 追加：棋譜欄のハイライト行を更新（対局終了後のナビゲーション対応）
        if (m_kifuRecordModel) {
            m_kifuRecordModel->setCurrentHighlightRow(row);
        }

        // ★ 追加：盤面適用後に手番表示を更新
        setCurrentTurn();
    }
    enableArrowButtons();
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
    ensureKifuLoadCoordinatorForLive_();
    if (!m_kifuLoadCoordinator) return;

    int ply = 0;
    if (m_kifuRecordModel) {
        ply = qMax(0, m_kifuRecordModel->rowCount() - 1);
    }

    // ツリーの再構築（既存）
    m_kifuLoadCoordinator->updateBranchTreeFromLive(ply);

    // ★ 文脈を固定したまま、＋/オレンジ/候補欄を更新
    m_kifuLoadCoordinator->rebuildBranchPlanAndMarksForLive(ply);
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
    return (m_playMode == HumanVsHuman);
}

bool MainWindow::isHumanSide_(ShogiGameController::Player p) const
{
    // 「どちら側が人間か」を PlayMode から判定
    switch (m_playMode) {
    case HumanVsHuman:
        return true; // 両方人間
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
        return (p == ShogiGameController::Player1); // 先手＝人
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        return (p == ShogiGameController::Player2); // 後手＝人
    case EvenEngineVsEngine:
    case HandicapEngineVsEngine:
        return false;
    default:
        // 未開始/検討系は人が操作主体という前提
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
    // ★ GameRecordModel を使って KIF/KI2/CSA/JKF/USEN/USI 形式を生成
    ensureGameRecordModel_();

    QStringList kifLines;
    QStringList ki2Lines;
    QStringList csaLines;

    // CSA出力用のUSI指し手リストを取得
    // m_usiMovesが空の場合はKifuLoadCoordinatorから取得
    QStringList usiMovesForCsa = m_usiMoves;
    if (usiMovesForCsa.isEmpty() && m_kifuLoadCoordinator) {
        usiMovesForCsa = m_kifuLoadCoordinator->usiMoves();
        qDebug().noquote() << "[MW] saveKifuToFile: usiMoves obtained from KifuLoadCoordinator, size =" << usiMovesForCsa.size();
    }
    // ★ 修正: SFENレコードからUSI指し手を生成
    if (usiMovesForCsa.isEmpty() && m_sfenRecord && m_sfenRecord->size() > 1) {
        usiMovesForCsa = sfenRecordToUsiMoves_();
        qDebug().noquote() << "[MW] saveKifuToFile: usiMoves generated from sfenRecord, size =" << usiMovesForCsa.size();
    }

    // ★★★ デバッグ: usiMovesForCsa の状態を確認 ★★★
    qDebug().noquote() << "[MW] saveKifuToFile: usiMovesForCsa.size() =" << usiMovesForCsa.size();
    if (!usiMovesForCsa.isEmpty()) {
        qDebug().noquote() << "[MW] saveKifuToFile: usiMovesForCsa[0..min(5,size)] ="
                           << usiMovesForCsa.mid(0, qMin(5, usiMovesForCsa.size()));
    }

    if (m_gameRecord) {
        // ExportContext を構築
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.startSfen     = m_startSfenStr;
        ctx.playMode      = m_playMode;
        ctx.human1        = m_humanName1;
        ctx.human2        = m_humanName2;
        ctx.engine1       = m_engineName1;
        ctx.engine2       = m_engineName2;
        
        // ★ 時間制御情報をTimeControlControllerから取得
        if (m_timeController) {
            ctx.hasTimeControl = m_timeController->hasTimeControl();
            ctx.initialTimeMs = static_cast<int>(m_timeController->baseTimeMs());
            ctx.byoyomiMs = static_cast<int>(m_timeController->byoyomiMs());
            ctx.fischerIncrementMs = static_cast<int>(m_timeController->incrementMs());
            ctx.gameStartDateTime = m_timeController->gameStartDateTime();
        }

        // GameRecordModel から KIF/KI2/CSA/JKF/USEN/USI 形式の行リストを生成
        kifLines = m_gameRecord->toKifLines(ctx);
        ki2Lines = m_gameRecord->toKi2Lines(ctx);
        csaLines = m_gameRecord->toCsaLines(ctx, usiMovesForCsa);
        QStringList jkfLines = m_gameRecord->toJkfLines(ctx);
        QStringList usenLines = m_gameRecord->toUsenLines(ctx, usiMovesForCsa);
        QStringList usiLines = m_gameRecord->toUsiLines(ctx, usiMovesForCsa);

        qDebug().noquote() << "[MW] saveKifuToFile: generated" << kifLines.size() << "KIF lines,"
                           << ki2Lines.size() << "KI2 lines,"
                           << csaLines.size() << "CSA lines,"
                           << jkfLines.size() << "JKF lines,"
                           << usenLines.size() << "USEN lines,"
                           << usiLines.size() << "USI lines via GameRecordModel";

        m_kifuDataList = kifLines;

        // KIF/KI2/CSA/JKF/USEN/USI形式が利用可能な場合は新しいダイアログを使用
        const QString path = KifuSaveCoordinator::saveViaDialogWithUsi(
            this,
            kifLines,
            ki2Lines,
            csaLines,
            jkfLines,
            usenLines,
            usiLines,
            m_playMode,
            m_humanName1, m_humanName2,
            m_engineName1, m_engineName2);

        if (!path.isEmpty()) {
            kifuSaveFileName = path;
            if (m_gameRecord) {
                m_gameRecord->clearDirty();  // 保存完了で変更フラグをクリア
            }
            ui->statusbar->showMessage(tr("棋譜を保存しました: %1").arg(path), 5000);
        }
    } else {
        // フォールバック: 従来の KifuContentBuilder を使用（KIF形式のみ）
        KifuExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.resolvedRows  = &m_resolvedRows;
        if (m_recordPresenter) {
            ctx.liveDisp = &m_recordPresenter->liveDisp();
        }
        ctx.commentsByRow = &m_commentsByRow;
        ctx.activeResolvedRow = m_activeResolvedRow;
        ctx.startSfen = m_startSfenStr;
        ctx.playMode  = m_playMode;
        ctx.human1    = m_humanName1;
        ctx.human2    = m_humanName2;
        ctx.engine1   = m_engineName1;
        ctx.engine2   = m_engineName2;

        kifLines = KifuContentBuilder::buildKifuDataList(ctx);
        qDebug().noquote() << "[MW] saveKifuToFile: generated" << kifLines.size() << "lines via KifuContentBuilder (fallback)";

        m_kifuDataList = kifLines;

        // KIF/KI2/CSA形式のダイアログを使用（JKF未対応）
        const QString path = KifuSaveCoordinator::saveViaDialogWithAllFormats(
            this,
            kifLines,
            ki2Lines,
            csaLines,
            m_playMode,
            m_humanName1, m_humanName2,
            m_engineName1, m_engineName2);

        if (!path.isEmpty()) {
            kifuSaveFileName = path;
            if (m_gameRecord) {
                m_gameRecord->clearDirty();  // 保存完了で変更フラグをクリア
            }
            ui->statusbar->showMessage(tr("棋譜を保存しました: %1").arg(path), 5000);
        }
    }
}

// ★ 追加: KIF形式で棋譜をクリップボードにコピー
void MainWindow::copyKifToClipboard()
{
    ensureGameRecordModel_();

    KifuClipboardService::ExportContext ctx = buildClipboardContext_();

    if (KifuClipboardService::copyKif(ctx)) {
        ui->statusbar->showMessage(tr("KIF形式の棋譜をクリップボードにコピーしました"), 3000);
    } else {
        ui->statusbar->showMessage(tr("KIF形式の棋譜データがありません"), 3000);
    }
}

// ★ 追加: KI2形式で棋譜をクリップボードにコピー
void MainWindow::copyKi2ToClipboard()
{
    ensureGameRecordModel_();

    KifuClipboardService::ExportContext ctx = buildClipboardContext_();

    if (KifuClipboardService::copyKi2(ctx)) {
        ui->statusbar->showMessage(tr("KI2形式の棋譜をクリップボードにコピーしました"), 3000);
    } else {
        ui->statusbar->showMessage(tr("KI2形式の棋譜データがありません"), 3000);
    }
}

// ★ 追加: CSA形式で棋譜をクリップボードにコピー
void MainWindow::copyCsaToClipboard()
{
    ensureGameRecordModel_();

    // ★★★ デバッグ: 各ソースの状態を確認 ★★★
    qDebug().noquote() << "[MW][CSA-DEBUG] ========== copyCsaToClipboard START ==========";
    qDebug().noquote() << "[MW][CSA-DEBUG] m_usiMoves.size() =" << m_usiMoves.size();
    qDebug().noquote() << "[MW][CSA-DEBUG] m_kifuLoadCoordinator =" << (m_kifuLoadCoordinator ? "valid" : "nullptr");
    if (m_kifuLoadCoordinator) {
        qDebug().noquote() << "[MW][CSA-DEBUG] m_kifuLoadCoordinator->usiMoves().size() =" << m_kifuLoadCoordinator->usiMoves().size();
    }
    qDebug().noquote() << "[MW][CSA-DEBUG] m_sfenRecord =" << (m_sfenRecord ? "valid" : "nullptr");
    if (m_sfenRecord) {
        qDebug().noquote() << "[MW][CSA-DEBUG] m_sfenRecord->size() =" << m_sfenRecord->size();
    }

    // CSA出力用のUSI指し手リストを取得
    QStringList usiMovesForCsa = m_usiMoves;
    qDebug().noquote() << "[MW][CSA-DEBUG] After m_usiMoves: usiMovesForCsa.size() =" << usiMovesForCsa.size();
    
    if (usiMovesForCsa.isEmpty() && m_kifuLoadCoordinator) {
        usiMovesForCsa = m_kifuLoadCoordinator->usiMoves();
        qDebug().noquote() << "[MW][CSA-DEBUG] After KifuLoadCoordinator: usiMovesForCsa.size() =" << usiMovesForCsa.size();
    }
    // ★ 修正: SFENレコードからUSI指し手を生成（対局中/終了後の完全なデータ）
    if (usiMovesForCsa.isEmpty() && m_sfenRecord && m_sfenRecord->size() > 1) {
        usiMovesForCsa = sfenRecordToUsiMoves_();
        qDebug().noquote() << "[MW][CSA-DEBUG] After sfenRecordToUsiMoves_: usiMovesForCsa.size() =" << usiMovesForCsa.size();
        qDebug().noquote() << "[MW] copyCsaToClipboard: generated" << usiMovesForCsa.size() << "USI moves from sfenRecord";
    }

    // ★★★ デバッグ: 最終的なUSI指し手リストを出力 ★★★
    qDebug().noquote() << "[MW][CSA-DEBUG] Final usiMovesForCsa.size() =" << usiMovesForCsa.size();
    for (qsizetype i = 0; i < qMin(usiMovesForCsa.size(), qsizetype(25)); ++i) {
        qDebug().noquote() << "[MW][CSA-DEBUG]   usiMovesForCsa[" << i << "] =" << usiMovesForCsa[i];
    }

    QStringList csaLines;

    if (m_gameRecord) {
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.startSfen     = m_startSfenStr;
        ctx.playMode      = m_playMode;
        ctx.human1        = m_humanName1;
        ctx.human2        = m_humanName2;
        ctx.engine1       = m_engineName1;
        ctx.engine2       = m_engineName2;
        
        // ★ 時間制御情報をTimeControlControllerから取得
        if (m_timeController) {
            ctx.hasTimeControl = m_timeController->hasTimeControl();
            ctx.initialTimeMs = static_cast<int>(m_timeController->baseTimeMs());
            ctx.byoyomiMs = static_cast<int>(m_timeController->byoyomiMs());
            ctx.fischerIncrementMs = static_cast<int>(m_timeController->incrementMs());
            ctx.gameStartDateTime = m_timeController->gameStartDateTime();
        }

        csaLines = m_gameRecord->toCsaLines(ctx, usiMovesForCsa);
        qDebug().noquote() << "[MW] copyCsaToClipboard: generated" << csaLines.size() << "CSA lines via GameRecordModel";
    }

    if (csaLines.isEmpty()) {
        ui->statusbar->showMessage(tr("CSA形式の棋譜データがありません"), 3000);
        return;
    }

    const QString csaText = csaLines.join(QStringLiteral("\n"));

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(csaText);
        ui->statusbar->showMessage(tr("CSA形式の棋譜をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[MW] copyCsaToClipboard: copied to clipboard," << csaText.size() << "chars";
    } else {
        qWarning() << "[MW] copyCsaToClipboard: clipboard is not available";
        ui->statusbar->showMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    }
    qDebug().noquote() << "[MW][CSA-DEBUG] ========== copyCsaToClipboard END ==========";
}

// ★ 追加: USI形式（全て）で棋譜をクリップボードにコピー
void MainWindow::copyUsiToClipboard()
{
    ensureGameRecordModel_();

    // USI出力用の指し手リストを取得
    QStringList usiMovesForOutput = m_usiMoves;
    if (usiMovesForOutput.isEmpty() && m_kifuLoadCoordinator) {
        usiMovesForOutput = m_kifuLoadCoordinator->usiMoves();
    }
    // ★ 修正: SFENレコードからUSI指し手を生成
    if (usiMovesForOutput.isEmpty() && m_sfenRecord && m_sfenRecord->size() > 1) {
        usiMovesForOutput = sfenRecordToUsiMoves_();
        qDebug().noquote() << "[MW] copyUsiToClipboard: generated" << usiMovesForOutput.size() << "USI moves from sfenRecord";
    }

    QStringList usiLines;

    if (m_gameRecord) {
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.startSfen     = m_startSfenStr;
        ctx.playMode      = m_playMode;
        ctx.human1        = m_humanName1;
        ctx.human2        = m_humanName2;
        ctx.engine1       = m_engineName1;
        ctx.engine2       = m_engineName2;

        usiLines = m_gameRecord->toUsiLines(ctx, usiMovesForOutput);
        qDebug().noquote() << "[MW] copyUsiToClipboard: generated" << usiLines.size() << "USI lines via GameRecordModel";
    }

    if (usiLines.isEmpty()) {
        ui->statusbar->showMessage(tr("USI形式の棋譜データがありません"), 3000);
        return;
    }

    const QString usiText = usiLines.join(QStringLiteral("\n"));

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(usiText);
        ui->statusbar->showMessage(tr("USI形式の棋譜をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[MW] copyUsiToClipboard: copied to clipboard," << usiText.size() << "chars";
    } else {
        qWarning() << "[MW] copyUsiToClipboard: clipboard is not available";
        ui->statusbar->showMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    }
}

// ★ 追加: USI形式（現在の指し手まで）で棋譜をクリップボードにコピー
void MainWindow::copyUsiCurrentToClipboard()
{
    ensureGameRecordModel_();

    // 現在の指し手までのUSI指し手リストを取得
    QStringList usiMovesForOutput = m_usiMoves;
    if (usiMovesForOutput.isEmpty() && m_kifuLoadCoordinator) {
        usiMovesForOutput = m_kifuLoadCoordinator->usiMoves();
    }
    // ★ 修正: SFENレコードからUSI指し手を生成
    if (usiMovesForOutput.isEmpty() && m_sfenRecord && m_sfenRecord->size() > 1) {
        usiMovesForOutput = sfenRecordToUsiMoves_();
        qDebug().noquote() << "[MW] copyUsiCurrentToClipboard: generated" << usiMovesForOutput.size() << "USI moves from sfenRecord";
    }

    // 現在の手数（m_currentMoveIndex）までに制限
    // m_currentMoveIndex は 0始まりの手数インデックス
    const int currentPlyLimit = m_currentMoveIndex;
    if (currentPlyLimit >= 0 && currentPlyLimit < usiMovesForOutput.size()) {
        usiMovesForOutput = usiMovesForOutput.mid(0, currentPlyLimit);
        qDebug().noquote() << "[MW] copyUsiCurrentToClipboard: limited to" << currentPlyLimit << "moves";
    }

    QStringList usiLines;

    if (m_gameRecord) {
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.startSfen     = m_startSfenStr;
        ctx.playMode      = m_playMode;
        ctx.human1        = m_humanName1;
        ctx.human2        = m_humanName2;
        ctx.engine1       = m_engineName1;
        ctx.engine2       = m_engineName2;

        usiLines = m_gameRecord->toUsiLines(ctx, usiMovesForOutput);
        qDebug().noquote() << "[MW] copyUsiCurrentToClipboard: generated" << usiLines.size() << "USI lines via GameRecordModel";
    }

    if (usiLines.isEmpty()) {
        ui->statusbar->showMessage(tr("USI形式の棋譜データがありません"), 3000);
        return;
    }

    const QString usiText = usiLines.join(QStringLiteral("\n"));

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(usiText);
        ui->statusbar->showMessage(tr("USI形式（現在の指し手まで）の棋譜をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[MW] copyUsiCurrentToClipboard: copied to clipboard," << usiText.size() << "chars";
    } else {
        qWarning() << "[MW] copyUsiCurrentToClipboard: clipboard is not available";
        ui->statusbar->showMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    }
}

// ★ 追加: JKF形式で棋譜をクリップボードにコピー
void MainWindow::copyJkfToClipboard()
{
    ensureGameRecordModel_();

    QStringList jkfLines;

    if (m_gameRecord) {
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.startSfen     = m_startSfenStr;
        ctx.playMode      = m_playMode;
        ctx.human1        = m_humanName1;
        ctx.human2        = m_humanName2;
        ctx.engine1       = m_engineName1;
        ctx.engine2       = m_engineName2;

        jkfLines = m_gameRecord->toJkfLines(ctx);
        qDebug().noquote() << "[MW] copyJkfToClipboard: generated" << jkfLines.size() << "JKF lines via GameRecordModel";
    }

    if (jkfLines.isEmpty()) {
        ui->statusbar->showMessage(tr("JKF形式の棋譜データがありません"), 3000);
        return;
    }

    const QString jkfText = jkfLines.join(QStringLiteral("\n"));

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(jkfText);
        ui->statusbar->showMessage(tr("JKF形式の棋譜をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[MW] copyJkfToClipboard: copied to clipboard," << jkfText.size() << "chars";
    } else {
        qWarning() << "[MW] copyJkfToClipboard: clipboard is not available";
        ui->statusbar->showMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    }
}

// ★ 追加: USEN形式で棋譜をクリップボードにコピー
void MainWindow::copyUsenToClipboard()
{
    ensureGameRecordModel_();

    // USEN出力用のUSI指し手リストを取得
    QStringList usiMovesForOutput = m_usiMoves;
    if (usiMovesForOutput.isEmpty() && m_kifuLoadCoordinator) {
        usiMovesForOutput = m_kifuLoadCoordinator->usiMoves();
    }
    // ★ 修正: SFENレコードからUSI指し手を生成
    if (usiMovesForOutput.isEmpty() && m_sfenRecord && m_sfenRecord->size() > 1) {
        usiMovesForOutput = sfenRecordToUsiMoves_();
        qDebug().noquote() << "[MW] copyUsenToClipboard: generated" << usiMovesForOutput.size() << "USI moves from sfenRecord";
    }

    QStringList usenLines;

    if (m_gameRecord) {
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.startSfen     = m_startSfenStr;
        ctx.playMode      = m_playMode;
        ctx.human1        = m_humanName1;
        ctx.human2        = m_humanName2;
        ctx.engine1       = m_engineName1;
        ctx.engine2       = m_engineName2;

        usenLines = m_gameRecord->toUsenLines(ctx, usiMovesForOutput);
        qDebug().noquote() << "[MW] copyUsenToClipboard: generated" << usenLines.size() << "USEN lines via GameRecordModel";
    }

    if (usenLines.isEmpty()) {
        ui->statusbar->showMessage(tr("USEN形式の棋譜データがありません"), 3000);
        return;
    }

    const QString usenText = usenLines.join(QStringLiteral("\n"));

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(usenText);
        ui->statusbar->showMessage(tr("USEN形式の棋譜をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[MW] copyUsenToClipboard: copied to clipboard," << usenText.size() << "chars";
    } else {
        qWarning() << "[MW] copyUsenToClipboard: clipboard is not available";
        ui->statusbar->showMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    }
}

// ★ 追加: SFEN形式で局面をクリップボードにコピー
void MainWindow::copySfenToClipboard()
{
    QString sfenStr;

    // デバッグ: 各種状態を出力
    qDebug().noquote() << "[MW] copySfenToClipboard: ========== DEBUG START ==========";
    qDebug().noquote() << "[MW] copySfenToClipboard: m_currentMoveIndex =" << m_currentMoveIndex;
    qDebug().noquote() << "[MW] copySfenToClipboard: m_activePly =" << m_activePly;
    qDebug().noquote() << "[MW] copySfenToClipboard: m_currentSelectedPly =" << m_currentSelectedPly;
    qDebug().noquote() << "[MW] copySfenToClipboard: m_isLiveAppendMode =" << m_isLiveAppendMode;
    qDebug().noquote() << "[MW] copySfenToClipboard: currentPly() =" << currentPly();
    qDebug().noquote() << "[MW] copySfenToClipboard: m_playMode =" << m_playMode;
    qDebug().noquote() << "[MW] copySfenToClipboard: m_sfenRecord =" << (m_sfenRecord ? "valid" : "nullptr");
    if (m_sfenRecord) {
        qDebug().noquote() << "[MW] copySfenToClipboard: m_sfenRecord->size() =" << m_sfenRecord->size();
        for (int i = 0; i < m_sfenRecord->size(); ++i) {
            qDebug().noquote() << "[MW] copySfenToClipboard: m_sfenRecord[" << i << "] =" << m_sfenRecord->at(i);
        }
    }
    
    // RecordPane の現在選択行を確認
    if (m_recordPane) {
        const QTableView* view = m_recordPane->kifuView();
        if (view) {
            const QModelIndex cur = view->currentIndex();
            qDebug().noquote() << "[MW] copySfenToClipboard: kifuView currentIndex valid =" << cur.isValid()
                               << ", row =" << cur.row();
        } else {
            qDebug().noquote() << "[MW] copySfenToClipboard: kifuView is nullptr";
        }
    } else {
        qDebug().noquote() << "[MW] copySfenToClipboard: m_recordPane is nullptr";
    }

    // 現在選択されている手数に対応するSFEN文字列を取得
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        int idx = -1;
        
        // 対局中かどうかを判定
        // m_playMode が対局モードで、かつゲームが終了していない場合のみ「対局中」とみなす
        const bool isPlayingMode = (m_playMode == EvenHumanVsEngine || 
                                    m_playMode == EvenEngineVsHuman || 
                                    m_playMode == EvenEngineVsEngine ||
                                    m_playMode == HandicapEngineVsHuman ||
                                    m_playMode == HandicapHumanVsEngine ||
                                    m_playMode == HandicapEngineVsEngine ||
                                    m_playMode == HumanVsHuman);
        
        // ゲームが終了しているかどうかをチェック
        const bool isGameOver = (m_match && m_match->gameOverState().isOver);
        
        // 対局中（対局モードかつ終了していない）の場合のみ最新局面を使用
        const bool isPlaying = isPlayingMode && !isGameOver;
        
        qDebug().noquote() << "[MW] copySfenToClipboard: isPlayingMode =" << isPlayingMode
                           << ", isGameOver =" << isGameOver
                           << ", isPlaying =" << isPlaying;
        
        if (isPlaying) {
            // 対局中は最新局面を使用
            idx = static_cast<int>(m_sfenRecord->size() - 1);
            qDebug().noquote() << "[MW] copySfenToClipboard: playing mode, using latest sfenRecord idx =" << idx;
        } else {
            // リプレイ/観戦モード、または対局終了後は選択されている手を使用
            idx = currentPly();
            qDebug().noquote() << "[MW] copySfenToClipboard: replay/finished mode, using currentPly() idx =" << idx;
        }
        
        // 範囲チェック
        if (idx < 0) {
            idx = 0;
            qDebug().noquote() << "[MW] copySfenToClipboard: idx adjusted to 0 (was negative)";
        } else if (idx >= m_sfenRecord->size()) {
            idx = static_cast<int>(m_sfenRecord->size() - 1);
            qDebug().noquote() << "[MW] copySfenToClipboard: idx adjusted to" << idx << "(was >= size)";
        }
        sfenStr = m_sfenRecord->at(idx);
        qDebug().noquote() << "[MW] copySfenToClipboard: final idx =" << idx
                           << ", sfenStr =" << sfenStr;
    } else if (m_gameController && m_gameController->board()) {
        qDebug().noquote() << "[MW] copySfenToClipboard: using board fallback (no sfenRecord)";
        // sfenRecordがない場合は、現在のボード状態からSFENを生成
        ShogiBoard* board = m_gameController->board();
        const QString boardSfen = board->convertBoardToSfen();
        QString standSfen = board->convertStandToSfen();
        if (standSfen.isEmpty()) {
            standSfen = QStringLiteral("-");
        }
        const QString turn = board->currentPlayer();
        // 手数は currentPly() + 1（SFEN形式では1始まり）
        const int moveNum = qMax(1, currentPly() + 1);
        sfenStr = QStringLiteral("%1 %2 %3 %4").arg(boardSfen, turn, standSfen, QString::number(moveNum));
        qDebug().noquote() << "[MW] copySfenToClipboard: generated from board =" << sfenStr;
    }

    qDebug().noquote() << "[MW] copySfenToClipboard: ========== DEBUG END ==========";

    if (sfenStr.isEmpty()) {
        ui->statusbar->showMessage(tr("SFEN形式の局面データがありません"), 3000);
        return;
    }

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(sfenStr);
        ui->statusbar->showMessage(tr("SFEN形式の局面をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[MW] copySfenToClipboard: copied to clipboard," << sfenStr.size() << "chars";
    } else {
        qWarning() << "[MW] copySfenToClipboard: clipboard is not available";
        ui->statusbar->showMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    }
}

// ★ 追加: BOD形式で局面をクリップボードにコピー
void MainWindow::copyBodToClipboard()
{
    // SFEN文字列を取得（copySfenToClipboard と同じロジック）
    QString sfenStr;
    int moveIndex = 0;  // 手数表示用
    QString lastMoveStr;  // 最終手表示用
    
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        int idx = -1;
        
        const bool isPlayingMode = (m_playMode == EvenHumanVsEngine || 
                                    m_playMode == EvenEngineVsHuman || 
                                    m_playMode == EvenEngineVsEngine ||
                                    m_playMode == HandicapEngineVsHuman ||
                                    m_playMode == HandicapHumanVsEngine ||
                                    m_playMode == HandicapEngineVsEngine ||
                                    m_playMode == HumanVsHuman);
        
        const bool isGameOver = (m_match && m_match->gameOverState().isOver);
        const bool isPlaying = isPlayingMode && !isGameOver;
        
        if (isPlaying) {
            idx = static_cast<int>(m_sfenRecord->size() - 1);
        } else {
            idx = currentPly();
        }
        
        if (idx < 0) idx = 0;
        else if (idx >= m_sfenRecord->size()) idx = static_cast<int>(m_sfenRecord->size() - 1);
        
        sfenStr = m_sfenRecord->at(idx);
        moveIndex = idx;  // 手数（0始まり、初期局面=0、1手目=1）
        
        // 最終手の情報を取得（棋譜モデルから）
        if (idx > 0 && m_kifuRecordModel && idx < m_kifuRecordModel->rowCount()) {
            QModelIndex modelIdx = m_kifuRecordModel->index(idx, 0);
            lastMoveStr = m_kifuRecordModel->data(modelIdx, Qt::DisplayRole).toString();
        }
    } else if (m_gameController && m_gameController->board()) {
        ShogiBoard* board = m_gameController->board();
        const QString boardSfen = board->convertBoardToSfen();
        QString standSfen = board->convertStandToSfen();
        if (standSfen.isEmpty()) standSfen = QStringLiteral("-");
        const QString turn = board->currentPlayer();
        const int moveNum = qMax(1, currentPly() + 1);
        sfenStr = QStringLiteral("%1 %2 %3 %4").arg(boardSfen, turn, standSfen, QString::number(moveNum));
        moveIndex = currentPly();
    }
    
    if (sfenStr.isEmpty()) {
        ui->statusbar->showMessage(tr("BOD形式の局面データがありません"), 3000);
        return;
    }
    
    // SFEN文字列を解析
    const QStringList parts = sfenStr.split(QLatin1Char(' '));
    if (parts.size() < 4) {
        ui->statusbar->showMessage(tr("SFEN形式の解析に失敗しました"), 3000);
        return;
    }
    
    const QString boardSfen = parts[0];
    const QString turnSfen = parts[1];
    const QString handSfen = parts[2];
    const int sfenMoveNum = parts[3].toInt();
    
    // 持ち駒を解析してマップに格納
    // 先手: 大文字 (P, L, N, S, G, B, R)
    // 後手: 小文字 (p, l, n, s, g, b, r)
    QMap<QChar, int> senteHand;
    QMap<QChar, int> goteHand;
    
    if (handSfen != QStringLiteral("-")) {
        int count = 0;
        for (qsizetype i = 0; i < handSfen.size(); ++i) {
            const QChar c = handSfen.at(i);
            if (c.isDigit()) {
                count = count * 10 + c.digitValue();
            } else {
                if (count == 0) count = 1;
                if (c.isUpper()) {
                    senteHand[c] += count;
                } else {
                    goteHand[c.toUpper()] += count;
                }
                count = 0;
            }
        }
    }
    
    // 持ち駒を日本語文字列に変換
    auto handToString = [](const QMap<QChar, int>& hand) -> QString {
        if (hand.isEmpty()) {
            return QStringLiteral("なし");
        }
        
        // 駒の順番: 飛、角、金、銀、桂、香、歩
        const QString order = QStringLiteral("RBGSNLP");
        const QMap<QChar, QString> pieceNames = {
            {QLatin1Char('R'), QStringLiteral("飛")},
            {QLatin1Char('B'), QStringLiteral("角")},
            {QLatin1Char('G'), QStringLiteral("金")},
            {QLatin1Char('S'), QStringLiteral("銀")},
            {QLatin1Char('N'), QStringLiteral("桂")},
            {QLatin1Char('L'), QStringLiteral("香")},
            {QLatin1Char('P'), QStringLiteral("歩")}
        };
        const QMap<int, QString> kanjiNumbers = {
            {2, QStringLiteral("二")},
            {3, QStringLiteral("三")},
            {4, QStringLiteral("四")},
            {5, QStringLiteral("五")},
            {6, QStringLiteral("六")},
            {7, QStringLiteral("七")},
            {8, QStringLiteral("八")},
            {9, QStringLiteral("九")},
            {10, QStringLiteral("十")},
            {11, QStringLiteral("十一")},
            {12, QStringLiteral("十二")},
            {13, QStringLiteral("十三")},
            {14, QStringLiteral("十四")},
            {15, QStringLiteral("十五")},
            {16, QStringLiteral("十六")},
            {17, QStringLiteral("十七")},
            {18, QStringLiteral("十八")}
        };
        
        QString result;
        for (qsizetype i = 0; i < order.size(); ++i) {
            const QChar piece = order.at(i);
            if (hand.contains(piece) && hand[piece] > 0) {
                result += pieceNames[piece];
                if (hand[piece] > 1) {
                    result += kanjiNumbers.value(hand[piece], QString::number(hand[piece]));
                }
                result += QStringLiteral("　");
            }
        }
        
        if (result.isEmpty()) {
            return QStringLiteral("なし");
        }
        return result.trimmed();
    };
    
    // 盤面を9x9配列に展開
    QVector<QVector<QString>> board(9, QVector<QString>(9));
    const QStringList ranks = boardSfen.split(QLatin1Char('/'));
    for (qsizetype rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
        const QString& rankStr = ranks[rank];
        int file = 0;
        bool promoted = false;
        for (qsizetype k = 0; k < rankStr.size() && file < 9; ++k) {
            const QChar c = rankStr.at(k);
            if (c == QLatin1Char('+')) {
                promoted = true;
            } else if (c.isDigit()) {
                const int skip = c.toLatin1() - '0';
                for (int s = 0; s < skip && file < 9; ++s) {
                    board[rank][file++] = QStringLiteral(" ・");
                }
                promoted = false;
            } else {
                QString piece;
                if (c.isUpper()) {
                    // 先手の駒
                    piece = QStringLiteral(" ");
                } else {
                    // 後手の駒
                    piece = QStringLiteral("v");
                }
                
                const QChar upperC = c.toUpper();
                const QMap<QChar, QString> pieceChars = {
                    {QLatin1Char('P'), promoted ? QStringLiteral("と") : QStringLiteral("歩")},
                    {QLatin1Char('L'), promoted ? QStringLiteral("杏") : QStringLiteral("香")},
                    {QLatin1Char('N'), promoted ? QStringLiteral("圭") : QStringLiteral("桂")},
                    {QLatin1Char('S'), promoted ? QStringLiteral("全") : QStringLiteral("銀")},
                    {QLatin1Char('G'), QStringLiteral("金")},
                    {QLatin1Char('B'), promoted ? QStringLiteral("馬") : QStringLiteral("角")},
                    {QLatin1Char('R'), promoted ? QStringLiteral("龍") : QStringLiteral("飛")},
                    {QLatin1Char('K'), QStringLiteral("玉")}
                };
                
                piece += pieceChars.value(upperC, QStringLiteral("？"));
                board[rank][file++] = piece;
                promoted = false;
            }
        }
        // 残りのマスを空白で埋める
        while (file < 9) {
            board[rank][file++] = QStringLiteral(" ・");
        }
    }
    
    // BOD形式の文字列を生成
    QStringList bodLines;
    
    // 後手の持駒
    bodLines << QStringLiteral("後手の持駒：%1").arg(handToString(goteHand));
    
    // 筋の番号行
    bodLines << QStringLiteral("  ９ ８ ７ ６ ５ ４ ３ ２ １");
    
    // 上端の罫線
    bodLines << QStringLiteral("+---------------------------+");
    
    // 段名
    const QStringList rankNames = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
    };
    
    // 盤面の各段
    for (int rank = 0; rank < 9; ++rank) {
        QString line = QStringLiteral("|");
        for (int file = 0; file < 9; ++file) {
            line += board[rank][file];
        }
        line += QStringLiteral("|") + rankNames[rank];
        bodLines << line;
    }
    
    // 下端の罫線
    bodLines << QStringLiteral("+---------------------------+");
    
    // 先手の持駒
    bodLines << QStringLiteral("先手の持駒：%1").arg(handToString(senteHand));
    
    // 手番
    if (turnSfen == QStringLiteral("b")) {
        bodLines << QStringLiteral("先手番");
    } else {
        bodLines << QStringLiteral("後手番");
    }
    
    // 手数と最終手
    if (moveIndex > 0) {
        // sfenMoveNum は次の手番の手数なので、表示は sfenMoveNum - 1
        const int displayMoveNum = sfenMoveNum - 1;
        if (!lastMoveStr.isEmpty()) {
            bodLines << QStringLiteral("手数＝%1  %2  まで").arg(displayMoveNum).arg(lastMoveStr);
        } else {
            bodLines << QStringLiteral("手数＝%1  まで").arg(displayMoveNum);
        }
    }
    
    const QString bodText = bodLines.join(QStringLiteral("\n"));
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(bodText);
        ui->statusbar->showMessage(tr("BOD形式の局面をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[MW] copyBodToClipboard: copied to clipboard," << bodText.size() << "chars";
    } else {
        qWarning() << "[MW] copyBodToClipboard: clipboard is not available";
        ui->statusbar->showMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    }
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

    // ★ GameRecordModel を使って KIF 形式を生成
    ensureGameRecordModel_();

    if (m_gameRecord) {
        // ExportContext を構築
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.startSfen     = m_startSfenStr;
        ctx.playMode      = m_playMode;
        ctx.human1        = m_humanName1;
        ctx.human2        = m_humanName2;
        ctx.engine1       = m_engineName1;
        ctx.engine2       = m_engineName2;

        // GameRecordModel から KIF 形式の行リストを生成
        m_kifuDataList = m_gameRecord->toKifLines(ctx);

        qDebug().noquote() << "[MW] overwriteKifuFile: generated" << m_kifuDataList.size() << "lines via GameRecordModel";
    } else {
        // フォールバック: 従来の KifuContentBuilder を使用
        KifuExportContext ctx;
        ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
        ctx.recordModel   = m_kifuRecordModel;
        ctx.resolvedRows  = &m_resolvedRows;
        if (m_recordPresenter) {
            ctx.liveDisp = &m_recordPresenter->liveDisp();
        }
        ctx.commentsByRow = &m_commentsByRow;
        ctx.activeResolvedRow = m_activeResolvedRow;
        ctx.startSfen = m_startSfenStr;
        ctx.playMode  = m_playMode;
        ctx.human1    = m_humanName1;
        ctx.human2    = m_humanName2;
        ctx.engine1   = m_engineName1;
        ctx.engine2   = m_engineName2;

        m_kifuDataList = KifuContentBuilder::buildKifuDataList(ctx);
        qDebug().noquote() << "[MW] overwriteKifuFile: generated" << m_kifuDataList.size() << "lines via KifuContentBuilder (fallback)";
    }

    QString error;
    const bool ok = KifuSaveCoordinator::overwriteExisting(kifuSaveFileName, m_kifuDataList, &error);
    if (ok) {
        if (m_gameRecord) {
            m_gameRecord->clearDirty();  // 保存完了で変更フラグをクリア
        }
        ui->statusbar->showMessage(tr("棋譜を上書き保存しました: %1").arg(kifuSaveFileName), 5000);
    } else {
        QMessageBox::warning(this, tr("KIF Save Error"), error);
    }
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

    // ★ GameRecordModel を使ってコメントを一括更新
    ensureGameRecordModel_();
    if (m_gameRecord) {
        m_gameRecord->setComment(moveIndex, newComment);
    }

    // ★ m_commentsByRow も同期（互換性・RecordPresenterへの供給用）
    while (m_commentsByRow.size() <= moveIndex) {
        m_commentsByRow.append(QString());
    }
    m_commentsByRow[moveIndex] = newComment;

    // RecordPresenter のコメント配列も更新（行選択時に正しいコメントを表示するため）
    if (m_recordPresenter) {
        QStringList updatedComments;
        updatedComments.reserve(m_commentsByRow.size());
        for (const auto& c : m_commentsByRow) {
            updatedComments.append(c);
        }
        m_recordPresenter->setCommentsByRow(updatedComments);
        qDebug().noquote() << "[MW] Updated RecordPresenter commentsByRow";
    }

    // 現在表示中のコメントを更新（両方のコメント欄に反映）
    const QString displayComment = newComment.trimmed().isEmpty() ? tr("コメントなし") : newComment;
    broadcastComment(displayComment, /*asHtml=*/true);

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

    qDebug().noquote() << "[MW] ensureGameRecordModel_: created and bound";
}

// ★ 追加: GameRecordModel::commentChanged スロット
void MainWindow::onGameRecordCommentChanged(int ply, const QString& /*comment*/)
{
    qDebug().noquote() << "[MW] GameRecordModel::commentChanged ply=" << ply;
}

// ★ 追加: 読み筋クリック処理
void MainWindow::onPvRowClicked(int engineIndex, int row)
{
    qDebug() << "[MainWindow] onPvRowClicked: engineIndex=" << engineIndex << " row=" << row;

    // 対象のモデルを取得
    ShogiEngineThinkingModel* model = (engineIndex == 0) ? m_modelThinking1 : m_modelThinking2;
    if (!model) {
        qDebug() << "[MainWindow] onPvRowClicked: model is null";
        return;
    }

    // モデルの行数をチェック
    if (row < 0 || row >= model->rowCount()) {
        qDebug() << "[MainWindow] onPvRowClicked: row out of range";
        return;
    }

    // ShogiInfoRecord を取得
    const ShogiInfoRecord* record = model->recordAt(row);
    if (!record) {
        qDebug() << "[MainWindow] onPvRowClicked: record is null";
        return;
    }

    // 読み筋（漢字表記）を取得
    QString kanjiPv = record->pv();
    qDebug() << "[MainWindow] onPvRowClicked: kanjiPv=" << kanjiPv;

    if (kanjiPv.isEmpty()) {
        qDebug() << "[MainWindow] onPvRowClicked: kanjiPv is empty";
        return;
    }

    // USI形式の読み筋を取得
    QString usiPvStr = record->usiPv();
    QStringList usiMoves;
    if (!usiPvStr.isEmpty()) {
        usiMoves = usiPvStr.split(' ', Qt::SkipEmptyParts);
        qDebug() << "[MainWindow] onPvRowClicked: usiMoves from record:" << usiMoves;
    }

    // USI moves が空の場合、UsiCommLogModel から検索を試みる
    if (usiMoves.isEmpty()) {
        UsiCommLogModel* logModel = (engineIndex == 0) ? m_lineEditModel1 : m_lineEditModel2;
        if (logModel) {
            QString fullLog = logModel->usiCommLog();
            QStringList lines = fullLog.split('\n');

            // 後ろから検索して、該当する深さの info pv を探す
            for (qsizetype i = lines.size() - 1; i >= 0; --i) {
                const QString& line = lines.at(i);
                if (line.contains(QStringLiteral("info ")) && line.contains(QStringLiteral(" pv "))) {
                    qsizetype pvPos = line.indexOf(QStringLiteral(" pv "));
                    if (pvPos >= 0) {
                        QString pvPart = line.mid(pvPos + 4).trimmed();
                        usiMoves = pvPart.split(' ', Qt::SkipEmptyParts);
                        if (!usiMoves.isEmpty()) {
                            qDebug() << "[MainWindow] onPvRowClicked: found usiMoves from log:" << usiMoves;
                            break;
                        }
                    }
                }
            }
        }
    }

    // 現在の局面SFENを取得（ShogiInfoRecordに保存されているbaseSfenを使用）
    QString currentSfen = record->baseSfen();
    qDebug() << "[MainWindow] onPvRowClicked: baseSfen from record:" << currentSfen;
    
    // baseSfenが空の場合のフォールバック
    if (currentSfen.isEmpty() && m_sfenRecord && m_sfenRecord->size() >= 2) {
        // エンジンの思考開始時の局面（position送信時の局面）
        currentSfen = m_sfenRecord->at(m_sfenRecord->size() - 2).trimmed();
        qDebug() << "[MainWindow] onPvRowClicked: fallback to sfenRecord[size-2]:" << currentSfen;
    } else if (currentSfen.isEmpty() && m_sfenRecord && !m_sfenRecord->isEmpty()) {
        // レコードが1つしかない場合はそれを使用
        currentSfen = m_sfenRecord->first().trimmed();
        qDebug() << "[MainWindow] onPvRowClicked: fallback to sfenRecord first:" << currentSfen;
    }
    if (currentSfen.isEmpty()) {
        currentSfen = m_currentSfenStr;
    }
    if (currentSfen.isEmpty()) {
        currentSfen = m_startSfenStr;
    }
    if (currentSfen.isEmpty()) {
        currentSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }

    qDebug() << "[MainWindow] onPvRowClicked: currentSfen=" << currentSfen;

    // PvBoardDialog を表示
    PvBoardDialog* dlg = new PvBoardDialog(currentSfen, usiMoves, this);
    dlg->setKanjiPv(kanjiPv);
    
    // 対局者名を設定（PlayModeに応じて適切な名前を選択）
    QString blackName;
    QString whiteName;
    switch (m_playMode) {
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    case EvenEngineVsEngine:
    case HandicapEngineVsEngine:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case HumanVsHuman:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    default:
        blackName = m_humanName1.isEmpty() ? m_engineName1 : m_humanName1;
        whiteName = m_humanName2.isEmpty() ? m_engineName2 : m_humanName2;
        if (blackName.isEmpty()) blackName = tr("先手");
        if (whiteName.isEmpty()) whiteName = tr("後手");
        break;
    }
    dlg->setPlayerNames(blackName, whiteName);
    
    // 起動時の局面に至った最後の手を設定（USI形式）
    // m_usiMovesが空の場合はm_gameMovesから生成
    QString lastUsiMove;
    qDebug() << "[MainWindow] onPvRowClicked: m_usiMoves.size()=" << m_usiMoves.size()
             << " m_gameMoves.size()=" << m_gameMoves.size();
    
    if (!m_usiMoves.isEmpty()) {
        lastUsiMove = m_usiMoves.last();
        qDebug() << "[MainWindow] onPvRowClicked: using m_usiMoves.last():" << lastUsiMove;
    } else if (!m_gameMoves.isEmpty()) {
        // m_gameMovesから最後の手をUSI形式に変換
        // m_gameMovesの座標は0始まりなので+1してUSI形式（1始まり）に変換
        const ShogiMove& lastMove = m_gameMoves.last();
        int fromFile = lastMove.fromSquare.x() + 1;  // 0始まり→1始まり
        int fromRank = lastMove.fromSquare.y() + 1;  // 0始まり→1始まり
        int toFile = lastMove.toSquare.x() + 1;      // 0始まり→1始まり
        int toRank = lastMove.toSquare.y() + 1;      // 0始まり→1始まり
        
        qDebug() << "[MainWindow] onPvRowClicked: lastMove from m_gameMoves (after +1):"
                 << " fromFile=" << fromFile << " fromRank=" << fromRank
                 << " toFile=" << toFile << " toRank=" << toRank
                 << " isPromotion=" << lastMove.isPromotion;
        
        if (lastMove.fromSquare.x() == 0 && lastMove.fromSquare.y() == 0) {
            // 駒打ちの場合: P*5e 形式（元の座標が(0,0)かどうかで判定）
            // movingPieceは大文字/小文字で先手/後手を区別
            QChar pieceChar = lastMove.movingPiece.toUpper();
            QChar rankChar = QChar('a' + toRank - 1);  // 1段目='a'
            lastUsiMove = QString("%1*%2%3").arg(pieceChar).arg(toFile).arg(rankChar);
        } else {
            // 通常の移動: 7g7f 形式
            QChar fromRankChar = QChar('a' + fromRank - 1);  // 1段目='a'
            QChar toRankChar = QChar('a' + toRank - 1);      // 1段目='a'
            lastUsiMove = QString("%1%2%3%4").arg(fromFile).arg(fromRankChar).arg(toFile).arg(toRankChar);
            if (lastMove.isPromotion) {
                lastUsiMove += '+';
            }
        }
        qDebug() << "[MainWindow] onPvRowClicked: generated USI move:" << lastUsiMove;
    } else {
        qDebug() << "[MainWindow] onPvRowClicked: both m_usiMoves and m_gameMoves are empty";
    }
    
    if (!lastUsiMove.isEmpty()) {
        qDebug() << "[MainWindow] onPvRowClicked: calling setLastMove with:" << lastUsiMove;
        dlg->setLastMove(lastUsiMove);
    }
    
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// ★ 追加: タブ選択変更時のスロット（インデックスを設定ファイルに保存）
void MainWindow::onTabCurrentChanged(int index)
{
    // タブインデックスを設定ファイルに保存
    SettingsService::setLastSelectedTabIndex(index);
    qDebug().noquote() << "[MW] onTabCurrentChanged: saved tab index =" << index;
}

// ★ 追加: ShogiMoveリストからUSI形式の指し手リストを生成
QStringList MainWindow::gameMovesToUsiMoves_(const QVector<ShogiMove>& moves) const
{
    QStringList usiMoves;
    usiMoves.reserve(moves.size());

    for (const ShogiMove& mv : moves) {
        QString usiMove;

        // 駒打ちの判定: SfenPositionTracer::dropFromSquareの仕様では先手=9, 後手=10
        // 従って x >= 9 は駒打ちを示す
        if (mv.fromSquare.x() >= 9) {
            // 駒打ちの場合: "P*5e" 形式
            QChar pieceChar = mv.movingPiece.toUpper();
            int toFile = mv.toSquare.x() + 1;         // 0始まり → 1始まり
            int toRank = mv.toSquare.y() + 1;         // 0始まり → 1始まり
            QChar toRankChar = QChar('a' + toRank - 1);  // 1段目 = 'a'
            usiMove = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toFile).arg(toRankChar);
        } else {
            // 通常移動の場合: "7g7f" 形式
            int fromFile = mv.fromSquare.x() + 1;     // 0始まり → 1始まり
            int fromRank = mv.fromSquare.y() + 1;     // 0始まり → 1始まり
            int toFile = mv.toSquare.x() + 1;         // 0始まり → 1始まり
            int toRank = mv.toSquare.y() + 1;         // 0始まり → 1始まり
            QChar fromRankChar = QChar('a' + fromRank - 1);  // 1段目 = 'a'
            QChar toRankChar = QChar('a' + toRank - 1);      // 1段目 = 'a'
            usiMove = QStringLiteral("%1%2%3%4").arg(fromFile).arg(fromRankChar).arg(toFile).arg(toRankChar);
            if (mv.isPromotion) {
                usiMove += QLatin1Char('+');
            }
        }

        usiMoves.append(usiMove);
    }

    return usiMoves;
}

// ★ 追加: SFENレコードからUSI形式の指し手リストを生成
// SFEN文字列の差分を解析して各手のUSI指し手を逆算する
QStringList MainWindow::sfenRecordToUsiMoves_() const
{
    QStringList usiMoves;
    
    if (!m_sfenRecord || m_sfenRecord->size() < 2) {
        qDebug().noquote() << "[MW] sfenRecordToUsiMoves_: sfenRecord is empty or too small";
        return usiMoves;
    }
    
    qDebug().noquote() << "[MW] sfenRecordToUsiMoves_: sfenRecord size =" << m_sfenRecord->size();
    
    // 連続するSFEN文字列から指し手を解析
    for (int i = 1; i < m_sfenRecord->size(); ++i) {
        const QString& prevSfen = m_sfenRecord->at(i - 1);
        const QString& currSfen = m_sfenRecord->at(i);
        
        // SFEN文字列を解析: "board turn hand moveNo"
        const QStringList prevParts = prevSfen.split(QLatin1Char(' '));
        const QStringList currParts = currSfen.split(QLatin1Char(' '));
        
        if (prevParts.size() < 3 || currParts.size() < 3) {
            qDebug().noquote() << "[MW] sfenRecordToUsiMoves_: invalid SFEN at index" << i;
            usiMoves.append(QString());
            continue;
        }
        
        const QString prevBoard = prevParts[0];
        const QString currBoard = currParts[0];
        
        // 盤面を9x9配列に展開
        // board[rank][file] where rank=0 is 1段目, file=0 is 9筋
        auto expandBoard = [](const QString& boardStr) -> QVector<QVector<QString>> {
            QVector<QVector<QString>> board(9, QVector<QString>(9));
            const QStringList ranks = boardStr.split(QLatin1Char('/'));
            for (qsizetype rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
                const QString& rankStr = ranks[rank];
                int file = 0;
                bool promoted = false;
                for (qsizetype k = 0; k < rankStr.size() && file < 9; ++k) {
                    QChar c = rankStr[k];
                    if (c == QLatin1Char('+')) {
                        promoted = true;
                    } else if (c.isDigit()) {
                        int skip = c.toLatin1() - '0';
                        for (int s = 0; s < skip && file < 9; ++s) {
                            board[rank][file++] = QString();
                        }
                        promoted = false;
                    } else {
                        QString piece = promoted ? QStringLiteral("+") + QString(c) : QString(c);
                        board[rank][file++] = piece;
                        promoted = false;
                    }
                }
            }
            return board;
        };
        
        QVector<QVector<QString>> prevBoardArr = expandBoard(prevBoard);
        QVector<QVector<QString>> currBoardArr = expandBoard(currBoard);
        
        // 変化した位置を検出
        QPoint fromPos(-1, -1);
        QPoint toPos(-1, -1);
        QString movedPiece;
        bool isDrop = false;
        bool isPromotion = false;
        
        // 駒がなくなった位置（from）と駒が現れた/変わった位置（to）を検索
        // 2パス方式: まず全ての変化を収集してから判断
        QVector<QPoint> emptyPositions;  // 駒がなくなった位置
        QVector<QPoint> filledPositions; // 駒が現れた/変わった位置
        
        for (int rank = 0; rank < 9; ++rank) {
            for (int file = 0; file < 9; ++file) {
                const QString& prev = prevBoardArr[rank][file];
                const QString& curr = currBoardArr[rank][file];
                
                if (prev != curr) {
                    if (!prev.isEmpty() && curr.isEmpty()) {
                        // 駒がなくなった位置 → from候補
                        emptyPositions.append(QPoint(file, rank));
                    } else if (!curr.isEmpty()) {
                        // 駒が現れた/変わった位置 → to候補
                        filledPositions.append(QPoint(file, rank));
                    }
                }
            }
        }
        
        // 通常移動: 1つの位置が空になり、1つの位置に駒が現れる/変わる
        if (emptyPositions.size() == 1 && filledPositions.size() == 1) {
            fromPos = emptyPositions[0];
            toPos = filledPositions[0];
            movedPiece = prevBoardArr[fromPos.y()][fromPos.x()];
            
            // 成りの判定
            const QString& movedPieceFinal = currBoardArr[toPos.y()][toPos.x()];
            QString baseFrom = movedPiece.startsWith(QLatin1Char('+')) ? movedPiece.mid(1) : movedPiece;
            QString baseTo = movedPieceFinal.startsWith(QLatin1Char('+')) ? movedPieceFinal.mid(1) : movedPieceFinal;
            if (baseFrom.toUpper() == baseTo.toUpper() && 
                movedPieceFinal.startsWith(QLatin1Char('+')) && !movedPiece.startsWith(QLatin1Char('+'))) {
                isPromotion = true;
            }
        }
        // 駒打ち: 駒が空から現れる（持ち駒から）
        else if (emptyPositions.isEmpty() && filledPositions.size() == 1) {
            isDrop = true;
            toPos = filledPositions[0];
        }
        // 取る手: 2つの位置が変化（移動元が空になり、移動先の駒が変わる）
        // emptyPositions.size() == 1 && filledPositions.size() == 1 でカバー済み
        
        // USI形式の指し手を生成
        QString usiMove;
        if (isDrop && toPos.x() >= 0) {
            // 駒打ち: "P*5e" 形式
            QString droppedPiece = currBoardArr[toPos.y()][toPos.x()];
            QChar pieceChar = droppedPiece.isEmpty() ? QLatin1Char('P') : droppedPiece[0].toUpper();
            int toFileNum = 9 - toPos.x();  // file=0は9筋、file=8は1筋
            QChar toRankChar = QChar('a' + toPos.y());
            usiMove = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toFileNum).arg(toRankChar);
        } else if (fromPos.x() >= 0 && toPos.x() >= 0) {
            // 通常移動: "7g7f" 形式
            int fromFileNum = 9 - fromPos.x();  // file=0は9筋、file=8は1筋
            int toFileNum = 9 - toPos.x();
            QChar fromRankChar = QChar('a' + fromPos.y());
            QChar toRankChar = QChar('a' + toPos.y());
            usiMove = QStringLiteral("%1%2%3%4").arg(fromFileNum).arg(fromRankChar).arg(toFileNum).arg(toRankChar);
            if (isPromotion) {
                usiMove += QLatin1Char('+');
            }
        }
        
        qDebug().noquote() << "[MW] sfenRecordToUsiMoves_: move" << i << "=" << usiMove
                           << "(from=" << fromPos << "to=" << toPos << "drop=" << isDrop << "promo=" << isPromotion << ")";
        
        usiMoves.append(usiMove);
    }
    
    return usiMoves;
}

// ★ 追加: クリップボード操作用コンテキスト構築
KifuClipboardService::ExportContext MainWindow::buildClipboardContext_() const
{
    KifuClipboardService::ExportContext ctx;
    ctx.gameInfoTable = m_gameInfoController ? m_gameInfoController->tableWidget() : nullptr;
    ctx.recordModel   = m_kifuRecordModel;
    ctx.gameRecord    = m_gameRecord;
    ctx.startSfen     = m_startSfenStr;
    ctx.playMode      = m_playMode;
    ctx.human1        = m_humanName1;
    ctx.human2        = m_humanName2;
    ctx.engine1       = m_engineName1;
    ctx.engine2       = m_engineName2;
    ctx.usiMoves      = m_usiMoves;
    ctx.sfenRecord    = m_sfenRecord;
    ctx.currentPly    = currentPly();
    ctx.isPlaying     = isCurrentlyPlaying_();
    return ctx;
}

// ★ 追加: 対局中かどうかを判定
bool MainWindow::isCurrentlyPlaying_() const
{
    const bool isPlayingMode = (m_playMode == EvenHumanVsEngine ||
                                m_playMode == EvenEngineVsHuman ||
                                m_playMode == EvenEngineVsEngine ||
                                m_playMode == HandicapEngineVsHuman ||
                                m_playMode == HandicapHumanVsEngine ||
                                m_playMode == HandicapEngineVsEngine ||
                                m_playMode == HumanVsHuman);
    const bool isGameOver = (m_match && m_match->gameOverState().isOver);
    return isPlayingMode && !isGameOver;
}
