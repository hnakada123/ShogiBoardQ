#include <QMessageBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QToolButton>
#include <QWidgetAction>
#include <QMetaType>
#include <QDebug>

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
#include "kiftosfenconverter.h"
#include "shogiclock.h"
#include "shogiutils.h"
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

// mainwindow.cpp の先頭（インクルードの後、どのメンバ関数より上）に追加
static inline QString pickLabelForDisp(const KifDisplayItem& d)
{
    return d.prettyMove;
}

static inline QString lineNameForRow(int row) {
    return (row == 0) ? QStringLiteral("Main") : QStringLiteral("Var%1").arg(row - 1);
}

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

namespace {
constexpr int COL_NO   = 0;   // 手数
constexpr int COL_P1   = 1;   // 先手の指し手
constexpr int COL_P2   = 2;   // 後手の指し手

static inline QString cellText(const QAbstractItemModel* m, int r, int c) {
    if (!m) return {};
    const QModelIndex ix = m->index(r, c);
    return ix.isValid() ? m->data(ix, Qt::DisplayRole).toString() : QString();
}

static void dumpTailState(const QAbstractItemModel* m, const char* tag) {
    if (!m) { qDebug() << tag << "[model=null]"; return; }
    const int rows = m->rowCount();
    if (rows <= 0) { qDebug() << tag << "rows=0"; return; }
    const int last = rows - 1;
    qDebug().noquote() << QString("%1 rows=%2 lastRow=%3  P1='%4'  P2='%5'")
                          .arg(tag).arg(rows).arg(last)
                          .arg(cellText(m, last, COL_P1))
                          .arg(cellText(m, last, COL_P2));
}
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_startSfenStr("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
    m_currentSfenStr("startpos"),
    m_errorOccurred(false),
    m_usi1(nullptr),
    m_usi2(nullptr),
    m_playMode(NotStarted),
    m_lineEditModel1(new UsiCommLogModel(this)),
    m_lineEditModel2(new UsiCommLogModel(this)),
    m_gameCount(0),
    m_gameController(nullptr),
    m_analysisModel(nullptr)
{
    ui->setupUi(this);

    // --- コンストラクタで一度だけ centralWidget をセット ---
    m_central = ui->centralwidget;
    m_centralLayout = new QVBoxLayout(m_central);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);

    QToolBar* tb = ui->toolBar;                  // Designerで作った場合の例
    tb->setIconSize(QSize(18, 18));              // ← 16px（お好みで 16/18/20/24 など）
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);  // ← テキストを消して高さを詰める
    tb->setStyleSheet(
        "QToolBar{margin:0px; padding:0px; spacing:2px;}"
        "QToolButton{margin:0px; padding:2px;}"
        );

    // GUIを構成するWidgetなどのnew生成
    initializeComponents();

    setupRecordPane();

    // m_kifuBranchModel や m_varEngine の初期化が済んでいる前提で
    setupBranchCandidatesWiring_();

    // 盤・駒台などの初期化
    startNewShogiGame(m_startSfenStr);

    // 横並びレイアウトの構築
    setupHorizontalGameLayout();

    // ★ EngineAnalysisTab はここで一度だけ初期化して m_tab を用意する
    setupEngineAnalysisTab();

    // セントラルウィジェット構築（m_tab を add する側）
    initializeCentralGameDisplay();

    // 対局のメニュー表示を一部隠す。
    //hideGameActions();

    // 棋譜欄の下の矢印ボタンを無効にする。
    disableArrowButtons();

    // 局面編集メニューの表示・非表示
    hidePositionEditMenu();

    // ウィンドウサイズの復元
    loadWindowSettings();

    // MainWindow ctor で、UIを作り終わった直後にツールチップ調整
    auto* tipFilter = new AppToolTipFilter(this);
    tipFilter->setPointSizeF(12.0);
    tipFilter->setCompact(true);

    auto toolbars = findChildren<QToolBar*>();
    for (int i = 0, n = toolbars.size(); i < n; ++i) {
        QToolBar* tb = toolbars.at(i);
        auto buttons = tb->findChildren<QToolButton*>();
        for (int j = 0, m = buttons.size(); j < m; ++j)
            buttons.at(j)->installEventFilter(tipFilter);
    }

    initMatchCoordinator();

    setupNameAndClockFonts_();

    // メニューのシグナルとスロット
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::saveSettingsAndClose);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::saveKifuToFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::overwriteKifuFile);
    connect(ui->actionEngineSettings, &QAction::triggered, this, &MainWindow::displayEngineSettingsDialog);
    connect(ui->actionVersionInfo, &QAction::triggered, this, &MainWindow::displayVersionInformation);
    connect(ui->actionOpenWebsite, &QAction::triggered, this, &MainWindow::openWebsiteInExternalBrowser);
    connect(ui->actionStartGame, &QAction::triggered, this, &MainWindow::initializeGame);
    connect(ui->actionResign, &QAction::triggered, this, &MainWindow::handleResignation);
    connect(ui->actionFlipBoard, &QAction::triggered, this, &MainWindow::onActionFlipBoardTriggered);
    connect(ui->actionCopyBoardToClipboard, &QAction::triggered, this, &MainWindow::copyBoardToClipboard);
    connect(ui->actionToggleEngineAnalysis, &QAction::triggered, this, &MainWindow::toggleEngineAnalysisVisibility);
    connect(ui->actionMakeImmediateMove, &QAction::triggered, this, &MainWindow::movePieceImmediately);
    connect(ui->actionEnlargeBoard, &QAction::triggered, this, &MainWindow::enlargeBoard);
    connect(ui->actionShrinkBoard, &QAction::triggered, this, &MainWindow::reduceBoardSize);
    connect(ui->actionUndoMove, &QAction::triggered, this, &MainWindow::undoLastTwoMoves);
    connect(ui->actionSaveBoardImage, &QAction::triggered, this, &MainWindow::saveShogiBoardImage);
    connect(ui->actionOpenKifuFile, &QAction::triggered, this, &MainWindow::chooseAndLoadKifuFile);
    connect(ui->actionConsideration, &QAction::triggered, this, &MainWindow::displayConsiderationDialog);
    connect(ui->actionAnalyzeKifu, &QAction::triggered, this, &MainWindow::displayKifuAnalysisDialog);
    connect(ui->actionNewGame, &QAction::triggered, this, &MainWindow::resetToInitialState);
    connect(ui->actionStartEditPosition, &QAction::triggered, this, &MainWindow::beginPositionEditing);
    connect(ui->actionEndEditPosition, &QAction::triggered, this, &MainWindow::finishPositionEditing);
    connect(ui->actionTsumeShogiSearch, &QAction::triggered, this, &MainWindow::displayTsumeShogiSearchDialog);
    connect(ui->breakOffGame, &QAction::triggered, this, &MainWindow::handleBreakOffGame);
    connect(ui->actionQuitEngine, &QAction::triggered, this, &MainWindow::handleBreakOffConsidaration);

    // 将棋盤表示・エラー・昇格ダイアログ等
    connect(m_gameController, &ShogiGameController::showPromotionDialog, this, &MainWindow::displayPromotionDialog);
    connect(m_shogiView, &ShogiView::errorOccurred, this, &MainWindow::displayErrorMessage);
    connect(m_gameController, &ShogiGameController::endDragSignal, this, &MainWindow::endDrag, Qt::UniqueConnection);
    connect(m_gameController, &ShogiGameController::moveCommitted, this, &MainWindow::onMoveCommitted, Qt::UniqueConnection);
}

