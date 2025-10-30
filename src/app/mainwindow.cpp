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

#include "mainwindow.h"
#include "promotedialog.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "ui_mainwindow.h"
#include "engineregistrationdialog.h"
#include "versiondialog.h"
#include "usi.h"
#include "startgamedialog.h"
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

using KifuIoService::makeDefaultSaveFileName;
using KifuIoService::writeKifuFile;
using namespace EngineSettingsConstants;
using GameOverCause = MatchCoordinator::Cause;
using BCDI = ::BranchCandidateDisplayItem;

namespace {
// 0始まり(0..8)なら1始まり(1..9)へ、すでに1..9なら変更なし
static inline QPoint normalizeBoardPoint_(const QPoint& p) {
    if (p.x() >= 0 && p.x() <= 8 && p.y() >= 0 && p.y() <= 8)
        return QPoint(p.x() + 1, p.y() + 1);
    return p;
}
}

// ---- 無名名前空間: 共有ユーティリティ ----
namespace {

inline int rankLetterToNum(QChar r) { // 'a'..'i' -> 1..9
    ushort u = r.toLower().unicode();
    return (u < 'a' || u > 'i') ? -1 : int(u - 'a') + 1;
}

inline QChar dropLetterWithSide(QChar upper, bool black) {
    return black ? upper.toUpper() : upper.toLower();
}

// 成駒トークン("+P"等) -> 1文字表現。非成駒はトークンそのまま1文字。
inline QChar tokenToOneChar(const QString& tok) {
    if (tok.isEmpty()) return QLatin1Char(' ');
    if (tok.size() == 1) return tok.at(0);
    static const QHash<QString,QChar> map = {
                                              {"+P",'Q'},{"+L",'M'},{"+N",'O'},{"+S",'T'},{"+B",'C'},{"+R",'U'},
                                              {"+p",'q'},{"+l",'m'},{"+n",'o'},{"+s",'t'},{"+b",'c'},{"+r",'u'},
                                              };
    const auto it = map.find(tok);
    return it == map.end() ? QLatin1Char(' ') : *it;
}

// ★打ちの fromSquare を駒台座標にマップ
inline QPoint dropFromSquare(QChar dropUpper, bool black) {
    const int x = black ? 9 : 10; // 先手=9, 後手=10
    int y = -1;
    switch (dropUpper.toUpper().unicode()) {
    case 'P': y = black ? 0 : 8; break;
    case 'L': y = black ? 1 : 7; break;
    case 'N': y = black ? 2 : 6; break;
    case 'S': y = black ? 3 : 5; break;
    case 'G': y = 4;            break; // 共通
    case 'B': y = black ? 5 : 3; break;
    case 'R': y = black ? 6 : 2; break;
    default:  y = -1;           break;
    }
    return QPoint(x, y);
}

} // anonymous namespace

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
    , m_gameCount(0)
    , m_gameController(nullptr)
    , m_analysisModel(nullptr)
{
    ui->setupUi(this);

    setupCentralWidgetContainer_();

    configureToolBarFromUi_();

    // コア部品（GC, View, 盤モデル etc.）は既存関数で初期化
    initializeComponents();

    // 画面骨格（棋譜/分岐/レイアウト/タブ/中央表示）
    buildGamePanels_();

    // 初期UI状態（ボタン無効化・編集メニュー非表示など）
    applyInitialUiState_();

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

    // ※ initializeComponents() 内で ensureTurnSyncBridge_() を呼んでいる想定。
    //   二重呼び出しを避けるため、ここでは呼ばない。
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
    setupRecordPane();

    // m_kifuBranchModel / m_varEngine の初期化が済んでいる前提で分岐配線
    setupBranchCandidatesWiring_();

    // 盤・駒台などの初期化（既存の newGame とは重なるが、従来の順序を維持）
    startNewShogiGame(m_startSfenStr);

    // 盤＋各パネルの横並びレイアウト構築
    setupHorizontalGameLayout();

    // EngineAnalysisTab を作ってタブ（m_tab）を用意
    setupEngineAnalysisTab();

    // m_tab を central に add する側を初期化
    initializeCentralGameDisplay();
}