// GUIを構成するWidgetなどを生成する。
void MainWindow::initializeComponents()
{
    // 将棋の対局全体を管理し、盤面の初期化、指し手の処理、合法手の検証、対局状態の管理を行うクラスのインスタンス
    m_gameController = new ShogiGameController;

    // 将棋盤と駒台を生成する。
    m_shogiView = new ShogiView;

    // 好みの倍率に設定（表示前にやるのがスムーズ）
    m_shogiView->setNameFontScale(0.30);

    // ───────────────── BoardInteractionController を配線 ─────────────────
    setupBoardInteractionController();

    // SFEN文字列のリスト
    m_sfenRecord = new QStringList;

    // 棋譜データのリスト
    m_moveRecords = new QList<KifuDisplay *>;
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

// 「jpg / jpeg」をまとめて扱うためのヘルパ
static bool hasFormat(const QSet<QString>& fmts, const QString& key) {
    if (key == "jpeg") return fmts.contains("jpeg") || fmts.contains("jpg");
    return fmts.contains(key);
}

// 対局のメニュー表示を一部隠す。
void MainWindow::hideGameActions()
{
    // 「投了」を隠す。
    ui->actionResign->setVisible(false);

    // 「待った」を隠す。
    ui->actionUndoMove->setVisible(false);

    // 「すぐ指させる」を隠す。
    ui->actionMakeImmediateMove->setVisible(false);

    // 「中断」を隠す。
    ui->breakOffGame->setVisible(false);
}

// 「表示」の「思考」 思考タブの表示・非表示
void MainWindow::toggleEngineAnalysisVisibility() {
    if (!m_analysisTab) return;
    m_analysisTab->setAnalysisVisible(ui->actionToggleEngineAnalysis->isChecked());
}

void MainWindow::handleUndoMove(int index)
{
    // まず index の範囲チェック
    if (index < 0 || index >= m_positionStrList.size()) {
        // 想定外だが安全のため何もしない
        return;
    }

    // 評価値グラフの実体は RecordPane が所有しているので、
    // ここで都度ゲッター経由で取得（配線漏れ/順序依存を回避）
    EvaluationChartWidget* ec = (m_recordPane ? m_recordPane->evalChart() : nullptr);

    switch (m_playMode) {
    // 平手 P1: Human, P2: Engine
    case EvenHumanVsEngine:
    // 駒落ち P1: Engine(下手), P2: Engine(上手)
    case HandicapEngineVsEngine:
        // 2手前のposition文字列
        m_positionStr1 = m_positionStrList.at(index);
        // 評価値グラフ（後手= P2 側）から最後を1点だけ削除
        if (ec) ec->removeLastP2();
        break;

    // 平手 P1: Engine, P2: Human
    case EvenEngineVsHuman:
        m_positionStr1 = m_positionStrList.at(index);
        if (ec) ec->removeLastP1();
        break;

    // 駒落ち P1: Human(下手), P2: Engine(上手)
    case HandicapHumanVsEngine:
        m_positionStr1 = m_positionStrList.at(index);
        if (ec) ec->removeLastP1();
        break;

    // 駒落ち P1: Engine(下手), P2: Human(上手)
    case HandicapEngineVsHuman:
        m_positionStr2 = m_positionStrList.at(index);
        if (ec) ec->removeLastP2();
        break;

    default:
        break;
    }

    // 待った前の評価値も末尾を1つだけ取り消し（空なら触らない）
    if (!m_scoreCp.isEmpty()) {
        m_scoreCp.removeLast();
    }
}

// 待ったボタンを押すと、2手戻る。
void MainWindow::undoLastTwoMoves()
{
    // --- UNDO 中は棋譜追記を抑止 ---
    m_isUndoInProgress = true;

    // (1) 進行中の人間用タイマーは止める（巻き戻し時間を混入させない）
    if (m_match) {
        m_match->disarmHumanTimerIfNeeded();
        // H2H の共通ターン計測を使っているなら用意があれば:
        // m_match->disarmTurnTimerIfNeeded();
    }

    // 2手戻すには現在インデックスが2以上必要
    if (m_currentMoveIndex < 2) {
        m_isUndoInProgress = false;  // ★ 早期returnでも必ず解除
        return;
    }

    const int moveNumber = m_currentMoveIndex - 2;

    // ---- 以降、巻き戻し処理 ----
    const bool prevGuard = m_onMainRowGuard;
    m_onMainRowGuard = true;

    // ★★★ 重要 ★★★
    // 棋譜モデルの2手削除は、m_currentMoveIndex を触る前に行う。
    // これにより「最後の手」は正しく“後手→先手”の順で落ちる。
    if (m_kifuRecordModel) {
        m_kifuRecordModel->removeLastItems(2);   // ← 一括削除API（推奨）
        // もし removeLastItems が未実装なら:
        // m_kifuRecordModel->removeLastItem();
        // m_kifuRecordModel->removeLastItem();
    }

    // --- 指し手配列を安全に2つ削除 ---
    if (m_gameMoves.size() >= 2) {
        m_gameMoves.removeLast();
        m_gameMoves.removeLast();
    } else {
        m_gameMoves.clear();
    }

    // 評価値などの巻き戻し（position の復元を先に）
    handleUndoMove(moveNumber);

    // --- position 文字列も2つ削除 ---
    if (m_positionStrList.size() >= 2) {
        m_positionStrList.removeLast();
        m_positionStrList.removeLast();
    } else {
        m_positionStrList.clear();
    }

    // --- 盤面（SFEN）を2手前へ（この時点で m_currentMoveIndex はまだ旧値） ---
    if (m_sfenRecord && moveNumber >= 0 && moveNumber < m_sfenRecord->size()) {
        const QString str = m_sfenRecord->at(moveNumber);
        if (m_gameController && m_gameController->board()) {
            m_gameController->board()->setSfen(str);
        }
    }

    // SFEN レコードも末尾2つ削除
    if (m_sfenRecord) {
        if (m_sfenRecord->size() >= 2) {
            m_sfenRecord->removeLast();
            m_sfenRecord->removeLast();
        } else {
            m_sfenRecord->clear();
        }
    }

    // ここで“ようやく”現在手数を更新
    m_currentMoveIndex = moveNumber;

    // ハイライトのクリア → 復元（0→1始まりに +1 補正版を使用）
    if (m_boardController) m_boardController->clearAllHighlights();
    updateHighlightsForPly_(m_currentMoveIndex);

    m_onMainRowGuard = prevGuard;

    // 時計を2手前へ
    if (m_shogiClock) m_shogiClock->undo();

    // 表示の整合を更新
    updateTurnAndTimekeepingDisplay();

    // いまの手番が人間なら計測再アーム & クリック可否の更新
    auto isHuman = [this](ShogiGameController::Player p) {
        switch (m_playMode) {
        case HumanVsHuman:                return true;
        case EvenHumanVsEngine:
        case HandicapHumanVsEngine:       return p == ShogiGameController::Player1;
        case HandicapEngineVsHuman:       return p == ShogiGameController::Player2;
        default:                          return false;
        }
    };

    const auto sideToMove = m_gameController ? m_gameController->currentPlayer()
                                             : ShogiGameController::NoPlayer;
    if (m_shogiView) m_shogiView->setMouseClickMode(isHuman(sideToMove));

    if (isHuman(sideToMove)) {
        QTimer::singleShot(0, this, [this]{
            if (!m_match) return;
            if (m_playMode == HumanVsHuman) {
                m_match->armTurnTimerIfNeeded();
            } else {
                m_match->armHumanTimerIfNeeded();
            }
        });
    }

    // --- UNDO 中フラグ OFF（最後に必ず解除） ---
    m_isUndoInProgress = false;
}

// 新規対局の準備をする。
// 将棋盤、駒台を初期化（何も駒がない）し、入力のSFEN文字列の配置に将棋盤、駒台の駒を
// 配置し、対局結果を結果なし、現在の手番がどちらでもない状態に設定する。
void MainWindow::initializeNewGame(QString& startSfenStr)
{
    try {
        m_gameController->newGame(startSfenStr);
    } catch (const std::exception& e) {
        // エラーメッセージを表示する。
        displayErrorMessage(e.what());
    }

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
    // ▲▽の記号はここでは名前に含めない。
    QString blackName;
    QString whiteName;

    switch (m_playMode) {
    // Player1: Human, Player2: Human
    case HumanVsHuman:
        blackName = m_humanName1;
        whiteName = m_humanName2;
        break;

    // Player1: Human, Player2: USI Engine
    case EvenHumanVsEngine:
        blackName = m_humanName1;
        whiteName = m_engineName2;
        break;

    // Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:
        blackName = m_engineName1;
        whiteName = m_humanName2;
        break;

    // Player1: USI Engine, Player2: USI Engine
    case EvenEngineVsEngine:
        blackName = m_engineName1;
        whiteName = m_engineName2;
        break;

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        blackName = m_humanName1;
        whiteName = m_engineName2;
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        blackName = m_engineName1;
        whiteName = m_humanName2;
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine（上手）
    case HandicapEngineVsEngine:
        blackName = m_engineName1;
        whiteName = m_engineName2;
        break;

    // まだ対局を開始していない状態
    case NotStarted:
        blackName = tr("先手");
        whiteName = tr("後手");
        break;

    // 棋譜解析モード
    case AnalysisMode:

    // 検討モード
    case ConsidarationMode:

    // 詰将棋探索モード
    case TsumiSearchMode:

    // 対局モードエラー
    case PlayModeError:
        break;
    }

    // 対局者名をセットする。
    m_shogiView->setBlackPlayerName(blackName);
    m_shogiView->setWhitePlayerName(whiteName);
}

// 駒台を含む将棋盤全体の画像をクリップボードにコピーする。
void MainWindow::copyBoardToClipboard() {
    BoardImageExporter::copyToClipboard(m_shogiView);
}


void MainWindow::saveShogiBoardImage() {
    BoardImageExporter::saveImage(this, m_shogiView);
}

// 対局モードに応じて将棋盤下部に表示されるエンジン名をセットする。
void MainWindow::setEngineNamesBasedOnMode() {
    // 例：先後に応じて入れ替え
    if (!m_lineEditModel1 || !m_lineEditModel2) return;
    switch (m_playMode) {
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
        m_lineEditModel1->setEngineName(m_engineName2);
        m_lineEditModel2->setEngineName(QString());
        break;
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        m_lineEditModel1->setEngineName(m_engineName1);
        m_lineEditModel2->setEngineName(QString());
        break;
    case EvenEngineVsEngine:
        m_lineEditModel1->setEngineName(m_engineName1);
        m_lineEditModel2->setEngineName(m_engineName2);
        break;
    case HandicapEngineVsEngine:
        m_lineEditModel1->setEngineName(m_engineName2);
        m_lineEditModel2->setEngineName(m_engineName1);
        break;
    default: break;
    }
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

            ShogiUtils::logAndThrowError(errorMessage);

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

    qDebug() << "tab parents:" << m_tab->parent();
    qDebug() << "tabs in window:" << this->findChildren<QTabWidget*>().size();

    auto tabs = this->findChildren<QTabWidget*>();
    qDebug() << "QTabWidget count =" << tabs.size();
    for (auto* t : tabs) {
        qDebug() << " tab*" << t
                 << " objectName=" << t->objectName()
                 << " parent=" << t->parent();
    }
    qDebug() << " m_tab =" << m_tab;
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

void MainWindow::sendCommandsToEngineOne()
{
    if (m_match) m_match->sendGameOverWinAndQuitTo(1);
}

void MainWindow::sendCommandsToEngineTwo()
{
    if (m_match) m_match->sendGameOverWinAndQuitTo(2);
}

// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    if (m_match) m_match->handleResign();
}

void MainWindow::handleEngineOneResignation()
{
    if (m_match) m_match->handleEngineResign(1);
}

void MainWindow::handleEngineTwoResignation()
{
    if (m_match) m_match->handleEngineResign(2);
}

// エンジン1の評価値グラフの再描画を行う。
void MainWindow::redrawEngine1EvaluationGraph()
{
    // 旧ロジックの「符号反転条件」を踏襲
    const bool invert =
        (m_playMode == EvenHumanVsEngine) ||
        (m_playMode == HandicapEngineVsEngine) ||
        (m_playMode == HandicapHumanVsEngine) ||
        (m_playMode == AnalysisMode);

    // ★ 司令塔から主エンジンを取得（m_usi1 は使わない）
    Usi* eng = (m_match ? m_match->primaryEngine() : nullptr);

    // エンジンが未初期化でも落ちないようにガード
    const int scoreCp = eng ? eng->lastScoreCp() : 0;

    auto* ec = m_recordPane ? m_recordPane->evalChart() : nullptr;

    // ---- デバッグ出力 ----
    qDebug() << "[EVAL][P1] redraw"
             << "moveIndex=" << m_currentMoveIndex
             << "invert=" << invert
             << "scoreCp=" << scoreCp
             << "playMode(int)=" << int(m_playMode)
             << "engine=" << static_cast<const void*>(eng)
             << "chart="  << static_cast<const void*>(ec)
             << "thread=" << QThread::currentThread();

    if (!eng) qDebug() << "[EVAL][P1][WARN] primaryEngine() is null";
    if (!ec)  qDebug() << "[EVAL][P1][WARN] evalChart() is null";

    if (ec) {
        ec->appendScoreP1(m_currentMoveIndex, scoreCp, invert);
        qDebug() << "[EVAL][P1] appendScoreP1 done"
                 << "idx=" << m_currentMoveIndex << "cp=" << scoreCp << "invert=" << invert;
    }

    // 記録も更新（整合のためエンジン未初期化時は 0 を入れておく）
    m_scoreCp.append(scoreCp);
}

// エンジン2の評価値グラフの再描画を行う。
void MainWindow::redrawEngine2EvaluationGraph()
{
    // 旧ロジック：駒落ちのエンジン対エンジン以外は反転
    const bool invert = (m_playMode != HandicapEngineVsEngine);

    // ★ 司令塔から 2nd エンジンを取得（m_usi2 は使わない）
    Usi* eng2 = (m_match ? m_match->secondaryEngine() : nullptr);

    // エンジン未初期化でも落ちないように 0 を既定値に
    const int scoreCp = eng2 ? eng2->lastScoreCp() : 0;

    auto* ec = m_recordPane ? m_recordPane->evalChart() : nullptr;

    // ---- デバッグ出力 ----
    qDebug() << "[EVAL][P2] redraw"
             << "moveIndex=" << m_currentMoveIndex
             << "invert=" << invert
             << "scoreCp=" << scoreCp
             << "playMode(int)=" << int(m_playMode)
             << "engine2=" << static_cast<const void*>(eng2)
             << "chart="  << static_cast<const void*>(ec)
             << "thread=" << QThread::currentThread();

    if (!eng2) qDebug() << "[EVAL][P2][WARN] secondaryEngine() is null";
    if (!ec)   qDebug() << "[EVAL][P2][WARN] evalChart() is null";

    if (ec) {
        ec->appendScoreP2(m_currentMoveIndex, scoreCp, invert);
        qDebug() << "[EVAL][P2] appendScoreP2 done"
                 << "idx=" << m_currentMoveIndex << "cp=" << scoreCp << "invert=" << invert;
    }

    // 既存仕様に合わせてそのまま記録（未初期化時は 0 を入れる）
    m_scoreCp.append(scoreCp);
}

// 棋譜を更新し、GUIの表示も同時に更新する。
void MainWindow::updateGameRecordAndGUI()
{
    // 追加：待った中は追記を抑止
    if (m_isUndoInProgress) {
        qDebug() << "clock: [KIFU] suppress updateGameRecord during UNDO";
        return;
    }

    // 棋譜欄の最後に表示する投了の文字列を設定する。
    // 対局モードが平手のエンジン対エンジンの場合
    if ((m_playMode == EvenHumanVsEngine) || (m_playMode == HandicapHumanVsEngine)) {
        m_lastMove = "▲投了";
    } else {
        m_lastMove = "△投了";
    }

    // 棋譜を更新し、UIの表示も同時に更新する。
    updateGameRecord(0);
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
    // KIF再生中は時計を動かさない（一本化）
    if (m_isReplayMode) {
        if (m_shogiClock) m_shogiClock->stopClock();
        if (m_match)      m_match->pokeTimeUpdateNow();
        return;
    }

    // 司令塔の終局状態で判定
    const bool gameOver = (m_match && m_match->gameOverState().isOver);

    // ★ 終局後は何もしない（時計は止める）
    if (gameOver) {
        qDebug() << "[ARBITER] suppress updateTurnAndTimekeepingDisplay (game over)";
        if (m_shogiClock) m_shogiClock->stopClock();
        if (m_match) {
            m_match->pokeTimeUpdateNow();        // 表示だけ整える
            m_match->disarmHumanTimerIfNeeded(); // 人間用ストップウォッチ停止
        }
        return;
    }

    // 1) 今走っている側を止めて残時間確定
    if (m_shogiClock) m_shogiClock->stopClock();

    // 2) 直前に指した側へ increment/秒読みを適用 + 考慮時間の記録
    const bool nextIsP1 = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    if (m_shogiClock) {
        if (nextIsP1) {
            // これから先手が指す → 直前に指したのは後手
            m_shogiClock->applyByoyomiAndResetConsideration2();
            updateGameRecord(m_shogiClock->getPlayer2ConsiderationAndTotalTime());
            m_shogiClock->setPlayer2ConsiderationTime(0);
        } else {
            // これから後手が指す → 直前に指したのは先手
            m_shogiClock->applyByoyomiAndResetConsideration1();
            updateGameRecord(m_shogiClock->getPlayer1ConsiderationAndTotalTime());
            m_shogiClock->setPlayer1ConsiderationTime(0);
        }
    }

    // 3) 表示更新（残時間ラベルなど）
    if (m_match) m_match->pokeTimeUpdateNow();

    // （※旧④は削除：USI向け btime/wtime は必要箇所で都度 computeGoTimesForUSI() を呼ぶ）

    // 5) 手番表示・時計再開
    updateTurnStatus(nextIsP1 ? 1 : 2);
    if (m_shogiClock) m_shogiClock->startClock();

    // 6) 新しい手番の「この手」の開始時刻を司令塔に記録
    if (m_match) {
        m_match->markTurnEpochNowFor(nextIsP1 ? MatchCoordinator::P1 : MatchCoordinator::P2);
    }

    // 7) 人間の手番なら人間用／H2H用の計測をアーム、そうでなければ解除
    if (isHumanTurn()) {
        if (m_match) {
            if (m_playMode == HumanVsHuman) {
                m_match->armTurnTimerIfNeeded();   // H2H 共通計測
            } else {
                m_match->armHumanTimerIfNeeded();  // HvE 人間側計測
            }
        }
    } else {
        if (m_match) m_match->disarmHumanTimerIfNeeded();
    }
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

// 対局ダイアログの設定を出力する。
void MainWindow::printGameDialogSettings(StartGameDialog* m_gamedlg)
{
    qDebug() << "MainWindow::printGameDialogSettings()";
    qDebug() << "m_gamedlg->isHuman1() = " << m_gamedlg->isHuman1();
    qDebug() << "m_gamedlg->isEngine1() = " << m_gamedlg->isEngine1();
    qDebug() << "m_gamedlg->humanName1() = " << m_gamedlg->humanName1();
    qDebug() << "m_gamedlg->engineName1() = " << m_gamedlg->engineName1();
    qDebug() << "m_gamedlg->engineNumber1() = " << m_gamedlg->engineNumber1();
    qDebug() << "m_gamedlg->basicTimeHour1() = " << m_gamedlg->basicTimeHour1();
    qDebug() << "m_gamedlg->basicTimeMinutes1() = " << m_gamedlg->basicTimeMinutes1();
    qDebug() << "m_gamedlg->byoyomiSec1() = " << m_gamedlg->byoyomiSec1();
    qDebug() << "m_gamedlg->addEachMoveSec1() = " << m_gamedlg->addEachMoveSec1();
    qDebug() << "m_gamedlg->isHuman2() = " << m_gamedlg->isHuman2();
    qDebug() << "m_gamedlg->isEngine2() = " << m_gamedlg->isEngine2();
    qDebug() << "m_gamedlg->humanName2() = " << m_gamedlg->humanName2();
    qDebug() << "m_gamedlg->engineName2() = " << m_gamedlg->engineName2();
    qDebug() << "m_gamedlg->engineNumber2() = " << m_gamedlg->engineNumber2();
    qDebug() << "m_gamedlg->basicTimeHour2() = " << m_gamedlg->basicTimeHour2();
    qDebug() << "m_gamedlg->basicTimeMinutes2() = " << m_gamedlg->basicTimeMinutes2();
    qDebug() << "m_gamedlg->byoyomiSec2() = " << m_gamedlg->byoyomiSec2();
    qDebug() << "m_gamedlg->addEachMoveSec2() = " << m_gamedlg->addEachMoveSec2();
    qDebug() << "m_gamedlg->startingPositionName() = " << m_gamedlg->startingPositionName();
    qDebug() << "m_gamedlg->startingPositionNumber() = " << m_gamedlg->startingPositionNumber();
    qDebug() << "m_gamedlg->maxMoves() = " << m_gamedlg->maxMoves();
    qDebug() << "m_gamedlg->consecutiveGames() = " << m_gamedlg->consecutiveGames();
    qDebug() << "m_gamedlg->isShowHumanInFront() = " << m_gamedlg->isShowHumanInFront();
    qDebug() << "m_gamedlg->isAutoSaveKifu() = " << m_gamedlg->isAutoSaveKifu();
    qDebug() << "m_gamedlg->isLoseOnTimeout() = " << m_gamedlg->isLoseOnTimeout();
    qDebug() << "m_gamedlg->isSwitchTurnEachGame() = " << m_gamedlg->isSwitchTurnEachGame();
}

// 対局者2をエンジン1で初期化して対局を開始する。
void MainWindow::initializeAndStartPlayer2WithEngine1()
{
    const int idx2 = m_startGameDialog->engineNumber2();
    const auto list = m_startGameDialog->getEngineList();
    const QString path2 = list.at(idx2).path;
    const QString name2 = m_startGameDialog->engineName2();

    const bool isHvE = (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);

    if (m_match) {
        if (isHvE) {
            // HvE でも m_usi1 を使う（後手エンジンでも実体は m_usi1）
            m_match->initializeAndStartEngineFor(MatchCoordinator::P1, path2, name2);
        } else {
            // EvE の後手は P2 へ
            m_match->initializeAndStartEngineFor(MatchCoordinator::P2, path2, name2);
        }
    }
}

// 対局者1をエンジン1で初期化して対局を開始する。
void MainWindow::initializeAndStartPlayer1WithEngine1()
{
    const int idx1 = m_startGameDialog->engineNumber1();
    const auto list = m_startGameDialog->getEngineList();
    const QString path1 = list.at(idx1).path;
    const QString name1 = m_startGameDialog->engineName1();

    const bool isHvE = (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);

    if (m_match) {
        if (isHvE) {
            // HvE は常に m_usi1 を使う
            m_match->initializeAndStartEngineFor(MatchCoordinator::P1, path1, name1);
        } else {
            // EvE のときは素直に先手へ
            m_match->initializeAndStartEngineFor(MatchCoordinator::P1, path1, name1);
        }
    }
}

// 対局者2をエンジン2で初期化して対局を開始する。
void MainWindow::initializeAndStartPlayer2WithEngine2()
{
    const int idx2 = m_startGameDialog->engineNumber2();
    const auto list = m_startGameDialog->getEngineList();
    const QString path2 = list.at(idx2).path;
    const QString name2 = m_startGameDialog->engineName2();

    const bool isHvE = (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);

    if (m_match) {
        if (isHvE) {
            // HvE でも m_usi1 を使う
            m_match->initializeAndStartEngineFor(MatchCoordinator::P1, path2, name2);
        } else {
            // EvE の後手は P2
            m_match->initializeAndStartEngineFor(MatchCoordinator::P2, path2, name2);
        }
    }
}

// 対局者1をエンジン2で初期化して対局を開始する。
void MainWindow::initializeAndStartPlayer1WithEngine2()
{
    const int idx1 = m_startGameDialog->engineNumber1();
    const auto list = m_startGameDialog->getEngineList();
    const QString path1 = list.at(idx1).path;
    const QString name1 = m_startGameDialog->engineName1();

    const bool isHvE = (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);

    if (m_match) {
        if (isHvE) {
            // HvE は常に m_usi1
            m_match->initializeAndStartEngineFor(MatchCoordinator::P1, path1, name1);
        } else {
            // EvE のときも先手は P1
            m_match->initializeAndStartEngineFor(MatchCoordinator::P1, path1, name1);
        }
    }
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

    // デバッグプリント
    printGameDialogSettings(m_startGameDialog);

    // エラーが発生した場合
    if (mode == PlayModeError) {
        // エラーメッセージを表示する。
        displayErrorMessage(tr("An error occurred in MainWindow::determinePlayMode. There is a mistake in the game options."));

        // デバッグプリント
        printGameDialogSettings(m_startGameDialog);
    }

    // 対局モードを返す。
    return mode;
}

// 対局モード（人間対エンジンなど）に応じて対局処理を開始する。
void MainWindow::startGameBasedOnMode()
{
    if (!m_match) return;

    // ★ USI "position ... moves" のベースを必ず用意（空だと " 7g7f" 事故になる）
    initializePositionStringsForMatch_();

    MatchCoordinator::StartOptions opt;
    opt.mode      = m_playMode;
    opt.sfenStart = m_startSfenStr;

    // エンジン情報（UIの選択値から）
    const auto engines = m_startGameDialog->getEngineList();

    const int idx1 = m_startGameDialog->engineNumber1();
    if (idx1 >= 0 && idx1 < engines.size()) {
        opt.engineName1 = m_startGameDialog->engineName1();
        opt.enginePath1 = engines.at(idx1).path;
    }

    const int idx2 = m_startGameDialog->engineNumber2();
    if (idx2 >= 0 && idx2 < engines.size()) {
        opt.engineName2 = m_startGameDialog->engineName2();
        opt.enginePath2 = engines.at(idx2).path;
    }

    // 先手がエンジンのモードだけ true（EvH／HandicapEvH）
    opt.engineIsP1 = (m_playMode == EvenEngineVsHuman
                   || m_playMode == HandicapEngineVsHuman);

    // 後手がエンジンのモードだけ true（HvE／HandicapHvE）
    opt.engineIsP2 = (m_playMode == EvenHumanVsEngine
                   || m_playMode == HandicapHumanVsEngine);

    // 単発エンジン（HvE/EvH/駒落ちの片側エンジン）か？
    const bool isSingleEngine =
        (m_playMode == EvenHumanVsEngine) ||
        (m_playMode == EvenEngineVsHuman) ||
        (m_playMode == HandicapHumanVsEngine) ||
        (m_playMode == HandicapEngineVsHuman);

    // ★ EvE のときだけ上下2段にする（HvH を含む“非 EvE”は上段のみ）
    const bool isEvE =
        (m_playMode == EvenEngineVsEngine) ||
        (m_playMode == HandicapEngineVsEngine);

    if (m_analysisTab) {
        m_analysisTab->setDualEngineVisible(isEvE);  // ← ここを !isSingleEngine から置換
    }

    // デバッグ出力
    qDebug().nospace()
        << "===============[ANALYSIS] singleEngine=" << (isSingleEngine?1:0)
        << " engineIsP1=" << (opt.engineIsP1?1:0)
        << " name1=" << opt.engineName1 << " path1=" << opt.enginePath1
        << " name2=" << opt.engineName2 << " path2=" << opt.enginePath2;

    // ここで「人間を手前」にそろえる（必要なときだけ1回反転）
    ensureHumanAtBottomIfApplicable_();

    // 司令塔へ構成/起動のみ委譲（startNewGame は既に済）
    m_match->configureAndStart(opt);

    // ★ 初手がエンジン手番なら、この場で初手エンジンを必ず起動する（先手・後手どちらでも）
    startInitialEngineMoveIfNeeded_();

    // …(エンジン起動/タブ再構築/チャート初期化など従来処理)…
    applyPendingEvalTrim_();   // ← 最後に必ず呼ぶ
    m_isResumeFromCurrent = false;     // 再開モード解除
}

// 棋譜解析結果を表示するためのテーブルビューを設定および表示する。
void MainWindow::displayAnalysisResults()
{
    // 既存モデルを破棄して作り直し（親=this を付けておくと自動破棄）
    if (m_analysisModel) {
        delete m_analysisModel;
        m_analysisModel = nullptr;
    }
    m_analysisModel = new KifuAnalysisListModel(this);

    // 解析結果用の簡易ダイアログ（メンバを持たず、その場で開いて閉じたら破棄）
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("棋譜解析結果"));
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);

    auto* view = new QTableView(dlg);
    view->setModel(m_analysisModel);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setAlternatingRowColors(true);
    view->verticalHeader()->setVisible(false);

    // --- 幅の自動調整方針 ---
    // 0列目：手数など → 内容に合わせる（最小幅は確保）
    // 1列目：指し手 → 内容に合わせる
    // 2列目：評価値 → 内容に合わせる
    // 3列目：読み/説明など長文 → 余白をすべて受ける（伸縮）
    auto* header = view->horizontalHeader();
    header->setStretchLastSection(true);                 // 最終列は常に余白を受ける
    header->setMinimumSectionSize(60);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::Stretch); // 最終列は伸びる

    // 固定幅指定は削除（setColumnWidth(...) は不要）

    // モデル更新時に一度だけ計測し直す（重い dataChanged 連打は避ける）
    auto doAutoSize = [view]() {
        // ResizeToContents の列を現在内容で再計測
        view->resizeColumnsToContents();
        // ただし最後の列は Stretch を維持
    };
    QObject::connect(m_analysisModel, &QAbstractItemModel::modelReset, view, doAutoSize);
    QObject::connect(m_analysisModel, &QAbstractItemModel::rowsInserted, view,
                     [doAutoSize](const QModelIndex&, int, int){ doAutoSize(); });

    // 初回表示時にも一度だけ実行（イベントループ戻し後）
    QTimer::singleShot(0, view, doAutoSize);

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(8,8,8,8);
    lay->addWidget(view);

    dlg->resize(1000, 600);
    dlg->setModal(false);
    dlg->setWindowModality(Qt::NonModal);
    dlg->show();
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

        try {
            // 本体は司令塔委譲版 analyzeGameRecord() を呼ぶ
            analyzeGameRecord();
        } catch (const std::exception& e) {
            // 例外時は解析を確実に後始末してからモデルを片付ける
            if (m_match) {
                m_match->handleBreakOffConsidaration();
            }
            if (m_analysisModel) {
                delete m_analysisModel;
                m_analysisModel = nullptr;
            }
            displayErrorMessage(e.what());
        }
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

    //begin
    qDebug() << "MainWindow::prepareDataCurrentPosition()";
    // 現在の手数
    qDebug() << "selPly = " << selPly;
    //end

    if (m_kifuRecordModel) {
        while (m_kifuRecordModel->rowCount() > (selPly + 1)) {
            m_kifuRecordModel->removeLastItem();
        }
    }

    //begin
    // m_kifuRevordModel のサイズ
    qDebug() << "m_kifuRecordModel->size() = " << (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);
    // m_kifuRecordModel の中身
    if (m_kifuRecordModel) {
        qDebug() << "m_kifuRecordModel->rowCount() = " << m_kifuRecordModel->rowCount();
        QModelIndex index;
        for (int i = 0; i < m_kifuRecordModel->rowCount(); i++) {
            index = m_kifuRecordModel->index(i, 0);
            const auto item = m_kifuRecordModel->data(index, Qt::DisplayRole);
            qDebug() << "m_kifuRecordModel[" << i << "] = " << item;
        }
    }
    //end

    if (m_gameMoves.size() > selPly) {
        while (m_gameMoves.size() > selPly) {
            m_gameMoves.removeLast();
        }
    } else {
        m_gameMoves.clear();
    }

    //begin
    qDebug() << "m_gameMoves.size() = " << m_gameMoves.size();
    for (int i = 0; i < m_gameMoves.size(); i++) {
        qDebug() << "m_gameMoves[" << i << "] = " << m_gameMoves.at(i);
    }

    if (m_positionStrList.size() > (selPly + 1)) {
        while (m_positionStrList.size() > (selPly + 1)) {
            m_positionStrList.removeLast();
        }
    } else {
        m_positionStrList.clear();
    }

    //begin
    qDebug() << "m_positionStrList.size() = " << m_positionStrList.size();
    for (int i = 0; i < m_positionStrList.size(); i++) {
        qDebug() << "m_positionStrList[" << i << "] = " << m_positionStrList.at(i);
    }
    //end


    if (m_sfenRecord) {
        if (m_sfenRecord->size() > (selPly + 1)) {
            while (m_sfenRecord->size() > (selPly + 1)) {
                m_sfenRecord->removeLast();
            }
        } else {
            // 既に selPly+1 以下なら何もしない（クリアしない）
        }
    }

    //begin
    qDebug() << "m_sfenRecord->size() = " << (m_sfenRecord ? m_sfenRecord->size() : -1);
    for (int i = 0; i < (m_sfenRecord ? m_sfenRecord->size() : 0); i++) {
        qDebug() << "m_sfenRecord[" << i << "] = " << m_sfenRecord->at(i);
    }
    //end

    // 評価値グラフを selPly まで巻き戻す ← ★ これを追加
    //trimEvalChartForResume_(selPly);

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

    //begin
    qDebug() << "m_resumeSfenStr = " << m_resumeSfenStr;
    qDebug() << "m_startSfenStr = " << m_startSfenStr;
    //end

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

// 現在の手番を設定する。
void MainWindow::setCurrentTurn()
{
    // 現在の手番が先手あるいは下手の場合
    if (m_shogiView->board()->currentPlayer() == "b") {
        // 現在の手番を対局者1に設定する。
        m_gameController->setCurrentPlayer(ShogiGameController::Player1);
    }
    // 現在の手番が後手あるいは上手の場合
    else if (m_shogiView->board()->currentPlayer() == "w") {
        // 現在の手番を対局者2に設定する。
        m_gameController->setCurrentPlayer(ShogiGameController::Player2);
    }
}