void MainWindow::applyInitialUiState_()
{
    // 対局メニューの一部は必要に応じて。デフォルトでは非実行
    // hideGameActions();

    // 棋譜欄の下の矢印ボタンを無効化
    disableArrowButtons();

    // 局面編集メニューを隠す
    hidePositionEditMenu();
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
    // 既存のユーティリティを使用（MainWindow::parseStartPositionToSfen）
    QString start = m_startSfenStr;
    if (start.isEmpty()) {
        // 既定：平手初期局面の完全SFEN
        start = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    } else if (start.startsWith(QLatin1String("startpos"))) {
        start = parseStartPositionToSfen(start);
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

// 新規対局の準備をする。
// 将棋盤、駒台を初期化（何も駒がない）し、入力のSFEN文字列の配置に将棋盤、駒台の駒を
// 配置し、対局結果を結果なし、現在の手番がどちらでもない状態に設定する。
void MainWindow::initializeNewGame(QString& startSfenStr)
{
    m_gameController->newGame(startSfenStr);

    if (!m_resumeSfenStr.isEmpty()) {
        auto* b = m_gameController ? m_gameController->board() : nullptr;
        if (b) {
            b->setSfen(m_resumeSfenStr);
            if (m_shogiView) m_shogiView->applyBoardAndRender(b);
        }
    }
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

// "sfen 〜"で始まる文字列startpositionstrを入力して"sfen "を削除したSFEN文字列を
// 返す。startposの場合は、
// "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"を
// 返す。
// 例．
//  0. 平手
// "startpos",
//  1. 香落ち
// "sfen lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  2. 右香落ち"
// "sfen 1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  3. 角落ち
// "sfen lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  4. 飛車落ち
// "sfen lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  5. 飛香落ち
// "sfen lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  6. 二枚落ち
// "sfen lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  7. 三枚落ち
// "sfen lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  8. 四枚落ち
// "sfen 1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
//  9. 五枚落ち
// "sfen 2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
// 10. 左五枚落ち
// "sfen 1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
// 11. 六枚落ち
// "sfen 2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
// 12. 八枚落ち
// "sfen 3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
// 13. 十枚落ち
// "sfen 4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"
QString MainWindow::parseStartPositionToSfen(QString startPositionStr)
{
    if (startPositionStr == "startpos") {
        return "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
    } else {
        static QRegularExpression r("sfen ");
        QRegularExpressionMatch match = r.match(startPositionStr);

        // SFEN形式の文字列に誤りがある場合
        if (!match.hasMatch()) {
            // エラーメッセージを表示する。
            const QString errorMessage = tr("An error occurred in MainWindow::getStartSfenStr. There is an error in the SFEN format string.");
            displayErrorMessage(errorMessage);

            return QString();
        } else {
            return startPositionStr.remove("sfen ");
        }
    }
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

// 将棋盤、駒台を初期化（何も駒がない）し、入力のSFEN文字列の配置に将棋盤、駒台の駒を
// 配置し、対局結果を結果なし、現在の手番がどちらでもない状態に設定する。
// 将棋盤の表示
// 将棋の駒画像を各駒文字（1文字）にセットする。
// 駒文字と駒画像をm_piecesに格納する。
// m_piecesの型はQMap<char, QIcon>
// m_boardにboardをセットする。
// 将棋盤データが更新されたら再描画する。
// 将棋盤と駒台のサイズは固定にする。
// 将棋盤と駒台のマスのサイズをセットする。
// 将棋盤と駒台の再描画
// 対局者名の設定
// 対局モードに応じて将棋盤上部に表示される対局者名をセットする。
// エンジン名の設定
// 対局モードに応じて将棋盤下部に表示されるエンジン名をセットする。
void MainWindow::startNewShogiGame(QString& startSfenStr)
{
    const bool resume = m_isResumeFromCurrent;

    // 評価値グラフと記録の初期化（再開時は消さない）
    if (auto ec = m_recordPane ? m_recordPane->evalChart() : nullptr) {
        if (!resume) ec->clearAll();       // ★ ガード
    }
    if (!resume) m_scoreCp.clear();        // ★ ガード

    // 新規対局の準備
    // 将棋盤、駒台を初期化（何も駒がない）し、入力のSFEN文字列の配置に将棋盤、駒台の駒を
    // 配置し、対局結果を結果なし、現在の手番がどちらでもない状態に設定する。
    initializeNewGame(startSfenStr);

    // 将棋盤の表示
    m_shogiView->applyBoardAndRender(m_gameController->board());
    m_shogiView->configureFixedSizing();

    // 対局者名の設定
    // 対局モードに応じて将棋盤上部に表示される対局者名をセットする。
    setPlayersNamesForMode();

    // エンジン名の設定
    // 対局モードに応じて将棋盤下部に表示されるエンジン名をセットする。
    setEngineNamesBasedOnMode();

    // ★ 追加：開局時は両者とも未着手
    m_p1HasMoved = false;
    m_p2HasMoved = false;
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

// 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
void MainWindow::updateTurnAndTimekeepingDisplay()
{
    // KIF再生中は時計を動かさない（統一）
    if (m_isReplayMode) {
        if (m_shogiClock) m_shogiClock->stopClock();
        if (m_match)      m_match->pokeTimeUpdateNow();
        return;
    }

    // 終局後は何もしない（時計停止と整えのみ）
    const bool gameOver = (m_match && m_match->gameOverState().isOver);
    if (gameOver) {
        if (m_shogiClock) m_shogiClock->stopClock();
        if (m_match) {
            m_match->pokeTimeUpdateNow();
            m_match->disarmHumanTimerIfNeeded();
        }
        return;
    }

    // 1) まず停止して残時間を確定
    if (m_shogiClock) m_shogiClock->stopClock();

    // 2) 次手番を判定（= GC の currentPlayer がこれから指す側）
    const bool nextIsP1 = (m_gameController &&
                           m_gameController->currentPlayer() == ShogiGameController::Player1);

    // 3) 直前手の byoyomi/increment 適用と経過テキストの取得（サービスへ）
    const QString elapsed =
        TimekeepingService::applyByoyomiAndCollectElapsed(m_shogiClock, nextIsP1);
    if (!elapsed.isEmpty()) {
        // 既存仕様どおり「この手」の消費/累計を1行追記
        updateGameRecord(elapsed);
    }

    // 4) UIの手番表示
    updateTurnStatus(nextIsP1 ? 1 : 2);

    // 5) 時計・司令塔の後処理（poke, start, epoch 記録、人間タイマのアーム/解除）
    TimekeepingService::finalizeTurnPresentation(m_shogiClock, m_match, m_gameController,
                                                 nextIsP1, m_isReplayMode);
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

// 対局モードを決定する。
PlayMode MainWindow::determinePlayMode(const int initPositionNumber, const bool isPlayer1Human, const bool isPlayer2Human) const
{
    // 平手の対局の場合
    if (initPositionNumber == 1) {
        // 平手の人間対人間
        if (isPlayer1Human && isPlayer2Human) return HumanVsHuman;

        // 平手の人間対エンジン
        if (isPlayer1Human && !isPlayer2Human) return EvenHumanVsEngine;

        // 平手のエンジン対人間
        if (!isPlayer1Human && isPlayer2Human) return EvenEngineVsHuman;

        // 平手のエンジン対エンジン
        if (!isPlayer1Human && !isPlayer2Human) return EvenEngineVsEngine;
    }
    // 駒落ちの対局の場合
    else {
        // 駒落ちの人間対人間
        if (isPlayer1Human && isPlayer2Human) return HumanVsHuman;

        // 駒落ちの人間対エンジン
        if (isPlayer1Human && !isPlayer2Human) return HandicapHumanVsEngine;

        // 駒落ちのエンジン対人間
        if (!isPlayer1Human && isPlayer2Human) return HandicapEngineVsHuman;

        // 駒落ちのエンジン対エンジン
        if (!isPlayer1Human && !isPlayer2Human) return HandicapEngineVsEngine;
    }

    // 対局モードエラー
    return PlayModeError;
}

// 対局モードを決定する。
PlayMode MainWindow::setPlayMode()
{
    // 対局の開始局面番号を取得する。
    int initPositionNumber = m_startGameDialog->startingPositionNumber();

    // 対局者1が人間であるかを取得する。
    bool isPlayerHuman1 = m_startGameDialog->isHuman1();

    // 対局者2が人間であるかを取得する。
    bool isPlayerHuman2 = m_startGameDialog->isHuman2();

    // 対局者1がエンジンであるかを取得する。
    bool isPlayerEngine1 = m_startGameDialog->isEngine1();

    // 対局者2がエンジンであるかを取得する。
    bool isPlayerEngine2 = m_startGameDialog->isEngine2();

    // 対局モードを決定する。
    PlayMode mode = determinePlayMode(initPositionNumber, isPlayerHuman1 && !isPlayerEngine1, isPlayerHuman2 && !isPlayerEngine2);

    // エラーが発生した場合
    if (mode == PlayModeError) {
        // エラーメッセージを表示する。
        displayErrorMessage(tr("An error occurred in MainWindow::determinePlayMode. There is a mistake in the game options."));
    }

    // 対局モードを返す。
    return mode;
}

// 対局モード（人間対エンジンなど）に応じて対局処理を開始する。
void MainWindow::startGameBasedOnMode()
{
    if (!m_match) return;

    // 司令塔が準備〜開始〜初手 go まで一括処理
    m_match->prepareAndStartGame(
        m_playMode,
        m_startSfenStr,
        m_sfenRecord,
        m_startGameDialog,
        m_bottomIsP1);

    // --- UI 後処理は MainWindow に残す（評価グラフの刈り込み等）
    applyPendingEvalTrim_();
    m_isResumeFromCurrent = false;
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
        // 解析は司令塔（MatchCoordinator）経由のため、存在を保証する
        if (!m_match) {
            initMatchCoordinator();
        }
        if (!m_match) {
            displayErrorMessage(u8"内部エラー: MatchCoordinator を初期化できません。");
            delete m_analyzeGameRecordDialog;
            m_analyzeGameRecordDialog = nullptr;
            return;
        }

        // 解析結果ビューを用意（モデル/ビューの生成・表示）
        displayAnalysisResults();

        // 本体は司令塔委譲版 analyzeGameRecord() を呼ぶ
        analyzeGameRecord();
    }

    delete m_analyzeGameRecordDialog;
    m_analyzeGameRecordDialog = nullptr;
}

// 対局者の残り時間と秒読み/加算設定だけを確定する（★自分自身は呼ばない）
void MainWindow::setRemainingTimeAndCountDown()
{
    // 1) ダイアログ値の取得（秒読み・加算は秒単位で来る想定）
    const bool loseOnTimeout = m_startGameDialog->isLoseOnTimeout();
    const int  byoyomiSec1     = m_startGameDialog->byoyomiSec1();
    const int  byoyomiSec2     = m_startGameDialog->byoyomiSec2();
    const int  addEachMoveSec1 = m_startGameDialog->addEachMoveSec1();
    const int  addEachMoveSec2 = m_startGameDialog->addEachMoveSec2();

    // 2) 方式判定
    //  - どちらかの byoyomi > 0 → 「秒読み方式」（useByoyomi = true）
    //  - それ以外でどちらかの increment > 0 → 「フィッシャー方式」（useByoyomi = false）
    //  - どちらも 0 → 「持ち時間のみ（便宜上 byoyomi=0 扱い）」→ useByoyomi = true（by=0）
    bool useByoyomi = true;
    if (byoyomiSec1 > 0 || byoyomiSec2 > 0) {
        useByoyomi = true;
    } else if (addEachMoveSec1 > 0 || addEachMoveSec2 > 0) {
        useByoyomi = false; // フィッシャー方式
    } else {
        useByoyomi = true;  // by=0 の秒読み扱い
    }

    // 3) ms に変換
    const int by1Ms  = byoyomiSec1     * 1000;
    const int by2Ms  = byoyomiSec2     * 1000;
    const int inc1Ms = addEachMoveSec1 * 1000;
    const int inc2Ms = addEachMoveSec2 * 1000;

    // 4) 司令塔へ集約設定（MainWindow 側の m_* 変数はもう使わない）
    if (m_match) {
        m_match->setTimeControlConfig(
            useByoyomi,
            by1Ms, by2Ms,
            inc1Ms, inc2Ms,
            loseOnTimeout
            );
    }

    // 5) （従来どおりなら）ShogiClock にも「時間切れ負け」を反映
    if (m_shogiClock) {
        m_shogiClock->setLoseOnTimeout(loseOnTimeout);
    }
}

// 対局ダイアログから各オプションを取得する。
void MainWindow::getOptionFromStartGameDialog()
{
    // 先手の人間の場合の対局者名
    m_humanName1 = m_startGameDialog->humanName1();

    // 後手の人間の場合の対局者名
    m_humanName2 = m_startGameDialog->humanName2();

    // 先手がUSIエンジンの場合のエンジン名
    m_engineName1 = m_startGameDialog->engineName1();

    // 後手がUSIエンジンの場合のエンジン名
    m_engineName2 = m_startGameDialog->engineName2();

    // 対局モードの設定
    m_playMode = setPlayMode();
}

// 「現在の局面から（履歴なし）」で呼ばれたときは平手にフォールバック
void MainWindow::prepareFallbackEvenStartForResume_()
{
    // 0手目から
    m_currentMoveIndex = 0;

    // 平手 startpos → SFEN へ
    m_startPosStr  = QStringLiteral("startpos");
    m_startSfenStr = parseStartPositionToSfen(m_startPosStr); // "… b - 1"

    // sfenRecord を確実に用意して 0手目を積む
    if (!m_sfenRecord) m_sfenRecord = new QStringList;
    m_sfenRecord->clear();
    m_sfenRecord->append(m_startSfenStr);

    // 他の保持データもまっさらに
    m_gameMoves.clear();
    m_positionStrList.clear();

    // 棋譜欄は「開始局面」を必ず表示
    if (m_kifuRecordModel) {
        while (m_kifuRecordModel->rowCount() > 0)
            m_kifuRecordModel->removeLastItem();
        m_kifuRecordModel->appendItem(new KifuDisplay(
            QStringLiteral("=== 開始局面 ==="),
            QStringLiteral("（１手 / 合計）")));
    }

    // 評価値グラフ／ハイライトも 0 手目に整える
    trimEvalChartForResume_(0);
    if (m_boardController) m_boardController->clearAllHighlights();
    updateHighlightsForPly_(0);

    // 再開用スナップショットは開始局面
    m_resumeSfenStr = m_startSfenStr;

    // 直前の終局フラグなどを必ず掃除
    resetGameFlags();
}

// 現在の局面で開始する場合に必要なListデータなどを用意する。
void MainWindow::prepareDataCurrentPosition()
{
    // 現在ユーザが選んだ手数（0=開始局面, 1..N）
    const int selPly = qMax(0, m_currentMoveIndex);

    m_isResumeFromCurrent = true;       // ← 再開モードを宣言
    m_pendingEvalTrimPly  = selPly;     // ← 何手目まで残すかを記録だけしておく

    // ★ 初回（履歴なし）で「現在の局面から」を選ばれたら → 平手で開始にフォールバック
    if (!m_sfenRecord || m_sfenRecord->isEmpty()) {
        qDebug() << "[resume] no history -> fallback to startpos (even game)";
        prepareFallbackEvenStartForResume_();
        return;
    }

    // ---- 以降、巻き戻し処理 ----
    const bool prevGuard = m_onMainRowGuard;
    m_onMainRowGuard = true;

    if (m_kifuRecordModel) {
        while (m_kifuRecordModel->rowCount() > (selPly + 1)) {
            m_kifuRecordModel->removeLastItem();
        }
    }

    if (m_gameMoves.size() > selPly) {
        while (m_gameMoves.size() > selPly) {
            m_gameMoves.removeLast();
        }
    } else {
        m_gameMoves.clear();
    }

    if (m_positionStrList.size() > (selPly + 1)) {
        while (m_positionStrList.size() > (selPly + 1)) {
            m_positionStrList.removeLast();
        }
    } else {
        m_positionStrList.clear();
    }

    if (m_sfenRecord) {
        if (m_sfenRecord->size() > (selPly + 1)) {
            while (m_sfenRecord->size() > (selPly + 1)) {
                m_sfenRecord->removeLast();
            }
        } else {
            // 既に selPly+1 以下なら何もしない（クリアしない）
        }
    }

    // ハイライトのクリア → 復元（0→1始まりに +1 補正版を使用）
    if (m_boardController) m_boardController->clearAllHighlights();
    updateHighlightsForPly_(selPly);

    m_onMainRowGuard = prevGuard;

    // 再開用のスナップショット（選択手の局面）
    m_resumeSfenStr.clear();
    if (m_sfenRecord && m_sfenRecord->size() > selPly) {
        m_resumeSfenStr = m_sfenRecord->at(selPly);
    }

    m_startSfenStr = m_sfenRecord->at(0);

    // ★ 直前の終局状態を必ずクリア（これが残っていると updateGameRecord が return します）
    resetGameFlags();
}

// 初期局面からの対局する場合の準備を行う。
void MainWindow::prepareInitialPosition()
{
    // 手数を0に戻す。
    m_currentMoveIndex = 0;

    // 開始局面文字列をSFEN形式でセットする。（sfen文字あり）
    static const QString startingPositionStr[14] = {
        //  0. 平手
        "startpos",
        //  1. 香落ち
        "sfen lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  2. 右香落ち"
        "sfen 1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  3. 角落ち
        "sfen lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  4. 飛車落ち
        "sfen lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  5. 飛香落ち
        "sfen lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  6. 二枚落ち
        "sfen lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  7. 三枚落ち
        "sfen lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  8. 四枚落ち
        "sfen 1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        //  9. 五枚落ち
        "sfen 2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        // 10. 左五枚落ち
        "sfen 1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        // 11. 六枚落ち
        "sfen 2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        // 12. 八枚落ち
        "sfen 3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
        // 13. 十枚落ち
        "sfen 4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"
    };

    // 「平手」「香落ち」等に開始局面文字列をセットする。
    m_startPosStr = startingPositionStr[m_startGameDialog->startingPositionNumber() - 1];

    // sfen文字列の取り出し
    // "sfen 〜"で始まる文字列startpositionstrを入力して"sfen "を削除したSFEN文字列を
    // 返す。startposの場合は、
    // "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"を
    // 返す。
    m_startSfenStr = parseStartPositionToSfen(m_startPosStr);

    // 棋譜欄の項目にヘッダを付け加える。
    m_kifuRecordModel->appendItem(new KifuDisplay("=== 開始局面 ===", "（１手 / 合計）"));

    // "sfen "を削除したSFEN文字列を格納する。
    m_sfenRecord->append(m_startSfenStr);
}

// 残り時間をセットしてタイマーを開始する。
void MainWindow::setTimerAndStart()
{
    // 時計が先に必要（MatchCoordinator のフックから触られても安全に）
    ensureClockReady_();

    // （以下は元コードのまま＝時刻の読み取り）
    const int basicTimeHour1    = m_startGameDialog->basicTimeHour1();
    const int basicTimeMinutes1 = m_startGameDialog->basicTimeMinutes1();
    const int basicTimeHour2    = m_startGameDialog->basicTimeHour2();
    const int basicTimeMinutes2 = m_startGameDialog->basicTimeMinutes2();
    const int byoyomi1          = m_startGameDialog->byoyomiSec1();
    const int byoyomi2          = m_startGameDialog->byoyomiSec2();
    const int binc              = m_startGameDialog->addEachMoveSec1();
    const int winc              = m_startGameDialog->addEachMoveSec2();

    const int remainingTime1 = basicTimeHour1 * 3600 + basicTimeMinutes1 * 60;
    const int remainingTime2 = basicTimeHour2 * 3600 + basicTimeMinutes2 * 60;

    const bool isLoseOnTimeout = m_startGameDialog->isLoseOnTimeout();
    const bool hasTimeLimit =
        (basicTimeHour1*3600 + basicTimeMinutes1*60) > 0 ||
        (basicTimeHour2*3600 + basicTimeMinutes2*60) > 0 ||
        byoyomi1 > 0 || byoyomi2 > 0 || binc > 0 || winc > 0;

    m_shogiClock->setLoseOnTimeout(isLoseOnTimeout);
    m_shogiClock->setPlayerTimes(remainingTime1, remainingTime2,
                                 byoyomi1, byoyomi2,
                                 binc, winc,
                                 hasTimeLimit);

    // 初期msを保持（棋譜表示等の計算に使用）
    m_initialTimeP1Ms = m_shogiClock->getPlayer1TimeIntMs();
    m_initialTimeP2Ms = m_shogiClock->getPlayer2TimeIntMs();

    // 再生中は時計を動かさない（表示だけ合わせて終了）
    updateTurnDisplay();
    m_shogiClock->updateClock();
    if (!m_isReplayMode) {
        m_shogiClock->startClock();
    }

    if (m_match && m_shogiClock) {
        m_match->setClock(m_shogiClock);     // ★後差し替えでも確実に配線
        m_match->pokeTimeUpdateNow();        // ★初期表示を即反映
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

// 対局を開始する。
void MainWindow::initializeGame()
{
    m_errorOccurred = false;
    if (m_shogiView) m_shogiView->setErrorOccurred(false);

    m_startGameDialog = new StartGameDialog;

    if (m_startGameDialog->exec() == QDialog::Accepted) {
        setReplayMode(false);

        // ★ 先に取得しておく（これが肝）
        const int startingPositionNumber = m_startGameDialog->startingPositionNumber();

        m_gameCount++;

        // ★ 2回目以降の対局でも、「現局面から」はフルリセットしない
        if (m_gameCount > 1) {
            if (startingPositionNumber > 0) {
                resetToInitialState();            // 既存のフルリセット
            }
        }

        // 対局オプションなど
        setRemainingTimeAndCountDown();
        getOptionFromStartGameDialog();

        ensureClockReady_();

        if (startingPositionNumber == 0) {
            // 現在局面から開始：ここで「選択手まで残して末尾だけ切る」
            prepareDataCurrentPosition();
        } else {
            prepareInitialPosition();
        }

        // 棋譜UI調整
        if (m_playMode) {
            disableArrowButtons();
            if (m_recordPane && m_recordPane->kifuView())
                m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::NoSelection);
        }

        if (m_match) {
            m_match->startNewGame(m_startSfenStr);
        }
        setCurrentTurn();
        setTimerAndStart();

        startGameBasedOnMode();
    }
    delete m_startGameDialog;
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
    // ファイル選択ダイアログ
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

    m_kifuLoadCoordinator = new KifuLoadCoordinator(
        m_gameMoves,
        m_resolvedRows,
        m_positionStrList,
        m_activeResolvedRow,
        m_activePly,
        m_currentSelectedPly,
        m_currentMoveIndex,
        m_sfenRecord,
        m_gameInfoTable,
        m_gameInfoDock,
        m_analysisTab,
        m_tab,
        m_shogiView,
        m_recordPane,
        m_kifuRecordModel,
        m_kifuBranchModel,
        m_branchCtl,
        m_kifuBranchView,
        m_branchDisplayPlan,
        this
    );

    // 分岐ツリーのクリックを MainWindow ではなく Coordinator に直結
    if (m_analysisTab) {
        QObject::disconnect(m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
                            this, &MainWindow::onBranchNodeActivated_);
        connect(m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
                m_kifuLoadCoordinator, &KifuLoadCoordinator::applyResolvedRowAndSelect,
                Qt::UniqueConnection);
    }

    // Coordinator -> MainWindow の通知は従来どおり
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::displayGameRecord,
            this, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
            this, &MainWindow::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::enableArrowButtons,
            this, &MainWindow::enableArrowButtons, Qt::UniqueConnection);
    connect(m_kifuLoadCoordinator, &KifuLoadCoordinator::setupBranchCandidatesWiring_,
            this, &MainWindow::setupBranchCandidatesWiring_, Qt::UniqueConnection);

    // 読み込み実行
    m_kifuLoadCoordinator->loadKifuFromFile(filePath);
}

void MainWindow::rebuildSfenRecord(const QString& initialSfen, const QStringList& usiMoves, bool hasTerminal)
{
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(initialSfen)) {
        tracer.setFromSfen(QStringLiteral(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
    }

    QStringList list;
    list << tracer.toSfenString();          // 0) 初期
    for (const QString& mv : usiMoves) {
        tracer.applyUsiMove(mv);
        list << tracer.toSfenString();      // 1..N) 各手後
    }
    if (hasTerminal) {
        list << tracer.toSfenString();      // N+1) 終局/中断表示用
    }

    if (!m_sfenRecord) m_sfenRecord = new QStringList;
    *m_sfenRecord = list;                    // COW
}

void MainWindow::rebuildGameMoves(const QString& initialSfen, const QStringList& usiMoves)
{
    m_gameMoves.clear();

    SfenPositionTracer tracer;
    tracer.setFromSfen(initialSfen);

    for (const QString& usi : usiMoves) {
        // ドロップ: "P*5e"
        if (usi.size() >= 4 && usi.at(1) == QLatin1Char('*')) {
            const bool black = tracer.blackToMove();
            const QChar dropUpper = usi.at(0).toUpper();      // P/L/N/S/G/B/R
            const int  file = usi.at(2).toLatin1() - '0';     // '1'..'9'
            const int  rank = rankLetterToNum(usi.at(3));     // 1..9
            if (file < 1 || file > 9 || rank < 1 || rank > 9) { tracer.applyUsiMove(usi); continue; }

            const QPoint from = dropFromSquare(dropUpper, black);     // ★ 駒台
            const QPoint to(file - 1, rank - 1);
            const QChar  moving   = dropLetterWithSide(dropUpper, black);
            const QChar  captured = QLatin1Char(' ');
            const bool   promo    = false;

            m_gameMoves.push_back(ShogiMove(from, to, moving, captured, promo));
            tracer.applyUsiMove(usi);
            continue;
        }

        // 通常手: "7g7f" or "2b3c+"
        if (usi.size() < 4) { tracer.applyUsiMove(usi); continue; }

        const int ff = usi.at(0).toLatin1() - '0';
        const int rf = rankLetterToNum(usi.at(1));
        const int ft = usi.at(2).toLatin1() - '0';
        const int rt = rankLetterToNum(usi.at(3));
        const bool isProm = (usi.size() >= 5 && usi.at(4) == QLatin1Char('+'));
        if (ff<1||ff>9||rf<1||rf>9||ft<1||ft>9||rt<1||rt>9) { tracer.applyUsiMove(usi); continue; }

        const QString fromTok = tracer.tokenAtFileRank(ff, usi.at(1));
        const QString toTok   = tracer.tokenAtFileRank(ft, usi.at(3));

        const QPoint from(ff - 1, rf - 1);
        const QPoint to  (ft - 1, rt - 1);

        const QChar moving   = tokenToOneChar(fromTok);
        const QChar captured = tokenToOneChar(toTok);

        m_gameMoves.push_back(ShogiMove(from, to, moving, captured, isProm));
        tracer.applyUsiMove(usi);
    }
}

void MainWindow::displayGameRecord(const QList<KifDisplayItem> disp)
{
    // 安全チェック
    if (!m_kifuRecordModel) return;
    if (!m_moveRecords) m_moveRecords = new QList<KifuDisplay*>();

    // クリア
    m_moveRecords->clear();
    m_currentMoveIndex = 0;
    m_kifuDataList.clear();
    m_kifuRecordModel->clearAllItems();

    // ヘッダ（開始局面）
    m_kifuRecordModel->appendItem(new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                                  QStringLiteral("（1手 / 合計）")));

    // ★ コメント表を（SFENの行数に合わせて）準備
    m_commentsByRow.clear();
    const int moveCount = disp.size();                             // 手数
    const int rowCount  = m_sfenRecord ? m_sfenRecord->size()
                                       : (moveCount + 1);          // 行=開始局面(0) + 各手(1..N)
    m_commentsByRow.resize(rowCount);                              // 既定は空

    // 先に「開始局面のコメント」を設定（= 初手の直前コメント）
    if (moveCount > 0)
        m_commentsByRow[0] = disp[0].comment;

    // 棋譜欄へ各手を追加しつつ、「その手のコメント」は disp[i+1] を採用
    for (int i = 0; i < moveCount; ++i) {
        const auto &it = disp[i];

        // 行→コメントの対応（ズレ補正）
        const int row = i + 1;                   // 1..N
        if (row < m_commentsByRow.size()) {
            if (i + 1 < moveCount) {
                m_commentsByRow[row] = disp[i + 1].comment;  // その手の“後”のコメント
            } else {
                m_commentsByRow[row].clear();                // 最終手の後は通常コメントなし
            }
        }

        // 棋譜欄へ行を追加（既存処理）
        m_lastMove = it.prettyMove;
        updateGameRecord(it.timeText);
    }

    // --- 表示／選択 & コメント同期は RecordPane + EngineAnalysisTab 経由 ---
    if (m_recordPane && m_recordPane->kifuView()) {
        QTableView* view = m_recordPane->kifuView();

        // 初期選択：開始局面
        view->setCurrentIndex(m_kifuRecordModel->index(0, 0));

        // コメント初期表示
        if (m_analysisTab || m_recordPane) {
            const QString head = (0 < m_commentsByRow.size() ? m_commentsByRow[0].trimmed()
                                                             : QString());
            const QString toShow = head.isEmpty() ? tr("コメントなし") : head;
            broadcastComment(toShow, /*asHtml=*/true);
        }

        // 行選択が変わったらコメントも更新
        if (view->selectionModel()) {
            // 既存接続があれば一度切る（多重接続防止）
            if (m_connKifuRowChanged)
                disconnect(m_connKifuRowChanged);

            m_connKifuRowChanged = connect(view->selectionModel(),
                                           &QItemSelectionModel::currentRowChanged,
                                           this,
                                           &MainWindow::onKifuCurrentRowChanged,
                                           Qt::UniqueConnection); // ← メンバ関数なのでOK
        }
    }
}

// 棋譜を1行だけUIへ反映する（ビジネスロジックなし / 表示専用）
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    if (m_isUndoInProgress) return;  // UNDO中は棋譜追記を抑止

    qCDebug(ClockLog) << "in MainWindow::updateGameRecord";
    qDebug() << "[UI] updateGameRecord append <<" << elapsedTime;
    qCDebug(ClockLog) << "elapsedTime=" << elapsedTime;

    // 1) 終局後、すでに終局行を追記済みなら何もしない
    const bool gameOverAppended =
        (m_match && m_match->gameOverState().isOver && m_match->gameOverState().moveAppended);
    if (gameOverAppended) {
        qCDebug(ClockLog) << "[KIFU] suppress updateGameRecord after game over";
        return;
    }

    // 2) 手文字列が空なら行を作らない（空行防止）
    const QString last = m_lastMove.trimmed();
    if (last.isEmpty()) {
        qCDebug(ClockLog) << "[KIFU] skip empty move line";
        return;
    }

    // 3) 手数をインクリメントして整形（表示上は左寄せ幅4桁）
    const QString moveNumberStr = QString::number(++m_currentMoveIndex);
    const QString spaceStr = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));

    // 4) 表示用の行（例: "   1 ▲７六歩"）
    const QString recordLine = spaceStr + moveNumberStr + QLatin1Char(' ') + last;

    // 5) KIF 出力用の行（先後記号は外し、消費時間を付与）
    QString kifuLine = recordLine + QStringLiteral(" ( ") + elapsedTime + QLatin1String(" )");
    kifuLine.remove(QStringLiteral("▲"));
    kifuLine.remove(QStringLiteral("△"));

    // 6) KIFテキスト蓄積
    m_kifuDataList.append(kifuLine);

    // 7) 表示モデル更新（モデルがなければ作る）
    if (!m_moveRecords) m_moveRecords = new QList<KifuDisplay*>();
    m_moveRecords->append(new KifuDisplay(recordLine, elapsedTime));

    if (m_kifuRecordModel && !m_moveRecords->isEmpty()) {
        m_kifuRecordModel->appendItem(m_moveRecords->last());
    }
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

// 詰み探索を開始する（TsumeShogiSearchDialogのOK後に呼ばれる）
void MainWindow::startTsumiSearch()
{
    // 1) 対局モードを詰み探索モードへ（UI の状態保持用）
    m_playMode = TsumiSearchMode;

    // 2) 選択手数を正規化して SFEN スナップショットを取得（moves は使わない）
    QString baseSfen;
    const int sel = qMax(0, m_currentMoveIndex);

    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        const int safe = qBound(0, sel, m_sfenRecord->size() - 1);
        baseSfen = m_sfenRecord->at(safe); // 例: "lnsgkgsnl/... b - 1"
    } else if (!m_startSfenStr.isEmpty()) {
        baseSfen = m_startSfenStr;
    } else if (!m_positionStrList.isEmpty()) {
        const int safe = qBound(0, sel, m_positionStrList.size() - 1);
        const QString pos = m_positionStrList.at(safe).trimmed();
        if (pos.startsWith(QStringLiteral("position sfen"))) {
            QString t = pos.mid(14).trimmed(); // "position sfen" の後ろ
            const int m = t.indexOf(QStringLiteral(" moves "));
            baseSfen = (m >= 0) ? t.left(m).trimmed() : t; // moves 以降は捨てる
        }
    }

    if (baseSfen.isEmpty()) {
        displayErrorMessage(u8"詰み探索用の局面（SFEN）が取得できません。棋譜を読み込むか局面を指定してください。");
        return;
    }

    // 3) 玉配置から手番を推定:
    //    - k（後手玉）のみ → 攻方=先手(b)
    //    - K（先手玉）のみ → 攻方=後手(w)
    //    - 両方 or どちらも無し → SFENトークン(b/w) → それも無ければ b
    auto decideTurnFromSfen = [](const QString& sfen)->QChar {
        const QStringList toks = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (toks.isEmpty()) return QLatin1Char('b');

        const QString& board = toks.at(0);
        int cntK = 0, cntk = 0;
        for (const QChar ch : board) {
            if (ch == QLatin1Char('K')) ++cntK;
            else if (ch == QLatin1Char('k')) ++cntk;
        }
        // どちらか片方だけのときはそれを優先
        if ((cntK > 0) ^ (cntk > 0)) {
            // kのみ → 後手玉を詰める → 攻方=先手(b)
            // Kのみ → 先手玉を詰める → 攻方=後手(w)
            return (cntk > 0) ? QLatin1Char('b') : QLatin1Char('w');
        }
        // 両方 or どちらも無し → SFEN手番トークンを参照
        if (toks.size() >= 2) {
            if (toks[1] == QLatin1String("b")) return QLatin1Char('b');
            if (toks[1] == QLatin1String("w")) return QLatin1Char('w');
        }
        return QLatin1Char('b'); // 最終フォールバック：先手
    };

    const QChar desiredTurn = decideTurnFromSfen(baseSfen);

    // 4) SFENの手番を強制上書き（手数トークンは1にリセットしておくと無難）
    auto forceTurnInSfen = [](const QString& sfen, QChar turn)->QString {
        QStringList toks = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (toks.size() >= 2) {
            toks[1] = QString(turn);
            if (toks.size() >= 4) toks[3] = QStringLiteral("1");
            return toks.join(QLatin1Char(' '));
        }
        return sfen;
    };
    const QString forcedSfen = forceTurnInSfen(baseSfen, desiredTurn);

    // 5) 送信用 position は固定局面のみ（moves を付けない）
    m_positionStr1 = QStringLiteral("position sfen %1").arg(forcedSfen);

    // 6) UI 手番表示を同期
    m_gameController->setCurrentPlayer(
        (desiredTurn == QLatin1Char('b'))
        ? ShogiGameController::Player1
        : ShogiGameController::Player2
    );

    // 7) ダイアログからエンジン/時間設定を取得
    const int  engineNumber = m_tsumeShogiSearchDialog->getEngineNumber();
    const auto engine       = m_tsumeShogiSearchDialog->getEngineList().at(engineNumber);

    int byoyomiMs = 0;
    if (!m_tsumeShogiSearchDialog->unlimitedTimeFlag()) {
        byoyomiMs = m_tsumeShogiSearchDialog->getByoyomiSec() * 1000; // 秒→ms
    }

    // 8) 司令塔に委譲
    if (m_match) {
        MatchCoordinator::AnalysisOptions opt;
        opt.enginePath  = engine.path;
        opt.engineName  = m_tsumeShogiSearchDialog->getEngineName();
        opt.positionStr = m_positionStr1;  // "position sfen <...>"
        opt.byoyomiMs   = byoyomiMs;
        opt.mode        = TsumiSearchMode; // → go mate を使う

        m_match->startAnalysis(opt);
    }
}

// 棋譜解析を開始する（エンジン制御は MatchCoordinator に委譲）
void MainWindow::analyzeGameRecord()
{
    // 1) UI 上のプレイモードは「棋譜解析モード」を保持
    m_playMode = AnalysisMode;

    if (!m_match || !m_analyzeGameRecordDialog) return;

    // 2) 解析設定（ダイアログ値）
    const int  byoyomiMs  = m_analyzeGameRecordDialog->byoyomiSec() * 1000;
    const int  engineIdx  = m_analyzeGameRecordDialog->engineNumber();
    const auto engine     = m_analyzeGameRecordDialog->engineList().at(engineIdx);
    const QString enginePath = engine.path;
    const QString engineName = m_analyzeGameRecordDialog->engineName();

    // 3) 解析開始手（初手から / 現在手から）
    const int startIndexRaw = m_analyzeGameRecordDialog->initPosition() ? 0 : m_currentMoveIndex;
    const int startIndex    = qBound(0, startIndexRaw, m_positionStrList.size() - 1);

    // 4) ループ上限（※テスト用で3手に絞っていた制限を解除）
    const int endIndex   = 3;

    // 5) 直前評価値（差分計算用）
    int previousScore = 0;

    for (int moveIndex = startIndex; moveIndex < endIndex; ++moveIndex) {
        // --- UI：現在手番の表示を GameController に反映（従来挙動を踏襲） ---
        if (m_gameMoves.at(moveIndex).movingPiece.isUpper()) {
            m_gameController->setCurrentPlayer(ShogiGameController::Player1);
        } else {
            m_gameController->setCurrentPlayer(ShogiGameController::Player2);
        }

        // --- 送信用 position を取得 ---
        const QString positionStr = m_positionStrList.at(moveIndex);

        // --- エンジン制御は司令塔へ。cleanup 互換のためモードは ConsidarationMode を指定 ---
        MatchCoordinator::AnalysisOptions opt;
        opt.enginePath  = enginePath;
        opt.engineName  = engineName;
        opt.positionStr = positionStr;
        opt.byoyomiMs   = byoyomiMs;
        opt.mode        = ConsidarationMode; // ★ engine 側の停止/片付けルートを使う

        // ★ ここが重要 ★
        // 初回だけ startAnalysis() でエンジンを起動し、
        // 2手目以降は continueAnalysis() で同一プロセスに次の position を送る。
        if (moveIndex == startIndex) {
            m_match->startAnalysis(opt); // 初回のみ起動（内部で destroy→起動）
        } else {
            m_match->continueAnalysis(positionStr, byoyomiMs); // 2手目以降は使い回し（quit しない）
        }

        // --- 解析結果の取得（司令塔から主エンジンを覗く） ---
        auto* eng = m_match->primaryEngine();
        const QString currentScore   = (eng ? eng->scoreStr()    : QStringLiteral("0"));
        const QString engineReadout  = (eng ? eng->pvKanjiStr()  : QString());

        // --- モデルへ追加：指し手/評価値/差分/読み筋 ---
        const QString currentMoveRecord =
            (m_moveRecords && moveIndex < m_moveRecords->size())
                ? m_moveRecords->at(moveIndex)->currentMove()
                : QString();

        const QString scoreDifference =
            QString::number(currentScore.toInt() - previousScore);
        previousScore = currentScore.toInt();

        if (m_analysisModel) {
            m_analysisModel->appendItem(
                new KifuAnalysisResultsDisplay(currentMoveRecord,
                                               currentScore,
                                               scoreDifference,
                                               engineReadout));
        }
        if (m_analysisResultsView) {
            m_analysisResultsView->scrollToBottom();
            m_analysisResultsView->update();
        }

        // --- 現局面から 1 手進める（既存の選択適用フローを踏襲） ---
        if (!m_resolvedRows.isEmpty()) {
            const int row    = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
            const int maxPly = m_resolvedRows[row].disp.size();
            const int cur    = qBound(0, m_activePly, maxPly);
            if (cur < maxPly) {
                applySelect(row, cur + 1);   // 旧 navigateToNextMove 相当
            }
        }

        // --- 評価値グラフ更新（先手側を使用） ---
        redrawEngine1EvaluationGraph();
    }

    // 6) 解析セッションの後始末（ここでだけ quit→破棄）
    //    ※ 途中では絶対に quit を送らない
    m_match->handleBreakOffConsidaration();
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

// 局面編集中のメニュー表示に変更する。
void MainWindow::displayPositionEditMenu()
{
    // 「局面編集開始」を隠す。
    ui->actionStartEditPosition->setVisible(false);

    // 「局面編集終了」を表示する。
    ui->actionEndEditPosition->setVisible(true);

    // 「平手初期配置」を表示する。
    ui->flatHandInitialPosition->setVisible(true);

    // 「全ての駒を駒台へ」を表示する。
    ui->returnAllPiecesOnStand->setVisible(true);

    // 「先後反転」を表示する。
    ui->reversal->setVisible(true);

    // 「詰将棋初期配置」を表示する。
    ui->shogiProblemInitialPosition->setVisible(true);

    // 「手番変更」を表示する。
    ui->turnaround->setVisible(true);
}

// 局面編集メニュー後のメニュー表示に変更する。
void MainWindow::hidePositionEditMenu()
{
    // 「局面編集開始」を表示する。
    ui->actionStartEditPosition->setVisible(true);

    // 「局面編集終了」を隠す。
    ui->actionEndEditPosition->setVisible(false);

    // 「平手初期配置」を隠す。
    ui->flatHandInitialPosition->setVisible(false);

    // 「詰将棋初期配置」を隠す。
    ui->shogiProblemInitialPosition->setVisible(false);

    // 「全ての駒を駒台へ」を隠す。
    ui->returnAllPiecesOnStand->setVisible(false);

    // 「先後反転」を隠す。
    ui->reversal->setVisible(false);

    // 「手番変更」を隠す。
    ui->turnaround->setVisible(false);
}

void MainWindow::beginPositionEditing()
{
    displayPositionEditMenu();

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

    ctx.startSfenStr   = &m_startSfenStr;
    ctx.currentSfenStr = &m_currentSfenStr;
    ctx.resumeSfenStr  = &m_resumeSfenStr;

    // 盤右の「編集終了」ボタンは MainWindow のヘルパで表示
    ctx.onShowEditExitButton = [this]() { this->showEditExitButtonOnBoard_(); };

    // 実行
    m_posEdit->beginPositionEditing(ctx);

    // ── 編集用アクション接続（重複防止） ───────────────────────────────
    if (ui) {
        connect(ui->returnAllPiecesOnStand,      &QAction::triggered, this, &MainWindow::resetPiecesToStand,         Qt::UniqueConnection);
        connect(ui->turnaround,                  &QAction::triggered, this, &MainWindow::toggleEditSideToMove,       Qt::UniqueConnection);
        connect(ui->flatHandInitialPosition,     &QAction::triggered, this, &MainWindow::setStandardStartPosition,   Qt::UniqueConnection);
        connect(ui->shogiProblemInitialPosition, &QAction::triggered, this, &MainWindow::setTsumeShogiStartPosition, Qt::UniqueConnection);
        connect(ui->reversal,                    &QAction::triggered, this, &MainWindow::onReverseTriggered,         Qt::UniqueConnection);
    }
}

void MainWindow::onReverseTriggered()
{
    if (m_match) m_match->flipBoard();
}

void MainWindow::finishPositionEditing()
{
    // --- A) 自動同期を停止（ここが肝） ---
    const bool prevGuard = m_onMainRowGuard;
    m_onMainRowGuard = true;

    hidePositionEditMenu();

    ensurePositionEditController_();
    if (!m_posEdit || !m_shogiView || !m_gameController) {
        m_onMainRowGuard = prevGuard;
        return;
    }

    // Controller に委譲して SFEN の確定・sfenRecord/開始SFEN の更新・UI 後片付けを実施
    PositionEditController::FinishEditContext ctx;
    ctx.view               = m_shogiView;
    ctx.gc                 = m_gameController;
    ctx.bic                = m_boardController;
    ctx.sfenRecord         = m_sfenRecord ? m_sfenRecord : nullptr;
    ctx.startSfenStr       = &m_startSfenStr;
    ctx.isResumeFromCurrent= &m_isResumeFromCurrent;

    // 盤右の「編集終了」ボタンを隠す（MainWindow ヘルパ）
    ctx.onHideEditExitButton = [this]() { this->hideEditExitButtonOnBoard_(); };

    m_posEdit->finishPositionEditing(ctx);

    // --- B) モデル/カウンタ系の初期化（既存の動きに合わせる） ---
    m_currentMoveIndex    = 0;
    m_totalMove           = 0;

    // USI "position ..." ひな形を再生成（空 moves 事故防止）
    initializePositionStringsForMatch_();

    // --- C) UI後片付け ---
    if (m_boardController) {
        m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
        m_boardController->clearAllHighlights();
    }
    if (ui) {
        disconnect(ui->returnAllPiecesOnStand,      &QAction::triggered, this, &MainWindow::resetPiecesToStand);
        disconnect(ui->turnaround,                  &QAction::triggered, this, &MainWindow::toggleEditSideToMove);
        disconnect(ui->flatHandInitialPosition,     &QAction::triggered, this, &MainWindow::setStandardStartPosition);
        disconnect(ui->shogiProblemInitialPosition, &QAction::triggered, this, &MainWindow::setTsumeShogiStartPosition);
        disconnect(ui->reversal,                    &QAction::triggered, this, &MainWindow::onReverseTriggered);
    }

    // 編集フラグは Controller 側でも落としていますが、念のため明示
    if (m_shogiView) {
        m_shogiView->setPositionEditMode(false);
        m_shogiView->setMouseClickMode(false);
        m_shogiView->update();
    }

    // --- D) 自動同期を再開 ---
    m_onMainRowGuard = prevGuard;

    qDebug() << "[EDIT-END] flags: editMode="
             << (m_shogiView ? m_shogiView->positionEditMode() : false)
             << " guard=" << m_onMainRowGuard
             << " m_startSfenStr=" << m_startSfenStr;
}

// 「全ての駒を駒台へ」をクリックした時に実行される。
// 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
void MainWindow::resetPiecesToStand()
{
    if (m_boardController)
        m_boardController->clearAllHighlights();

    m_shogiView->resetAndEqualizePiecesOnStands();
}

// 平手初期局面に盤面を初期化する。
void MainWindow::setStandardStartPosition()
{
    if (m_boardController)
        m_boardController->clearAllHighlights();

    m_shogiView->initializeToFlatStartingPosition();
}

// 詰将棋の初期局面に盤面を初期化する。
void MainWindow::setTsumeShogiStartPosition()
{
    if (m_boardController)
        m_boardController->clearAllHighlights();

    m_shogiView->shogiProblemInitialPosition();
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
// エンジンにstopコマンドを送る。
// エンジンに対し思考停止を命令するコマンド。エンジンはstopを受信したら、できるだけすぐ思考を中断し、
// bestmoveで指し手を返す。
void MainWindow::movePieceImmediately()
{
    // エンジン同士の対局の場合
    if ((m_playMode == EvenEngineVsEngine) || (m_playMode == HandicapEngineVsEngine)) {
        // 現在の手番が先手あるいは下手の場合
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            // エンジン1に対してstopコマンドを送る。
            m_usi1->sendStopCommand();
        }
        // 現在の手番が後手あるいは上手の場合
        else {
            // エンジン2に対してstopコマンドを送る。
            m_usi2->sendStopCommand();
        }
    }
    // それ以外の対局の場合
    else {
        // エンジン1に対してstopコマンドを送る。
        m_usi1->sendStopCommand();
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

// 終局理由つきの終局表記をセット（棋譜欄の最後に出す "▲投了" / "△時間切れ" 等）
void MainWindow::setGameOverMove(GameOverCause cause, bool loserIsPlayerOne)
{
    const QString who = loserIsPlayerOne ? QStringLiteral("▲") : QStringLiteral("△");
    const QString what = (cause == GameOverCause::Timeout)
                             ? QStringLiteral("時間切れ")
                             : QStringLiteral("投了");
    qDebug().nospace() << "[UI] setGameOverMove append '" << who << what << "'";

    // 司令塔が存在しない/終局でないなら何もしない
    if (!m_match || !m_match->gameOverState().isOver) return;

    // 司令塔の「一意追記フラグ」で重複防止
    if (m_match->gameOverState().moveAppended) {
        qCDebug(ClockLog) << "[KIFU] setGameOverMove suppressed (already appended)";
        return;
    }

    // ★ まず時計停止（この時点の残りmsを固定）
    if (m_shogiClock) m_shogiClock->stopClock();

    //begin
    qDebug() << "[KIFU] setGameOverMove cause="
             << (cause == GameOverCause::Resignation ? "RESIGN" : "TIMEOUT")
             << " loser=" << (loserIsPlayerOne ? "P1" : "P2");
    //end

    // 表記（▲/△は絶対座席）
    const QChar mark = glyphForPlayer(loserIsPlayerOne);
    const QString line = (cause == GameOverCause::Resignation)
                             ? QString("%1投了").arg(mark)
                             : QString("%1時間切れ").arg(mark);

    // ★ 「この手」の思考秒 / 合計消費秒を算出
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // この手の開始エポック（司令塔から取得）
    qint64 epochMs = m_match->turnEpochFor(loserIsPlayerOne ? MatchCoordinator::P1
                                                            : MatchCoordinator::P2);
    qint64 considerMs = (epochMs > 0) ? (now - epochMs) : 0;
    if (considerMs < 0) considerMs = 0; // 念のため

    // 合計消費 = 初期持ち時間 − 現在の残り時間
    qint64 remMs  = loserIsPlayerOne
                       ? (m_shogiClock ? m_shogiClock->getPlayer1TimeIntMs() : 0)
                       : (m_shogiClock ? m_shogiClock->getPlayer2TimeIntMs() : 0);
    const qint64 initMs = loserIsPlayerOne ? m_initialTimeP1Ms : m_initialTimeP2Ms;
    qint64 totalUsedMs  = initMs - remMs;
    if (totalUsedMs < 0) totalUsedMs = 0;

    const QString elapsed = QStringLiteral("%1/%2")
                                .arg(fmt_mmss(considerMs),
                                     fmt_hhmmss(totalUsedMs));

    // 1回だけ即時追記
    appendKifuLine(line, elapsed);

    // 人間用ストップウォッチ解除（HvE/HvH）
    m_match->disarmHumanTimerIfNeeded();

    // ★ 司令塔へ「追記完了」を通知 → 以後の重複追記を司令塔側でブロック
    m_match->markGameOverMoveAppended();

    qDebug().nospace() << "[KIFU] setGameOverMove appended"
                       << " cause=" << (cause == GameOverCause::Resignation ? "RESIGN" : "TIMEOUT")
                       << " loser=" << (loserIsPlayerOne ? "P1" : "P2")
                       << " elapsed=" << elapsed;

    // --- ここから追記（関数の最後でOK） ---
    enableArrowButtons();

    // 末尾の手（=直近局面）を現在選択にしておくと、その後の「局面編集」も確実に末尾から始まる
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        m_currentSelectedPly = m_sfenRecord->size() - 1;
        m_activePly         = m_currentSelectedPly;
    }

    if (m_recordPane) {
        if (auto* view = m_recordPane->kifuView()) {
            view->setSelectionMode(QAbstractItemView::SingleSelection);
        }
    }
    // 可能なら“リプレイモード”に入れて時計・強調等を安定化
    setReplayMode(true);

    // ★ 追加：現局面再開モードの解除（選択復活）
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

    // 行に対応する disp/sfen/gm を差し替えたうえで
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
    m_kifuLoadCoordinator->showBranchCandidates(row, safePly);
}

void MainWindow::syncBoardAndHighlightsAtRow(int ply1)
{
    if (m_shogiView && m_shogiView->positionEditMode()) {
        qDebug() << "[UI] syncBoardAndHighlightsAtRow skipped (edit-mode). ply=" << ply1;
        return;
    }
    if (!m_sfenRecord) return;

    const int maxPly = m_sfenRecord->size() - 1;
    if (maxPly < 0) return;

    const int safePly = qBound(0, ply1, maxPly);

    // UI 側の現在手を同期（旧仕様踏襲）
    m_currentSelectedPly = safePly;
    m_currentMoveIndex   = safePly;

    ensureBoardSyncPresenter_();
    if (m_boardController) m_boardController->clearAllHighlights();
    m_boardSync->syncBoardAndHighlightsAtRow(safePly);

    m_activePly = m_currentSelectedPly;
}

void MainWindow::applySfenAtCurrentPly()
{
    if (m_shogiView && m_shogiView->positionEditMode()) {
        qDebug() << "[UI] applySfenAtCurrentPly skipped (edit-mode). m_currentSelectedPly=" << m_currentSelectedPly;
        return;
    }
    if (!m_sfenRecord || m_sfenRecord->isEmpty()) return;

    ensureBoardSyncPresenter_();
    m_boardSync->applySfenAtPly(qBound(0, m_currentSelectedPly, m_sfenRecord->size() - 1));
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

void MainWindow::resetGameFlags()
{
    // 司令塔に終局状態の全クリアを依頼（isOver / moveAppended / hasLast など一括）
    if (m_match) {
        m_match->clearGameOverState();
    }
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

    MatchCoordinator::Deps d;
    d.gc    = m_gameController;
    d.clock = m_shogiClock;
    d.view  = m_shogiView;
    d.usi1  = m_usi1;
    d.usi2  = m_usi2;

    // --- ここから追加（EvE 用：司令塔にモデルを渡す） ---
    d.comm1  = m_lineEditModel1;     // UsiCommLogModel*（先手）
    d.think1 = m_modelThinking1;     // ShogiEngineThinkingModel*（先手）
    d.comm2  = m_lineEditModel2;     // UsiCommLogModel*（後手）
    d.think2 = m_modelThinking2;     // ShogiEngineThinkingModel*（後手）

    //d.hooks.appendEvalP1 = [this] { redrawEngine1EvaluationGraph(); };
    //d.hooks.appendEvalP2 = [this] { redrawEngine2EvaluationGraph(); };
    // --- 追加ここまで ---

    // （MainWindow::setupDeps / ensure... など hooks を設定している塊に追加）
    qDebug() << "[WIRE][Eval] installing eval hooks";

    d.hooks.appendEvalP1 = [this] {
        // GUIスレッドに投げる（EvE のスレッドからでも安全）
        QMetaObject::invokeMethod(this, [this]{
            qDebug() << "[HOOK] appendEvalP1 -> redrawEngine1EvaluationGraph() on thread" << QThread::currentThread();
            redrawEngine1EvaluationGraph();
        }, Qt::QueuedConnection);
    };

    d.hooks.appendEvalP2 = [this] {
        QMetaObject::invokeMethod(this, [this]{
            qDebug() << "[HOOK] appendEvalP2 -> redrawEngine2EvaluationGraph() on thread" << QThread::currentThread();
            redrawEngine2EvaluationGraph();
        }, Qt::QueuedConnection);
    };

    qDebug() << "[WIRE][Eval] appendEvalP1 set =" << bool(d.hooks.appendEvalP1)
             << " appendEvalP2 set =" << bool(d.hooks.appendEvalP2);

    // ---------- Hooks: MainWindow の既存APIに委譲 ----------
    // 手番表示（1=先手,2=後手）
    d.hooks.updateTurnDisplay = [this](MatchCoordinator::Player cur){
        updateTurnStatus(static_cast<int>(cur));
    };

    // 対局者名表示（引数は捨て、既存ロジックに委譲）
    d.hooks.setPlayersNames = [this](const QString&, const QString&){
        setPlayersNamesForMode();
    };

    // エンジン名表示（引数は捨て、既存ロジックに委譲）
    d.hooks.setEngineNames = [this](const QString&, const QString&){
        setEngineNamesBasedOnMode();
    };

    // 対局中メニュー表示の切り替え
    d.hooks.setGameActions = [this](bool inProgress){
        setGameInProgressActions(inProgress);
    };

    // 盤面再描画（GC→View 反映）
    d.hooks.renderBoardFromGc = [this](){
        if (m_shogiView && m_gameController)
            m_shogiView->applyBoardAndRender(m_gameController->board());
    };

    // ★修正：タイトルが空なら従来メッセージ／空でなければ message をそのまま表示
    d.hooks.showGameOverDialog = [this](const QString& title, const QString& message){
        const bool titleEmpty = title.isEmpty();
        const QString dlgTitle = titleEmpty ? tr("Game Over") : title;
        const QString dlgText  = titleEmpty ? tr("The game has ended. %1").arg(message)
                                            : message;
        QMessageBox::information(this, dlgTitle, dlgText);
    };

    // 任意ログ
    d.hooks.log = [](const QString& s){ qDebug().noquote() << s; };

    // 時計値の問い合わせ
    d.hooks.remainingMsFor = [this](MatchCoordinator::Player p)->qint64 {
        if (!m_shogiClock) return 0;
        return (p == MatchCoordinator::P1)
                   ? m_shogiClock->getPlayer1TimeIntMs()
                   : m_shogiClock->getPlayer2TimeIntMs();
    };
    d.hooks.incrementMsFor = [this](MatchCoordinator::Player p)->qint64 {
        if (!m_shogiClock) return 0;
        return (p == MatchCoordinator::P1)
                   ? m_shogiClock->getBincMs()
                   : m_shogiClock->getWincMs();
    };
    d.hooks.byoyomiMs = [this]()->qint64 {
        return m_shogiClock ? m_shogiClock->getCommonByoyomiMs() : 0;
    };

    // USI送受（必要最低限）
    d.hooks.sendGoToEngine = nullptr; // 必要になったら実装
    d.hooks.sendStopToEngine = [this](Usi* u){
        if (u) u->sendStopCommand();
    };
    d.hooks.sendRawToEngine = [this](Usi* u, const QString& cmd){
        if (u) u->sendRaw(cmd);
    };

    // 新規対局の初期化（盤/名前/表示など）
    d.hooks.initializeNewGame = [this](const QString& sfenStart){
        QString s = sfenStart;     // 既存APIが非常参照なのでコピーして渡す
        startNewShogiGame(s);
    };

    d.hooks.showMoveHighlights = [this](const QPoint& from, const QPoint& to){
        if (m_boardController) m_boardController->showMoveHighlights(from, to);
    };

    d.hooks.appendKifuLine = [this](const QString& text, const QString& elapsed){
        // 既存の安全な1行追記ラッパー
        appendKifuLine(text, elapsed);
    };

    // ---------- 生成 or 置き換え ----------
    if (m_match) {
        delete m_match;
        m_match = nullptr;
    }
    m_match = new MatchCoordinator(d, this);

    // wireMatchSignals_ の直前などで
    qDebug() << "[DBG] signal index:"
             << m_match->metaObject()->indexOfSignal("timeUpdated(long long,long long,bool,long long)");

    wireMatchSignals_();

    // ★ PlayMode を司令塔へ伝える（追加）
    m_match->setPlayMode(m_playMode);

    // ---------- UIシグナル接続 ----------
    // ゲーム終了 → 時計を終了表示にして残り時間を更新
    connect(
        m_match,
        static_cast<void (MatchCoordinator::*)(const MatchCoordinator::GameEndInfo&)>(
            &MatchCoordinator::gameEnded),
        this,
        static_cast<void (MainWindow::*)(const MatchCoordinator::GameEndInfo&)>(
            &MainWindow::onMatchGameEnded),
        Qt::UniqueConnection
        );

    // 盤反転通知 → 明示スロットへ一本化
    connect(m_match, &MatchCoordinator::boardFlipped,
            this, &MainWindow::onBoardFlipped,
            Qt::UniqueConnection);

    connect(m_match, &MatchCoordinator::gameOverStateChanged,
            this,     &MainWindow::onGameOverStateChanged,
            Qt::UniqueConnection);

    // 初期のエンジンポインタを供給（null可）
    m_match->updateUsiPtrs(m_usi1, m_usi2);
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

// 司令塔から届く単一の対局終了イベント（UI反映＋棋譜「投了/時間切れ」＋時間追記）
void MainWindow::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    qDebug().nospace()
    << "[UI] onMatchGameEnded ENTER cause="
    << ((info.cause==MatchCoordinator::Cause::Timeout)?"Timeout":"Resign")
    << " loser=" << ((info.loser==MatchCoordinator::P1)?"P1":"P2");

    if (m_match) m_match->disarmHumanTimerIfNeeded();
    if (m_shogiClock) m_shogiClock->stopClock();
    if (m_shogiView)  m_shogiView->setMouseClickMode(false);

    // 1) 「▲/△投了」or「▲/△時間切れ」行を必ず挿入
    const bool loserIsP1 =
        (info.loser == MatchCoordinator::P1);
    const GameOverCause cause =
        (info.cause == MatchCoordinator::Cause::Timeout)
            ? GameOverCause::Timeout
            : GameOverCause::Resignation;

    qDebug() << "[UI] setGameOverMove before";
    setGameOverMove(cause, /*loserIsPlayerOne=*/loserIsP1);
    qDebug() << "[UI] setGameOverMove after";

    // 2) 同じ手に消費時間(MM:SS/HH:MM:SS)を追記
    if (m_shogiClock) {
        m_shogiClock->markGameOver();
        if (loserIsP1) {
            m_shogiClock->applyByoyomiAndResetConsideration1();
            const QString s = m_shogiClock->getPlayer1ConsiderationAndTotalTime();
            qDebug() << "[UI] updateGameRecord(P1) <<" << s;
            updateGameRecord(s);
        } else {
            m_shogiClock->applyByoyomiAndResetConsideration2();
            const QString s = m_shogiClock->getPlayer2ConsiderationAndTotalTime();
            qDebug() << "[UI] updateGameRecord(P2) <<" << s;
            updateGameRecord(s);
        }
    } else {
        qDebug() << "[UI] m_shogiClock == null, skip time append";
    }

    // 3) 重複防止フラグは司令塔に一元化
    if (m_match) {
        qDebug() << "[UI] markGameOverMoveAppended";
        m_match->markGameOverMoveAppended();
        m_match->pokeTimeUpdateNow();
    }

    enableArrowButtons();
    if (m_recordPane && m_recordPane->kifuView())
        m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::SingleSelection);

    qDebug() << "[UI] onMatchGameEnded LEAVE";
}

void MainWindow::toggleEditSideToMove()
{
    if (!m_gameController) return;

    auto cur = m_gameController->currentPlayer();
    m_gameController->setCurrentPlayer(
        (cur == ShogiGameController::Player1)
            ? ShogiGameController::Player2
            : ShogiGameController::Player1);

    // 編集中のハイライト/表示を更新（必要に応じて）
    if (m_shogiView) m_shogiView->update();
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

void MainWindow::initializePositionStringsForMatch_()
{
    // 司令塔に開始局面の文字列初期化を委譲（Main 側の一時バッファは使わない）
    if (!m_match) return;

    QString sfenBase;
    if (!m_startSfenStr.isEmpty()) {
        sfenBase = m_startSfenStr;
    } else if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        sfenBase = m_sfenRecord->first();
    } else {
        sfenBase.clear(); // 司令塔側で startpos を使う
    }
    m_match->initializePositionStringsForStart(sfenBase);
}

void MainWindow::updateTurnDisplay()
{
    // TurnManager を（無ければ）生成して接続：QObject 階層から取得
    TurnManager* tm = this->findChild<TurnManager*>("TurnManager");
    if (!tm) {
        tm = new TurnManager(this);
        tm->setObjectName(QStringLiteral("TurnManager"));
        // ★ 手番変化を一元配信：ShogiClock と盤ハイライトの更新を既存関数に委譲
        connect(tm, &TurnManager::changed, this, [this](ShogiGameController::Player now){
            const int cur = (now == ShogiGameController::Player2) ? 2 : 1;
            updateTurnStatus(cur);
        }, Qt::UniqueConnection);
    }

    // 初期同期は ensureTurnSyncBridge_() により
    //   GC::currentPlayerChanged → TurnManager::setFromGc → changed(UI更新)
    // が自動で行われるため、ここでは何もしない。
}

void MainWindow::wireMatchSignals_()
{
    if (!m_match) return;
    if (m_timeConn) { QObject::disconnect(m_timeConn); m_timeConn = {}; }

    auto sig = static_cast<void (MatchCoordinator::*)(qint64,qint64,bool,qint64)>
        (&MatchCoordinator::timeUpdated);

    m_timeConn = connect(m_match, sig,
                         this, &MainWindow::onMatchTimeUpdated,
                         Qt::UniqueConnection);
    Q_ASSERT(m_timeConn);

    connect(m_match, &MatchCoordinator::requestAppendGameOverMove,
            this, &MainWindow::onRequestAppendGameOverMove,
            Qt::UniqueConnection);
}

static inline GameOverCause toUiCause(MatchCoordinator::Cause c) { return c; }

void MainWindow::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    const bool loserIsP1 = (info.loser == MatchCoordinator::P1);
    setGameOverMove(toUiCause(info.cause), loserIsP1);
}

void MainWindow::onMatchTimeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 /*urgencyMs*/)
{
    m_lastP1Turn = p1turn;
    m_lastP1Ms   = p1ms;
    m_lastP2Ms   = p2ms;

    if (m_shogiView) {
        m_shogiView->blackClockLabel()->setText(fmt_hhmmss(p1ms));
        m_shogiView->whiteClockLabel()->setText(fmt_hhmmss(p2ms));
        m_shogiView->setBlackTimeMs(p1ms);
        m_shogiView->setWhiteTimeMs(p2ms);
    }
    applyTurnHighlights_(p1turn);
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

// 盤/駒台クリックで移動要求が来たときに呼ばれるスロット
void MainWindow::onMoveRequested_(const QPoint& from, const QPoint& to)
{   
    // 編集モードでは対局用の合法手検証は通さず、編集専用の移動APIを使う
    if (m_boardController && m_boardController->mode() == BoardInteractionController::Mode::Edit) {
        QPoint hFrom = from, hTo = to;  // 以降のAPIが参照を取るのでローカルコピー

        const bool ok = (m_gameController && m_gameController->editPosition(hFrom, hTo));

        // ★対局では GC が endDragSignal を出すが、編集モードでは出さないためここで必ず終了
        if (m_shogiView) m_shogiView->endDrag();

        // 失敗時は選択とドラッグ状態をクリアして抜ける
        if (!ok) {
            if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, ok); // ← false で finalizeDrag() 呼ばれる
            return;
        }

        // ハイライト更新（直前手の赤/黄・選択解除などは BIC に委譲）
        if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, ok);

        // 盤の再描画
        if (m_shogiView) m_shogiView->update();

        return; // Edit はここで完結
    }

    // 以降は通常対局処理（従来どおり）
    // 着手前の手番（＝この手を指す側）を控える
    const auto moverBefore = m_gameController->currentPlayer();

    // validateAndMove は参照を取りうるためローカルコピーで渡す
    QPoint hFrom = from, hTo = to;

    bool ok = false;
    ok = m_gameController->validateAndMove(
                hFrom, hTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves
                );

    if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, ok);
    if (!ok) return;

    // 対局モードごとの後処理
    switch (m_playMode) {
    case HumanVsHuman:
        handleMove_HvH_(moverBefore, hFrom, hTo);
        break;

    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        handleMove_HvE_(hFrom, hTo);
        break;

    default:
        break;
    }
}

void MainWindow::handleMove_HvH_(ShogiGameController::Player moverBefore,
                                 const QPoint& /*from*/, const QPoint& /*to*/)
{
    // ★ 時間/消費の扱いは司令塔に一元化、UI更新は最小限（配線）
    if (m_match) {
        const auto moverP = (moverBefore == ShogiGameController::Player1)
        ? MatchCoordinator::P1 : MatchCoordinator::P2;
        m_match->finishTurnTimerAndSetConsiderationFor(moverP);
    }

    updateTurnAndTimekeepingDisplay();
    pumpUi();

    QTimer::singleShot(0, this, [this]{
        if (m_match) m_match->armTurnTimerIfNeeded();
    });
    if (m_shogiView) m_shogiView->setMouseClickMode(true);
}

// 人間 vs エンジン：人間が指した直後の処理（棋譜追記＋1手返しを司令塔へ一本化）
void MainWindow::handleMove_HvE_(const QPoint& humanFrom, const QPoint& humanTo)
{
    if (!m_match) return;
    // validateAndMove() でセット済みの m_lastMove（「▲７六歩」等の整形文字列）を司令塔へ渡す
    m_match->onHumanMove_HvE(humanFrom, humanTo, m_lastMove);
}

bool MainWindow::isGameOver_() const
{
    return (m_match && m_match->gameOverState().isOver);
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

    qRegisterMetaType<KifDisplayItem>("KifDisplayItem");
    qRegisterMetaType<QList<KifDisplayItem>>("QList<KifDisplayItem>");
    qRegisterMetaType<QStringList>("QStringList");

    if (!m_branchCtl) {
        m_branchCtl = new BranchCandidatesController(
            m_varEngine.get(),
            m_kifuBranchModel,
            this
        );
        qDebug().nospace() << "[WIRE] BranchCandidatesController created ve="
                           << (void*)m_varEngine.get()
                           << " model=" << (void*)m_kifuBranchModel;
    }

    // Plan方式の配線（ラムダ無し）
    {
        const bool okA = connect(m_branchCtl,
                                 &BranchCandidatesController::planActivated,
                                 this,
                                 &MainWindow::onBranchPlanActivated_,
                                 Qt::UniqueConnection);
        qDebug() << "[WIRE] connect planActivated -> onBranchPlanActivated_ :" << okA;
    }
    {
        const bool okB = connect(m_recordPane,
                                 &RecordPane::branchActivated,               // シグナル: (const QModelIndex&)
                                 this,
                                 &MainWindow::onRecordPaneBranchActivated_,   // スロット:  (const QModelIndex&)
                                 Qt::UniqueConnection);
        qDebug() << "[WIRE] connect RecordPane.branchActivated -> MainWindow.onRecordPaneBranchActivated_ :" << okB;
    }
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

void MainWindow::showBranchCandidatesFromPlan(int row, int ply1)
{
    // MainWindow 実装はやめ、Coordinator へ委譲
    if (!m_kifuLoadCoordinator) return;
    m_kifuLoadCoordinator->showBranchCandidates(row, ply1);
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

void MainWindow::updateHighlightsForPly_(int selPly)
{
    if (!m_boardController) return;

    m_boardController->clearAllHighlights();

    // selPly は「この手数時点の盤面」。直前に指された手は selPly-1 にある
    if (selPly <= 0) return;
    if (selPly - 1 >= m_gameMoves.size()) return;

    const ShogiMove& mv = m_gameMoves.at(selPly - 1);

    const QPoint from = normalizeBoardPoint_(mv.fromSquare);
    const QPoint to   = normalizeBoardPoint_(mv.toSquare);

    m_boardController->showMoveHighlights(from, to);
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

// 指定手数 selPly（0=開始局面, 1..）まで評価値グラフを巻き戻す
void MainWindow::trimEvalChartForResume_(int selPly)
{
    // 評価値グラフは RecordPane が所有。毎回ゲッターで取得して順序依存を回避
    EvaluationChartWidget* ec = (m_recordPane ? m_recordPane->evalChart() : nullptr);
    if (!ec) return;

    const int ply = qMax(0, selPly);

    // P1/P2 がそれまでに指した回数（= その側の評価点の“最大許容量”）
    // P1 は奇数手、P2 は偶数手を担当
    const int targetP1 = (ply + 1) / 2;  // 例: ply=4 → 2（1,3手目）
    const int targetP2 = (ply) / 2;      // 例: ply=4 → 2（2,4手目）

    // どの系列を残すか（=どの側の評価点を保持するか）は PlayMode に依存
    bool keepP1 = false, keepP2 = false;
    switch (m_playMode) {
    case EvenEngineVsHuman:       // P1=Engine, P2=Human
    case HandicapHumanVsEngine:   // あなたの実装では P1 側を描画（handleUndoMove と同じ規則）
        keepP1 = true; break;

    case EvenHumanVsEngine:       // P1=Human, P2=Engine
    case HandicapEngineVsHuman:   // あなたの実装では P2 側を描画（handleUndoMove と同じ規則）
        keepP2 = true; break;

    case HandicapEngineVsEngine:  // 両側 Engine
        keepP1 = keepP2 = true; break;

    default:
        // 編集モード等、不明なら両方消す（=0）
        keepP1 = keepP2 = false; break;
    }

    // まず不要側は全部落とす
    int curP1 = ec->countP1();
    int curP2 = ec->countP2();
    if (!keepP1) while (curP1-- > 0) ec->removeLastP1();
    if (!keepP2) while (curP2-- > 0) ec->removeLastP2();

    // 使う側だけ、目標数まで末尾から削る
    if (keepP1) {
        curP1 = ec->countP1();
        while (curP1 > targetP1) { ec->removeLastP1(); --curP1; }
    }
    if (keepP2) {
        curP2 = ec->countP2();
        while (curP2 > targetP2) { ec->removeLastP2(); --curP2; }
    }

    // スカラー履歴（最後に push している CP 値の束）も整合を取る
    const int targetTotal = (keepP1 ? targetP1 : 0) + (keepP2 ? targetP2 : 0);
    while (!m_scoreCp.isEmpty() && m_scoreCp.size() > targetTotal)
        m_scoreCp.removeLast();
}

void MainWindow::applyPendingEvalTrim_()
{
    if (m_pendingEvalTrimPly >= 0) {
        trimEvalChartForResume_(m_pendingEvalTrimPly); // ← ここで初めて刈る
        m_pendingEvalTrimPly = -1;
    }
    m_isResumeFromCurrent = false;  // ← 再開モード解除
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

// 編集モードに入ったタイミングで呼ぶ：ボタン表示＆スロット接続（ラムダ不使用）
void MainWindow::showEditExitButtonOnBoard_()
{
    if (!m_shogiView) return;

    // ShogiView 側で作成&配置（必要なら内部で作成される）
    m_shogiView->relayoutEditExitButton();

    // ボタン取得
    QPushButton* exitBtn = m_shogiView->findChild<QPushButton*>(QStringLiteral("editExitButton"));
    if (!exitBtn) return;

    // 重複接続防止のため一旦切る
    QObject::disconnect(exitBtn, SIGNAL(clicked()), this, SLOT(finishPositionEditing()));

    // 旧式マクロ接続（ラムダ不使用）
    connect(exitBtn, SIGNAL(clicked()), this, SLOT(finishPositionEditing()));

    // 表示
    exitBtn->show();
    exitBtn->raise();
}

// 編集モードを抜けるときに呼ぶ：ボタン非表示（接続は残しても害なし）
void MainWindow::hideEditExitButtonOnBoard_()
{
    if (!m_shogiView) return;
    if (QPushButton* exitBtn = m_shogiView->findChild<QPushButton*>(QStringLiteral("editExitButton"))) {
        exitBtn->hide();
    }
}

void MainWindow::ensureHumanAtBottomIfApplicable_()
{
    if (!m_startGameDialog) return;

    const bool humanP1  = m_startGameDialog->isHuman1();
    const bool humanP2  = m_startGameDialog->isHuman2();
    const bool oneHuman = (humanP1 ^ humanP2); // HvE または EvH のときだけ true

    if (!oneHuman) return; // HvH / EvE は対象外（仕様どおり）

    // 現在「手前が先手か？」と「人間が先手か？」が食い違っていたら1回だけ反転
    const bool needFlip = (humanP1 != m_bottomIsP1);
    if (needFlip) {
        // 指定の関数経由で実反転。内部で m_match->flipBoard() が呼ばれる
        onActionFlipBoardTriggered(false);
        // onBoardFlipped() が呼ばれ、m_bottomIsP1 はトグルされます
    }
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

    // 任意（ログやUI反映）
    connect(m_gameStart, &GameStartCoordinator::willStart,
            this, &MainWindow::onGameWillStart_);

    connect(m_gameStart, &GameStartCoordinator::started,
            this, &MainWindow::onGameStarted_);

    connect(m_gameStart, &GameStartCoordinator::startFailed,
            this, &MainWindow::onGameStartFailed_);
}

// 対局開始前の UI/状態初期化（ハイライトや選択をクリア等）
void MainWindow::onPreStartCleanupRequested_()
{
    if (m_boardController) m_boardController->clearAllHighlights();
    // 棋譜欄や解析UIのリセット、各種フラグ初期化など
    // setReplayMode(false); 等、既存処理をここへ集約
}

// 対局開始前の時計適用（GameStartCoordinator からの依頼を受ける）
void MainWindow::onApplyTimeControlRequested_(const GameStartCoordinator::TimeControl& tc)
{
    if (!m_shogiClock) return;

    // 1) 進行中なら一旦止めて経過分を確定させる（安全）
    m_shogiClock->stopClock();

    // 2) ms → sec 変換（切り捨て：UIは分単位入力が多く端数は通常発生しない想定）
    auto msToSecFloor = [](qint64 ms) -> int {
        return (ms <= 0) ? 0 : static_cast<int>(ms / 1000);
    };

    const bool limited = tc.enabled;

    const int p1BaseSec = limited ? msToSecFloor(tc.p1.baseMs) : 0;
    const int p2BaseSec = limited ? msToSecFloor(tc.p2.baseMs) : 0;

    const int byo1Sec   = msToSecFloor(tc.p1.byoyomiMs);
    const int byo2Sec   = msToSecFloor(tc.p2.byoyomiMs);
    const int inc1Sec   = msToSecFloor(tc.p1.incrementMs);
    const int inc2Sec   = msToSecFloor(tc.p2.incrementMs);

    // 3) byoyomi と increment は排他的：byoyomi が1つでも設定されていれば byoyomi を優先
    const bool useByoyomi = (byo1Sec > 0) || (byo2Sec > 0);

    const int finalByo1 = useByoyomi ? byo1Sec : 0;
    const int finalByo2 = useByoyomi ? byo2Sec : 0;
    const int finalInc1 = useByoyomi ? 0       : inc1Sec;
    const int finalInc2 = useByoyomi ? 0       : inc2Sec;

    // 4) 時間切れ敗北の扱い
    //    有限時間なら true、無制限（考慮時間のみ計測）なら false
    m_shogiClock->setLoseOnTimeout(limited);

    // 5) 時計へ一括適用（内部で各種状態/キャッシュが初期化され、timeUpdated がemitされる）
    m_shogiClock->setPlayerTimes(
        p1BaseSec, p2BaseSec,
        finalByo1, finalByo2,
        finalInc1, finalInc2,
        /*isLimitedTime=*/limited
        );

    // 6) 初期手番の推定（SFENの手番 ' b ' = 先手, ' w ' = 後手）
    //    StartOptionsはここに来ないため、MainWindowが保持するSFENから推定します。
    auto sideFromSfen = [](const QString& sfen) -> int {
        // " ... b ... " / " ... w ... " を想定
        return sfen.contains(QStringLiteral(" w ")) ? 2 : 1; // 既定は先手
    };
    const QString sfenForStart =
        !m_startSfenStr.isEmpty() ? m_startSfenStr :
            (!m_currentSfenStr.isEmpty() ? m_currentSfenStr : QStringLiteral("startpos b - 1"));
    m_shogiClock->setCurrentPlayer(sideFromSfen(sfenForStart));

    // ここでは startClock() は呼びません。
    // 実際の開始/手番側の起動は MatchCoordinator / GameStartCoordinator 側のフローに委譲します。
    qDebug().noquote()
        << "[Clock] applied:"
        << "p1=" << p1BaseSec << "s,"
        << "p2=" << p2BaseSec << "s,"
        << (useByoyomi
                ? QStringLiteral("byoyomi(%1/%2)s").arg(finalByo1).arg(finalByo2)
                : QStringLiteral("increment(%1/%2)s").arg(finalInc1).arg(finalInc2))
        << "limited=" << limited
        << "startSide=" << (m_shogiClock->currentPlayer() == 1 ? "P1" : "P2");
}

// ログやUI反映用（任意）
void MainWindow::onGameWillStart_(const MatchCoordinator::StartOptions& opt)
{
    Q_UNUSED(opt);
    // ステータスバー表示など
}
void MainWindow::onGameStarted_(const MatchCoordinator::StartOptions& opt)
{
    Q_UNUSED(opt);
    // ボタン状態やラベルの更新など
}
void MainWindow::onGameStartFailed_(const QString& reason)
{
    // QMessageBox::critical(this, tr("開始失敗"), reason);
}