// 対局を開始する。
void MainWindow::initializeGame()
{
    //begin
    // m_kifuRecordModel の中身
    qDebug() << "MainWindow::initializeGame()";
    if (m_kifuRecordModel) {
        qDebug() << "m_kifuRecordModel->rowCount() = " << m_kifuRecordModel->rowCount();
        QModelIndex index;
        for (int i = 0; i < m_kifuRecordModel->rowCount(); i++) {
            index = m_kifuRecordModel->index(i, 0);
            const auto item = m_kifuRecordModel->data(index, Qt::DisplayRole);
            qDebug() << "m_kifuRecordModel[" << i << "] = " << item;
        }
    }
    //end

    m_errorOccurred = false;
    if (m_shogiView) m_shogiView->setErrorOccurred(false);

    m_startGameDialog = new StartGameDialog;

    if (m_startGameDialog->exec() == QDialog::Accepted) {
        setReplayMode(false);

        // ★ 先に取得しておく（これが肝）
        const int startingPositionNumber = m_startGameDialog->startingPositionNumber();
        qDebug() << "startingPositionNumber:" << startingPositionNumber;

        m_gameCount++;

        // ★ 2回目以降の対局でも、「現局面から」はフルリセットしない
        if (m_gameCount > 1) {
            if (startingPositionNumber > 0) {
                resetToInitialState();            // 既存のフルリセット
            }
        }

        //begin
        qDebug() << "check point2 MainWindow::initializeGame()";
        if (m_kifuRecordModel) {
            qDebug() << "m_kifuRecordModel->rowCount() = " << m_kifuRecordModel->rowCount();
            QModelIndex index;
            for (int i = 0; i < m_kifuRecordModel->rowCount(); i++) {
                index = m_kifuRecordModel->index(i, 0);
                const auto item = m_kifuRecordModel->data(index, Qt::DisplayRole);
                qDebug() << "m_kifuRecordModel[" << i << "] = " << item;
            }
        }
        //end

        // 対局オプションなど
        setRemainingTimeAndCountDown();
        getOptionFromStartGameDialog();

        ensureClockReady_();

        if (startingPositionNumber == 0) {
            // 現在局面から開始：ここで「選択手まで残して末尾だけ切る」
            prepareDataCurrentPosition();
        } else {
            try {
                prepareInitialPosition();
            } catch (const std::exception& e) {
                displayErrorMessage(e.what());
                delete m_startGameDialog;
                return;
            }
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

        try {
            startGameBasedOnMode();
        } catch (const std::exception& e) {
            displayErrorMessage(e.what());
        }
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
    // ファイル選択ダイアログを表示し、選択されたファイルのパスを取得する。
    QString filePath = QFileDialog::getOpenFileName(this, tr("KIFファイルを開く"), "", tr("KIF Files (*.kif *.kifu)"));

    // ユーザーがファイルを選択した場合（キャンセルしなかった場合）、ファイルを開く。
    if (!filePath.isEmpty()) {
        loadKifuFromFile(filePath);
    }
}

// ファイルからテキストの行を読み込み、QStringListとして返す。
QStringList MainWindow::loadTextLinesFromFile(const QString& filePath)
{
    // テキスト行を格納するリスト
    QStringList textLines;

    // ファイルを開く
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        displayErrorMessage(tr("An error occurred in MainWindow::loadTextLinesFromFile. Could not open the file: %1.").arg(filePath));
        qDebug() << "ファイルを開けませんでした：" << filePath;

        // 空のリストを返す。
        return textLines;
    }

    // テキスト行を1行ずつ読み込む。
    QTextStream in(&file);

    while (!in.atEnd()) {
        textLines.append(in.readLine());
    }

    file.close();

    // 読み込んだテキスト行を返す。
    return textLines;
}

// ---- 無名名前空間: 共有ユーティリティ ----
namespace {

inline bool isTerminalPretty(const QString& s) {
    static const QStringList keywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    for (const auto& kw : keywords)
        if (s.contains(kw)) return true;
    return false;
}

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

// ===================== 司令塔（Plan方式専用） =====================
void MainWindow::loadKifuFromFile(const QString& filePath)
{
    // --- IN ログ ---
    qDebug().noquote() << "[MAIN] loadKifuFromFile IN file=" << filePath;

    // ★ ロード中フラグ（applyResolvedRowAndSelect 等の分岐更新を抑止）
    m_loadingKifu = true;

    setReplayMode(true);

    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    const QString initialSfen = prepareInitialSfen(filePath, teaiLabel);

    // 2) 解析（本譜＋分岐＋コメント）を一括取得
    KifParseResult res;
    QString parseWarn;
    KifToSfenConverter::parseWithVariations(filePath, res, &parseWarn);

    // 先手/後手名などヘッダ反映
    {
        const QList<KifGameInfoItem> infoItems = KifToSfenConverter::extractGameInfo(filePath);
        populateGameInfo(infoItems);
        applyPlayersFromGameInfo(infoItems);
    }

    // 本譜（表示／USI）
    const QList<KifDisplayItem>& disp = res.mainline.disp;
    m_usiMoves = res.mainline.usiMoves;

    // 終局/中断判定（見た目文字列で簡易判定）
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    auto isTerminalPretty = [&](const QString& s)->bool {
        for (const auto& kw : kTerminalKeywords) if (s.contains(kw)) return true;
        return false;
    };
    const bool hasTerminal = (!disp.isEmpty() && isTerminalPretty(disp.back().prettyMove));

    if (m_usiMoves.isEmpty() && !hasTerminal && disp.isEmpty()) {
        QMessageBox::warning(this, tr("読み込み失敗"),
                             tr("%1 から指し手を取得できませんでした。").arg(filePath));
        qDebug().noquote() << "[MAIN] loadKifuFromFile OUT (no moves)";
        m_loadingKifu = false; // 早期return時も必ず解除
        return;
    }

    // 3) 本譜の SFEN 列と m_gameMoves を再構築
    rebuildSfenRecord(initialSfen, m_usiMoves, hasTerminal);
    rebuildGameMoves(initialSfen, m_usiMoves);

    // 3.5) USI position コマンド列を構築（0..N）
    //     initialSfen は prepareInitialSfen() が返す手合い込みの SFEN
    //     m_usiMoves は 1..N の USI 文字列（"7g7f" 等）
    m_positionStrList.clear();
    m_positionStrList.reserve(m_usiMoves.size() + 1);

    const QString base = QStringLiteral("position sfen %1").arg(initialSfen);
    m_positionStrList.push_back(base);  // 0手目：moves なし

    QStringList acc; // 先頭からの累積
    acc.reserve(m_usiMoves.size());
    for (int i = 0; i < m_usiMoves.size(); ++i) {
        acc.push_back(m_usiMoves.at(i));
        // i+1 手目：先頭から i+1 個の moves を連結
        m_positionStrList.push_back(base + QStringLiteral(" moves ") + acc.join(' '));
    }

    // （任意）ログで確認
    qDebug().noquote() << "[USI] position list built. count=" << m_positionStrList.size();
    if (!m_positionStrList.isEmpty()) {
        qDebug().noquote() << "[USI] pos[0]=" << m_positionStrList.first();
        if (m_positionStrList.size() > 1)
            qDebug().noquote() << "[USI] pos[1]=" << m_positionStrList.at(1);
    }

    // 4) 棋譜表示へ反映（本譜）
    displayGameRecord(disp);

    // 5) 本譜スナップショットを保持（以降の解決・描画に使用）
    m_dispMain = disp;          // 表示列（1..N）
    m_sfenMain = *m_sfenRecord; // 0..N の局面列
    m_gmMain   = m_gameMoves;   // 1..N のUSIムーブ

    // 6) 変化を取りまとめ（必要に応じて保持：Plan生成やツリー表示では m_resolvedRows を主に使用）
    m_variationsByPly.clear();
    m_variationsSeq.clear();
    for (const KifVariation& kv : res.variations) {
        KifLine L = kv.line;
        L.startPly = kv.startPly;         // “その変化が始まる絶対手数（1-origin）”
        if (L.disp.isEmpty()) continue;
        m_variationsByPly[L.startPly].push_back(L);
        m_variationsSeq.push_back(L);     // 入力順（KIF出現順）を保持
    }

    // 7) 棋譜テーブルの初期選択（開始局面を選択）
    if (m_recordPane && m_recordPane->kifuView()) {
        QTableView* view = m_recordPane->kifuView();
        if (view->model() && view->model()->rowCount() > 0) {
            const QModelIndex idx0 = view->model()->index(0, 0);
            if (view->selectionModel()) {
                view->selectionModel()->setCurrentIndex(
                    idx0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            view->scrollTo(idx0, QAbstractItemView::PositionAtTop);
        }
    }

    // 8) 解決行を1本（本譜のみ）作成 → 0手適用
    {
        m_resolvedRows.clear();

        ResolvedRow r;
        r.startPly = 1;
        r.parent   = -1;           // ★本譜
        r.disp     = disp;          // 1..N
        r.sfen     = *m_sfenRecord; // 0..N
        r.gm       = m_gameMoves;   // 1..N
        r.varIndex = -1;            // 本譜

        m_resolvedRows.push_back(r);
        m_activeResolvedRow = 0;
        m_activePly         = 0;

        // apply 内では m_loadingKifu を見て分岐候補の更新を抑止
        applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/0);
    }

    // 9) UIの整合
    enableArrowButtons();
    logImportSummary(filePath, m_usiMoves, disp, teaiLabel, parseWarn, QString());

    // 10) 解決済み行を構築（親探索規則で親子関係を決定）
    buildResolvedLinesAfterLoad();

    // 11) 分岐レポート → Plan 構築（Plan方式の基礎データ）
    dumpBranchSplitReport();
    buildBranchCandidateDisplayPlan();
    dumpBranchCandidateDisplayPlan();

    // 12) 分岐ツリーへ供給（黄色ハイライトは applyResolvedRowAndSelect 内で同期）
    if (m_analysisTab) {
        QVector<EngineAnalysisTab::ResolvedRowLite> rows;
        rows.reserve(m_resolvedRows.size());
        for (const auto& r : m_resolvedRows) {
            EngineAnalysisTab::ResolvedRowLite x;
            x.startPly = r.startPly;
            x.disp     = r.disp;
            x.sfen     = r.sfen;
            rows.push_back(std::move(x));
        }
        m_analysisTab->setBranchTreeRows(rows);
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }

    // 13) VariationEngine への投入（他機能で必要な可能性があるため保持）
    if (!m_varEngine) m_varEngine = std::make_unique<KifuVariationEngine>();
    {
        QVector<UsiMove> usiMain;
        usiMain.reserve(m_usiMoves.size());
        for (const auto& u : m_usiMoves) usiMain.push_back(UsiMove(u));
        m_varEngine->ingest(res, m_sfenMain, usiMain, m_dispMain);
    }

    // ←ここで SFEN を各行に流し込む
    ensureResolvedRowsHaveFullSfen();
    // ←そして表を出す
    dumpAllRowsSfenTable();

    ensureResolvedRowsHaveFullGameMoves();

    dumpAllLinesGameMoves();

    // 14) （Plan方式化に伴い）WL 構築や従来の候補再計算は廃止
    //     rebuildBranchWhitelist();                         // ← 削除
    //     m_branchCtl->refreshCandidatesForPly(...);        // ← 呼ばない

    // 15) ブランチ候補ワイヤリング（planActivated -> applyResolvedRowAndSelect）
    if (!m_branchCtl) {
        setupBranchCandidatesWiring_(); // 内部で planActivated の connect を済ませる
    }
    if (m_kifuBranchModel) {
        // 起動直後は候補を出さない（0手目）：モデルクリア＆ビュー非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
    }

    // ★ ロード完了 → 抑止解除
    m_loadingKifu = false;

    // 16) ツリーは読み込み後ロック（ユーザ操作で枝を生やさない）
    m_branchTreeLocked = true;
    qDebug() << "[BRANCH] tree locked after load";

    qDebug().noquote() << "[MAIN] loadKifuFromFile OUT";
}

// ===================== ヘルパ実装 =====================

QString MainWindow::prepareInitialSfen(const QString& filePath, QString& teaiLabel) const
{
    const QString sfen = KifToSfenConverter::detectInitialSfenFromFile(filePath, &teaiLabel);
    return sfen.isEmpty()
        ? QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
        : sfen;
}

QList<KifDisplayItem>
MainWindow::parseDisplayMovesAndDetectTerminal(const QString& filePath,
                                               bool& hasTerminal,
                                               QString* warn) const
{
    QString w;
    const QList<KifDisplayItem> disp =
        KifToSfenConverter::extractMovesWithTimes(filePath, &w);
    if (warn) *warn = w;
    hasTerminal = (!disp.isEmpty() && isTerminalPretty(disp.back().prettyMove));
    return disp;
}

QStringList MainWindow::convertKifToUsiMoves(const QString& filePath, QString* warn) const
{
    KifToSfenConverter conv;
    QString w;
    const QStringList moves = conv.convertFile(filePath, &w);
    if (warn) *warn = w;
    return moves;
}

void MainWindow::rebuildSfenRecord(const QString& initialSfen,
                                   const QStringList& usiMoves,
                                   bool hasTerminal)
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

void MainWindow::rebuildGameMoves(const QString& initialSfen,
                                  const QStringList& usiMoves)
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

void MainWindow::logImportSummary(const QString& filePath,
                                  const QStringList& usiMoves,
                                  const QList<KifDisplayItem>& disp,
                                  const QString& teaiLabel,
                                  const QString& warnParse,
                                  const QString& warnConvert) const
{
    if (!warnParse.isEmpty())
        qWarning().noquote() << "[KIF parse warnings]\n" << warnParse.trimmed();
    if (!warnConvert.isEmpty())
        qWarning().noquote() << "[KIF convert warnings]\n" << warnConvert.trimmed();

    qDebug().noquote() << QStringLiteral("KIF読込: %1手（%2）")
                          .arg(usiMoves.size())
                          .arg(QFileInfo(filePath).fileName());
    for (int i = 0; i < qMin(5, usiMoves.size()); ++i) {
        qDebug().noquote() << QStringLiteral("USI[%1]: %2")
                              .arg(i + 1)
                              .arg(usiMoves.at(i));
    }

    qDebug().noquote() << QStringLiteral("手合割: %1")
                          .arg(teaiLabel.isEmpty()
                                   ? QStringLiteral("平手(既定)")
                                   : teaiLabel);

    // 本譜（表示用）。コメントがあれば直後に出力。
    for (const auto& it : disp) {
        const QString time = it.timeText.isEmpty()
                                 ? QStringLiteral("00:00/00:00:00")
                                 : it.timeText;
        qDebug().noquote() << QStringLiteral("「%1」「%2」").arg(it.prettyMove, time);
        if (!it.comment.trimmed().isEmpty()) {
            qDebug().noquote() << QStringLiteral("  └ コメント: %1")
                                  .arg(it.comment.trimmed());
        }
    }

    // SFEN（抜粋）
    if (m_sfenRecord) {
        for (int i = 0; i < qMin(12, m_sfenRecord->size()); ++i) {
            qDebug().noquote() << QStringLiteral("%1) %2")
                                  .arg(i)
                                  .arg(m_sfenRecord->at(i));
        }
    }

    // m_gameMoves（従来通り）
    qDebug() << "m_gameMoves size:" << m_gameMoves.size();
    for (int i = 0; i < m_gameMoves.size(); ++i) {
        qDebug().noquote() << QString("%1) ").arg(i + 1) << m_gameMoves[i];
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

static QString toHtmlWithLinks(const QString& plain)
{
    // 改行などを <br> に変換してHTML化
    QString html = Qt::convertFromPlainText(plain, Qt::WhiteSpaceNormal);

    // http/https のURLをリンクに
    static const QRegularExpression re(QStringLiteral(R"((https?://[^\s<>"']+))"));
    html.replace(re, QStringLiteral(R"(<a href="\1">\1</a>)"));
    return html;
}

void MainWindow::updateBranchTextForRow(int row)
{
    QString raw;
    if (row >= 0 && row < m_commentsByRow.size())
        raw = m_commentsByRow.at(row).trimmed();

    // HTMLを使うならここで makeBranchHtml(raw) に置換
    const QString toShow = raw.isEmpty() ? tr("コメントなし") : raw;
    broadcastComment(toShow, /*asHtml=*/true);
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

    // ※ 本関数はUI反映のみ。考慮時間の確定 / 秒読み・加算の適用は
    //    MatchCoordinator 側で行い、その結果（elapsedTime 等）だけを受け取って表示する。
}

// bestmove resignコマンドを受信した場合の終了処理を行う。
void MainWindow::processResignCommand()
{
    // エンジンに対してgameover winコマンドを送る（EvE のみ）
    if ((m_playMode == EvenEngineVsEngine) || (m_playMode == HandicapEngineVsEngine)) {
        if (m_gameController && m_gameController->currentPlayer() == ShogiGameController::Player1) {
            // いま先手番＝後手が「投了」した → 先手勝ち
            if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();
            m_lastMove = QStringLiteral("△投了");
        } else {
            // いま後手番＝先手が「投了」した → 後手勝ち
            if (m_usi2) m_usi2->sendGameOverWinAndQuitCommands();
            m_lastMove = QStringLiteral("▲投了");
        }
    }

    // 矢印ボタンの再有効化
    enableArrowButtons();

    // 棋譜テーブルの選択モードを復帰（RecordPane 経由）
    if (m_recordPane) {
        if (auto* view = m_recordPane->kifuView()) {
            view->setSelectionMode(QAbstractItemView::SingleSelection);
        }
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

// 詰み探索を開始する。
void MainWindow::startTsumiSearch()
{
    // 対局モードを棋譜解析モードに設定する。
    m_playMode = TsumiSearchMode;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを削除する。
    if (m_usi1 != nullptr) delete m_usi1;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを生成する。
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode,this);

    connect(m_usi1, &Usi::bestMoveResignReceived, this, &MainWindow::processResignCommand);

    // GUIに登録された将棋エンジン番号を取得する。
    int engineNumber1 = m_tsumeShogiSearchDialog->getEngineNumber();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile1 = m_tsumeShogiSearchDialog->getEngineList().at(engineNumber1).path;

    // 将棋エンジン名を取得する。
    QString engineName1 = m_tsumeShogiSearchDialog->getEngineName();

    m_usi1->setLogIdentity("[E1]", "P1", engineName1);

    if (m_usi1) m_usi1->setSquelchResignLogging(false);

    try {
        // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
        m_usi1->initializeAndStartEngineCommunication(m_engineFile1, engineName1);
    } catch (const std::exception& e) {
        // エラーを再スローし、エラーメッセージを表示する。
        throw;
    }
}

// positionコマンド文字列を生成し、リストに格納する。
void MainWindow::createPositionCommands()
{
    // 開始局面文字列をSFEN形式でセットする。（sfen文字あり）
    m_startPosStr = "sfen " + m_sfenRecord->at(0);

    // positionコマンド文字列の生成
    m_positionStr1 = "position " + m_startPosStr + " moves";

    // positionコマンド文字列リストをクリアする。
    m_positionStrList.clear();

    // 対局開始の初期状態のpositionコマンド文字列をリストに追加する。
    m_positionStrList.append(m_positionStr1);

    // 初手から最終手までの指し手までループする。
    for (int moveIndex = 1; moveIndex < m_gameMoves.size(); ++moveIndex) {
        // 次の手番のデータをセットする。
        // 移動元
        QPoint from = m_gameMoves.at(moveIndex - 1).fromSquare;

        // 移動先
        QPoint to = m_gameMoves.at(moveIndex - 1).toSquare;

        // 成るかどうかのフラグ
        bool isPromotion = m_gameMoves.at(moveIndex - 1).isPromotion;

        // 移動元のマスの筋と段の番号を取得する。
        int fromX = from.x() + 1;
        int fromY = from.y() + 1;
        int toX = to.x() + 1;
        int toY = to.y() + 1;

        // 移動元の筋が盤面内の場合
        if ((fromX >= 1) && (fromX <= 9)) {
            m_positionStr1 += " " + QString::number(fromX) + m_usi1->rankToAlphabet(fromY);
        }
        // 移動元の筋が先手あるいは下手の持ち駒の場合
        else if (fromX == 10) {
            m_positionStr1 += " " + m_usi1->convertFirstPlayerPieceNumberToSymbol(fromY);
        }
        // 移動元の筋が後手あるいは上手の持ち駒の場合
        else if (fromX == 11) {
            m_positionStr1 += " " + m_usi1->convertSecondPlayerPieceNumberToSymbol(fromY);
        }

        // 移動先の筋と段の番号からpositionコマンド文字列を生成する。
        m_positionStr1 += QString::number(toX) + m_usi1->rankToAlphabet(toY);

        // 成った場合、文字列"+"を追加する。
        if (isPromotion) m_positionStr1 += "+";

        // 対局開始の初期状態のpositionコマンド文字列をリストに追加する。
        m_positionStrList.append(m_positionStr1);
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

    // 4) ループ上限（元コードのデバッグ制限 8 手を踏襲）
    const int totalMoves = qMin(m_positionStrList.size(), m_gameMoves.size());
    const int endIndex   = qMin(totalMoves, 8);  // ← 必要なら制限解除可

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

        m_match->startAnalysis(opt);

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

    // 6) 単発解析セッションの確実な後始末（quit→破棄）
    //    ※ MatchCoordinator 側の cleanup は ConsidarationMode 前提
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

// 局面編集を開始する（BIC対応版）
void MainWindow::beginPositionEditing()
{
    // いったん初期表示へ（盤面・棋譜・評価値などをクリーンに）
    resetToInitialState();

    // 指し手インデックス類をリセット
    m_currentMoveIndex = 0;
    m_totalMove        = 0;

    // いま表示している局面を SFEN として保持
    m_startSfenStr = parseStartPositionToSfen(m_currentSfenStr);

    // 局面編集フラグ ON（BIC の挙動にも効く）
    if (m_shogiView) {
        m_shogiView->setPositionEditMode(true);
        m_shogiView->setMouseClickMode(true);  // 編集中はクリック可
        m_shogiView->update();
    }

    // メニューの切替
    displayPositionEditMenu();

    //hideGameActions();

    // SFEN 列を初期化（0手＝開始局面のみ）
    if (m_sfenRecord) {
        m_sfenRecord->clear();
        m_sfenRecord->append(m_startSfenStr);
    }

    // 棋譜モデルを初期化（RecordPane 用）
    if (!m_kifuRecordModel) m_kifuRecordModel = new KifuRecordListModel(this);
    m_kifuRecordModel->clearAllItems();
    m_kifuRecordModel->appendItem(
        new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                        QStringLiteral("（１手 / 合計）"))
    );

    // 矢印ボタン無効化＆棋譜テーブルの選択禁止
    disableArrowButtons();
    if (m_recordPane) {
        if (auto* view = m_recordPane->kifuView()) {
            view->setSelectionMode(QAbstractItemView::NoSelection);
        }
    }

    // 手番を GameController に反映（盤の b/w に合わせる）
    if (m_shogiView && m_gameController) {
        if (m_shogiView->board()->currentPlayer() == "b") {
            m_gameController->setCurrentPlayer(ShogiGameController::Player1);
        } else if (m_shogiView->board()->currentPlayer() == "w") {
            m_gameController->setCurrentPlayer(ShogiGameController::Player2);
        }
    }

    // --- ここから BIC（BoardInteractionController）への切替点 ---
    // 未生成なら念のため配線
    if (!m_boardController) {
        setupBoardInteractionController();
    }

    // 編集モードにセットし、ハイライトはコントローラ経由で全消し
    if (m_boardController) {
        m_boardController->setMode(BoardInteractionController::Mode::Edit);
        m_boardController->clearAllHighlights();
    }

    // 編集用アクションの接続（重複を避けるため UniqueConnection）
    if (ui) {
        connect(ui->returnAllPiecesOnStand, &QAction::triggered,
                this, &MainWindow::resetPiecesToStand,
                Qt::UniqueConnection);

        connect(ui->turnaround, &QAction::triggered,
                this, &MainWindow::toggleEditSideToMove,
                Qt::UniqueConnection);

        connect(ui->flatHandInitialPosition, &QAction::triggered,
                this, &MainWindow::setStandardStartPosition,
                Qt::UniqueConnection);

        connect(ui->shogiProblemInitialPosition, &QAction::triggered,
                this, &MainWindow::setTsumeShogiStartPosition,
                Qt::UniqueConnection);

        connect(ui->reversal, &QAction::triggered,
                this, &MainWindow::onReverseTriggered,
                Qt::UniqueConnection);
    }
}

void MainWindow::onReverseTriggered() {
    if (m_match) m_match->flipBoard();
}

// 局面編集を終了した場合の処理を行う（BIC 対応版）
void MainWindow::finishPositionEditing()
{
    // 1) 編集結果の SFEN を反映
    if (m_sfenRecord) m_sfenRecord->clear();
    if (m_gameController) {
        m_gameController->updateSfenRecordAfterEdit(m_sfenRecord);
    }

    // 2) BIC を通常モードへ & ハイライトはコントローラ経由で全消去
    if (m_boardController) {
        m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
        m_boardController->clearAllHighlights();
    }

    // 3) 編集系アクションの切断（UniqueConnection で張っていたものだけ外す）
    if (ui) {
        disconnect(ui->returnAllPiecesOnStand,     &QAction::triggered, this, &MainWindow::resetPiecesToStand);
        disconnect(ui->turnaround,                 &QAction::triggered, this, &MainWindow::toggleEditSideToMove);
        disconnect(ui->flatHandInitialPosition,    &QAction::triggered, this, &MainWindow::setStandardStartPosition);
        disconnect(ui->shogiProblemInitialPosition,&QAction::triggered, this, &MainWindow::setTsumeShogiStartPosition);
        disconnect(ui->reversal,                   &QAction::triggered, this, &MainWindow::onReverseTriggered);
    }

    // 4) 編集メニューを閉じる
    hidePositionEditMenu();

    // 5) 編集フラグOFF & 盤クリックは一旦無効化（ゲーム開始時に再有効化）
    if (m_shogiView) {
        m_shogiView->setPositionEditMode(false);
        m_shogiView->setMouseClickMode(false);
        m_shogiView->update();
    }

    // 6) （必要なら）ドラッグ終了シグナルの接続はそのままでも問題なし
    //    ※ 既に UniqueConnection で接続済みのためここで外す必要はありません。
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

    try {
        m_shogiView->shogiProblemInitialPosition();
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
    }
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void MainWindow::swapBoardSides()
{
    if (m_boardController)
        m_boardController->clearAllHighlights();

    m_shogiView->flipBoardSides();
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

// 棋譜をファイルに上書き保存する。
void MainWindow::overwriteKifuFile()
{
    // 保存棋譜ファイル名が空の場合
    // （まだファイル保存を行っていない場合）
    if (kifuSaveFileName.isEmpty()) {
        // 棋譜をファイルに保存する。
        saveKifuToFile();
    }
    // 保存棋譜ファイルが空でない場合（一度、棋譜ファイルに保存している場合）
    else {
        // 保存棋譜ファイルを開いて棋譜ファイルデータを書き込む。
        QFile file(kifuSaveFileName);

        // ファイルを開けない場合
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // エラーメッセージを表示する。
            displayErrorMessage(tr("An error occurred in MainWindow::overWriteKifuFile. Could not open the saved game file."));

            return;
        }

        // fileという名前のQFileオブジェクトに関連付けられたテキストストリームを作成する。
        QTextStream out(&file);

        // 棋譜ファイルデータを書き込む。
        for (int i = 0; i < m_kifuDataList.size(); i++) {
            out << m_kifuDataList.at(i) << "\n";
        }

        // ファイルを閉じる。
        file.close();
    }
}

// 棋譜をファイルに保存する。
void MainWindow::saveKifuToFile()
{
    // カレントディレクトリをホームディレクトリに設定する。
    QDir::setCurrent(QDir::homePath());

    // 保存ファイルのデフォルト名を作成する。
    makeDefaultSaveFileName();

    // 対局者名が指定されていない場合、ファイル名をuntitled.kifuに設定する。
    if (defaultSaveFileName == "_vs.kifu") {
       defaultSaveFileName = "untitled.kifu";
    }

    // ファイルダイアログでデフォルトファイル名を表示する。
    // 実際に保存するファイル名を取得する。
    kifuSaveFileName = QFileDialog::getSaveFileName(this, tr("Save File"), defaultSaveFileName, tr("Kif(*.kifu)"));

    // 保存ファイル名が空でない時、ファイルを開いて棋譜ファイルデータを書き込む。
    if (!kifuSaveFileName.isEmpty()) {
        // 保存ファイル名を設定する。
        QFile file(kifuSaveFileName);

        // ファイルを開けない場合
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // エラーメッセージを表示する。
            displayErrorMessage(tr("An error occurred in MainWindow::saveKifuFile. Could not save the game file."));

            return;
        }

        // fileという名前のQFileオブジェクトに関連付けられたテキストストリームを作成する。
        QTextStream out(&file);

        // 棋譜ファイルデータを書き込む。
        for (int i = 0; i < m_kifuDataList.size(); i++) {
            out << m_kifuDataList.at(i) << "\n";
        }

        // ファイルを閉じる。
        file.close();
    }

    // カレントディレクトリをShogiBoardQの実行ファイルのあるディレクトリに設定する。
    QDir::setCurrent(QApplication::applicationDirPath());
}

// 保存ファイルのデフォルト名を作成する。
void MainWindow::makeDefaultSaveFileName()
{
    QDateTime dateTime = QDateTime::currentDateTime();

    // 日付から保存ファイル名を設定する。
    defaultSaveFileName = dateTime.toString("yyyyMMdd_hhmmss");

    // 対局モードに応じて対局者名とファイル拡張子を追加する。
    switch (m_playMode) {
    // Player1: Human, Player2: Human
    case HumanVsHuman:
        defaultSaveFileName += "_" + m_humanName1 + "vs" + m_humanName2 + ".kifu";
        break;

    // Player1: Human, Player2: USI Engine
    case EvenHumanVsEngine:
        defaultSaveFileName += "_" + m_humanName1 + "vs" + m_engineName2 + ".kifu";
        break;

    // Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:
        defaultSaveFileName += "_" + m_engineName1 + "vs" + m_humanName2 + ".kifu";
        break;

    // Player1: USI Engine, Player2: USI Engine
    case EvenEngineVsEngine:
        defaultSaveFileName += "_" + m_engineName1 + "vs" + m_engineName2 + ".kifu";
        break;

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        defaultSaveFileName += "_" + m_humanName1 + "vs" + m_engineName2 + ".kifu";
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        defaultSaveFileName += "_" + m_engineName1 + "vs" + m_humanName2 + ".kifu";
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine（上手）
    case HandicapEngineVsEngine:
        defaultSaveFileName += "_" + m_engineName1 + "vs" + m_engineName2 + ".kifu";
        break;

    // まだ対局を開始していない状態
    case NotStarted:

    // 解析モード
    case AnalysisMode:

    // 検討モード
    case ConsidarationMode:

    // 詰将棋探索モード
    case TsumiSearchMode:

    // エラー
    case PlayModeError:
        break;
    }
}

// 棋譜ファイルのヘッダー部分の対局者名を作成する。
void MainWindow::getPlayersName(QString& playersName1, QString& playersName2)
{
    // 対局モード
    switch (m_playMode) {
    // Player1: Human, Player2: Human
    case HumanVsHuman:
        playersName1 = "先手：" + m_humanName1;
        playersName2 = "後手：" + m_humanName2;
        break;

    // Player1: Human, Player2: USI Engine
    case EvenHumanVsEngine:
        playersName1 = "先手：" + m_humanName1;
        playersName2 = "後手：" + m_engineName2;
        break;

    // Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:
        playersName1 = "先手：" + m_engineName1;
        playersName2 = "後手：" + m_humanName2;
        break;

    // Player1: USI Engine, Player2: USI Engine
    case EvenEngineVsEngine:
        playersName1 = "先手：" + m_engineName1;
        playersName2 = "後手：" + m_engineName2;
        break;

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        playersName1 = "下手：" + m_humanName1;
        playersName2 = "上手：" + m_engineName2;
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        playersName1 = "下手：" + m_engineName1;
        playersName2 = "上手：" + m_humanName2;
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine（上手）
    case HandicapEngineVsEngine:
        playersName1 = "下手：" + m_engineName1;
        playersName2 = "上手：" + m_engineName2;
        break;

    // まだ対局を開始していない状態
    case NotStarted:

    // 解析モード
    case AnalysisMode:

    // 検討モード
    case ConsidarationMode:


    // 詰将棋探索モード
    case TsumiSearchMode:

    // エラー
    case PlayModeError:
        break;
    }
}

// 棋譜ファイルのヘッダー部分を作成する。
void MainWindow::makeKifuFileHeader()
{
    // 棋譜ファイルのヘッダー部分を作成する。
    m_kifuDataList.append("#KIF version=2.0 encoding=UTF-8");

    // 開始日時
    QDateTime dateTime = QDateTime::currentDateTime();
    m_kifuDataList.append("開始日時：" + dateTime.toString("yyyy/MM/dd hh:mm:ss"));

    // 手合割
    m_kifuDataList.append("手合割：" + m_startGameDialog->startingPositionName());

    // 対局者名
    QString playersName1;
    QString playersName2;

    // 対局者名を取得する。
    getPlayersName(playersName1, playersName2);

    // 先手　あるいは　下手
    m_kifuDataList.append(playersName1);

    // 後手　あるいは　上手
    m_kifuDataList.append(playersName2);

    // 手数、指し手、消費時間の文字列
    m_kifuDataList.append("手数----指手---------消費時間--");
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

ShogiGameController::Player MainWindow::humanPlayerSide() const
{
    // 本関数（Human vs Engine）では：
    // EvenHumanVsEngine / HandicapHumanVsEngine → Player1 が人間
    // HandicapEngineVsHuman → Player2 が人間
    if (m_playMode == HandicapEngineVsHuman) return ShogiGameController::Player2;
    return ShogiGameController::Player1;
}

bool MainWindow::isHumanTurn() const
{
    return m_gameController->currentPlayer() == humanPlayerSide();
}

// ★ 追加: 旗落ち時の終了処理（勝敗通知とクリーンアップ）
//   ここはプロジェクトの既存ハンドラに合わせてください。
//   例では: mover が時間切れ → 相手の勝ち。
//   Usiに直接 win/lose+quit を送る or 既存の handleEngineOne/TwoResignation を呼ぶ、のいずれか。
void MainWindow::handleFlagFallForMover(bool moverP1)
{
    qDebug().nospace()
        << "[GUI] Flag-fall: mover=" << (moverP1 ? "P1" : "P2");

    // 既存のハンドラ流儀に合わせる場合は、以下のどちらかを選択:
    // A) Usi 経由で gameover/quit を直接送る（分かりやすい）
    if (moverP1) {
        if (m_usi1) m_usi1->sendGameOverLoseAndQuitCommands(); // 時間切れ側
        if (m_usi2) m_usi2->sendGameOverWinAndQuitCommands();  // 勝ち側
    } else {
        if (m_usi2) m_usi2->sendGameOverLoseAndQuitCommands(); // 時間切れ側
        if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();  // 勝ち側
    }

    // B) 既存の「投了ハンドラ」に乗せる場合（プロジェクト命名に応じて置換）
    if (moverP1) {
        handleEngineOneResignation(); // P1が負け扱い
    } else {
        handleEngineTwoResignation(); // P2が負け扱い
    }

    // ゲーム終了へ
    // （必要に応じてUI閉じ処理やログ表示などを追加）
}

void MainWindow::stopClockAndSendGameOver(Winner w)
{
    m_shogiClock->stopClock();

    // Engine vs Engine のときは両方に通知
    if (m_playMode == EvenEngineVsEngine || m_playMode == HandicapEngineVsEngine) {
        if (w == Winner::P1) {
            m_usi1->sendGameOverWinAndQuitCommands();
            m_usi2->sendGameOverLoseAndQuitCommands();
        } else {
            m_usi1->sendGameOverLoseAndQuitCommands();
            m_usi2->sendGameOverWinAndQuitCommands();
        }
    } else {
        // Human が混ざるモードの保険（片側だけエンジンに送る）
        if (w == Winner::P1) {
            if (m_playMode == EvenEngineVsHuman || m_playMode == HandicapEngineVsHuman)
                m_usi1->sendGameOverWinAndQuitCommands();
            else if (m_playMode == EvenHumanVsEngine || m_playMode == HandicapHumanVsEngine)
                m_usi2->sendGameOverLoseAndQuitCommands();
        } else {
            if (m_playMode == EvenEngineVsHuman || m_playMode == HandicapEngineVsHuman)
                m_usi1->sendGameOverLoseAndQuitCommands();
            else if (m_playMode == EvenHumanVsEngine || m_playMode == HandicapHumanVsEngine)
                m_usi2->sendGameOverWinAndQuitCommands();
        }
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
                                .arg(fmt_mmss(considerMs))
                                .arg(fmt_hhmmss(totalUsedMs));

    // 1回だけ即時追記
    //begin
    qDebug().nospace() << "[KIFU] setGameOverMove appending '" << line << "'"
                       << " elapsed=" << elapsed;
    //end
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

// 互換ラッパ：既存の呼び出しを生かしたまま内部で新 API に委譲
void MainWindow::setResignationMove(bool isPlayerOneResigning)
{
    setGameOverMove(GameOverCause::Resignation, isPlayerOneResigning);
}

// 先手エンジンが投了
void MainWindow::onEngine1Resigns()
{
    // 司令塔へ委譲（終局処理・時計停止・USI通知・gameEnded 発火までを司令塔で実施）
    if (m_match) m_match->handleEngineResign(1);
}

// 後手エンジンが投了
void MainWindow::onEngine2Resigns()
{
    // 司令塔へ委譲
    if (m_match) m_match->handleEngineResign(2);
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

void MainWindow::addGameInfoTabIfMissing()
{
    ensureGameInfoTable();
    if (!m_tab) return;

    // Dock で表示していたら解除
    if (m_gameInfoDock && m_gameInfoDock->widget() == m_gameInfoTable) {
        m_gameInfoDock->setWidget(nullptr);
        m_gameInfoDock->deleteLater();
        m_gameInfoDock = nullptr;
    }

    // まだタブに無ければ追加
    if (m_tab->indexOf(m_gameInfoTable) == -1) {
        int anchorIdx = -1;

        // 1) EngineAnalysisTab（検討タブ）の直後に入れる
        if (m_analysisTab)
            anchorIdx = m_tab->indexOf(m_analysisTab);

        // 2) 念のため、タブタイトルで「コメント/Comments」を探してその直後に入れるフォールバック
        if (anchorIdx < 0) {
            for (int i = 0; i < m_tab->count(); ++i) {
                const QString t = m_tab->tabText(i);
                if (t.contains(tr("コメント")) || t.contains("Comments", Qt::CaseInsensitive)) {
                    anchorIdx = i;
                    break;
                }
            }
        }

        const int insertPos = (anchorIdx >= 0) ? anchorIdx + 1 : m_tab->count();
        m_tab->insertTab(insertPos, m_gameInfoTable, tr("対局情報"));
    }
}

void MainWindow::populateGameInfo(const QList<KifGameInfoItem>& items)
{
    ensureGameInfoTable();

    m_gameInfoTable->clearContents();
    m_gameInfoTable->setRowCount(items.size());

    for (int row = 0; row < items.size(); ++row) {
        const auto& it = items.at(row);
        auto *keyItem   = new QTableWidgetItem(it.key);
        auto *valueItem = new QTableWidgetItem(it.value);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        m_gameInfoTable->setItem(row, 0, keyItem);
        m_gameInfoTable->setItem(row, 1, valueItem);
    }

    m_gameInfoTable->resizeColumnToContents(0);

    // まだタブに載ってなければ、このタイミングで追加しておくと確実
    addGameInfoTabIfMissing();
}

QString MainWindow::findGameInfoValue(const QList<KifGameInfoItem>& items,
                                      const QStringList& keys) const
{
    for (const auto& it : items) {
        // KifGameInfoItem.key は「先手」「後手」等（末尾コロンは normalize 済み）
        if (keys.contains(it.key)) {
            const QString v = it.value.trimmed();
            if (!v.isEmpty()) return v;
        }
    }
    return QString();
}

void MainWindow::applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items)
{
    // 優先度：
    // 1) 先手／後手
    // 2) 下手／上手（→ 下手=Black, 上手=White）
    // 3) 先手省略名／後手省略名
    QString black = findGameInfoValue(items, { QStringLiteral("先手") });
    QString white = findGameInfoValue(items, { QStringLiteral("後手") });

    // 下手/上手にも対応（必要な側だけ補完）
    const QString shitate = findGameInfoValue(items, { QStringLiteral("下手") });
    const QString uwate   = findGameInfoValue(items, { QStringLiteral("上手") });

    if (black.isEmpty() && !shitate.isEmpty())
        black = shitate;                   // 下手 → Black
    if (white.isEmpty() && !uwate.isEmpty())
        white = uwate;                     // 上手 → White

    // 省略名でのフォールバック
    if (black.isEmpty())
        black = findGameInfoValue(items, { QStringLiteral("先手省略名") });
    if (white.isEmpty())
        white = findGameInfoValue(items, { QStringLiteral("後手省略名") });

    // 取得できた方だけ反映（既存表示を尊重）
    if (!black.isEmpty() && m_shogiView)
        m_shogiView->setBlackPlayerName(black);
    if (!white.isEmpty() && m_shogiView)
        m_shogiView->setWhitePlayerName(white);
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
    // 表示モデルが無ければ何もしない
    if (!m_kifuBranchModel) return;

    // 読み込み中はスキップ（ノイズ抑制）
    if (m_loadingKifu) {
        qDebug() << "[BRANCH] skip during loading (populateBranchListForPly)";
        return;
    }

    // アクティブ行の安全チェック
    if (m_activeResolvedRow < 0 || m_activeResolvedRow >= m_resolvedRows.size()) {
        qDebug() << "[BRANCH] populateBranchListForPly: active row invalid"
                 << " row=" << m_activeResolvedRow
                 << " rows=" << m_resolvedRows.size();
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_kifuBranchView
                                   ? m_kifuBranchView
                                   : (m_recordPane ? m_recordPane->branchView() : nullptr)) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }

    // この時点の文脈（どの手で候補を見せているか）を保持
    m_branchPlyContext = ply;

    // ★ Plan データのみで分岐候補欄を構築・表示
    //    表示/非表示の最終制御（単一候補かつ現在手と同一なら非表示 等）は
    //    showBranchCandidatesFromPlan() 側に集約している前提。
    showBranchCandidatesFromPlan(/*row*/m_activeResolvedRow, /*ply1*/ply);
}

// ヘルパ（0→1 始まり）
static inline QPoint toOne(const QPoint& z) { return QPoint(z.x() + 1, z.y() + 1); }

void MainWindow::syncBoardAndHighlightsAtRow(int ply1)
{
    //begin
    qDebug().nospace() << "[UI] syncBoardAndHighlightsAtRow ply=" << ply1;
    //end

    if (!m_sfenRecord) return;

    // ---- 0) 範囲正規化（0=初期局面, 1.. = 手数）----
    const int maxPly = m_sfenRecord->size() - 1;
    if (maxPly < 0) return;
    const int safePly = qBound(0, ply1, maxPly);

    // UI 側の現在手を同期（applySfenAtCurrentPly が参照）
    m_currentSelectedPly = safePly;
    m_currentMoveIndex   = safePly;

    // ---- 1) 盤面更新は “現在行の SFEN” 一本化 ----
    // 既存描画ルートを使って、m_sfenRecord[safePly] を反映
    if (m_boardController) {
        m_boardController->clearAllHighlights();
    }
    applySfenAtCurrentPly();   // (*m_sfenRecord)[m_currentSelectedPly] を描画

    //begin
    qDebug() << "checkpoint1";
    //end
    // ---- 2) ハイライトは “USI（gm）” 基準 ----
    // safePly=0 は初期局面で手なし
    if (!m_boardController || safePly <= 0) return;

    //begin
    qDebug() << "checkpoint2";
    //end

    const int mvIdx = safePly - 1;

    //begin
    qDebug().nospace() << "[UI] syncBoardAndHighlightsAtRow ply=" << safePly
                       << " mvIdx=" << mvIdx
                       << " gm.size=" << m_gameMoves.size();
    // m_gameMovesの中身を確認
    for (int i = 0; i < m_gameMoves.size(); ++i) {
        const ShogiMove& mv = m_gameMoves.at(i);
        qDebug().nospace() << "  gm[" << i << "] move=" << mv.movingPiece
                           << " from=" << mv.fromSquare
                           << " to=" << mv.toSquare;
    }
    //end

    if (mvIdx < 0 || mvIdx >= m_gameMoves.size()) return;

    //begin
    qDebug() << "checkpoint3";
    //end

    const ShogiMove& lastMove = m_gameMoves.at(mvIdx);

    //begin
    qDebug().nospace()
        << "@@@@@@@@@@@[UI] syncBoardAndHighlightsAtRow ply=" << safePly
        << " move=" << lastMove.movingPiece
        << " from=" << lastMove.fromSquare
        << " to=" << lastMove.toSquare;
    //end

    // ドロップ（持ち駒打ち）対策：無効座標を除外
    const bool hasFrom =
        (lastMove.fromSquare.x() >= 0 && lastMove.fromSquare.y() >= 0);

    const QPoint to1 = toOne(lastMove.toSquare);
    if (hasFrom) {
        const QPoint from1 = toOne(lastMove.fromSquare);

        //begin
        qDebug().nospace()
            << "[UI] syncBoardAndHighlightsAtRow ply=" << safePly
            << " move=" << lastMove.movingPiece
            << " from=" << from1
            << " to=" << to1;
        //end

        m_boardController->showMoveHighlights(from1, to1);
    } else {
        // 打つ手は移動元をハイライトしない（必要なら駒台強調などに差し替え）
        m_boardController->showMoveHighlights(QPoint(), to1);
    }

    m_activePly = m_currentSelectedPly;
}

// 現在表示用の棋譜列（disp）を使ってモデルを再構成し、selectPly 行を選択・同期する
void MainWindow::showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly)
{
    // いま表示中の棋譜列を保持（分岐⇄本譜の復帰で再利用）
    m_dispCurrent = disp;

    // （既存）モデルへ反映：ここで displayGameRecord(disp) が呼ばれ、
    // その過程で m_currentMoveIndex が 0 に戻る実装になっている
    displayGameRecord(disp);

    // ★ RecordPane 内のビューを使う
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view || !view->model()) return;

    // 行数（0 は「=== 開始局面 ===」、1..N が各手）
    const int rc  = view->model()->rowCount();
    const int row = qBound(0, selectPly, rc > 0 ? rc - 1 : 0);

    // 対象行を選択
    const QModelIndex idx = view->model()->index(row, 0);
    if (!idx.isValid()) return;

    if (auto* sel = view->selectionModel()) {
        sel->setCurrentIndex(idx,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        view->setCurrentIndex(idx);
    }
    view->scrollTo(idx, QAbstractItemView::PositionAtCenter);

    // ＝＝＝＝＝＝＝＝＝＝＝＝ ここが肝 ＝＝＝＝＝＝＝＝＝＝＝＝
    // displayGameRecord() が 0 に戻した “現在の手数” を、選択行へ復元する。
    // （row は 0=開始局面, 1..N=それぞれの手。よって「次の追記」は row+1 手目になる）
    m_currentSelectedPly = row;
    m_currentMoveIndex   = row;

    // 盤面・ハイライトも現在手に同期（applySfenAtCurrentPly は m_currentSelectedPly を参照）
    syncBoardAndHighlightsAtRow(row);
}

// 現在の手数（m_currentSelectedPly）に対応するSFENを盤面へ反映
void MainWindow::applySfenAtCurrentPly()
{
    qDebug().noquote() << "[UI] applySfenAtCurrentPly ply=" << m_currentSelectedPly;

    if (!m_sfenRecord || m_sfenRecord->isEmpty()) return;

    const int idx = qBound(0, m_currentSelectedPly, m_sfenRecord->size() - 1);
    QString sfen = m_sfenRecord->at(idx);

    //begin
    qDebug().nospace() << "[UI] applySfenAtCurrentPly applying SFEN: " << sfen;
    //end

    // 既存の盤面反映APIに合わせてください
    m_gameController->board()->setSfen(sfen);

    // 盤面データの適用後に View へ即時反映（矢印ナビでも盤が連動）
    if (m_shogiView && m_gameController) {
        // ShogiView に現在の Board を適用して再描画
        m_shogiView->applyBoardAndRender(m_gameController->board());
    } else if (m_shogiView) {
        // 最低限の再描画（Board 取得APIがない場合のフォールバック）
        m_shogiView->update();
    }
}

void MainWindow::applyVariationByKey(int startPly, int bucketIndex)
{
    // --- バリデーション ---
    auto it = m_variationsByPly.constFind(startPly);
    if (it == m_variationsByPly.cend()) return;
    const VariationBucket& bucket = it.value();
    if (bucket.isEmpty() || bucketIndex < 0 || bucketIndex >= bucket.size()) return;

    const KifLine& v = bucket[bucketIndex];
    if (v.disp.isEmpty()) return;

    // --- 表示用・SFEN・ゲームムーブを「本譜prefix + 分岐」で合成 ---
    QList<KifDisplayItem> mergedDisp;
    if (v.startPly > 1)
        mergedDisp = m_dispMain.mid(0, v.startPly - 1);
    mergedDisp += v.disp;

    QStringList mergedSfen = m_sfenMain.mid(0, v.startPly);  // [0..startPly-1] まで本譜
    mergedSfen += v.sfenList.mid(1);                         // 先頭(基底)は重複するので除外

    QVector<ShogiMove> mergedGm;
    if (v.startPly > 1)
        mergedGm = m_gmMain.mid(0, v.startPly - 1);
    mergedGm += v.gameMoves;

    // --- 現在の表示・局面ソースに反映 ---
    if (m_sfenRecord) *m_sfenRecord = mergedSfen;
    m_gameMoves = mergedGm;

    // 分岐の最初の手（= startPly手目）を選択
    const int selPly = v.startPly;
    showRecordAtPly(mergedDisp, /*selectPly=*/selPly);
    m_currentSelectedPly = selPly;

    // 棋譜欄の見た目も合わせる（明示的に選択＆スクロール）
    if (m_kifuRecordModel) {
        QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
        if (view) {
            const QModelIndex idx = m_kifuRecordModel->index(selPly, 0);
            if (idx.isValid()) {
                if (auto* sel = view->selectionModel()) {
                    sel->setCurrentIndex(idx,
                        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                } else {
                    view->setCurrentIndex(idx);
                }
                view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
            }
        }
    }

    // 盤・ハイライトを必ず即時同期（古いハイライト残り対策）
    syncBoardAndHighlightsAtRow(selPly);

    // 分岐候補欄を同手目で再表示（“本譜へ戻る”行付き）
    populateBranchListForPly(v.startPly);

    // （任意）コメント欄など行依存UIを更新したい場合
    updateBranchTextForRow(selPly);

    // 矢印ボタンの有効/無効を更新
    enableArrowButtons();
}

void MainWindow::onKifuPlySelected(int ply)
{
    if (ply < 0) return;

    // ★ 読み込み中はスキップ（ノイズ抑制）
    if (m_loadingKifu) {
        qDebug() << "[BRANCH] skip during loading (onKifuPlySelected)";
        return;
    }

    // ★ 現在のアクティブ行が有効かチェック（安全側）
    if (m_activeResolvedRow < 0 || m_activeResolvedRow >= m_resolvedRows.size()) {
        qDebug() << "[BRANCH] onKifuPlySelected: active row invalid"
                 << " row=" << m_activeResolvedRow
                 << " rows=" << m_resolvedRows.size();
        return;
    }

    // ★ コンテキスト記録（どの手を基準に候補を出しているか）
    m_branchPlyContext = ply;

    // ★ Plan データだけで分岐候補欄を構築・表示
    //   （単一候補かつ現在手と同一なら非表示、などの最終制御は
    //     showBranchCandidatesFromPlan() 側で実施）
    showBranchCandidatesFromPlan(/*row*/m_activeResolvedRow, /*ply1*/ply);
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

void MainWindow::buildResolvedLinesAfterLoad()
{
    //begin
    qDebug().noquote() << "--- buildResolvedLinesAfterLoad() called.";
    //end

    // 既存: Resolved行の構築や m_varEngine->ingest(), rebuildBranchWhitelist() の後
    m_branchTreeLocked = true;
    qDebug() << "[BRANCH] tree locked (after buildResolvedLinesAfterLoad)";

    // 本関数は「後勝ち」をやめ、各変化の“親”を
    // 直前→さらに前…と遡って「b(=この変化の startPly) > a(=先行変化の startPly)」
    // を満たす最初の変化行にする（無ければ本譜 row=0）という規則で構築する。

    m_resolvedRows.clear();

    // --- 行0 = 本譜 ---
    ResolvedRow mainRow;
    mainRow.startPly = 1;
    mainRow.parent   = -1;           // ★追加：本譜の親は -1 を明示
    mainRow.disp     = m_dispMain;   // 1..N（表示）
    mainRow.sfen     = m_sfenMain;   // 0..N（局面）
    mainRow.gm       = m_gmMain;     // 1..N（USIムーブ）
    mainRow.varIndex = -1;           // 本譜
    m_resolvedRows.push_back(mainRow);

    if (m_variationsSeq.isEmpty()) {
        qDebug().noquote() << "[RESOLVED] only mainline (no variations).";
        return;
    }

    // 変化 vi → 解決済み行 index の写像（empty の変化は -1）
    QVector<int> varToRowIndex(m_variationsSeq.size(), -1);

    // 親行を解決するローカルラムダ：
    // 直前→さらに前…へ遡り、startPly(prev) < startPly(cur) を満たす
    // 先行変化を見つけたら、その変化の“解決済み行 index”を返す。無ければ 0（本譜）。
    auto resolveParentRowIndex = [&](int vi)->int {
        const int b = qMax(1, m_variationsSeq.at(vi).startPly);
        for (int prev = vi - 1; prev >= 0; --prev) {
            const KifLine& p = m_variationsSeq.at(prev);
            if (p.disp.isEmpty()) continue;          // 空の変化は親候補にしない
            const int a = qMax(1, p.startPly);
            if (b > a && varToRowIndex.at(prev) >= 0) {
                return varToRowIndex.at(prev);        // その変化の“行 index”
            }
        }
        return 0; // 見つからなければ本譜
    };

    // --- 行1.. = 各変化 ---
    for (int vi = 0; vi < m_variationsSeq.size(); ++vi) {
        const KifLine& v = m_variationsSeq.at(vi);
        if (v.disp.isEmpty()) {
            varToRowIndex[vi] = -1;
            continue;
        }

        const int start = qMax(1, v.startPly);   // 1-origin
        const int parentRow = resolveParentRowIndex(vi);
        const ResolvedRow& base = m_resolvedRows.at(parentRow);

        // 1) プレフィクス（1..start-1）を“親行”から切り出し、足りない分は本譜で補完
        QList<KifDisplayItem> prefixDisp = base.disp;
        if (prefixDisp.size() > start - 1) prefixDisp.resize(start - 1);

        QVector<ShogiMove> prefixGm = base.gm;
        if (prefixGm.size() > start - 1) prefixGm.resize(start - 1);

        QStringList prefixSfen = base.sfen;
        if (prefixSfen.size() > start) prefixSfen.resize(start);

        // 親行が短い場合は本譜で延長（disp/gm/sfen を一致させる）
        while (prefixDisp.size() < start - 1 && prefixDisp.size() < m_dispMain.size()) {
            const int idx = prefixDisp.size(); // 0.. → 手数は idx+1
            prefixDisp.append(m_dispMain.at(idx));
        }
        while (prefixGm.size() < start - 1 && prefixGm.size() < m_gmMain.size()) {
            const int idx = prefixGm.size();
            prefixGm.append(m_gmMain.at(idx));
        }
        while (prefixSfen.size() < start && prefixSfen.size() < m_sfenMain.size()) {
            const int sidx = prefixSfen.size(); // 0.. → sfen は 0=初期,1=1手後...
            prefixSfen.append(m_sfenMain.at(sidx));
        }

        // 2) 変化本体を連結
        ResolvedRow row;
        row.startPly = start;
        row.parent   = parentRow;    // ★追加：親行 index を保存
        row.varIndex = vi;

        row.disp = prefixDisp;
        row.disp += v.disp;

        row.gm = prefixGm;
        row.gm += v.gameMoves;

        // v.sfenList は [0]=基底（start-1 手後）, [1..]=各手後 を想定
        row.sfen = prefixSfen;                  // 0..start-1 は既に parent→本譜で整合
        if (v.sfenList.size() >= 2) {
            row.sfen += v.sfenList.mid(1);      // start.. の各手後
        } else if (v.sfenList.size() == 1) {
            // 念のため：基底のみしか無い場合でも、prefix は揃っているので何もしない
        }

        // 追加
        m_resolvedRows.push_back(row);
        varToRowIndex[vi] = m_resolvedRows.size() - 1;
    }

    qDebug().noquote() << "[RESOLVED] lines=" << m_resolvedRows.size();

    auto varIdOf = [&](int varIndex)->int {
        if (varIndex < 0) return -1; // 本譜
        if (m_varEngine) {
            const int id = m_varEngine->varIdForSourceIndex(varIndex);
            return (id >= 0) ? id : (varIndex + 1); // フォールバック
        }
        return varIndex + 1; // 既存仕様に準拠（空行スキップ条件が一致している前提）
    };

    for (int i = 0; i < m_resolvedRows.size(); ++i) {
        const auto& r = m_resolvedRows[i];
        QStringList pm; pm.reserve(r.disp.size());
        for (const auto& d : r.disp) pm << d.prettyMove;

        const bool isMain = (i == 0);
        const QString label = isMain
                                  ? "Main"
                                  : QString("Var%1(id=%2)").arg(r.varIndex).arg(varIdOf(r.varIndex)); // ★ id 併記

        // ★修正：表示も保存済みの r.parent を使用（再計算しない）
        qDebug().noquote()
            << "  [" << label << "]"
            << " parent=" << r.parent
            << " start="  << r.startPly
            << " ndisp="  << r.disp.size()
            << " nsfen="  << r.sfen.size()
            << " seq=「 " << pm.join(" / ") << " 」";
    }
}

void MainWindow::applyResolvedRowAndSelect(int row, int selPly)
{
    if (row < 0 || row >= m_resolvedRows.size()) {
        qDebug() << "[APPLY] invalid row =" << row << " rows=" << m_resolvedRows.size();
        return;
    }

    // 行を確定
    m_activeResolvedRow = row;
    const auto& r = m_resolvedRows[row];

    // 盤面＆棋譜モデル（まず行データを丸ごと差し替え）
    *m_sfenRecord = r.sfen;   // 0..N のSFEN列
    m_gameMoves   = r.gm;     // 1..N のUSI列

    // ★ 手数を正規化して共有メンバへ反映（0=初期局面）
    const int maxPly = r.disp.size();
    m_activePly = qBound(0, selPly, maxPly);

    // 棋譜欄へ反映（モデル差し替え＋選択手へスクロール）
    showRecordAtPly(r.disp, m_activePly);
    m_currentSelectedPly = m_activePly;

    // RecordPane 経由で安全に選択・スクロール
    if (m_kifuRecordModel) {
        const QModelIndex idx = m_kifuRecordModel->index(m_activePly, 0);
        if (idx.isValid()) {
            if (m_recordPane) {
                if (QTableView* view = m_recordPane->kifuView()) {
                    if (view->model() == m_kifuRecordModel) {
                        view->setCurrentIndex(idx);
                        view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                    }
                }
            }
        }
    }

    // ======== 分岐候補の更新（Plan 方式に一本化）========
    {
        if (m_loadingKifu) {
            // ★ 読み込み中は分岐更新をスキップ（ノイズ抑制）
            qDebug() << "[BRANCH] skip during loading (applyResolvedRowAndSelect)";
            if (m_kifuBranchModel) {
                m_kifuBranchModel->clearBranchCandidates();
                m_kifuBranchModel->setHasBackToMainRow(false);
            }
            if (QTableView* view = m_kifuBranchView
                                        ? m_kifuBranchView
                                        : (m_recordPane ? m_recordPane->branchView() : nullptr)) {
                if (view) {
                    view->setVisible(false);
                    view->setEnabled(false);
                }
            }
        } else {
            // ★ ここだけで十分：Plan から分岐候補欄を構築・表示
            showBranchCandidatesFromPlan(/*row*/m_activeResolvedRow, /*ply1*/m_activePly);
        }
    }
    // ======== 差し替えここまで ========

    // 盤面ハイライト・矢印ボタン
    syncBoardAndHighlightsAtRow(m_activePly);
    enableArrowButtons();

    qDebug().noquote() << "[APPLY] row=" << row
                       << " ply=" << m_activePly
                       << " rows=" << m_resolvedRows.size()
                       << " dispSz=" << r.disp.size();

    // 分岐ツリー側の黄色ハイライト同期（EngineAnalysisTab に移譲）
    if (m_analysisTab) {
        m_analysisTab->highlightBranchTreeAt(/*row*/m_activeResolvedRow,
                                             /*ply*/m_activePly,
                                             /*centerOn*/false);
    }

    // ★ 棋譜欄の「分岐あり」行をオレンジでマーク
    updateKifuBranchMarkersForActiveRow();
}

void MainWindow::BranchRowDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
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

void MainWindow::ensureBranchRowDelegateInstalled()
{
    // RecordPane から棋譜ビューを取得
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view) return;

    if (!m_branchRowDelegate) {
        // デリゲートをビューの子として作成し、ビューに適用
        m_branchRowDelegate = new BranchRowDelegate(view);
        view->setItemDelegate(m_branchRowDelegate);
    } else {
        // 念のため親が違う場合は付け替え
        if (m_branchRowDelegate->parent() != view) {
            m_branchRowDelegate->setParent(view);
            view->setItemDelegate(m_branchRowDelegate);
        }
    }

    // 「分岐あり」マーカーの集合をデリゲートへ渡す
    m_branchRowDelegate->setMarkers(&m_branchablePlySet);
}

// ====== アクティブ行に対する「分岐あり手」の再計算 → ビュー再描画 ======
void MainWindow::updateKifuBranchMarkersForActiveRow()
{
    m_branchablePlySet.clear();

    // まずビュー参照を取得（nullでも安全に抜ける）
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);

    if (m_resolvedRows.isEmpty()) {
        if (view && view->viewport()) view->viewport()->update();
        return;
    }

    const int active = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[active];

    // r.disp: 1..N の手表示, r.sfen: 0..N の局面列
    for (int ply = 1; ply <= r.disp.size(); ++ply) {
        const int idx = ply - 1;                 // sfen は ply-1 が基底
        if (idx < 0 || idx >= r.sfen.size()) continue;

        const auto itPly = m_branchIndex.constFind(ply);
        if (itPly == m_branchIndex.constEnd()) continue;

        const QString base = r.sfen.at(idx);
        const auto itBase = itPly->constFind(base);
        if (itBase == itPly->constEnd()) continue;

        // 同じ手目に候補が2つ以上 → 分岐点としてマーキング
        if (itBase->size() >= 2) {
            m_branchablePlySet.insert(ply);
        }
    }

    // デリゲート装着（未装着ならここで装着）
    ensureBranchRowDelegateInstalled();

    // 再描画
    if (view && view->viewport()) view->viewport()->update();

#ifdef SHOGIBOARDQ_DEBUG_KIF
    if (!m_branchablePlySet.isEmpty()) {
        QStringList ls; for (int p : m_branchablePlySet) ls << QString::number(p);
        qDebug().noquote() << "[KIF-HL] branchable ply =" << ls;
    }
#endif
}

void MainWindow::resetGameFlags()
{
    // 司令塔に終局状態の全クリアを依頼（isOver / moveAppended / hasLast など一括）
    if (m_match) {
        m_match->clearGameOverState();
    }
}

void MainWindow::syncClockTurnAndEpoch()
{
    // 時計の手番表示と動作を現在手番に同期
    m_shogiClock->stopClock();
    const int cur = (m_gameController->currentPlayer() == ShogiGameController::Player2) ? 2 : 1;
    updateTurnStatus(cur);
    m_shogiClock->startClock();

    // エポック管理は司令塔へ一元化
    if (m_match) {
        m_match->resetTurnEpochs();
        const bool p1turn = (m_gameController->currentPlayer() == ShogiGameController::Player1);
        m_match->markTurnEpochNowFor(p1turn ? MatchCoordinator::P1 : MatchCoordinator::P2);
    }
}

void MainWindow::startMatchEpoch(const QString& tag)
{
    // 必要に応じてUTF-8へ（既存の const char* 版がUTF-8前提なら）
    const QByteArray utf8 = tag.toUtf8();
    startMatchEpoch(utf8.constData());  // 既存の const char* 版へ委譲（同期処理を想定）
}

void MainWindow::startMatchEpoch(const char* tag)
{
    ShogiUtils::startGameEpoch();
    qDebug() << "[ARBITER] epoch started (" << tag << ")";
}

void MainWindow::wireResignToArbiter(Usi* /*engine*/, bool /*asP1*/)
{
    if (m_match) {
        // 司令塔側で P1/P2 ともに張り替える
        m_match->wireResignSignals();
    }
}

void MainWindow::setConsiderationForJustMoved(qint64 thinkMs)
{
    // validateAndMove 後の currentPlayer() は「これから指す側」
    // ＝直前に指した側はその逆
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        m_shogiClock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
    } else {
        m_shogiClock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
    }
}

bool MainWindow::engineThinkApplyMove(Usi* engine,
                                      QString& positionStr,
                                      QString& ponderStr,
                                      QPoint* outFrom,
                                      QPoint* outTo)
{
    return (m_match
                ? m_match->engineThinkApplyMove(engine, positionStr, ponderStr, outFrom, outTo)
                : false);
}

void MainWindow::destroyEngine(Usi*& e)
{
    if (!m_match) return;

    // 司令塔が実エンジンを所有しているため、idx を解決して削除させる
    if (e == m_match->enginePtr(1)) {
        m_match->destroyEngine(1);
    } else if (e == m_match->enginePtr(2)) {
        m_match->destroyEngine(2);
    } else {
        // どちらでもない/すでに司令塔外なら念のため delete
        delete e;
    }
    e = nullptr;
}

// PvE：m_usi1 を用意して共通初期化
void MainWindow::initSingleEnginePvE(bool engineIsP1)
{
    destroyEngine(m_usi1);
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode, this);

    // 念のため存在していれば m_usi2 もクリア系だけ実施
    if (m_usi2) { m_usi2->resetResignNotified(); m_usi2->clearHardTimeout(); }

    m_usi1->resetResignNotified();
    m_usi1->clearHardTimeout();

    // 先手エンジンかどうかでアービタ接続
    wireResignToArbiter(m_usi1, engineIsP1);

    // === ここを修正：タグと設定名を側に合わせる ===
    const QString engineTag  = engineIsP1 ? QStringLiteral("[E1]") : QStringLiteral("[E2]");
    const QString playerTag  = engineIsP1 ? QStringLiteral("P1")   : QStringLiteral("P2");
    const QString cfgName    = engineIsP1
                                ? m_startGameDialog->engineName1()
                                : m_startGameDialog->engineName2();

    // ログ識別子（[E1]/[E2], P1/P2, 表示名）
    m_usi1->setLogIdentity(engineTag, playerTag, cfgName);
    m_usi1->setSquelchResignLogging(false);

    // ★ 追加：MatchCoordinator に新しいポインタを渡す
    if (m_match) m_match->updateUsiPtrs(m_usi1, m_usi2);

    resetGameFlags();
}

// いま手番が人間か？
bool MainWindow::isHumanTurnNow(bool engineIsP1) const
{
    const auto cur = m_gameController->currentPlayer();
    const auto engineSide = engineIsP1 ? ShogiGameController::Player1
                                       : ShogiGameController::Player2;
    return (cur != engineSide);
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
    qDebug() << "tab parents:" << m_tab->parent();
    qDebug() << "tabs in window:" << this->findChildren<QTabWidget*>().size();

    auto tabs = this->findChildren<QTabWidget*>();
    qDebug() << "QTabWidget count =" << tabs.size();
    for (auto* t : tabs) {
        qDebug() << " tab*" << t
                 << " objectName=" << t->objectName()
                 << " parent=" << t->parent();
    }
    qDebug() << " m_tab =" << m_tab;

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

    // 汎用ゲーム終了ダイアログ表示（title, message）
    d.hooks.showGameOverDialog = [this](const QString& title, const QString& message){
        QMessageBox::information(this,
                                 title.isEmpty() ? tr("Game Over") : title,
                                 tr("The game has ended. %1").arg(message));
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
    // ライブ追記（現局面から再開）で既に m_positionStr1 を正しく用意済みなら再初期化しない
    if (m_isLiveAppendMode && !m_positionStr1.isEmpty()) return;

    m_positionStr1.clear();
    m_positionPonder1.clear();
    if (!m_positionStrList.isEmpty()) m_positionStrList.clear();

    // 再開時：m_gameMoves には selPly までが残っている前提
    const bool hasHistory = !m_gameMoves.isEmpty();

    if (!m_startSfenStr.isEmpty() && m_startSfenStr != QStringLiteral("startpos")) {
        m_positionStr1 = hasHistory
            ? QStringLiteral("position sfen %1 moves").arg(m_startSfenStr)
            : QStringLiteral("position sfen %1").arg(m_startSfenStr); // 末尾に "moves" を付けない
    } else {
        m_positionStr1 = hasHistory
            ? QStringLiteral("position startpos moves")
            : QStringLiteral("position startpos");
    }

    // 既存着手を前置（USI表記化）
    if (hasHistory) {
        auto rankToAlphabet = [](int rank1){ return QChar('a' + (rank1 - 1)); };
        for (int i = 0; i < m_gameMoves.size(); ++i) {
            const auto& mv = m_gameMoves.at(i);
            const int fromX = mv.fromSquare.x() + 1;
            const int fromY = mv.fromSquare.y() + 1;
            const int toX   = mv.toSquare.x()   + 1;
            const int toY   = mv.toSquare.y()   + 1;

            // 打ち駒などの分岐はプロジェクトの既存実装に合わせて必要なら拡張
            QString usi;
            if (1 <= fromX && fromX <= 9 && 1 <= toX && toX <= 9) {
                usi = QString::number(fromX) + rankToAlphabet(fromY)
                    + QString::number(toX)   + rankToAlphabet(toY)
                    + (mv.isPromotion ? QStringLiteral("+") : QString());
            } else {
                // 打ち駒など（必要に応じて既存の Piece→USI 変換関数を使用）
                // usi = "...";
            }
            m_positionStr1 += QLatin1Char(' ');
            m_positionStr1 += usi;
        }
    }

    m_positionStrList.append(m_positionStr1);
}

// EvH（エンジンが先手）の初手を起動する（position ベース → go → bestmove を適用）
void MainWindow::startInitialEngineMoveEvH_()
{
    const bool engineIsP1 =
        (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);
    if (!engineIsP1) return;

    // 司令塔から主エンジンを取得（MainWindowでm_usi1は使わない）
    Usi* eng = (m_match ? m_match->primaryEngine() : nullptr);
    if (!eng) {
        qWarning() << "[EvH] engine instance not ready; skip first move.";
        return;
    }

    // ベースの "position ... moves" が空なら初期化（安全側）
    if (m_positionStr1.isEmpty()) {
        initializePositionStringsForMatch_();
    }

    // 念のため "position" が含まれているかチェック（ログで原因特定しやすくする）
    if (!m_positionStr1.startsWith("position ")) {
        qWarning() << "[EvH] position base is invalid:" << m_positionStr1;
        // 最低限、初期局面で開始
        m_positionStr1 = QStringLiteral("position startpos moves");
    }

    // エンジン初手の思考 → bestmove の from/to を受け取る
    QPoint eFrom(-1, -1), eTo(-1, -1);

    // 時間ルールは司令塔から取得
    const auto tc = m_match ? m_match->timeControl() : MatchCoordinator::TimeControl{};
    const int  engineByoyomiMs = tc.byoyomiMs1;   // 先手（エンジン）用
    const bool useByoyomi      = tc.useByoyomi;

    // USIへ渡す btime/wtime を“pre-add風”に整形して毎回その場で作成
    const auto [bTime, wTime] = currentBWTimesForUSI_();

    try {
        // USI "position ... moves" と "go ..." を一括で処理して bestmove を返す
        eng->handleEngineVsHumanOrEngineMatchCommunication(
            m_positionStr1,        // [in/out] position base に bestmove を連結してくる
            m_positionPonder1,     // [in/out] 使っていなければ空のままでOK
            eFrom, eTo,            // [out] エンジンの着手が返る
            engineByoyomiMs,
            bTime, wTime,          // ← m_bTime/m_wTime は使わず、その場生成
            tc.incMs1, tc.incMs2,  // 先手/後手の1手加算（フィッシャー用）
            useByoyomi
            );
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
        return;
    }

    // 返ってきた bestmove を盤へ適用
    bool ok = false;
    try {
        ok = m_gameController->validateAndMove(
            eFrom, eTo, m_lastMove, m_playMode,
            m_currentMoveIndex, m_sfenRecord, m_gameMoves
            );
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
        return;
    }
    if (!ok) return;

    // ハイライト・思考時間の反映
    if (m_boardController)
        m_boardController->showMoveHighlights(eFrom, eTo);

    const qint64 thinkMs = eng->lastBestmoveElapsedMs();
    // この直後は人間手番＝直前に指したのは P1（エンジン）
    if (m_shogiClock)
        m_shogiClock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));

    // UI更新と履歴
    updateTurnAndTimekeepingDisplay();
    m_positionStrList.append(m_positionStr1);
    redrawEngine1EvaluationGraph();
    pumpUi();

    // 次は人間手番：クリック受付と人間用タイマー
    const bool gameOver = (m_match && m_match->gameOverState().isOver);
    if (!gameOver) {
        if (m_shogiView) m_shogiView->setMouseClickMode(true);
        QTimer::singleShot(0, this, [this]{
            if (m_match) m_match->armHumanTimerIfNeeded();
        });
    }
}


// 駒落ち（"w" 開始）など、開始手番が後手で、後手がエンジンのときに初手を自動で指させる。
// 先手エンジン（EvH）の場合もここから既存の startInitialEngineMoveEvH_() を呼ぶ。
void MainWindow::startInitialEngineMoveIfNeeded_()
{
    if (!m_match || !m_gameController) return;

    const bool engineIsP1 =
        (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);
    const bool engineIsP2 =
        (m_playMode == EvenHumanVsEngine) || (m_playMode == HandicapHumanVsEngine);

    // いまの手番（SFENに基づき、MatchCoordinator→ShogiGameController で設定済み）
    const auto sideToMove = m_gameController->currentPlayer();

    // 1) 先手エンジンなら既存処理で初手を実行
    if (engineIsP1 && sideToMove == ShogiGameController::Player1) {
        startInitialEngineMoveEvH_();
        return;
    }

    // 2) 後手エンジンで後手番から開始（＝香落ちなど "w - 1"）
    if (!(engineIsP2 && sideToMove == ShogiGameController::Player2)) return;

    // 司令塔から主エンジンを取得
    Usi* eng = m_match->primaryEngine();
    if (!eng) {
        qWarning() << "[HvE] engine instance not ready; skip first move.";
        return;
    }

    // ベースの "position ... moves" が空なら初期化（安全側）
    if (m_positionStr1.isEmpty()) {
        initializePositionStringsForMatch_();
    }
    if (!m_positionStr1.startsWith("position ")) {
        qWarning() << "[HvE] position base is invalid:" << m_positionStr1;
        m_positionStr1 = QStringLiteral("position startpos moves");
    }

    // エンジン初手（後手）の思考 → bestmove の from/to を受け取る
    QPoint eFrom(-1, -1), eTo(-1, -1);

    // 時間ルールを取得（後手の秒読みに注意）
    const auto tc = m_match->timeControl();
    const int  engineByoyomiMs = tc.byoyomiMs2;   // 後手（エンジン）用
    const bool useByoyomi      = tc.useByoyomi;

    // USIへ渡す btime/wtime はその場で取得（pre-add整形は司令塔側に委譲）
    const auto [bTime, wTime] = currentBWTimesForUSI_();

    try {
        // USI "position ... moves" と "go ..." を一括で処理して bestmove を返す
        eng->handleEngineVsHumanOrEngineMatchCommunication(
            m_positionStr1,        // [in/out] position base に bestmove を連結してくる
            m_positionPonder1,     // [in/out] 使っていなければ空のままでOK
            eFrom, eTo,            // [out] エンジンの着手が返る
            engineByoyomiMs,
            bTime, wTime,
            tc.incMs1, tc.incMs2,
            useByoyomi
            );
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
        return;
    }

    // 返ってきた bestmove を盤へ適用
    bool ok = false;
    try {
        ok = m_gameController->validateAndMove(
            eFrom, eTo, m_lastMove, m_playMode,
            m_currentMoveIndex, m_sfenRecord, m_gameMoves
            );
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
        return;
    }
    if (!ok) return;

    // ハイライト・思考時間の反映
    if (m_boardController)
        m_boardController->showMoveHighlights(eFrom, eTo);

    const qint64 thinkMs = eng->lastBestmoveElapsedMs();
    // この直後は人間手番＝直前に指したのは P2（エンジン）
    if (m_shogiClock) {
        m_shogiClock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
        // byoyomi/Fischer適用 & 直近考慮秒の確定
        m_shogiClock->applyByoyomiAndResetConsideration2();
    }

    // UI更新と履歴
    updateTurnAndTimekeepingDisplay();
    m_positionStrList.append(m_positionStr1);
    redrawEngine1EvaluationGraph();
    pumpUi();

    // 次は人間手番：クリック受付と人間用タイマー
    const bool gameOver = (m_match && m_match->gameOverState().isOver);
    if (!gameOver) {
        if (m_shogiView) m_shogiView->setMouseClickMode(true);
        QTimer::singleShot(0, this, [this]{
            if (m_match) m_match->armHumanTimerIfNeeded();
        });
    }
}
// mainwindow.cpp のどこでもOK（他のメンバ実装と同じスコープ）
void MainWindow::updateTurnDisplay()
{
    const int cur = (m_gameController->currentPlayer() == ShogiGameController::Player2) ? 2 : 1;
    updateTurnStatus(cur);
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
    // 直近状態をキャッシュ
    m_lastP1Turn = p1turn;
    m_lastP1Ms   = p1ms;
    m_lastP2Ms   = p2ms;

    // 時計ラベルを更新
    if (m_shogiView) {
        m_shogiView->blackClockLabel()->setText(fmt_hhmmss(p1ms));
        m_shogiView->whiteClockLabel()->setText(fmt_hhmmss(p2ms));
        m_shogiView->setBlackTimeMs(p1ms);
        m_shogiView->setWhiteTimeMs(p2ms);
    }

    // 手番強調＋緊急色を統合適用
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

void MainWindow::onMoveRequested_(const QPoint& from, const QPoint& to)
{
    // 着手前の手番（＝この手を指す側）を控える
    const auto moverBefore = m_gameController->currentPlayer();

    // validateAndMove は参照を取りうるためローカルコピーで渡す
    QPoint hFrom = from, hTo = to;

    bool ok = false;
    try {
        ok = m_gameController->validateAndMove(
            hFrom, hTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves
            );
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
        if (m_boardController) m_boardController->onMoveApplied(from, to, /*ok=*/false);
        return;
    }

    // 適用結果通知（ドラッグ/ハイライト確定）
    if (m_boardController) m_boardController->onMoveApplied(from, to, ok);
    if (!ok) return;

    // 人間の着手ハイライト
    if (m_boardController) m_boardController->showMoveHighlights(hFrom, hTo);

    // モード別の後続処理
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

void MainWindow::handleMove_HvE_(const QPoint& humanFrom, const QPoint& humanTo)
{
    if (m_match) m_match->finishHumanTimerAndSetConsideration();

    // “同○” 判定用ヒントを司令塔の主エンジンへ
    if (Usi* eng = (m_match ? m_match->primaryEngine() : nullptr)) {
        eng->setPreviousFileTo(humanTo.x());
        eng->setPreviousRankTo(humanTo.y());
    }

    updateTurnAndTimekeepingDisplay();
    if (m_shogiView) m_shogiView->setMouseClickMode(false);

    // USIへ渡す btime/wtime はその都度ローカル生成
    auto [bTime, wTime] = currentBWTimesForUSI_();

    const bool engineIsP1 =
        (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);

    // 直後の手番がエンジンなら 1手だけ指させる
    if (!isHumanTurnNow(engineIsP1)) {
        QPoint eFrom = humanFrom;  // 人間の着手を渡す（必須）
        QPoint eTo   = humanTo;    // 人間の着手を渡す（必須）

        // 司令塔から時間ルール取得
        const auto tc = m_match ? m_match->timeControl() : MatchCoordinator::TimeControl{};
        const int engineByoyomiMs = engineIsP1 ? tc.byoyomiMs1 : tc.byoyomiMs2;

        try {
            Usi* eng = (m_match ? m_match->primaryEngine() : nullptr);
            if (!eng) {
                qWarning() << "[HvE] engine instance not ready; skip engine move.";
                if (m_shogiView) m_shogiView->setMouseClickMode(true);
                return;
            }

            eng->handleHumanVsEngineCommunication(
                m_positionStr1, m_positionPonder1,
                eFrom, eTo,
                engineByoyomiMs,
                bTime, wTime,
                m_positionStrList,
                tc.incMs1, tc.incMs2,
                tc.useByoyomi
                );
        } catch (const std::exception& e) {
            displayErrorMessage(e.what());
            if (m_shogiView) m_shogiView->setMouseClickMode(true);
            return;
        }

        // 受け取った bestmove を盤へ適用
        bool ok2 = false;
        try {
            ok2 = m_gameController->validateAndMove(
                eFrom, eTo, m_lastMove, m_playMode,
                m_currentMoveIndex, m_sfenRecord, m_gameMoves
                );
        } catch (const std::exception& e) {
            displayErrorMessage(e.what());
            if (m_shogiView) m_shogiView->setMouseClickMode(true);
            return;
        }

        if (ok2) {
            if (m_boardController) m_boardController->showMoveHighlights(eFrom, eTo);

            // エンジン思考時間 → 時計へ
            const qint64 thinkMs =
                (m_match && m_match->primaryEngine())
                    ? m_match->primaryEngine()->lastBestmoveElapsedMs()
                    : 0;
            if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
                m_shogiClock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            } else {
                m_shogiClock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            }

            updateTurnAndTimekeepingDisplay();
            m_positionStrList.append(m_positionStr1);
            redrawEngine1EvaluationGraph();
            pumpUi();
        }
    }

    // 終局なら入力/計測は再開しない
    if (isGameOver_()) return;

    if (m_shogiView) m_shogiView->setMouseClickMode(true);
    QTimer::singleShot(0, this, [this]{
        if (m_match) m_match->armHumanTimerIfNeeded();
    });
}

std::pair<QString, QString> MainWindow::currentBWTimesForUSI_() const
{
    QString bTime = QStringLiteral("0"), wTime = QStringLiteral("0");
    if (m_match) {
        qint64 bMs = 0, wMs = 0;
        m_match->computeGoTimesForUSI(bMs, wMs);
        bTime = QString::number(bMs);
        wTime = QString::number(wMs);
    } else if (m_shogiClock) {
        bTime = QString::number(m_shogiClock->getPlayer1TimeIntMs());
        wTime = QString::number(m_shogiClock->getPlayer2TimeIntMs());
    }
    return {bTime, wTime};
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

    // 旧方式は念のため切断
    QObject::disconnect(m_branchCtl, &BranchCandidatesController::applyLineRequested,
                        this,        &MainWindow::onApplyLineRequested_);
    QObject::disconnect(m_branchCtl, &BranchCandidatesController::backToMainRequested,
                        this,        &MainWindow::onBackToMainlineRequested);
}

// ==== 分岐候補クリック：解決済みラインを適用 ====
void MainWindow::onApplyResolvedLine(const ResolvedLine& line)
{
    // ResolvedLine → ResolvedRow へ詰め替え（最低限）
    ResolvedRow r;
    r.startPly = line.startPly > 0 ? line.startPly : 1;
    r.disp     = line.disp;

    // SFEN/USI は再計算ルートが別にあるなら空でも可。
    // ここでは一旦「本譜の先頭SFENだけ」差し込んでおく（安全策）。
    r.sfen.clear();
    if (!m_sfenMain.isEmpty())
        r.sfen << m_sfenMain.front(); // 0手のSFENだけ確保
    r.gm.clear();
    r.varIndex = -1; // ad-hoc id

    // 表示用行に追加して適用（既存の適用ルートを流用）
    const int row = m_resolvedRows.size();
    m_resolvedRows.push_back(r);

    // 選択＆適用（必要なら 0 手目で適用 → 1手ずつ進める等に変更）
    applyResolvedRowAndSelect(row, /*selPly=*/0);

    // 分岐ツリーにも反映したい場合（任意）
    if (m_analysisTab) {
        QVector<EngineAnalysisTab::ResolvedRowLite> rows;
        rows.reserve(m_resolvedRows.size());
        for (const auto& rr : m_resolvedRows) {
            EngineAnalysisTab::ResolvedRowLite x;
            x.startPly = rr.startPly;
            x.disp     = rr.disp;
            x.sfen     = rr.sfen;
            rows.push_back(std::move(x));
        }
        m_analysisTab->setBranchTreeRows(rows);
        m_analysisTab->highlightBranchTreeAt(row, /*ply=*/0, /*centerOn=*/true);
    }
}

// ==== 「本譜へ戻る」 ====
void MainWindow::onBackToMainlineRequested()
{
    if (m_branchTreeLocked) {
        return; // ← ツリー変更を一切行わない
    }

    // guard
    if (!m_shogiView || !m_gameController) return;

    // m_dispMain / m_usiMoves は loadKifuFromFile() 時に保持済み（あなたの現行コードより）
    const QList<KifDisplayItem> disp = m_dispMain;
    const QStringList           usi  = m_usiMoves;

    // 終局語句の有無を判定して SFEN を復元
    auto isTerminalPretty = [](const QString& s)->bool {
        static const QStringList kw = {
            QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
            QStringLiteral("千日手"), QStringLiteral("切れ負け"),
            QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
            QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
            QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
        };
        for (const auto& k : kw) if (s.contains(k)) return true;
        return false;
    };
    const bool hasTerminal = (!disp.isEmpty() && isTerminalPretty(disp.back().prettyMove));

    QString baseSfen = m_startSfenStr;
    if (baseSfen.startsWith(QStringLiteral("sfen "))) baseSfen.remove(0, 5);
    if (baseSfen.isEmpty()) {
        baseSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }

    rebuildSfenRecord(baseSfen, usi, hasTerminal);
    rebuildGameMoves(baseSfen, usi);

    // 本譜の表示へ復帰
    displayGameRecord(disp);

    // 盤表示の整合
    m_shogiView->applyBoardAndRender(m_gameController->board());
    m_shogiView->configureFixedSizing();

    // 分岐ツリーの先頭を強調（任意）
    if (m_analysisTab) {
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }
}

void MainWindow::onApplyLineRequested_(const QList<KifDisplayItem>& disp,
                                       const QStringList& usiStrs)
{
    // ==== 分岐ツリー固定中：新しい枝を作らず、既存行へ“表示だけ”切替 ====
    if (m_branchTreeLocked) {
        const int ply = m_branchPlyContext;   // 分岐候補を出した絶対手数

        if (ply <= 0 || disp.isEmpty()) {
            qDebug() << "[BRANCH] locked: nothing to do."
                     << " ply=" << ply << " dispSz=" << disp.size();
            return;
        }

        // ユーザーが選んだ「この手」のラベル（優先: 絶対手数位置 / 予備: 先頭）
        QString want;
        if (disp.size() >= ply) {
            want = pickLabelForDisp(disp.at(ply - 1));
        }
        if (want.isEmpty()) {
            want = pickLabelForDisp(disp.first());
        }

        qDebug().noquote()
            << "[BRANCH] locked: switch by label only"
            << " absPly=" << ply
            << " want=" << (want.isEmpty() ? "<EMPTY>" : want);

        // ROW[0..] を小さい順に走査して、行内の (ply - startPly) のラベル一致を探す
        int hitRow = -1;
        for (int r = 0; r < m_resolvedRows.size(); ++r) {
            const auto& rr = m_resolvedRows[r];
            const int li = ply - 1;
            if (li < 0 || li >= rr.disp.size()) continue;     // その手が行の範囲外
            const QString rowLbl = pickLabelForDisp(rr.disp.at(li));

            //begin
            qDebug().nospace() << "[BRANCH] locked: check row=" << r
                               << " startPly=" << rr.startPly
                               << " li=" << li
                               << " rowLbl=" << (rowLbl.isEmpty() ? "<EMPTY>" : rowLbl);
            //end

            if (!rowLbl.isEmpty() && rowLbl == want) {
                hitRow = r;                                   // 最小のROWを採用
                break;
            }
        }

        if (hitRow >= 0) {
            const auto& rr = m_resolvedRows[hitRow];
            // 表示選択位置：その手を指し終えた直後に合わせる（安全にクランプ）
            const int sel = qBound(0, ply - rr.startPly + 1, rr.disp.size());
            qDebug() << "[BRANCH] locked: switch view -> row=" << hitRow
                     << " startPly=" << rr.startPly
                     << " sel=" << sel
                     << " want=" << want;
            applyResolvedRowAndSelect(hitRow, sel);           // 棋譜欄だけを切替
        } else {
            qDebug().noquote()
                << "[BRANCH] locked: no row matched by label"
                << " absPly=" << ply
                << " want=" << (want.isEmpty() ? "<EMPTY>" : want);
        }
        return;    // ← ツリーは固定。新規枝は生やさない
    }

    // ==== （通常時）従来どおり新規行適用 ====
    qDebug().nospace() << "[APPLY] onApplyLineRequested_: disp=" << disp.size()
                       << " usiStrs=" << usiStrs.size();

    ResolvedLine line;
    line.startPly = 1;
    line.disp     = disp;
    line.usi.reserve(usiStrs.size());
    for (const auto& s : usiStrs) line.usi.push_back(UsiMove(s));
    onApplyResolvedLine(line);
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

// 行番号から表示名を作る（Main / VarN）
QString MainWindow::rowNameFor_(int row) const {
    if (row < 0 || row >= m_resolvedRows.size()) return QString("<?>");
    const auto& rr = m_resolvedRows[row];
    return (rr.varIndex < 0) ? QStringLiteral("Main")
                             : QStringLiteral("Var%1").arg(rr.varIndex);
}

// 1始まり hand-ply のラベル（無ければ ""）
QString MainWindow::labelAt_(const ResolvedRow& rr, int ply) const {
    const int li = ply - 1;
    if (li < 0 || li >= rr.disp.size()) return QString();
    return pickLabelForDisp(rr.disp.at(li));
}

// 1..p までの完全一致（両方に手が存在し、かつ全ラベル一致）なら true
bool MainWindow::prefixEqualsUpTo_(int rowA, int rowB, int p) const {
    if (rowA < 0 || rowA >= m_resolvedRows.size()) return false;
    if (rowB < 0 || rowB >= m_resolvedRows.size()) return false;
    const auto& A = m_resolvedRows[rowA];
    const auto& B = m_resolvedRows[rowB];
    for (int k = 1; k <= p; ++k) {
        const QString a = labelAt_(A, k);
        const QString b = labelAt_(B, k);
        if (a.isEmpty() || b.isEmpty() || a != b) return false;
    }
    return true;
}

// デバッグ出力：各ラインごとに、各手k（=直前まで一致）で次手(k+1)が割れるなら
// 「分岐あり <各ライン名 次手>」、割れなければ「分岐なし」を出力
void MainWindow::dumpBranchSplitReport() const
{
    if (m_resolvedRows.isEmpty()) return;

    // 行ごとに出力
    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        const QString header = rowNameFor_(r);
        qDebug().noquote() << header;

        const int maxPly = rr.disp.size();
        for (int p = 1; p <= maxPly; ++p) {
            const QString curLbl = labelAt_(rr, p);

            // この行と「1..p まで完全一致」する仲間を抽出
            QList<int> group;
            for (int j = 0; j < m_resolvedRows.size(); ++j) {
                if (prefixEqualsUpTo_(r, j, p)) group << j;
            }

            // その仲間たちの「次手 (p+1)」を集計（存在するものだけ）
            struct NextMove { QString who; QString lbl; };
            QVector<NextMove> nexts; nexts.reserve(group.size());
            QSet<QString> uniq;
            for (int j : group) {
                const QString lblNext = labelAt_(m_resolvedRows[j], p + 1);
                if (lblNext.isEmpty()) continue; // ここで終端は候補に出さない
                const QString who = rowNameFor_(j);
                nexts.push_back({who, lblNext});
                uniq.insert(lblNext);
            }

            if (uniq.size() > 1) {
                // 分岐あり：各ライン名と次手を「、」で列挙
                QStringList parts;
                parts.reserve(nexts.size());
                for (const auto& nm : nexts) {
                    parts << QString("%1 %2").arg(nm.who, nm.lbl);
                }
                qDebug().noquote()
                    << QString("%1 %2 分岐あり %3")
                           .arg(p)
                           .arg(curLbl.isEmpty() ? QStringLiteral("<EMPTY>") : curLbl)
                           .arg(parts.join(QStringLiteral("、")));
            } else {
                qDebug().noquote()
                    << QString("%1 %2 分岐なし")
                           .arg(p)
                           .arg(curLbl.isEmpty() ? QStringLiteral("<EMPTY>") : curLbl);
            }
        }

        // 行間の空行
        qDebug().noquote() << "";
    }
}

void MainWindow::buildBranchCandidateDisplayPlan()
{
    m_branchDisplayPlan.clear();

    const int R = m_resolvedRows.size();
    if (R == 0) return;

    auto labelAt = [&](int row, int li)->QString {
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    auto prefixEquals = [&](int r1, int r2, int uptoLi)->bool {
        // li=0 のときは「初手より前の共通部分」は空なので常に一致とみなす
        for (int i = 0; i < uptoLi; ++i) {
            if (labelAt(r1, i) != labelAt(r2, i)) return false;
        }
        return true;
    };

    // 各行 r の各ローカル添字 li（0-based）について、
    // 「初手から li-1 まで完全一致する行」をグループ化し、
    // その li 手目に 2 種類以上の指し手があれば分岐とみなす。
    // ★ 表示は “その手（li）” に出す（＝1手先に送らない）
    for (int r = 0; r < R; ++r) {
        const int len = m_resolvedRows[r].disp.size();
        if (len == 0) continue;

        for (int li = 0; li < len; ++li) {
            // この行 r と「初手から li-1 まで一致」する行
            QVector<int> group;
            group.reserve(R);
            for (int g = 0; g < R; ++g) {
                if (li < m_resolvedRows[g].disp.size() && prefixEquals(r, g, li)) {
                    group.push_back(g);
                }
            }
            if (group.size() <= 1) continue; // 比較相手がいない

            // グループの li 手目ラベルを集計
            QHash<QString, QVector<int>> labelToRows;
            for (int g : group) {
                const QString lbl = labelAt(g, li);
                labelToRows[lbl].push_back(g);
            }
            if (labelToRows.size() <= 1) continue; // 全員同じ指し手 → 分岐ではない

            // 表示先 ply（1-based）。li は 0-based
            const int targetPly = li + 1;
            if (targetPly > m_resolvedRows[r].disp.size()) continue;

            // 見出し（この行 r の li 手目）
            const QString baseForDisplay = labelAt(r, li);

            // ===== 重複整理（自分の行 > Main(=row0) > 若い VarN）=====
            struct TmpKeep { QString lbl; int keepRow; };
            QVector<TmpKeep> keeps; keeps.reserve(labelToRows.size());

            for (auto it = labelToRows.constBegin(); it != labelToRows.constEnd(); ++it) {
                const QString lbl = it.key();
                const QVector<int>& rowsWithLbl = it.value();

                int keep = -1;

                // 1) 自分の行 r を最優先
                bool hasSelf = false;
                for (int cand : rowsWithLbl) {
                    if (cand == r) { keep = cand; hasSelf = true; break; }
                }

                // 2) 自分が無ければ Main(row=0)
                if (!hasSelf) {
                    bool hasMain = false;
                    for (int cand : rowsWithLbl) {
                        if (cand == 0) { keep = 0; hasMain = true; break; }
                    }

                    // 3) それも無ければ最小 row（VarN の若い方）
                    if (!hasMain) {
                        keep = rowsWithLbl.first();
                        for (int cand : rowsWithLbl) {
                            if (cand < keep) keep = cand;
                        }
                    }
                }

                keeps.push_back({ lbl, keep });
            }

            // 表示順: Main が先、次に row 昇順
            std::sort(keeps.begin(), keeps.end(), [](const TmpKeep& a, const TmpKeep& b){
                if (a.keepRow == 0 && b.keepRow != 0) return true;
                if (a.keepRow != 0 && b.keepRow == 0) return false;
                return a.keepRow < b.keepRow;
            });

            QVector<BranchCandidateDisplayItem> items;
            items.reserve(keeps.size());
            for (const auto& k : keeps) {
                BranchCandidateDisplayItem itx;
                itx.row      = k.keepRow;
                itx.varN     = (k.keepRow == 0 ? -1 : k.keepRow - 1);
                itx.lineName = lineNameForRow(k.keepRow); // "Main" / "VarN"
                itx.label    = k.lbl;
                items.push_back(itx);
            }

            // 保存
            BranchCandidateDisplay plan;
            plan.ply       = targetPly;
            plan.baseLabel = baseForDisplay;
            plan.items     = std::move(items);
            m_branchDisplayPlan[r].insert(targetPly, std::move(plan));
        }
    }
}

void MainWindow::dumpBranchCandidateDisplayPlan() const
{
    if (m_resolvedRows.isEmpty()) return;

    auto labelAt = [&](int row, int ply1)->QString {
        // ply1 は 1-based
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    // 行ごとに
    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        qDebug().noquote() << (r == 0 ? "Main" : QString("Var%1").arg(r - 1));

        const int len = m_resolvedRows[r].disp.size();
        const auto itRow = m_branchDisplayPlan.constFind(r);

        for (int ply1 = 1; ply1 <= len; ++ply1) {
            const QString base = labelAt(r, ply1);
            bool has = false;
            QVector<BranchCandidateDisplayItem> items;

            if (itRow != m_branchDisplayPlan.constEnd()) {
                const auto& mp = itRow.value();
                auto itP = mp.constFind(ply1);
                if (itP != mp.constEnd()) {
                    has   = true;
                    items = itP.value().items;
                }
            }

            if (!has) {
                qDebug().noquote()
                    << QString("%1 %2 分岐候補表示なし")
                       .arg(ply1).arg(base.isEmpty() ? "<EMPTY>" : base);
            } else {
                // "Main ▲…、Var0 ▲…、Var2 ▲…" の並びを構築
                QStringList parts;
                parts.reserve(items.size());
                for (const auto& it : items) {
                    parts << QString("%1 %2").arg(it.lineName, it.label);
                }
                qDebug().noquote()
                    << QString("%1 %2 分岐候補表示あり %3")
                       .arg(ply1)
                       .arg(base.isEmpty() ? "<EMPTY>" : base)
                       .arg(parts.join(QStringLiteral("、")));
            }
        }

        // 行間の空行
        qDebug().noquote() << "";
    }
}

void MainWindow::showBranchCandidatesFromPlan(int row, int ply1)
{
    if (!m_branchCtl || !m_kifuBranchModel) return;

    // 0手目や行範囲外は非表示
    if (ply1 <= 0 || row < 0 || row >= m_resolvedRows.size()) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }

    // Plan 参照
    const auto itRow = m_branchDisplayPlan.constFind(row);
    if (itRow == m_branchDisplayPlan.constEnd()) {
        // Plan なし → 非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }
    const auto& mp = itRow.value();
    const auto itP = mp.constFind(ply1);
    if (itP == mp.constEnd()) {
        // この手の Plan なし → 非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }

    const BranchCandidateDisplay& plan = itP.value(); // ply/baseLabel/items

    // 「候補が1つ＆現在指し手と同じなら隠す」ルール
    const QString currentLbl = [&, this]{
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    }();

    bool hide = false;
    if (plan.items.size() == 1) {
        const auto& only = plan.items.front();
        if (!only.label.isEmpty() && only.label == currentLbl) hide = true;
    }

    if (hide || plan.items.isEmpty()) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(false);
            view->setEnabled(false);
        }
        return;
    }

    // 表示（Controller経由で Plan をそのまま流し込む）
    // MainWindow ローカル型 → 公開型(グローバル)へ明示変換
    QVector<BCDI> pubItems;
    pubItems.reserve(plan.items.size());
    for (const auto& it : plan.items) {
        BCDI x;
        x.row      = it.row;
        x.varN     = it.varN;
        x.lineName = it.lineName;
        x.label    = it.label;
        pubItems.push_back(std::move(x));
    }
    m_branchCtl->refreshCandidatesFromPlan(ply1, pubItems, plan.baseLabel);

    // ビューの可視化
    if (QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
        const int rows = m_kifuBranchModel->rowCount();
        const bool show = (rows > 0);
        view->setVisible(show);
        view->setEnabled(show);
        if (show) {
            const QModelIndex idx0 = m_kifuBranchModel->index(0, 0);
            if (idx0.isValid() && view->selectionModel()) {
                view->selectionModel()->setCurrentIndex(
                    idx0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            view->scrollToTop();
        }
    }

    // UI 状態
    m_branchPlyContext   = ply1;
    m_activeResolvedRow  = row; // ←行は applyResolvedRowAndSelect でも更新されますが念のため同期
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

void MainWindow::ensureResolvedRowsHaveFullSfen()
{
    if (m_resolvedRows.isEmpty()) return;

    qDebug() << "[SFEN] ensureResolvedRowsHaveFullSfen BEGIN";

    // Main の SFEN（VEが空なら既存の行0を使う）
    QStringList veMain = (m_varEngine ? m_varEngine->mainlineSfen() : QStringList());
    if (veMain.isEmpty() && !m_resolvedRows[0].sfen.isEmpty())
        veMain = m_resolvedRows[0].sfen;

    auto sfenFromVeForRow = [&](const ResolvedRow& rr)->QStringList {
        if (rr.varIndex < 0) {
            // 本譜
            return veMain;
        }
        if (!m_varEngine) return {};
        const int vid = m_varEngine->variationIdFromSourceIndex(rr.varIndex);
        return m_varEngine->sfenForVariationId(vid);
    };

    const int rowCount = m_resolvedRows.size();
    for (int r = 0; r < rowCount; ++r) {
        auto& rr = m_resolvedRows[r];

        const int need = rr.disp.size() + 1;     // 0..N
        const int s    = qMax(1, rr.startPly);   // 1-origin
        const int base = s - 1;                  // 直前局面の添字

        // 親行（無ければ Main=0）
        int parentRow = 0;
        if (rr.parent >= 0 && rr.parent < rowCount) {
            parentRow = rr.parent;
        }

        // 親のプレフィクス（親が空なら mainline を使う）
        const QStringList parentPrefix =
            (!m_resolvedRows[parentRow].sfen.isEmpty()
                 ? m_resolvedRows[parentRow].sfen
                 : veMain);

        // この行の VE 配列（base を先頭とする 0..M）
        const QStringList veRow = sfenFromVeForRow(rr);

        // 合成先。既存をベースにするが、0..base は必ず親で上書きする
        QStringList full = rr.sfen;

        // --- 1) 0..base を親の SFEN で「強制上書き」 -----------------------
        //     ※ ここが今回の肝：古い/別行の値が残らないようにする
        for (int i = 0; i <= base; ++i) {
            if (i >= parentPrefix.size()) break;   // 親に無ければ打ち切り
            const QString pv = parentPrefix.at(i);
            if (i < full.size()) {
                full[i] = pv;                      // 常に上書き
            } else if (!pv.isEmpty()) {
                full.append(pv);                   // 既知のものだけ追加
            } else {
                break;                             // 空は追加しない
            }
        }

        // デバッグ（境界確認）
        auto hashS = [](const QString& s){ return qHash(s); };
        auto safeAt = [&](const QStringList& a, int i)->QString{
            return (0<=i && i<a.size() ? a.at(i) : QString());
        };

        qDebug().noquote()
                << "[SFEN] row=" << r
                << " base=" << base
                << " pre(base-1)#=" << hashS(safeAt(full, base-1))
                << " pre(base)#="   << hashS(safeAt(full, base))
                << " pre(base+1)#=" << hashS(safeAt(full, base+1));


        // --- 2) VE の部分配列を base から重ねる（空は書かない／ギャップは埋めない） ---
        bool gap = false;
        if (!veRow.isEmpty()) {
            for (int j = 0; j < veRow.size(); ++j) {
                const int pos = base + j;
                const QString v = veRow.at(j);
                if (v.isEmpty()) continue;

                if (pos < full.size()) {
                    full[pos] = v;
                } else if (pos == full.size()) {
                    full.append(v);
                } else {
                    // 間の index が未充足（空で埋めない方針なので打ち切り）
                    gap = true;
                    break;
                }
            }
        } else {
            // veRow が空：壊さずスキップ（警告のみ）
            if (full.size() < need) {
                qWarning().noquote()
                        << "[SFEN] SKIP(fill) row=" << r
                        << " startPly=" << rr.startPly
                        << " need=" << need
                        << " prefix(sz=" << parentPrefix.size() << ")"
                        << " veRow(sz=0)  -- keep r.sfen as-is";
            }
        }

        // --- 3) 検査＆警告 ---------------------------------------------------
        bool missing = (full.size() < need);
        if (!missing) {
            for (int i = 0; i < need; ++i) {
                if (full.at(i).isEmpty()) { missing = true; break; }
            }
        }
        if (missing || gap) {
            qWarning().noquote()
                    << "[SFEN] WARN(fill) row=" << r
                    << " startPly=" << rr.startPly
                    << " need=" << need
                    << " prefix(sz=" << parentPrefix.size() << ")"
                    << " veRow(sz=" << veRow.size() << ")"
                    << (gap ? " GAP" : "");
        }

        // デバッグ（境界確認：合成後）

        qDebug().noquote()
                << "[SFEN] row=" << r
                << " post(base-1)#=" << hashS(safeAt(full, base-1))
                << " post(base)#="   << hashS(safeAt(full, base))
                << " post(base+1)#=" << hashS(safeAt(full, base+1));


        rr.sfen = std::move(full);
    }

    qDebug() << "[SFEN] ensureResolvedRowsHaveFullSfen END";
}


void MainWindow::dumpAllRowsSfenTable() const
{
    if (m_resolvedRows.isEmpty()) return;

    auto labelAt = [&](int row, int ply1)->QString {
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        qDebug().noquote() << (r == 0 ? "Main" : QString("Var%1").arg(r - 1));

        // 0: 開始局面
        const QString s0 = (!rr.sfen.isEmpty() ? rr.sfen.first() : QStringLiteral("<SFEN MISSING>"));
        qDebug().noquote() << QStringLiteral("0 開始局面 %1").arg(s0);

        // 1..N
        for (int ply1 = 1; ply1 <= rr.disp.size(); ++ply1) {
            const QString lbl  = labelAt(r, ply1);
            QString sfen = (ply1 >= 0 && ply1 < rr.sfen.size()) ? rr.sfen.at(ply1) : QString();
            if (sfen.isEmpty()) sfen = QStringLiteral("<SFEN MISSING>");

            qDebug().noquote()
                << QString("%1 %2 %3")
                       .arg(ply1)
                       .arg(lbl.isEmpty() ? QStringLiteral("<EMPTY>") : lbl)
                       .arg(sfen);
        }
        qDebug().noquote() << "";
    }
}

// MainWindow.cpp (privateメソッドとして)
inline QString MainWindow::sfenAt_(int row, int ply1) const
{
    if (row < 0 || row >= m_resolvedRows.size()) return {};
    const auto& s = m_resolvedRows[row].sfen;      // 0..N （ensureResolvedRowsHaveFullSfenで揃っている前提）
    if (s.isEmpty()) return {};
    const int idx = qBound(0, ply1, s.size() - 1); // ply1 は 0=初期局面, 1=1手後...
    return s.at(idx);
}

void MainWindow::dumpAllLinesGameMoves() const
{
    if (m_resolvedRows.isEmpty()) return;

    // 終局判定（見た目ベース）
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    auto isTerminalPretty = [&](const QString& s)->bool {
        for (const auto& kw : kTerminalKeywords) if (s.contains(kw)) return true;
        return false;
    };

    auto lineNameFor = [](int row)->QString {
        return (row == 0) ? QStringLiteral("Main")
                          : QStringLiteral("Var%1").arg(row - 1);
    };
    auto labelAt = [&](const ResolvedRow& rr, int li)->QString {
        return (li >= 0 && li < rr.disp.size())
                 ? pickLabelForDisp(rr.disp.at(li))
                 : QString();
    };
    auto fmtPt = [](const QPoint& p)->QString {
        return QStringLiteral("(%1,%2)").arg(p.x()).arg(p.y());
    };

    // QChar/char/整数 いずれでも文字列化できる汎用ヘルパ
    auto pieceToQString = [](auto v)->QString {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, QChar>) {
            return v.isNull() ? QString() : QString(v);
        } else if constexpr (std::is_same_v<T, char>) {
            return v ? QString(QChar(v)) : QString();
        } else if constexpr (std::is_integral_v<T>) {
            return v ? QString(QChar(static_cast<ushort>(v))) : QString();
        } else {
            return QString();
        }
    };

    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        qDebug().noquote() << lineNameFor(r);

        // 0) 開始局面（From/To 等は出さない）
        qDebug().noquote() << "0 開始局面";

        const int M = rr.disp.size();
        for (int li = 0; li < M; ++li) {
            const QString pretty = labelAt(rr, li);
            const bool terminal  = isTerminalPretty(pretty);

            // ヘッダ（手数 + 見た目ラベル）
            const QString head = QStringLiteral("%1 %2").arg(li + 1).arg(pretty);

            if (terminal || li >= rr.gm.size()) {
                // 投了など or GM不足 → そのまま
                qDebug().noquote() << head;
                continue;
            }

            const ShogiMove& mv = rr.gm.at(li);

            // ドロップ（打つ手）は fromSquare が無効座標の可能性
            const bool hasFrom = (mv.fromSquare.x() >= 0 && mv.fromSquare.y() >= 0);
            const QString fromS = hasFrom ? fmtPt(mv.fromSquare) : QString();
            const QString toS   = fmtPt(mv.toSquare);

            const QString moving = pieceToQString(mv.movingPiece);
            const QString cap    = pieceToQString(mv.capturedPiece);
            const bool promoted  = mv.isPromotion;   // ← 正しいフィールド名

            qDebug().noquote()
                << QString("%1 From:%2 To:%3 Moving:%4 Captured:%5 Promotion:%6")
                       .arg(head)
                       .arg(fromS)
                       .arg(toS)
                       .arg(moving)
                       .arg(cap)
                       .arg(promoted ? "true" : "false");
        }

        qDebug().noquote() << "";
    }
}

// ==== MainWindow.cpp （適当な実装ファイル）====

// デバッグのオン/オフ（必要に応じて false に）
static bool kGM_VERBOSE = true;

// 1セルを "file-rank" 表示（USI基準とL2R基準の両方を出す）
static inline QString idxHuman(int idx) {
    const int col = idx % 9;       // 0..8   左→右
    const int row = idx / 9;       // 0..8   上→下
    const int fileL2R = col + 1;   // 1..9   左→右
    const int rankTop = row + 1;   // 1..9   上→下
    const int fileUSI = 9 - col;   // 9..1   右→左（一般的なUSIの筋）
    const int rankUSI = 9 - row;   // 9..1   下→上（一般的なUSIの段）
    return QStringLiteral("[idx=%1 L2R(%2,%3) USI(%4,%5)]")
            .arg(idx).arg(fileL2R).arg(rankTop).arg(fileUSI).arg(rankUSI);
}

// 盤面(81マス)をトークン列に展開（空は ""。駒は "P","p","+P" のように '+' 付きも保持）
static QVector<QString> sfenBoardTo81Tokens(const QString& sfen)
{
    const QString board = sfen.section(QLatin1Char(' '), 0, 0); // 盤面部分
    QVector<QString> cells; cells.reserve(81);

    for (int i = 0; i < board.size() && cells.size() < 81; ++i) {
        const QChar ch = board.at(i);
        if (ch == QLatin1Char('/')) continue;
        if (ch.isDigit()) {
            const int n = ch.digitValue();
            for (int k = 0; k < n; ++k) cells.push_back(QString()); // 空マス
            continue;
        }
        if (ch == QLatin1Char('+')) {
            if (i + 1 < board.size()) {
                cells.push_back(QStringLiteral("+") + board.at(i + 1));
                ++i;
            }
            continue;
        }
        // 通常駒1文字
        cells.push_back(QString(ch));
    }
    while (cells.size() < 81) cells.push_back(QString());

    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] sfenBoardTo81Tokens parsed"
                           << " len=" << cells.size()
                           << " board=\"" << board << "\"";
    }
    return cells;
}

static inline bool tokenEmpty(const QString& t) { return t.isEmpty(); }
static inline bool tokenPromoted(const QString& t) { return (!t.isEmpty() && t.at(0) == QLatin1Char('+')); }
static inline QChar tokenBasePiece(const QString& t) {
    if (t.isEmpty()) return QChar();
    return t.back(); // '+P' → 'P', 'p' → 'p'
}

// SFENペアから 1手分の ShogiMove を復元（差分なし=終局などは false）
static bool deriveMoveFromSfenPair(const QString& prevSfen,
                                   const QString& nextSfen,
                                   ShogiMove* out)
{
    const QVector<QString> A = sfenBoardTo81Tokens(prevSfen);
    const QVector<QString> B = sfenBoardTo81Tokens(nextSfen);

    int fromIdx = -1, toIdx = -1;
    QString fromTok, toTokPrev, toTokNew;

    // どのセルが変わったか（詳細ログ用）
    QVector<int> diffs; diffs.reserve(4);

    for (int i = 0; i < 81; ++i) {
        const QString& a = A.at(i);
        const QString& b = B.at(i);
        if (a == b) continue;

        diffs.push_back(i);

        // 移動元：a に駒があり b が空
        if (!tokenEmpty(a) && tokenEmpty(b)) {
            fromIdx = i;
            fromTok = a;
            continue;
        }
        // 移動先：b に駒があり a と異なる
        if (!tokenEmpty(b) && a != b) {
            toIdx     = i;
            toTokNew  = b;
            toTokPrev = a; // 取りがあった場合は a に相手駒がいた
        }
    }

    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] deriveMoveFromSfenPair  diffs=" << diffs.size();
        for (int i = 0; i < diffs.size(); ++i) {
            const int d = diffs.at(i);
            qDebug().noquote()
                << "      diff[" << i << "] idx=" << d
                << "  A=\"" << A.at(d) << "\""
                << "  B=\"" << B.at(d) << "\""
                << "  " << idxHuman(d);
        }
        qDebug().noquote() << "      picked fromIdx=" << fromIdx
                           << (fromIdx>=0 ? (" tok=\""+fromTok+"\" "+idxHuman(fromIdx)) : QString())
                           << "  toIdx=" << toIdx
                           << (toIdx>=0 ? (" tokPrev=\""+toTokPrev+"\" tokNew=\""+toTokNew+"\" "+idxHuman(toIdx)) : QString());
    }

    if (fromIdx < 0 && toIdx < 0) {
        // 盤が全く同じ → 投了など「着手なし」
        if (kGM_VERBOSE) qDebug() << "[GM] no board delta (resign/terminal/comment only)";
        return false;
    }

    // 盤座標 ← idx
    auto idxToPointL2R     = [](int idx)->QPoint { return QPoint(idx % 9, idx / 9); };
    auto idxToPointFlipped = [](int idx)->QPoint { return QPoint(8 - (idx % 9), idx / 9); };

    // 出力フィールド（※最終的に out へ入れるのは FLIP 側）
    QPoint from(-1, -1), to(-1, -1);
    QChar moving, captured;
    bool isPromotion = false;

    if (fromIdx < 0 && toIdx >= 0) {
        // 打つ手（ドロップ）
        from = QPoint(-1, -1);
        // ★ 採用は FLIP 側
        to   = idxToPointFlipped(toIdx);
        moving    = tokenBasePiece(toTokNew);              // 駒台から打った駒
        captured  = QChar();                               // 取りは無い
        isPromotion = false;                               // 打ちは成りなし
    } else if (fromIdx >= 0 && toIdx >= 0) {
        // 通常移動（★ 採用は FLIP 側）
        from     = idxToPointFlipped(fromIdx);
        to       = idxToPointFlipped(toIdx);
        moving   = tokenBasePiece(fromTok);               // 元の升の駒（非成り形）
        captured = tokenEmpty(toTokPrev) ? QChar() : tokenBasePiece(toTokPrev);
        isPromotion = tokenPromoted(toTokNew);
    } else {
        if (kGM_VERBOSE) qDebug() << "[GM] inconsistent from/to detection";
        return false;
    }

    if (kGM_VERBOSE) {
        // 参考ログ：L2R と FLIP の両方を出す（採用は FLIP）
        QPoint l2rFrom(-1,-1), l2rTo(-1,-1);
        if (fromIdx >= 0) l2rFrom = idxToPointL2R(fromIdx);
        if (toIdx   >= 0) l2rTo   = idxToPointL2R(toIdx);

        QPoint flipFrom(-1,-1), flipTo(-1,-1);
        if (fromIdx >= 0) flipFrom = idxToPointFlipped(fromIdx);
        if (toIdx   >= 0) flipTo   = idxToPointFlipped(toIdx);

        qDebug().noquote()
            << "      L2R  from=" << l2rFrom << " to=" << l2rTo;
        qDebug().noquote()
            << "      FLIP from=" << flipFrom << " to=" << flipTo << "  <-- chosen";

        qDebug().noquote()
            << "      moving=" << moving
            << " captured=" << (captured.isNull() ? QChar(' ') : captured)
            << " promoted=" << (isPromotion ? "T" : "F");
    }

    if (out) *out = ShogiMove(from, to, moving, captured, isPromotion);
    return true;
}

// 各 ResolvedRow の SFEN 列から gm（ShogiMove 列）を復元する（詳細ログ付き）
void MainWindow::ensureResolvedRowsHaveFullGameMoves()
{
    qDebug() << "[GM] ensureResolvedRowsHaveFullGameMoves BEGIN";
    for (int i = 0; i < m_resolvedRows.size(); ++i) {
        auto& r = m_resolvedRows[i];

        const int nsfen = r.sfen.size();     // 0..N
        const int ndisp = r.disp.size();     // 1..N
        // resign など「盤が変わらない終端」は 1 手として数えないので、基本は min(ndisp, nsfen-1)
        const int want  = qMax(0, qMin(ndisp, nsfen - 1));

        const QString label = (i == 0)
                              ? QStringLiteral("Main")
                              : QStringLiteral("Var%1(id=%2)")
                                    .arg(r.varIndex)
                                    .arg(m_varEngine ? m_varEngine->varIdForSourceIndex(r.varIndex)
                                                     : (r.varIndex + 1));

        qDebug().noquote()
            << QString("[GM] row=%1 \"%2\" start=%3 nsfen=%4 ndisp=%5 current_gm=%6 want=%7")
                  .arg(i).arg(label).arg(r.startPly).arg(nsfen).arg(ndisp).arg(r.gm.size()).arg(want);

        // サイズが一致していればスキップ（必要に応じて強制再構築したい場合はこの if を外す）
        if (r.gm.size() == want) {
            qDebug().noquote() << QString("[GM] row=%1 keep (size match)").arg(i);
            continue;
        }

        r.gm.clear();

        for (int ply1 = 1; ply1 <= want; ++ply1) {
            const QString prev = r.sfen.at(ply1 - 1);
            const QString next = r.sfen.at(ply1);
            const QString pretty = r.disp.at(ply1 - 1).prettyMove;

            ShogiMove mv;
            const bool ok = deriveMoveFromSfenPair(prev, next, &mv); // ★ 内部で FLIP（USI向き）を採用

            if (!ok) {
                // 盤が同一 → 投了やコメントのみなど（gm へは積まない）
                qDebug().noquote()
                    << QString("[GM] row=%1 ply=%2 \"%3\" : NO-DELTA (terminal/comment)")
                           .arg(i).arg(ply1).arg(pretty);
                continue;
            }

            r.gm.push_back(mv);

            auto qcharToStr = [](QChar c)->QString { return c.isNull() ? QString(" ") : QString(c); };
            qDebug().noquote()
                << QString("[GM] row=%1 ply=%2 \"%3\"  From:(%4,%5) To:(%6,%7) Moving:%8 Captured:%9 Promotion:%10")
                       .arg(i).arg(ply1).arg(pretty)
                       .arg(mv.fromSquare.x()).arg(mv.fromSquare.y())
                       .arg(mv.toSquare.x()).arg(mv.toSquare.y())
                       .arg(qcharToStr(mv.movingPiece))
                       .arg(qcharToStr(mv.capturedPiece))
                       .arg(mv.isPromotion ? "true" : "false");
        }

        qDebug().noquote()
            << QString("[GM] row=%1 built gm.size=%2 / want=%3").arg(i).arg(r.gm.size()).arg(want);
    }
    qDebug() << "[GM] ensureResolvedRowsHaveFullGameMoves END";
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

// MainWindow.cpp
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

void MainWindow::removeLastKifuPlies_(int n)
{
    if (!m_kifuRecordModel || n <= 0) return;

    const bool prevGuard = m_onMainRowGuard;
    m_onMainRowGuard = true;

    for (int i = 0; i < n; ++i) {
        dumpTailState(m_kifuRecordModel, "[UNDO] before");
        const int before = m_kifuRecordModel->rowCount();
        if (before <= 0) break;

        qDebug() << "[UNDO] call removeLastItem() #" << (i+1);
        m_kifuRecordModel->removeLastItem();

        // モデル内部の詰め替え／行削除を確定
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        dumpTailState(m_kifuRecordModel, "[UNDO] after ");
    }

    m_onMainRowGuard = prevGuard;
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
                                .arg(mmss(considerMs))
                                .arg(hhmmss(totalUsedMs));

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

// フルリセットの代わりに、現局面再開用の軽量クリア
void MainWindow::resetUiForResumeFromCurrent_()
{
    // GUI 側のメイン棋譜用バッファは残す（再開時に selPly まで前置が必要なため）
    m_currentMoveIndex = 0;           // 使っていれば
    m_isLiveAppendMode = true;        // 現局面再開はライブ扱いに

    // ハイライトや選択状態の残骸があればクリア
    if (m_shogiView) m_shogiView->removeHighlightAllData();

    enterLiveAppendMode_();   // 現局面から再開モード（選択無効化）

    const bool prevGuard = m_onMainRowGuard;
    m_onMainRowGuard = true;

    if (m_boardController) m_boardController->clearAllHighlights();

    if (m_shogiView) {
        m_shogiView->blackClockLabel()->setText("00:00:00");
        m_shogiView->whiteClockLabel()->setText("00:00:00");
    }

    // ★ 再開中はグラフを消さない（RecordPane から毎回取得）
    if (!m_isResumeFromCurrent) {
        if (auto* ec = (m_recordPane ? m_recordPane->evalChart() : nullptr)) {
            ec->clearAll();
        }
    }

    if (m_modelThinking1)  m_modelThinking1->clearAllItems();
    if (m_modelThinking2)  m_modelThinking2->clearAllItems();

    auto resetInfo = [](UsiCommLogModel* m){
        if (!m) return;
        m->setEngineName({});
        m->setPredictiveMove({});
        m->setSearchedMove({});
        m->setSearchDepth({});
        m->setNodeCount({});
        m->setNodesPerSecond({});
        m->setHashUsage({});
    };
    resetInfo(m_lineEditModel1);
    resetInfo(m_lineEditModel2);

    if (m_tab) m_tab->setCurrentIndex(0);

    // ★ 追加：次の追記手番の基準&局面再適用
    const int maxPly = (m_sfenRecord && !m_sfenRecord->isEmpty())
                       ? (m_sfenRecord->size() - 1) : 0;
    m_currentSelectedPly = qBound(0, m_currentSelectedPly, maxPly);
    m_currentMoveIndex   = m_currentSelectedPly;  // 次は N+1 手目から
    applySfenAtCurrentPly();
    updateHighlightsForPly_(m_currentSelectedPly); // （必要なら）

    m_onMainRowGuard = prevGuard;
}

void MainWindow::enterLiveAppendMode_()
{
    m_isLiveAppendMode = true;
    if (!m_recordPane) return;
    if (auto* view = m_recordPane->kifuView()) {
        view->setSelectionMode(QAbstractItemView::NoSelection);
        view->setFocusPolicy(Qt::NoFocus);
        if (auto* sel = view->selectionModel()) sel->clearSelection();
    }
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

void MainWindow::purgeBranchPlayback_()
{
    // KIF再生（分岐解決）状態を完全クリア
    m_resolvedRows.clear();
    m_activeResolvedRow = -1;

    if (m_kifuBranchModel) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
    }
    if (QTableView* view = m_kifuBranchView
                               ? m_kifuBranchView
                               : (m_recordPane ? m_recordPane->branchView() : nullptr)) {
        view->setVisible(false);
        view->setEnabled(false);
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
    //begin
    qDebug() << "=================[EVAL] applyPendingEvalTrim_ ENTER"
             << " pendingEvalTrimPly=" << m_pendingEvalTrimPly
             << " isResumeFromCurrent=" << m_isResumeFromCurrent;
    //end
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
