#include <QApplication>
#include <QLayout>
#include <QMessageBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QDesktopServices>
#include <QTextStream>
#include <QDir>
#include <QTimer>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QHeaderView>
#include <QValueAxis>
#include <QPushButton>
#include <QLineEdit>
#include <QSpacerItem>
#include <QClipboard>
#include <QModelIndex>
#include <QScrollArea>
#include <QFileDialog>
#include <QImage>
#include <QRegularExpression>
#include <QSettings>
#include <QDebug>
#include <QDateTime>
#include <QToolBar>
#include <QToolButton>
#include <QMenuBar>
#include <QMenu>
#include <QTimer>
#include <QStandardPaths>
#include <QImageWriter>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextOption>
#include <QRegularExpression>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointer>

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

using namespace EngineSettingsConstants;

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
    m_selectedField(nullptr),
    m_selectedField2(nullptr),
    m_movedField(nullptr),
    m_waitingSecondClick(false),
    m_analysisModel(nullptr)
{
    ui->setupUi(this);

    QToolBar* tb = ui->toolBar;                  // Designerで作った場合の例
    tb->setIconSize(QSize(18, 18));                  // ← 16px（お好みで 16/18/20/24 など）
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);  // ← テキストを消して高さを詰める

    // さらに詰めたい場合のパディング調整（任意）
    tb->setStyleSheet(
        "QToolBar{margin:0px; padding:0px; spacing:2px;}"
        "QToolButton{margin:0px; padding:2px;}"
    );

    // GUIを構成するWidgetなどのnew生成
    initializeComponents();

    setupRecordPane();

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
    startNewShogiGame(m_startSfenStr);

    // 対局者名と残り時間、将棋盤と棋譜、矢印ボタン、評価値グラフのグループを横に並べて表示
    setupHorizontalGameLayout();

    // 対局者名と残り時間、将棋盤、棋譜、矢印ボタン、評価値グラフのウィジェットと
    // info行の予想手、探索手、エンジンの読み筋のウィジェットを縦ボックス化して
    // セントラルウィジェットにセットする。
    initializeCentralGameDisplay();

    // 対局のメニュー表示を一部隠す。
    hideGameActions();

    // 棋譜欄の下の矢印ボタンを無効にする。
    disableArrowButtons();

    // 局面編集メニューの表示・非表示
    hidePositionEditMenu();

    // GUI全体のウィンドウサイズの読み込み。
    // 前回起動したウィンドウサイズに設定する。
    loadWindowSettings();

    // Thinking/USIログのモデルが揃った後で
    setupEngineAnalysisTab();

    // MainWindow ctor で、UIを作り終わった直後に
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

    // メニューのシグナルとスロット（メニューやボタンをクリックした際に実行される関数の指定）
    // メニューの項目をクリックすると、スロットの関数が実行される。
    // 「終了」
    // 設定ファイルにGUI全体のウィンドウサイズを書き込む。
    // また、将棋盤のマスサイズも書き込む。その後、ShogiBoardQを終了する。
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::saveSettingsAndClose);

    // 「名前を付けて保存」
    // 棋譜をファイルに保存する。
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::saveKifuToFile);

    // 「上書き保存」
    // 棋譜をファイルに上書き保存する。
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::overwriteKifuFile);

    // 「エンジン設定」
    // エンジン設定のダイアログを起動する。
    connect(ui->actionEngineSettings, &QAction::triggered, this, &MainWindow::displayEngineSettingsDialog);

    // 「バージョン情報」
    // GUIのバージョン情報を表示する。
    connect(ui->actionVersionInfo, &QAction::triggered, this, &MainWindow::displayVersionInformation);

    // 「ホームページ」
    // GUIのWebサイトをブラウザで表示する。
    connect(ui->actionOpenWebsite, &QAction::triggered, this, &MainWindow::openWebsiteInExternalBrowser);

    // 「対局」
    connect(ui->actionStartGame, &QAction::triggered, this, &MainWindow::initializeGame);

    // 「投了」
    // メニューで投了をクリックした時の処理を行う。
    connect(ui->actionResign, &QAction::triggered, this, &MainWindow::handleResignation);

    // 「盤面の回転」
    // 将棋盤を180度回転させる。それに伴って、対局者名、残り時間も入れ替える。
    // m_shogiView->flipMode ノーマル:0、反転:1
    // flipModeは、0か1かのフラグでm_shogiViewをリペイントする際に、
    // 先手が将棋盤の手前か後手が手前になるかが決まる。
    connect(ui->actionFlipBoard, &QAction::triggered, this, &MainWindow::flipBoardAndUpdatePlayerInfo);

    // 「将棋盤の画像をクリップボードにコピー」
    // 駒台を含む将棋盤全体の画像をクリップボードにコピーする。
    connect(ui->actionCopyBoardToClipboard, &QAction::triggered, this, &MainWindow::copyBoardToClipboard);

    // 「表示」の「思考」 思考タブの表示・非表示
    // info行の予想手、探索手、エンジンの読み筋を縦ボックス化したm_widget3の表示・非表示
    connect(ui->actionToggleEngineAnalysis, &QAction::triggered, this, &MainWindow::toggleEngineAnalysisVisibility);

    // 「すぐ指させる」
    // エンジンにstopコマンドを送る。
    // エンジンに対し思考停止を命令するコマンド。エンジンはstopを受信したら、できるだけすぐ思考を中断し、
    // bestmoveで指し手を返す。
    connect(ui->actionMakeImmediateMove, &QAction::triggered, this, &MainWindow::movePieceImmediately);

    // 「将棋盤の拡大」
    connect(ui->actionEnlargeBoard, &QAction::triggered, this, &MainWindow::enlargeBoard);

    // 「将棋盤の縮小」
    connect(ui->actionShrinkBoard, &QAction::triggered, this, &MainWindow::reduceBoardSize);

    // 「待った」
    // 待ったをした時、2手戻る。
    connect(ui->actionUndoMove, &QAction::triggered, this, &MainWindow::undoLastTwoMoves);

    // 「将棋盤の画像をファイルに保存」
    connect(ui->actionSaveBoardImage, &QAction::triggered, this, &MainWindow::saveShogiBoardImage);

    // 「開く」 棋譜ファイルを選択して読み込む。
    connect(ui->actionOpenKifuFile, &QAction::triggered, this, &MainWindow::chooseAndLoadKifuFile);

    // 「検討」
    connect(ui->actionConsideration, &QAction::triggered, this, &MainWindow::displayConsiderationDialog);

    // 「棋譜解析」
    connect(ui->actionAnalyzeKifu, &QAction::triggered, this, &MainWindow::displayKifuAnalysisDialog);

    // 「新規」
    // ShogiBoardQを初期画面表示に戻す。
    connect(ui->actionNewGame, &QAction::triggered, this, &MainWindow::resetToInitialState);

    // 「局面編集開始」
    connect(ui->actionStartEditPosition, &QAction::triggered, this, &MainWindow::beginPositionEditing);

    // 「局面編集終了」
    connect(ui->actionEndEditPosition, &QAction::triggered, this, &MainWindow::finishPositionEditing);

    // 「詰み探索」
    connect(ui->actionTsumeShogiSearch, &QAction::triggered, this, &MainWindow::displayTsumeShogiSearchDialog);

    // 対局終了ダイアログを表示する。
    connect(m_gameController, &ShogiGameController::gameOver, this, &MainWindow::displayGameOutcome);

    // 対局中に成るか不成で指すかのダイアログを表示する。
    connect(m_gameController, &ShogiGameController::showPromotionDialog, this, &MainWindow::displayPromotionDialog);

    // 将棋盤表示でエラーが発生した場合、エラーメッセージを表示する。
    connect(m_shogiView, &ShogiView::errorOccurred, this, &MainWindow::displayErrorMessage);

    // 駒のドラッグを終了する。
    connect(m_gameController, &ShogiGameController::endDragSignal, this, &MainWindow::endDrag);
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
    wireBoardInteractionController();

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

// 対局中のメニュー表示に変更する。
void MainWindow::setGameInProgressActions()
{
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

// 駒の移動元と移動先のマスのハイライトを消去する。
void MainWindow::clearMoveHighlights()
{
    if (m_boardController) {
        m_boardController->clearAllHighlights(); // ない場合は追加してください
        return;
    }
}

// 駒の移動元と移動先のマスをそれぞれ別の色でハイライトする。
void MainWindow::addMoveHighlights()
{
    // コントローラ経由で直前の1手をハイライト
    if (!m_boardController) return;

    // 直前の手が無ければ何もしない
    if (m_currentMoveIndex <= 0 || m_currentMoveIndex > m_gameMoves.size())
        return;

    const ShogiMove& mv = m_gameMoves.at(m_currentMoveIndex - 1);

    // BoardInteractionController に統一（座標は 0-based のまま渡す前提）
    m_boardController->showMoveHighlights(mv.fromSquare, mv.toSquare);
}

// 待ったをした場合、position文字列のセットと評価値グラフの値を削除する。
void MainWindow::handleUndoMove(int index)
{
    switch (m_playMode) {
    // 平手 Player1: Human, Player2: USI Engine
    case EvenHumanVsEngine:
    // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine（上手）
    case HandicapEngineVsEngine:
        // 待ったをした時の2手前のposition文字列をセット
        m_positionStr1 = m_positionStrList.at(index);

        // 待ったボタンを押す前の評価値グラフの値を削除
        // (X, Y) = (m_numberOfMoves, m_scoreCp.last())
        m_evalChart->removeLastP2();
        break;

    // 平手 Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:
        // 待ったをした時の2手前のposition文字列をセット
        m_positionStr1 = m_positionStrList.at(index);

        // 待ったボタンを押す前の評価値グラフの値を削除
        // (X, Y) = (m_numberOfMoves, m_scoreCp.last())
        m_evalChart->removeLastP1();
        break;

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        // 待ったをした時の2手前のposition文字列をセット
        m_positionStr1 = m_positionStrList.at(index);

        // 待ったボタンを押す前の評価値グラフの値を削除
        // (X, Y) = (m_numberOfMoves, m_scoreCp.last())
        m_evalChart->removeLastP1();
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        // 待ったをした時の2手前のposition文字列をセット
        m_positionStr2 = m_positionStrList.at(index);

        // 待ったボタンを押す前の評価値グラフの値を削除
        // (X, Y) = (m_numberOfMoves, m_scoreCp.last())
        m_evalChart->removeLastP2();
        break;

    // まだ対局を開始していない状態
    case NotStarted:

    // Player1: Human, Player2: Human
    case HumanVsHuman:

    // Player1: USI Engine, Player2: USI Engine
    case EvenEngineVsEngine:

    // 解析モード
    case AnalysisMode:

    // 検討モード
    case ConsidarationMode:

    // 詰将棋探索モード
    case TsumiSearchMode:

    // 対局モードエラー
    case PlayModeError:
        break;
    }

    // 待ったボタンを押す前の評価値を削除
    m_scoreCp.removeLast();
}

// 待ったボタンを押すと、2手戻る。
void MainWindow::undoLastTwoMoves()
{
    // --- 追加(1): いま計測中なら止める（巻き戻し時間を混入させない） ---
    if (m_turnTimerArmed) {        // H2H 用（両者人間）
        m_turnTimerArmed = false;
        m_turnTimer.invalidate();
    }
    if (m_humanTimerArmed) {       // HvE 用（人間側の手番タイマー）
        m_humanTimerArmed = false;
        m_humanTurnTimer.invalidate();
    }

    // ---- ここから下は既存処理（2手巻き戻し） ----

    int moveNumber = m_currentMoveIndex - 2;

    if (moveNumber >= 0) {
        m_gameMoves.removeLast();
        m_gameMoves.removeLast();

        handleUndoMove(moveNumber);

        m_positionStrList.removeLast();
        m_positionStrList.removeLast();

        m_currentMoveIndex = moveNumber;

        QString str = m_sfenRecord->at(m_currentMoveIndex);
        m_gameController->board()->setSfen(str);

        m_sfenRecord->removeLast();
        m_sfenRecord->removeLast();

        clearMoveHighlights();

        m_kifuRecordModel->removeLastItem();
        m_kifuRecordModel->removeLastItem();

        // 持ち時間・考慮時間も2手前へ
        m_shogiClock->undo();
    }

    if (m_currentMoveIndex) addMoveHighlights();

    // --- 追加(2): 盤と時計を戻し終えたので、表示の整合を先に更新 ---
    updateTurnAndTimekeepingDisplay();

    // --- 追加(3): 今の手番が「人間」なら計測を再アーム ---
    // Human/Engine の判定ヘルパ（既にお持ちならそれを使ってください）
    auto isHuman = [this](ShogiGameController::Player p) {
        switch (m_playMode) {
            case HumanVsHuman:                return true;                        // 両方人間
            case EvenHumanVsEngine:
            case HandicapHumanVsEngine:       return p == ShogiGameController::Player1; // 先手=人間
            case HandicapEngineVsHuman:       return p == ShogiGameController::Player2; // 後手=人間
            default:                           return false;                       // エンジンvsエンジンなど
        }
    };

    const auto sideToMove = m_gameController->currentPlayer();

    // 操作可否（人間の手番ならクリック可）
    m_shogiView->setMouseClickMode(isHuman(sideToMove));

    if (isHuman(sideToMove)) {
        // 描画負荷を思考時間に入れたくなければ次フレームでアーム
        QTimer::singleShot(0, this, [this]{
            // H2H なら armTurnTimerIfNeeded()、HvE なら armHumanTimerIfNeeded() を使う設計にしている場合は適宜切替
            if constexpr (true) {
                armTurnTimerIfNeeded();      // H2H 共通タイマー
            } else {
                armHumanTimerIfNeeded();     // HvE 用を使っている場合はこちら
            }
        });
    }

    // ★ 簡易復元：現在手数で決め打ち
    if (m_currentMoveIndex <= 0) {
        m_p1HasMoved = false; m_p2HasMoved = false;
    } else if (m_currentMoveIndex == 1) {
        // 直近に指したのは“今の手番の反対側”
        const bool nextIsP1 = (m_gameController->currentPlayer() == ShogiGameController::Player1);
        m_p1HasMoved = !nextIsP1;
        m_p2HasMoved =  nextIsP1;
    } else {
        // 2手以上進んでいれば両者一度は指している
        m_p1HasMoved = true; m_p2HasMoved = true;
    }
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


// 将棋盤を180度回転させる。それに伴って、対局者名、残り時間も入れ替える。
// m_shogiView->flipMode ノーマル:0、反転:1
// flipModeは、0か1かのフラグでm_shogiViewをリペイントする際に、
// 先手が将棋盤の手前か後手が手前になるかが決まる。
void MainWindow::flipBoardAndUpdatePlayerInfo()
{
    // 将棋盤が反転している場合（後手が手前）
    if (m_shogiView->getFlipMode()) {
        // 将棋盤を正常に戻すフラグflipModeを設定する。
        m_shogiView->setFlipMode(0);

        // 将棋の駒画像を各駒文字（1文字）にセットする。
        // 駒文字と駒画像をm_piecesに格納する。
        // m_piecesの型はQMap<char, QIcon>
        m_shogiView->setPieces();
    }
    // 将棋盤が正常な場合（先手が手前）
    else {
        //  将棋盤を反転させるフラグflipModeを設定する。
        m_shogiView->setFlipMode(1);

        // 将棋の駒画像を各駒文字（1文字）にセットする。
        // 駒文字と駒画像をm_piecesに格納する。
        // m_piecesの型はQMap<char, QIcon>
        m_shogiView->setPiecesFlip();
    }
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

// 対局者名と残り時間、将棋盤、棋譜、矢印ボタン、評価値グラフのウィジェットと
// info行の予想手、探索手、エンジンの読み筋のウィジェットを縦ボックス化して
// セントラルウィジェットにセットする。
void MainWindow::initializeCentralGameDisplay()
{
    // ★ 追加：分析タブを先に用意（m_tab を内部から引き出す）
    setupEngineAnalysisTab();

    auto* vboxLayout = new QVBoxLayout;
    vboxLayout->addWidget(m_hsplit);
    vboxLayout->setSpacing(0);
    vboxLayout->addWidget(m_tab);     // ← 既存どおり（中身は EngineAnalysisTab の QTabWidget）

    QWidget* newWidget = new QWidget;
    newWidget->setLayout(vboxLayout);

    QWidget* oldWidget = centralWidget();
    setCentralWidget(newWidget);
    if (oldWidget) {
        oldWidget->setParent(nullptr);
        oldWidget->deleteLater();
    }
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

// 将棋エンジン1に対して、gameover winコマンドとquitコマンドを送信する。
void MainWindow::sendCommandsToEngineOne()
{
    if (m_playMode != HumanVsHuman) {
        m_usi1->sendGameOverWinAndQuitCommands();
    }
}

// 将棋エンジン2に対して、gameover winコマンドとquitコマンドを送信する。
void MainWindow::sendCommandsToEngineTwo()
{
    // 対局モードが平手のエンジン対エンジンまたは、駒落ちのエンジン対エンジンの場合
    if ((m_playMode == EvenEngineVsEngine) || (m_playMode == HandicapEngineVsEngine)) {
        m_usi2->sendGameOverWinAndQuitCommands();
    }
}

// 将棋クロックの停止と将棋エンジンへの対局終了コマンド送信処理を行う。
void MainWindow::stopClockAndSendCommands()
{
    // 将棋クロックを停止する。
    m_shogiClock->stopClock();

    // 将棋エンジン1に対して、gameover winコマンドとquitコマンドを送信する。
    sendCommandsToEngineOne();

    // 将棋エンジン2に対して、gameover winコマンドとquitコマンドを送信する。
    sendCommandsToEngineTwo();
}

// 対局結果の表示とGUIの更新処理を行う。
void MainWindow::displayResultsAndUpdateGui()
{
    // --- 終局では人間用ストップウォッチを無効化 ---
    if (m_turnTimerArmed) { m_turnTimerArmed = false; m_turnTimer.invalidate(); }
    if (m_humanTimerArmed){ m_humanTimerArmed = false; m_humanTurnTimer.invalidate(); }

    // 残時間と考慮msを確定
    if (m_shogiClock) m_shogiClock->stopClock();

    // 盤操作を無効化
    if (m_shogiView) m_shogiView->setMouseClickMode(false);

    auto showOutcomeDeferred = [this](ShogiGameController::Result res) {
        updateRemainingTimeDisplay();
        QTimer::singleShot(0, this, [this, res]() { displayGameOutcome(res); });
    };

    // ★ 投了/時間切れ：現在手番の側の考慮時間を記録する
    if (m_gameController && m_shogiClock &&
        m_gameController->currentPlayer() == ShogiGameController::Player1) {
        m_shogiClock->applyByoyomiAndResetConsideration1();
        updateGameRecord(m_shogiClock->getPlayer1ConsiderationAndTotalTime());
        showOutcomeDeferred(ShogiGameController::Player2Wins);
    } else if (m_shogiClock) {
        m_shogiClock->applyByoyomiAndResetConsideration2();
        updateGameRecord(m_shogiClock->getPlayer2ConsiderationAndTotalTime());
        showOutcomeDeferred(ShogiGameController::Player1Wins);
    }

    enableArrowButtons();

    // ★ ここを m_kifuView → RecordPane 経由に変更
    if (m_recordPane && m_recordPane->kifuView()) {
        m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::SingleSelection);
    }

    const qint64 tms = ShogiUtils::nowMs();
    const char* causeStr =
        (m_lastGameOverCause == GameOverCause::Timeout)     ? "TIMEOUT" :
        (m_lastGameOverCause == GameOverCause::Resignation) ? "RESIGNATION" :
                                                              "OTHER";

    qDebug().nospace()
        << "[ARBITER] decision " << causeStr
        << " loser=" << (m_lastLoserIsP1 ? "P1" : "P2")
        << " at t+" << tms << "ms";
}

// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    stopClockAndSendCommands();
    m_shogiClock->markGameOver();

    const bool p1Resigns = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    setGameOverMove(GameOverCause::Resignation, /*loserIsPlayerOne=*/p1Resigns);

    displayResultsAndUpdateGui();
}

void MainWindow::handleEngineTwoResignation()
{
    m_shogiClock->stopClock();
    m_shogiClock->markGameOver();

    setGameOverMove(GameOverCause::Resignation, /*loserIsPlayerOne=*/false);
    stopClockAndSendGameOver(Winner::P1);

    displayResultsAndUpdateGui();
}

void MainWindow::handleEngineOneResignation()
{
    m_shogiClock->stopClock();
    m_shogiClock->markGameOver();

    setGameOverMove(GameOverCause::Resignation, /*loserIsPlayerOne=*/true);
    stopClockAndSendGameOver(Winner::P2);

    displayResultsAndUpdateGui();
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

    if (auto ec = m_recordPane ? m_recordPane->evalChart() : nullptr) {
        ec->appendScoreP1(m_currentMoveIndex, m_usi1->lastScoreCp(), invert);
    }

    // 従来通り記録も残す
    m_scoreCp.append(m_usi1->lastScoreCp());
}

// エンジン2の評価値グラフの再描画を行う。
void MainWindow::redrawEngine2EvaluationGraph()
{
    if (!m_usi2) return;  // 念のため

    // 旧ロジック：駒落ちのエンジン対エンジン以外は反転
    const bool invert = (m_playMode != HandicapEngineVsEngine);

    if (auto ec = m_recordPane ? m_recordPane->evalChart() : nullptr) {
        ec->appendScoreP2(m_currentMoveIndex, m_usi2->lastScoreCp(), invert);
    }

    // 既存仕様に合わせてそのまま記録
    m_scoreCp.append(m_usi2->lastScoreCp());
}

// 指す駒を左クリックで選択した場合、そのマスをオレンジ色にする。
void MainWindow::selectPieceAndHighlight(const QPoint& field)
{
    // クリックされたマスの将棋盤を取得する。
    auto* board = m_shogiView->board();

    // クリックされたマスの筋番号と段番号を取得する。
    const int file = field.x();
    const int rank = field.y();

    // クリックされたマスの駒の文字を取得する。
    QChar value = board->getPieceCharacter(file, rank);

    // 駒台の筋番号
    constexpr int BlackStandFile = 10;
    constexpr int WhiteStandFile = 11;

    if (!m_shogiView->positionEditMode()) {
        // 自分の駒台を選択したかどうかを判定する。
        bool isMyStand = ((m_gameController->currentPlayer() == m_gameController->Player1 && file == BlackStandFile) ||
                          (m_gameController->currentPlayer() == m_gameController->Player2 && file == WhiteStandFile));

        // 相手の駒台を選択した場合
        if ((file >= BlackStandFile) && !isMyStand) {
            // ドラッグ操作のリセットを行う。
            finalizeDrag();

            // 何もしない。
            return;
        }
    }

    // 駒台から駒を選択したかを判定する。
    bool isStand = (file == BlackStandFile || file == WhiteStandFile);

    // 駒台の駒の枚数が0以下の場合、何もしない。
    if (isStand && board->getPieceStand().value(value) <= 0) return;

    // 盤上をクリックした場合：駒がない空マスなら何もしない
    if (value == QChar(' ')) return;

    // ハイライト処理
    // 選択したマスをクリックポイントに設定する。
    m_clickPoint = field;

    // 前に選択したマスのハイライトを削除する。
    m_shogiView->removeHighlight(m_selectedField);

    // 選択したマスをオレンジ色にする。
    m_selectedField = new ShogiView::FieldHighlight(file, rank, QColor(255, 0, 0, 50));

    // 選択したマスを表示する。
    m_shogiView->addHighlight(m_selectedField);
}

// 棋譜を更新し、GUIの表示も同時に更新する。
void MainWindow::updateGameRecordAndGUI()
{
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

// ドラッグ操作のリセットを行う。
void MainWindow::finalizeDrag()
{
    // ドラッグを終了する。駒を移動してカーソルを戻す。
    endDrag();

    // 2回目のクリックを待っているかどうかを示すフラグをリセットする。
    m_waitingSecondClick = false;

    // 駒の移動元と移動先のマスのハイライトを消去する。
    if (m_boardController) m_boardController->clearSelectionHighlight();
}

// 将棋クロックの手番を設定する。
void MainWindow::updateTurnStatus(int currentPlayer)
{
    if (!m_shogiView) return;

    m_shogiClock->setCurrentPlayer(currentPlayer);

    // 1=先手, 2=後手 以外が来たら何もしない（既存の表示を維持）
    if (currentPlayer == 1) {
        m_shogiView->setActiveSide(true);   // 先手手番
    } else if (currentPlayer == 2) {
        m_shogiView->setActiveSide(false);  // 後手手番
    }
}

// 手番に応じて将棋クロックの手番変更する。
void MainWindow::updateTurnDisplay()
{
    // 手番が対局者1の場合
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // 将棋クロックの手番を対局者1に設定し、GUIの手番表示を更新する。
        updateTurnStatus(1);
    }
    // 手番が対局者2の場合
    else if (m_gameController->currentPlayer() == ShogiGameController::Player2) {
        // 将棋クロックの手番を対局者2に設定し、GUIの手番表示を更新する。
        updateTurnStatus(2);
    }
}

// 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
void MainWindow::updateTurnAndTimekeepingDisplay()
{
    // ★ 終局後は何もしない（時計は止める）
    if (m_gameIsOver) {
        qDebug() << "[ARBITER] suppress updateTurnAndTimekeepingDisplay (game over)";
        m_shogiClock->stopClock();
        updateRemainingTimeDisplay();   // 表示だけ整えるなら任意
        disarmHumanTimerIfNeeded();     // 人間用ストップウォッチがあれば停止
        return;
    }

    // 1) 今走っている側を止めて残時間確定
    m_shogiClock->stopClock();

    // 2) 直前に指した側へ increment/秒読みを適用 + 考慮時間の記録
    const bool nextIsP1 = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    if (nextIsP1) {
        // これから先手が指す → 直前に指したのは後手
        m_shogiClock->applyByoyomiAndResetConsideration2();
        updateGameRecord(m_shogiClock->getPlayer2ConsiderationAndTotalTime());
        m_shogiClock->setPlayer2ConsiderationTime(0);
        m_p2HasMoved = true;   // ← 後手は着手済み扱い
    } else {
        // これから後手が指す → 直前に指したのは先手
        m_shogiClock->applyByoyomiAndResetConsideration1();
        updateGameRecord(m_shogiClock->getPlayer1ConsiderationAndTotalTime());
        m_shogiClock->setPlayer1ConsiderationTime(0);
        m_p1HasMoved = true;   // ← 先手は着手済み扱い
    }

    // 3) 表示更新
    updateRemainingTimeDisplay();

    // 4) ★ USI に渡す残時間（ms）を“pre-add風”に整形
    qint64 bMs = 0, wMs = 0;
    computeGoTimesForUSI(bMs, wMs);
    m_bTime = QString::number(bMs);
    m_wTime = QString::number(wMs);

    // 5) 手番表示・時計再開
    updateTurnStatus(nextIsP1 ? 1 : 2);
    m_shogiClock->startClock();

    // ★ 新しい手番の「この手」の開始時刻を記録
    {
        const qint64 now = ShogiUtils::nowMs();
        if (nextIsP1) m_turnEpochP1Ms = now;
        else          m_turnEpochP2Ms = now;
    }

    if (isHumanTurn()) armHumanTimerIfNeeded();
    else               disarmHumanTimerIfNeeded();
}

void MainWindow::disarmHumanTimerIfNeeded()
{
    if (!m_humanTimerArmed) return;     // 既に無効なら何もしない
    m_humanTimerArmed = false;
    m_humanTurnTimer.invalidate();      // QElapsedTimer を無効化（elapsed() は 0 扱い）
    // qDebug() << "[HUMAN] timer disarmed";
}

// 局面編集モードで右クリックした駒を成る・不成の表示に変換する。
void MainWindow::togglePiecePromotionOnClick(const QPoint &field)
{
    // 駒をクリックした場合
    if (m_clickPoint.isNull()) {
        // 指す駒を左クリックで選択した時にそのマスをオレンジ色にする。
        selectPieceAndHighlight(field);

        // ハイライトを削除する。
        // 以下の行が無いと赤と黄色が混ざってしまう。
        m_shogiView->removeHighlight(m_movedField);

        // マスに駒がある場合
        if (m_shogiView->board()->getPieceCharacter(field.x(), field.y()) != ' ') {
            // 将棋盤内の駒は成駒になれるが駒台の駒は成れない。
            if (field.x() < 10) {
                // 右クリックで駒を成・不成に変換する。
                m_gameController->switchPiecePromotionStatusOnRightClick(field.x(), field.y());

                // クリックをリセットして選択していたハイライトを削除する。
                if (m_boardController) m_boardController->clearSelectionHighlight();
            }
        }
    }
    // すでに移動させる駒を選択している場合
    else {
        // クリックをリセットして選択していたハイライトを削除する。
        if (m_boardController) m_boardController->clearSelectionHighlight();
    }
}

// 対局結果を表示する。
void MainWindow::displayGameOutcome(ShogiGameController::Result result)
{
    QString text;

    switch (result) {
    // 対局者1が勝利した場合
    case ShogiGameController::Player1Wins:
        // 対局モード
        switch (m_playMode) {
        // 人間対人間
        case HumanVsHuman:

        // 平手の人間対エンジン
        case EvenHumanVsEngine:

        // 平手のエンジン対人間
        case EvenEngineVsHuman:

        // 平手のエンジン対エンジン
        case EvenEngineVsEngine:
            // 先手（黒）の勝ち
            text = tr("Sente (Black) wins.");
            break;

        // 駒落ちのエンジン対人間
        case HandicapEngineVsHuman:

        // 駒落ちの人間対エンジン
        case HandicapHumanVsEngine:

        // 駒落ちのエンジン対エンジン
        case HandicapEngineVsEngine:
            // 下手の勝ち
            text = tr("Shitate (Lower player) wins.");
            break;

        default:
            // プレイヤー1の勝ち
            text = tr("Player 1 wins.");
        }

        break;

    // 対局者2が勝利した場合
    case ShogiGameController::Player2Wins:
        // 対局モード
        switch (m_playMode) {

        // 人間対人間
        case HumanVsHuman:

        // 平手の人間対エンジン
        case EvenHumanVsEngine:

        // 平手のエンジン対人間
        case EvenEngineVsHuman:

        // 平手のエンジン対エンジン
        case EvenEngineVsEngine:
            // 後手（白）の勝ち
            text = tr("Gote (White) wins.");

            break;

        // 駒落ちのエンジン対人間
        case HandicapEngineVsHuman:

        // 駒落ちの人間対エンジン
        case HandicapHumanVsEngine:

        // 駒落ちのエンジン対エンジン
        case HandicapEngineVsEngine:
            // 上手の勝ち
            text = tr("Uwate (Upper player) wins.");

            break;

        default:
            // プレイヤー2の勝ち
            text = tr("Player 2 wins.");
        }

        break;

    default:
        // 引き分け
        text = tr("It is a draw.");
    }

    // 対局結果を表示する。
    // 非同期に開く（モーダル表示はされるが、open() なのでイベントループは回る）
    auto *box = new QMessageBox(QMessageBox::Information,
                                tr("Game Over"),
                                tr("The game has ended. %1").arg(text),
                                QMessageBox::Ok,
                                this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
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
    // GUIに登録された将棋エンジン番号を取得する。
    int engineNumber2 = m_startGameDialog->engineNumber2();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile2 = m_startGameDialog->getEngineList().at(engineNumber2).path;

    // 将棋エンジン名を取得する。
    QString engineName2 = m_startGameDialog->engineName2();

    try {
        // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
        m_usi1->initializeAndStartEngineCommunication(m_engineFile2, engineName2);
    } catch (const std::exception& e) {
        // エラーを再スローし、エラーメッセージを表示する。
        throw;
    }
}

// 対局者1をエンジン1で初期化して対局を開始する。
void MainWindow::initializeAndStartPlayer1WithEngine1()
{
    // GUIに登録された将棋エンジン番号を取得する。
    int engineNumber1 = m_startGameDialog->engineNumber1();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile1 = m_startGameDialog->getEngineList().at(engineNumber1).path;

    // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
    QString engineName1 = m_startGameDialog->engineName1();

    try {
        // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
        m_usi1->initializeAndStartEngineCommunication(m_engineFile1, engineName1);
    } catch (const std::exception& e) {
        // エラーを再スローし、エラーメッセージを表示する。
        throw;
    }
}

// 対局者2をエンジン2で初期化して対局を開始する。
void MainWindow::initializeAndStartPlayer2WithEngine2()
{
    // GUIに登録された将棋エンジン番号を取得する。
    int engineNumber2 = m_startGameDialog->engineNumber2();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile2 = m_startGameDialog->getEngineList().at(engineNumber2).path;

    // 将棋エンジン名を取得する。
    QString engineName2 = m_startGameDialog->engineName2();

    try {
        // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
        m_usi2->initializeAndStartEngineCommunication(m_engineFile2, engineName2);
    } catch (const std::exception& e) {
        // エラーを再スローし、エラーメッセージを表示する。
        throw;
    }
}

// 対局者1をエンジン2で初期化して対局を開始する。
void MainWindow::initializeAndStartPlayer1WithEngine2()
{
    // GUIに登録された将棋エンジン番号を取得する。
    int engineNumber1 = m_startGameDialog->engineNumber1();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile1 = m_startGameDialog->getEngineList().at(engineNumber1).path;

    // 将棋エンジン名を取得する。
    QString engineName1 = m_startGameDialog->engineName1();

    try {
        // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
        m_usi2->initializeAndStartEngineCommunication(m_engineFile1, engineName1);
    } catch (const std::exception& e) {
        // エラーを再スローし、エラーメッセージを表示する。
        throw;
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
    // 将棋エンジン対人間の場合、人間側を手前にするかどうかを取得する。
    bool isShowHumanInFront = m_startGameDialog->isShowHumanInFront();

    // 対局モード
    switch (m_playMode) {

    // Player1: Human, Player2: Human
    case HumanVsHuman:
        //begin
        qDebug() << "HumanVsHuman = " << HumanVsHuman;
        //end

        startHumanVsHumanGame();

        break;

    // Player1: Human, Player2: USI Engine
    case EvenHumanVsEngine:
        //begin
        qDebug() << "EvenHumanVsEngine = " << EvenHumanVsEngine;
        //end

        startHumanVsEngineGame();

        break;

    // Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:
        //begin
        qDebug() << "EvenEngineVsHuman = " << EvenEngineVsHuman;
        //end

        if (isShowHumanInFront) {
            // 盤面を反転して対局者の情報を更新し、人間側が手前に来るようにする。
            flipBoardAndUpdatePlayerInfo();
        }

        startEngineVsHumanGame();

        break;

    // Player1: USI Engine, Player2: USI Engine
    case EvenEngineVsEngine:
        //begin
        qDebug() << "EvenEngineVsEngine = " << EvenEngineVsEngine;
        //end

        startEngineVsEngineGame();

        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        //begin
        qDebug() << "HandicapEngineVsHuman = " << HandicapEngineVsHuman;
        //end

        if (isShowHumanInFront) {
            // 盤面を反転して対局者の情報を更新し、人間側が手前に来るようにする。
            flipBoardAndUpdatePlayerInfo();
        }

        startHumanVsEngineGame();

        break;

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        //begin
        qDebug() << "HandicapHumanVsEngine = " << HandicapHumanVsEngine;
        //end

        startEngineVsHumanGame();

        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine（上手）
    case HandicapEngineVsEngine:
        //begin
        qDebug() << "HandicapEngineVsEngine = " << HandicapEngineVsEngine;
        //end

        startEngineVsEngineGame();

        break;

    default:
        qDebug() << "Invalid PlayMode: " << m_playMode;
    }
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
    view->verticalHeader()->setVisible(false);
    view->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    view->setColumnWidth(0, 200);
    view->setColumnWidth(1, 200);
    view->setColumnWidth(2, 200);
    view->setColumnWidth(3, 200);

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(8,8,8,8);
    lay->addWidget(view);

    dlg->resize(1000, 600);
    dlg->open(); // 非モーダル表示（必要なら dlg->exec() に変更）
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

    if (m_analyzeGameRecordDialog->exec() == QDialog::Accepted) {
        // 解析結果ビューを生成して表示（ビューはローカルでOK）
        displayAnalysisResults();

        try {
            analyzeGameRecord();
        } catch (const std::exception& e) {
            // 解析モデルはメンバなのでクリーンアップ
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
    // 「時間切れを負けにする」フラグ
    m_isLoseOnTimeout = m_startGameDialog->isLoseOnTimeout();

    const int byoyomiSec1     = m_startGameDialog->byoyomiSec1();
    const int byoyomiSec2     = m_startGameDialog->byoyomiSec2();
    const int addEachMoveSec1 = m_startGameDialog->addEachMoveSec1();
    const int addEachMoveSec2 = m_startGameDialog->addEachMoveSec2();

    if (byoyomiSec1 > 0 || byoyomiSec2 > 0) {
        // 秒読み方式
        m_useByoyomi         = true;
        m_byoyomiMilliSec1   = byoyomiSec1 * 1000;
        m_byoyomiMilliSec2   = byoyomiSec2 * 1000;
        m_addEachMoveMiliSec1 = 0;
        m_addEachMoveMiliSec2 = 0;
    } else if (addEachMoveSec1 > 0 || addEachMoveSec2 > 0) {
        // フィッシャー（1手ごと加算）方式
        m_useByoyomi         = false;
        m_byoyomiMilliSec1   = 0;
        m_byoyomiMilliSec2   = 0;
        m_addEachMoveMiliSec1 = addEachMoveSec1 * 1000;
        m_addEachMoveMiliSec2 = addEachMoveSec2 * 1000;
    } else {
        // どちらも0：持ち時間のみ（便宜上 byoyomi=0 として扱う）
        m_useByoyomi         = true;
        m_byoyomiMilliSec1   = 0;
        m_byoyomiMilliSec2   = 0;
        m_addEachMoveMiliSec1 = 0;
        m_addEachMoveMiliSec2 = 0;
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

// 現在の局面で開始する場合に必要なListデータなどを用意する。
void MainWindow::prepareDataCurrentPosition()
{
    // 前対局で格納したSFEN文字列の数
    int n = m_sfenRecord->size();

    // 前対局で選択した途中の局面以降のListデータを削除する。
    for (int i = n; i > m_currentMoveIndex + 1; i--) {
        // 棋譜欄データを部分削除する。
        m_moveRecords->removeLast();

        // SFEN文字列データの部分削除する。
        m_sfenRecord->removeLast();
    }

    // ListModelRecordの全データを削除する。
    m_kifuRecordModel->clearAllItems();

    // 前対局で選択した途中の局面まで棋譜欄に棋譜データを1行ずつ格納する。
    for (int i = 0; i < m_currentMoveIndex; i++) {
        m_kifuRecordModel->appendItem(m_moveRecords->at(i));
    }

    // 途中からの対局の場合、駒の移動元と移動先のマスをそれぞれ別の色でハイライトする。
    if (n > 1) addMoveHighlights();

    // 開始局面文字列をSFEN形式でセットする。（sfen文字あり）
    m_startPosStr = "sfen " + m_sfenRecord->last();

    // 開始局面文字列をSFEN形式でセットする。（sfen文字なし）
    m_startSfenStr = m_sfenRecord->last();

    // 現局面のSFEN文字列（sfen文字あり）
    m_currentSfenStr = m_startPosStr;
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

// 対局者1の残り時間の文字色を赤色に指定する。
void MainWindow::setPlayer1TimeTextToRed()
{
    // パレットを赤色に指定する。
    QPalette palette = m_shogiView->blackClockLabel()->palette();
    palette.setColor(QPalette::WindowText, Qt::red);

    // 対局者1の残り時間の文字色を赤色に指定する。
    m_shogiView->blackClockLabel()->setPalette(palette);

    // 対局者1の残り時間を0にする。
    m_bTime = "0";
}

// 対局者2の残り時間の文字色を赤色に指定する。
void MainWindow::setPlayer2TimeTextToRed()
{  
    // パレットを赤色に指定する。
    QPalette palette = m_shogiView->whiteClockLabel()->palette();
    palette.setColor(QPalette::WindowText, Qt::red);

    // 対局者1の残り時間の文字色を赤色に指定する。
    m_shogiView->whiteClockLabel()->setPalette(palette);

    // 対局者2の残り時間を0にする。
    m_wTime = "0";
}

// 残り時間をセットしてタイマーを開始する。
void MainWindow::setTimerAndStart()
{
    // 将棋クロックのインスタンスを生成する。
    m_shogiClock = new ShogiClock;

    // GUIの残り時間表示を更新する。
    connect(m_shogiClock, &ShogiClock::timeUpdated, this, &MainWindow::updateRemainingTimeDisplay);

    // 対局者1の残り時間が0になった場合、対局者1の残り時間の文字色を赤色に指定する。
    connect(m_shogiClock, &ShogiClock::player1TimeOut, this, &MainWindow::setPlayer1TimeTextToRed);

    // 対局者2の残り時間が0になった場合、対局者2の残り時間の文字色を赤色に指定する。
    connect(m_shogiClock, &ShogiClock::player2TimeOut, this, &MainWindow::setPlayer2TimeTextToRed);

    // 対局者の持ち時間が0になった場合、時間切れの処理を行う。
    connect(m_shogiClock, &ShogiClock::player1TimeOut, this, &MainWindow::onPlayer1TimeOut);
    connect(m_shogiClock, &ShogiClock::player2TimeOut, this, &MainWindow::onPlayer2TimeOut);


    // 対局者1の持ち時間の時間を取得する。
    int basicTimeHour1 = m_startGameDialog->basicTimeHour1();

    // 対局者1の持ち時間の分を取得する。
    int basicTimeMinutes1 = m_startGameDialog->basicTimeMinutes1();

    // 対局者2の持ち時間の時間を取得する。
    int basicTimeHour2 = m_startGameDialog->basicTimeHour2();

    // 対局者2の持ち時間の分を取得する。
    int basicTimeMinutes2 = m_startGameDialog->basicTimeMinutes2();

    // 対局者1の秒読み時間を取得する。
    int byoyomi1 = m_startGameDialog->byoyomiSec1();

    // 対局者2の秒読み時間を取得する。
    int byoyomi2 = m_startGameDialog->byoyomiSec2();

    // 対局者1の1手ごとの加算時間を取得する。
    // 秒読みが0より大きい場合、1手ごとの加算時間は必ず0になる。
    int binc = m_startGameDialog->addEachMoveSec1();

    // 対局者2の1手ごとの加算時間を取得する。
    // 秒読みが0より大きい場合、1手ごとの加算時間は必ず0になる。
    int winc = m_startGameDialog->addEachMoveSec2();

    // 対局者1の残り時間を秒に変換する。
    int remainingTime1 = basicTimeHour1 * 3600 + basicTimeMinutes1 * 60;

    // 対局者2の残り時間を秒に変換する。
    int remainingTime2 = basicTimeHour2 * 3600 + basicTimeMinutes2 * 60;

    // 時間切れを負けにするかどうかのフラグを取得する。
    bool isLoseOnTimeout = m_startGameDialog->isLoseOnTimeout();

    // 「持ち時間制か？」（どちらかの基本時間/秒読み/加算が設定されているか）
    bool hasTimeLimit =
        (basicTimeHour1*3600 + basicTimeMinutes1*60) > 0 ||
        (basicTimeHour2*3600 + basicTimeMinutes2*60) > 0 ||
        byoyomi1 > 0 || byoyomi2 > 0 ||
        binc > 0 || winc > 0;

    // 0になったら負け扱いにするか（表示とロジックを分離）
    m_shogiClock->setLoseOnTimeout(isLoseOnTimeout);

    //begin
    qDebug() << "in MainWindow::setTimerAndStart";
    qDebug() << "basicTimeHour1 = " << basicTimeHour1;
    qDebug() << "basicTimeMinutes1 = " << basicTimeMinutes1;
    qDebug() << "basicTimeHour2 = " << basicTimeHour2;
    qDebug() << "basicTimeMinutes2 = " << basicTimeMinutes2;
    qDebug() << "byoyomi1 = " << byoyomi1;
    qDebug() << "byoyomi2 = " << byoyomi2;
    qDebug() << "binc = " << binc;
    qDebug() << "winc = " << winc;
    qDebug() << "remainingTime1 = " << remainingTime1;
    qDebug() << "remainingTime2 = " << remainingTime2;
    qDebug() << "m_isLoseOnTimeout = " << m_isLoseOnTimeout;
    //end

    // 各対局者の残り時間を設定する。
    m_shogiClock->setPlayerTimes(remainingTime1, remainingTime2, byoyomi1, byoyomi2, binc, winc,
                                 hasTimeLimit);

    // setPlayerTimes(...) の直後など “開始前” に一度だけ
    m_initialTimeP1Ms = m_shogiClock->getPlayer1TimeIntMs();
    m_initialTimeP2Ms = m_shogiClock->getPlayer2TimeIntMs();

    // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
    updateTurnDisplay();

    // 残り時間を更新する。
    m_shogiClock->updateClock();

    // タイマーを開始する。
    m_shogiClock->startClock();
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
    // エラーが発生していない状態に設定する。
    m_errorOccurred = false;

    // 将棋盤に関するクラスのエラー設定をエラーが発生していない状態に設定する。
    m_shogiView->setErrorOccurred(false);

    // 選択したマスを色付けするのに必要な変数
    m_selectedField = nullptr;
    m_selectedField2 = nullptr;

    // 駒を指した先のマス（色付けで使用）
    m_movedField = nullptr;

    // 対局ダイアログを生成する。
    m_startGameDialog = new StartGameDialog;

    // 対局ダイアログを実行し、OKボタンをクリックした場合
    if (m_startGameDialog->exec() == QDialog::Accepted) {
        // ゲームカウントを1加算する。
        m_gameCount++;

        // 対局数が2以上の時、ShogiBoardQを初期画面表示に戻す。
        if (m_gameCount > 1) resetToInitialState();

        // 対局者の残り時間と秒読み時間を設定する。
        setRemainingTimeAndCountDown();

        // 対局ダイアログから各オプションを取得する。
        getOptionFromStartGameDialog();

        // 「将棋エンジン 対 将棋エンジン」
        const bool engineVsEngine =
            (m_playMode == EvenEngineVsEngine) ||
            (m_playMode == HandicapEngineVsEngine);

        if (m_analysisTab) {
            m_analysisTab->setSecondEngineVisible(engineVsEngine);
        }

        // 棋譜ファイル情報の作成
        makeKifuFileHeader();

        int startingPositionNumber = m_startGameDialog->startingPositionNumber();

        // 開始局面が「現在の局面」の場合
        if (startingPositionNumber == 0) {
            // 現在の局面で開始する場合に必要なListデータ等の準備
            prepareDataCurrentPosition();
        }
        // 開始局面が「初期局面」の場合
        else {
            try {
                // 初期局面からの対局の場合の準備
                prepareInitialPosition();
            } catch (const std::exception& e) {
                // エラーメッセージを表示する。
                displayErrorMessage(e.what());

                // 対局ダイアログを削除する。
                delete m_startGameDialog;

                return;
            }
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
        startNewShogiGame(m_startSfenStr);

        // 対局メニューの表示・非表示
        setGameInProgressActions();

        // 対局が人間対人間以外の場合
        if (m_playMode) {
            disableArrowButtons();
            if (m_recordPane && m_recordPane->kifuView()) {
                m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::NoSelection);
            }
        }

        // 現在の手番を設定
        setCurrentTurn();

        // 残り時間をセットしてタイマーを開始する。
        setTimerAndStart();

        //begin
        qDebug() << "---- initializeGame() ---";
        qDebug() << "m_bTime = " << m_bTime;
        qDebug() << "m_wTime = " << m_wTime;
        //end

        try {
            // 対局モード（人間対エンジンなど）に応じて対局処理を開始する。
            startGameBasedOnMode();
        } catch (const std::exception& e) {
            // エラーメッセージを表示する。
            displayErrorMessage(e.what());
        }
    }

    // 対局ダイアログを削除する。
    delete m_startGameDialog;
}

// GUIの残り時間表示を更新する。
void MainWindow::updateRemainingTimeDisplay()
{
    const QString p1 = m_shogiClock->getPlayer1TimeString(); // 実残りms→切り上げ
    const QString p2 = m_shogiClock->getPlayer2TimeString();

    m_shogiView->blackClockLabel()->setText(p1);
    m_shogiView->whiteClockLabel()->setText(p2);

    const bool p1turn = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    const qint64 activeMs = p1turn ? m_shogiClock->getPlayer1TimeIntMs()
                                   : m_shogiClock->getPlayer2TimeIntMs();

    const bool hasByoyomi = p1turn ? m_shogiClock->hasByoyomi1()
                                   : m_shogiClock->hasByoyomi2();
    const bool inByoyomi  = p1turn ? m_shogiClock->byoyomi1Applied()
                                   : m_shogiClock->byoyomi2Applied();
    const bool enableUrgency = (!hasByoyomi) || inByoyomi;
    const qint64 msForUrgency = enableUrgency ? activeMs
                                              : std::numeric_limits<qint64>::max();

    m_shogiView->applyClockUrgency(msForUrgency);

    // （任意）盤面描画用に ms を渡す。描画側がここから時間文字列を再計算しない設計が安全。
    m_shogiView->setBlackTimeMs(m_shogiClock->getPlayer1TimeIntMs());
    m_shogiView->setWhiteTimeMs(m_shogiClock->getPlayer2TimeIntMs());

    qCDebug(ClockLog) << "[UI] P1(ms)=" << m_shogiClock->getPlayer1TimeIntMs()
                      << "P2(ms)=" << m_shogiClock->getPlayer2TimeIntMs()
                      << "P1(label)=" << p1
                      << "P2(label)=" << p2;
}

void ShogiView::applyClockUrgency(qint64 activeRemainMs)
{
    Urgency next = Urgency::Normal;
    if      (activeRemainMs <= kWarn5Ms)  next = Urgency::Warn5;
    else if (activeRemainMs <= kWarn10Ms) next = Urgency::Warn10;

    if (next != m_urgency) {
        m_urgency = next;
        setUrgencyVisuals(m_urgency);
    }
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
    m_clickPoint = QPoint();
    m_shogiView->removeHighlightAllData();
    m_shogiView->blackClockLabel()->setText("00:00:00");
    m_shogiView->whiteClockLabel()->setText("00:00:00");

    // 棋譜と評価値
    if (m_kifuRecordModel) m_kifuRecordModel->clearAllItems();
    if (m_evalChart)       m_evalChart->clearAll();

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

// ===================== 司令塔 =====================
void MainWindow::loadKifuFromFile(const QString& filePath)
{
    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    const QString initialSfen = prepareInitialSfen(filePath, teaiLabel);

    m_isKifuReplay = true;

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

    // 本譜
    const QList<KifDisplayItem>& disp = res.mainline.disp;
    m_usiMoves = res.mainline.usiMoves;

    // 終局/中断判定（本譜末尾の見た目テキストで判定）
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
        return;
    }

    // 3) 本譜のSFEN列と m_gameMoves を再構築
    rebuildSfenRecord(initialSfen, m_usiMoves, hasTerminal);
    rebuildGameMoves(initialSfen, m_usiMoves);

    // 4) 棋譜表示へ反映（本譜）
    displayGameRecord(disp);

    // ★★★ 追加：本譜スナップショットを保存（buildResolvedLinesAfterLoadの材料）
    m_dispMain = disp;
    m_sfenMain = *m_sfenRecord;   // 0..N のSFEN列
    m_gmMain   = m_gameMoves;     // 1..N のUSIムーブ

    // ★★★ 追加：（任意だが推奨）分岐のバケツ化と順序リスト
    m_variationsByPly.clear();
    m_variationsSeq.clear();
    for (const KifVariation& kv : res.variations) {
        KifLine L = kv.line;
        L.startPly = kv.startPly;
        if (L.disp.isEmpty()) continue;
        m_variationsByPly[L.startPly].push_back(L);
        m_variationsSeq.push_back(L);
    }

    // 5) テーブル初期選択（RecordPane経由）…（あなたの現状コードのまま）
    // 5) RecordPane 側のビューを初期化（このままでOK）
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

    // ★★★ ここから追加：最小構成の解決済み行を作る
    {
        m_resolvedRows.clear();

        ResolvedRow r;
        r.startPly = 1;
        r.disp     = disp;           // 本譜の表示列（1..N）
        r.sfen     = *m_sfenRecord;  // 0..N のSFEN
        r.gm       = m_gameMoves;    // 1..N のUSIムーブ
        r.varIndex = -1;             // 本譜

        m_resolvedRows.push_back(r);
        m_activeResolvedRow = 0;
        m_activePly         = 0;

        // 0手目（開始局面）を適用
        applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/0);
    }

    // 6) そのほか UI の整合
    enableArrowButtons();
    logImportSummary(filePath, m_usiMoves, disp, teaiLabel, parseWarn, QString());

    // 7) 分岐候補欄を簡易反映（先頭手を一覧化）
    if (m_kifuBranchModel) {
        m_kifuBranchModel->clearAllItems();
        for (const KifVariation& v : res.variations) {
            if (v.line.disp.isEmpty()) continue;
            // 最初の手を候補として表示
            auto* d = new KifuBranchDisplay(v.line.disp.first().prettyMove, m_kifuBranchModel);
            m_kifuBranchModel->appendItem(d);
        }
    }

    // 8) 「後勝ち」で解決済み行を構築（既存実装）
    buildResolvedLinesAfterLoad();

    // 9) EngineAnalysisTab へ分岐ツリー行データを供給
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

        // 読み込み直後は「本譜 0手」をハイライト（必要なら）
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }

    // 10) ログ（任意）
    logImportSummary(filePath, m_usiMoves, disp, teaiLabel, parseWarn, QString());
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
        if (m_analysisTab) {
            const QString head = (0 < m_commentsByRow.size() ? m_commentsByRow[0].trimmed()
                                                             : QString());
            m_analysisTab->setCommentText(head.isEmpty() ? tr("コメントなし") : head);
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

    // コメント表示は EngineAnalysisTab に集約
    if (m_analysisTab) {
        // setCommentText(const QString&) は前回ご案内の通り EngineAnalysisTab に追加済みの想定
        m_analysisTab->setCommentText(toShow);
    }
    // 旧: m_branchText / m_branchTextInTab1 には一切触れない
}

// 棋譜を更新し、GUIの表示も同時に更新する。
// elapsedTimeは指し手にかかった時間を表す文字列
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    qCDebug(ClockLog) << "in MainWindow::updateGameRecord";
    qCDebug(ClockLog) << "elapsedTime=" << elapsedTime;

    // ★ 終局後に終局行を既に追記済みなら、これ以上は書かない
    if (m_gameIsOver && m_gameoverMoveAppended) {
        qCDebug(ClockLog) << "[KIFU] suppress updateGameRecord after game over";
        return;
    }

    // ★ 手文字列が空のときは棋譜行を作らない（空行防止）
    if (m_lastMove.trimmed().isEmpty()) {
        qCDebug(ClockLog) << "[KIFU] skip empty move line";
        return;
    }

    // --- ここから下は「再生中」でも実行してOK（棋譜欄の行を作る） ---

    // 手数をインクリメントし、文字列に変換する。
    QString moveNumberStr = QString::number(++m_currentMoveIndex);

    // 手数表示のための空白を算出し、生成する。
    QString spaceStr = QString(qMax(0, 4 - moveNumberStr.length()), ' ');

    // 表示用レコードを生成する。（手数と最後の手）
    QString recordLine = spaceStr + moveNumberStr + " " + m_lastMove;

    // KIF形式レコードを生成する。（消費時間含む）
    QString kifuLine = recordLine + " ( " + elapsedTime + ")";
    kifuLine.remove(QStringLiteral("▲"));
    kifuLine.remove(QStringLiteral("△"));

    // KIF形式のデータを保存する。
    m_kifuDataList.append(kifuLine);

    // 指し手のレコードを生成し、保存する。
    if (m_moveRecords) {
        m_moveRecords->append(new KifuDisplay(recordLine, elapsedTime));
    }
    if (m_kifuRecordModel && m_moveRecords && !m_moveRecords->isEmpty()) {
        m_kifuRecordModel->appendItem(m_moveRecords->last());
    }

    // === ここから下が時計まわり。KIF再生中はスキップ ===
    if (m_isKifuReplay) {
        qCDebug(ClockLog) << "[KIFU] replay mode: skip clock logging";
        return;
    }

    // 追加の安全策：ポインタ存在チェック
    if (!m_shogiClock || !m_gameController) {
        qCDebug(ClockLog) << "[KIFU] clock/controller not ready, skip";
        return;
    }

    // live対局時のみログ
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // これから先手が考える＝最後に指したのは後手
        qCDebug(ClockLog) << "[KIFU] last-move consider_ms(P2)="
                          << m_shogiClock->getPlayer2ConsiderationTimeMs()
                          << " rem_ms(P2)=" << m_shogiClock->getPlayer2TimeIntMs();
    } else {
        qCDebug(ClockLog) << "[KIFU] last-move consider_ms(P1)="
                          << m_shogiClock->getPlayer1ConsiderationTimeMs()
                          << " rem_ms(P1)=" << m_shogiClock->getPlayer1TimeIntMs();
    }
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

// 検討を開始する。
void MainWindow::startConsidaration()
{
    // 対局モードを棋譜解析モードに設定する。
    m_playMode = ConsidarationMode;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを削除する。
    if (m_usi1 != nullptr) delete m_usi1;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを生成する。
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode,this);

    connect(m_usi1, &Usi::bestMoveResignReceived, this, &MainWindow::processResignCommand);

    // GUIに登録された将棋エンジン番号を取得する。
    int engineNumber1 = m_considerationDialog->getEngineNumber();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile1 = m_considerationDialog->getEngineList().at(engineNumber1).path;

    // 将棋エンジン名を取得する。
    QString engineName1 = m_considerationDialog->getEngineName();

    m_usi1->setLogIdentity("[E1]", "P1", engineName1);

    if (m_usi1) m_usi1->setSquelchResignLogging(false);

    try {
        // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
        m_usi1->initializeAndStartEngineCommunication(m_engineFile1, engineName1);
    } catch (const std::exception& e) {
        // エラーを再スローし、エラーメッセージを表示する。
        throw;
    }

    // 手番を設定する。
    if (m_gameMoves.at(m_currentMoveIndex).movingPiece.isUpper()) {
        m_gameController->setCurrentPlayer(ShogiGameController::Player1);
    } else {
        m_gameController->setCurrentPlayer(ShogiGameController::Player2);
    }

    // positionコマンド文字列の生成
    m_positionStr1 = m_positionStrList.at(m_currentMoveIndex);

    // 時間無制限の場合
    if (m_considerationDialog->unlimitedTimeFlag()) {
        m_byoyomiMilliSec1 = 0;
    }
    // 時間制限の場合
    else {
        // 秒読み時間を設定（単位はミリ秒）
        m_byoyomiMilliSec1 = m_considerationDialog->getByoyomiSec() * 1000;
    }

    // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
    m_usi1->executeAnalysisCommunication(m_positionStr1, m_byoyomiMilliSec1);
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

// 棋譜解析を開始する。
void MainWindow::analyzeGameRecord()
{
    // 対局モードを棋譜解析モードに設定する。
    m_playMode = AnalysisMode;

    // 秒読み時間を設定（単位はミリ秒）
    m_byoyomiMilliSec1 = m_analyzeGameRecordDialog->byoyomiSec() * 1000;

    int startIndex;

    if (m_analyzeGameRecordDialog->initPosition()) {
        startIndex = 0;
    } else {
        startIndex = m_currentMoveIndex;
    }

    // 将棋エンジンとUSIプロトコルに関するクラスの削除と生成
    if (m_usi1 != nullptr) delete m_usi1;
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode,this);

    // GUIに登録された将棋エンジン番号を取得する。
    int engineNumber1 = m_analyzeGameRecordDialog->engineNumber();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile1 = m_analyzeGameRecordDialog->engineList().at(engineNumber1).path;

    // 将棋エンジン名を取得する。
    QString engineName1 = m_analyzeGameRecordDialog->engineName();

    m_usi1->setLogIdentity("[E1]", "P1", engineName1);

    if (m_usi1) m_usi1->setSquelchResignLogging(false);

    m_usi1->initializeAndStartEngineCommunication(m_engineFile1, engineName1);

    // 前の手の評価値
    int previousScore = 0;

    // 初手から最終手までの指し手までループする。
    //for (int moveIndex = startIndex; moveIndex < m_gameMoves.size(); ++moveIndex) {
    for (int moveIndex = startIndex; moveIndex < 8; ++moveIndex) {
        // 各手に対応するpositionコマンド文字列を取得する。
        m_positionStr1 = m_positionStrList.at(moveIndex);

        std::cout << "m_gameMoves.at(" << moveIndex << ").movingPiece = " << m_gameMoves.at(moveIndex).movingPiece.toLatin1() << std::endl;

        // 手番を設定する。
        if (m_gameMoves.at(moveIndex).movingPiece.isUpper()) {
            m_gameController->setCurrentPlayer(ShogiGameController::Player1);
        } else {
            m_gameController->setCurrentPlayer(ShogiGameController::Player2);
        }

        m_usi1->executeAnalysisCommunication(m_positionStr1, m_byoyomiMilliSec1);

        // 指し手を取得する。
        QString currentMoveRecord = m_moveRecords->at(moveIndex)->currentMove();

        // 評価値を取得する。
        QString currentScore = m_usi1->scoreStr();

        // 今回の指し手と前回の指し手の評価値の差を求める。
        QString scoreDifference = QString::number(currentScore.toInt() - previousScore);

        // 将棋エンジンの読み筋を取得する。
        QString engineReadout = m_usi1->pvKanjiStr();

        // 前の手の評価値として現在の評価値を代入する。
        previousScore = currentScore.toInt();

        // 棋譜解析結果をモデルに追加する。
        m_analysisModel->appendItem(new KifuAnalysisResultsDisplay(currentMoveRecord, currentScore, scoreDifference, engineReadout));

        // 棋譜解析結果の一番下が見えるようにビューを表示する。
        m_analysisResultsView->scrollToBottom();

        // 棋譜解析結果のビューを更新する。
        m_analysisResultsView->update();

        // 現局面から1手進める（Controller/スロットに依存しない）
        if (!m_resolvedRows.isEmpty()) {
            const int row    = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
            const int maxPly = m_resolvedRows[row].disp.size();
            const int cur    = qBound(0, m_activePly, maxPly);
            if (cur < maxPly) {
                applySelect(row, cur + 1);   // ← 旧 navigateToNextMove() 相当
            }
        }

        // 評価値グラフを更新する。
        redrawEngine1EvaluationGraph();
    }

    // 将棋エンジンにquitコマンドを送る。
    m_usi1->sendQuitCommand();
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
    resize(settings.value("SizeRelated/mainWindowSize").toSize());
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

// 局面編集を開始する。
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
    setGameInProgressActions();

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
        wireBoardInteractionController();
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
                this, &MainWindow::switchTurns,
                Qt::UniqueConnection);

        connect(ui->flatHandInitialPosition, &QAction::triggered,
                this, &MainWindow::setStandardStartPosition,
                Qt::UniqueConnection);

        connect(ui->shogiProblemInitialPosition, &QAction::triggered,
                this, &MainWindow::setTsumeShogiStartPosition,
                Qt::UniqueConnection);

        connect(ui->reversal, &QAction::triggered,
                this, &MainWindow::swapBoardSides,
                Qt::UniqueConnection);
    }

    // ドラッグ終了シグナルは従来どおり（BIC 内で start/endDrag を呼んでいます）
    if (m_gameController) {
        connect(m_gameController, &ShogiGameController::endDragSignal,
                this, &MainWindow::endDrag,
                Qt::UniqueConnection);
    }
}

// 局面編集を終了した場合の処理を行う。
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
        disconnect(ui->turnaround,                 &QAction::triggered, this, &MainWindow::switchTurns);
        disconnect(ui->flatHandInitialPosition,    &QAction::triggered, this, &MainWindow::setStandardStartPosition);
        disconnect(ui->shogiProblemInitialPosition,&QAction::triggered, this, &MainWindow::setTsumeShogiStartPosition);
        disconnect(ui->reversal,                   &QAction::triggered, this, &MainWindow::swapBoardSides);
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
    // 左クリックで選択したマスのポインタを初期化する。
    m_clickPoint = QPoint();

    // 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
    m_shogiView->resetAndEqualizePiecesOnStands();
}

// 手番を変更する。
void MainWindow::switchTurns()
{
    // 手番が先手あるいは下手の場合
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // 手番を後手あるいは上手に設定する。
        m_gameController->setCurrentPlayer(ShogiGameController::Player2);
    }
    // 手番が後手あるいは上手の場合
    else {
        // 手番を先手あるいは下手に設定する。
        m_gameController->setCurrentPlayer(ShogiGameController::Player1);
    }
}

// 平手初期局面に盤面を初期化する。
void MainWindow::setStandardStartPosition()
{
    // 左クリックで選択したマスのポインタを初期化する。
    m_clickPoint = QPoint();

    // 平手初期局面に盤面を初期化する。
    m_shogiView->initializeToFlatStartingPosition();
}

// 詰将棋の初期局面に盤面を初期化する。
void MainWindow::setTsumeShogiStartPosition()
{
    // 左クリックで選択したマスのポインタを初期化する。
    m_clickPoint = QPoint();

    try {
        // 詰将棋の初期局面に盤面を初期化する。
        m_shogiView->shogiProblemInitialPosition();
    } catch (const std::exception& e) {
        // エラーメッセージを表示する。
        displayErrorMessage(e.what());
    }
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void MainWindow::swapBoardSides()
{
    // 左クリックで選択したマスのポインタを初期化する
    m_clickPoint = QPoint();

    // 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
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

void MainWindow::armHumanTimerIfNeeded()
{
    if (!m_humanTimerArmed && isHumanTurn()) {
        m_humanTurnTimer.start();
        m_humanTimerArmed = true;
        // qDebug() << "[HUMAN] turn start, timer armed";
    }
}

void MainWindow::finishHumanTimerAndSetConsideration()
{
    // 直前に指した「人間側」を取得（HvE なら固定、HvH なら呼び出し側で適切に）
    const auto side = humanPlayerSide(); // ShogiGameController::Player1 / Player2

    // ShogiClock が積算中の“その手の考慮時間(ms)”を取得
    const qint64 clkMs = (side == ShogiGameController::Player1)
        ? m_shogiClock->player1ConsiderationMs()
        : m_shogiClock->player2ConsiderationMs();

    // 表示用にそのまま流し込む（内部ロジックと完全一致）
    if (side == ShogiGameController::Player1) {
        m_shogiClock->setPlayer1ConsiderationTime(static_cast<int>(clkMs));
    } else {
        m_shogiClock->setPlayer2ConsiderationTime(static_cast<int>(clkMs));
    }

    // ローカルの人間用タイマーは無効化しておく（ズレを残さない）
    if (m_humanTurnTimer.isValid()) m_humanTurnTimer.invalidate();
    m_humanTimerArmed = false;

    // qDebug() << "[HUMAN] committed, ShogiClock-based =" << clkMs << "ms";
}

// mainwindow.cpp
void MainWindow::armTurnTimerIfNeeded() {
    if (!m_turnTimerArmed) {
        m_turnTimer.start();
        m_turnTimerArmed = true;
        // qDebug() << "[H2H] turn timer armed";
    }
}

void MainWindow::finishTurnTimerAndSetConsiderationFor(ShogiGameController::Player mover) {
    if (!m_turnTimerArmed) return;
    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;

    if (mover == ShogiGameController::Player1) {
        m_shogiClock->setPlayer1ConsiderationTime(static_cast<int>(ms));
    } else {
        m_shogiClock->setPlayer2ConsiderationTime(static_cast<int>(ms));
    }

    m_turnTimerArmed = false;
    m_turnTimer.invalidate();
    // qDebug() << "[H2H] move committed by" << mover << "elapsed" << ms << "ms";
}

void MainWindow::computeGoTimesForUSI(qint64& outB, qint64& outW)
{
    // ShogiClock の「内部残時間」（ms）を取得
    const qint64 rawB = qMax<qint64>(0, m_shogiClock->getPlayer1TimeIntMs());
    const qint64 rawW = qMax<qint64>(0, m_shogiClock->getPlayer2TimeIntMs());

    if (m_useByoyomi) {
        // ★ 秒読み方式：btime/wtime は「メイン持ち時間のみ」
        // いま秒読み“適用中”なら 0 を送る
        outB = m_shogiClock->byoyomi1Applied() ? 0 : rawB;
        outW = m_shogiClock->byoyomi2Applied() ? 0 : rawW;
        return;
    }

    // ★ フィッシャー方式：常に pre-add に正規化（内部は post-add なので1回だけ引く）
    outB = rawB;
    outW = rawW;

    const qint64 incB = static_cast<qint64>(m_addEachMoveMiliSec1);
    const qint64 incW = static_cast<qint64>(m_addEachMoveMiliSec2);
    if (incB > 0) outB = qMax<qint64>(0, outB - incB);
    if (incW > 0) outW = qMax<qint64>(0, outW - incW);
}

void MainWindow::refreshGoTimes()
{
    qint64 b, w;
    computeGoTimesForUSI(b, w);
    m_bTime = QString::number(b);
    m_wTime = QString::number(w);
}

// ★ 追加: いまの手番の側の「この手で使える上限(ms)」を算出
//   - 秒読みあり: main残 + byoyomi
//   - 秒読みなし(インクリ): main残のみ（incは次手で付与されるため“上限”に含めない）
//   - refreshGoTimes() の直後に呼ぶ想定（m_bTime/m_wTime が最新）
int MainWindow::computeMoveBudgetMsForCurrentTurn() const
{
    const bool p1turn = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    const int  mainMs = p1turn ? m_bTime.toInt() : m_wTime.toInt();
    const int  byoMs  = m_useByoyomi ? m_byoyomiMilliSec1 : 0;
    return mainMs + byoMs;
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
    // 冪等ガード（多重記録防止）
    if (m_gameIsOver || m_gameoverMoveAppended) return;

    // ★ まず完全停止（以後の go/position を出さない）
    m_gameIsOver = true;

    // ★ この局の bestmove は一切採用しないよう USI へ宣言
    if (m_usi1) m_usi1->markHardTimeout();
    if (m_usi2) m_usi2->markHardTimeout();

    // 時計停止＆棋譜「▲時間切れ」を一度だけ記録
    m_shogiClock->markGameOver();
    setGameOverMove(GameOverCause::Timeout, /*loserIsPlayerOne=*/true);
    m_gameoverMoveAppended = true;

    // 正しい向きで gameover / quit
    if (m_usi1) m_usi1->sendGameOverLoseAndQuitCommands();
    if (m_usi2) m_usi2->sendGameOverWinAndQuitCommands();

    // 終了時のノイズ抑止
    if (m_usi1) m_usi1->setSquelchResignLogging(true);
    if (m_usi2) m_usi2->setSquelchResignLogging(true);

    qDebug().nospace() << "[ARBITER] timeout P1 at t+" << ShogiUtils::nowMs() << "ms";

    displayResultsAndUpdateGui();
}

// 後手が時間切れ → 後手敗北
void MainWindow::onPlayer2TimeOut()
{
    if (m_gameIsOver || m_gameoverMoveAppended) return;

    m_gameIsOver = true;

    if (m_usi1) m_usi1->markHardTimeout();
    if (m_usi2) m_usi2->markHardTimeout();

    m_shogiClock->markGameOver();
    setGameOverMove(GameOverCause::Timeout, /*loserIsPlayerOne=*/false);
    m_gameoverMoveAppended = true;

    if (m_usi2) m_usi2->sendGameOverLoseAndQuitCommands();
    if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();

    if (m_usi1) m_usi1->setSquelchResignLogging(true);
    if (m_usi2) m_usi2->setSquelchResignLogging(true);

    qDebug().nospace() << "[ARBITER] timeout P2 at t+" << ShogiUtils::nowMs() << "ms";

    displayResultsAndUpdateGui();
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
    if (m_gameoverMoveAppended) return;

    // ★ まず時計停止（この時点の残りmsを固定）
    m_shogiClock->stopClock();

    // 同一内容の重複ガード（任意）
    if (m_hasLastGameOver &&
        m_lastGameOverCause == cause &&
        m_lastLoserIsP1    == loserIsPlayerOne) {
        m_gameIsOver = true;
        disarmHumanTimerIfNeeded();
        qDebug() << "[KIFU] setGameOverMove suppressed (duplicate content)";
        return;
    }

    // メタ保持
    m_hasLastGameOver   = true;
    m_lastGameOverCause = cause;
    m_lastLoserIsP1     = loserIsPlayerOne;

    // 表記（▲/△は絶対座席）
    const QChar mark = glyphForPlayer(loserIsPlayerOne);
    const QString line = (cause == GameOverCause::Resignation)
                           ? QString("%1投了").arg(mark)
                           : QString("%1時間切れ").arg(mark);

    // ★ 自前で「この手の思考秒 / 合計消費秒」を算出
    const qint64 now = ShogiUtils::nowMs();

    // この手の開始エポック
    const qint64 epochMs = loserIsPlayerOne ? m_turnEpochP1Ms : m_turnEpochP2Ms;
    qint64 considerMs    = (epochMs > 0) ? (now - epochMs) : 0;
    if (considerMs < 0) considerMs = 0; // 念のため

    // 合計消費 = 初期持ち時間 − 現在の残り時間
    const qint64 remMs   = loserIsPlayerOne
                           ? m_shogiClock->getPlayer1TimeIntMs()
                           : m_shogiClock->getPlayer2TimeIntMs();
    const qint64 initMs  = loserIsPlayerOne ? m_initialTimeP1Ms : m_initialTimeP2Ms;
    qint64 totalUsedMs   = initMs - remMs;
    if (totalUsedMs < 0) totalUsedMs = 0;

    const QString elapsed = QStringLiteral("%1/%2")
                                .arg(fmt_mmss(considerMs))
                                .arg(fmt_hhmmss(totalUsedMs));

    // 1回だけ即時追記
    appendKifuLine(line, elapsed);

    // 以後の更新を抑止
    m_gameoverMoveAppended = true;
    m_gameIsOver           = true;
    disarmHumanTimerIfNeeded();

    qDebug().nospace() << "[KIFU] setGameOverMove appended"
                       << " cause=" << (cause == GameOverCause::Resignation ? "RESIGN" : "TIMEOUT")
                       << " loser=" << (loserIsPlayerOne ? "P1" : "P2")
                       << " elapsed=" << elapsed;
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

void MainWindow::onEngine1Resigns()
{
    qDebug() << "[ARBITER] onEngine1Resigns() ENTER";

    // 二重実行の防止：終局処理や棋譜追記は一度きり
    if (m_gameIsOver) return;
    if (m_gameoverMoveAppended) return;

    // まず終局フラグを立てて以降の go/position を止める
    m_gameIsOver = true;

    qDebug().nospace() << "[ARBITER] RESIGN P1 at t+" << ShogiUtils::nowMs() << "ms";

    // 時計停止＆棋譜に「▲投了」を一度だけ記録
    m_shogiClock->markGameOver();
    setGameOverMove(GameOverCause::Resignation, /*loserIsPlayerOne=*/true);
    m_gameoverMoveAppended = true;

    // USIに通知：負け側（Engine1）へ lose、相手（Engine2）へ win を送る
    if (m_usi1) m_usi1->sendGameOverLoseAndQuitCommands();
    if (m_usi2) m_usi2->sendGameOverWinAndQuitCommands();

    // 以後の余計な投了・info を黙殺（ログ方針に合わせて）
    if (m_usi1) m_usi1->setSquelchResignLogging(true);
    if (m_usi2) m_usi2->setSquelchResignLogging(true);

    // 最終UI更新
    displayResultsAndUpdateGui();
}

void MainWindow::onEngine2Resigns()
{
    qDebug() << "[ARBITER] onEngine2Resigns() ENTER";

    if (m_gameIsOver) { /* 既に終局 */ }
    if (m_gameoverMoveAppended) return;           // ★ 二重防止（棋譜）

    m_gameIsOver = true;                           // まずループ停止
    qDebug().nospace() << "[ARBITER] RESIGN P2 at t+" << ShogiUtils::nowMs() << "ms";

    m_shogiClock->markGameOver();
    setGameOverMove(GameOverCause::Resignation, /*loserIsPlayerOne=*/false);

    m_gameoverMoveAppended = true;                 // ★ ここで“書いた”とマーク

    if (m_usi2) m_usi2->sendGameOverLoseAndQuitCommands();
    if (m_usi1) m_usi1->sendGameOverWinAndQuitCommands();

    if (m_usi1) m_usi1->setSquelchResignLogging(true);
    if (m_usi2) m_usi2->setSquelchResignLogging(true);

    displayResultsAndUpdateGui();
}

void MainWindow::ensureGameInfoTable()
{
    if (m_gameInfoTable) return;

    m_gameInfoTable = new QTableWidget(this);
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
    // 再入防止（applyResolvedRowAndSelect 内で選択を動かすと再度シグナルが来るため）
    if (m_onMainRowGuard)
        return;
    m_onMainRowGuard = true;

    // 解決済み行が無ければ何もしない
    if (m_resolvedRows.isEmpty()) {
        m_onMainRowGuard = false;
        return;
    }

    // いまアクティブな“本譜 or 分岐”行
    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);

    // 盤面・棋譜欄・分岐候補・矢印・分岐ツリー（EngineAnalysisTab経由）まで一括同期
    applyResolvedRowAndSelect(row, qMax(0, selPly));

    m_onMainRowGuard = false;
}

void MainWindow::populateBranchListForPly(int ply)
{
    if (!m_kifuBranchModel) return;

    const int rows = m_resolvedRows.size();

    // ★ 先に早期リターン（rows==0 もここで抜ける）
    if (ply <= 0 || rows == 0) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (m_kifuBranchView) m_kifuBranchView->setEnabled(false);
        qDebug().noquote() << "[BRANCH] hide (ply<=0 or no rows)";
        return;
    }

    // ここから rows>0 が保証されるので qBound を安全に使える
    const int active = qBound(0, m_activeResolvedRow, rows - 1);

    qDebug().noquote() << "[BRANCH] populate ply=" << ply
                       << " activeRow=" << active
                       << " resolved=" << rows;

    // アクティブ行の基底SFEN（ply-1局面）
    const auto& self = m_resolvedRows[active];
    const int idx = ply - 1;
    if (idx < 0 || idx >= self.sfen.size()) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (m_kifuBranchView) m_kifuBranchView->setEnabled(false);
        qDebug().noquote() << "[BRANCH] hide (base sfen out of range)";
        return;
    }
    const QString base = self.sfen.at(idx);

    // 事前計算テーブルから候補取得（SFEN一本化）
    const auto byPly = m_branchIndex.constFind(ply);
    if (byPly == m_branchIndex.constEnd()) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (m_kifuBranchView) m_kifuBranchView->setEnabled(false);
        qDebug().noquote() << "[BRANCH] hide (no index at ply)";
        return;
    }
    const auto byBase = byPly->constFind(base);
    if (byBase == byPly->constEnd()) {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (m_kifuBranchView) m_kifuBranchView->setEnabled(false);
        qDebug().noquote() << "[BRANCH] hide (no group for base)";
        return;
    }

    // 候補（self を先頭へ）
    QList<BranchCandidate> ordered = byBase.value();
    if (!ordered.isEmpty() && ordered.first().row != active) {
        int selfPos = -1;
        for (int i = 0; i < ordered.size(); ++i)
            if (ordered[i].row == active) { selfPos = i; break; }
        if (selfPos > 0) {
            const auto me = ordered.takeAt(selfPos);
            ordered.prepend(me);
        }
    }

    // モデルへ反映
    QList<KifDisplayItem> items;
    m_branchRowMap.clear();
    for (const auto& c : ordered) {
        items << KifDisplayItem{ c.text, QString() };
        m_branchRowMap.append(qMakePair(c.row, c.ply));
    }

    const bool show = (items.size() >= 2);
    if (show) {
        m_kifuBranchModel->setBranchCandidatesFromKif(items);
        m_kifuBranchModel->setHasBackToMainRow(active != 0);
        if (m_kifuBranchView) m_kifuBranchView->setEnabled(true);
    } else {
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (m_kifuBranchView) m_kifuBranchView->setEnabled(false);
    }

    // デバッグ
    QStringList ls; for (const auto& it : items) ls << it.prettyMove;
    qDebug().noquote() << "[BRANCH] baseKey=" << (base.left(24) + "...")
                       << " items=" << ls;
    for (int i = 0; i < m_branchRowMap.size(); ++i) {
        qDebug().noquote() << "[BRANCH] idx" << i
                           << " -> row=" << m_branchRowMap[i].first
                           << " ply=" << m_branchRowMap[i].second;
    }
}

void MainWindow::syncBoardAndHighlightsAtRow(int row)
{
    if (!m_sfenRecord) return;

    // sfen の最大インデックスは size-1（0=初期局面）
    const int last = m_sfenRecord->size() - 1;
    if (last < 0) return;

    // row を安全な範囲にクリップ
    const int safeRow = qBound(0, row, last);

    // 内部状態をまず揃える
    m_currentSelectedPly = safeRow;
    m_currentMoveIndex   = safeRow;

    // 盤面 → ハイライト の順で更新（ハイライトは1手目以降のみ）
    clearMoveHighlights();
    applySfenAtCurrentPly();

    if (safeRow > 0 && safeRow - 1 < m_gameMoves.size()) {
        addMoveHighlights();
    } else {
        // 必要ならデバッグログ
        // qDebug() << "[SYNC] highlight skipped: row=" << safeRow << " gameMoves=" << m_gameMoves.size();
    }
}

void MainWindow::showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly)
{
    // いま表示中の棋譜列を保持
    m_dispCurrent = disp;

    // モデルへ反映（既存）
    displayGameRecord(disp);

    // ★ RecordPane 内のビューを使う
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view || !view->model()) return;

    const int rc  = view->model()->rowCount();               // ← view の model を使う
    const int row = qBound(0, selectPly, rc > 0 ? rc - 1 : 0);

    const QModelIndex idx = view->model()->index(row, 0);    // ← view の model を使う
    if (!idx.isValid()) return;

    if (auto* sel = view->selectionModel()) {
        sel->setCurrentIndex(idx,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        view->setCurrentIndex(idx);
    }
    view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

// 現在の手数（m_currentSelectedPly）に対応するSFENを盤面へ反映
void MainWindow::applySfenAtCurrentPly()
{
    if (!m_sfenRecord || m_sfenRecord->isEmpty()) return;

    const int idx = qBound(0, m_currentSelectedPly, m_sfenRecord->size() - 1);
    QString sfen = m_sfenRecord->at(idx);

    // 既存の盤面反映APIに合わせてください
    m_gameController->board()->setSfen(sfen);
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

// 分岐候補ビューで行がクリック/アクティベートされたとき
void MainWindow::onBranchCandidateActivated(const QModelIndex& index)
{
    // --- バリデーション ---
    if (!index.isValid() || !m_kifuBranchModel) return;

    const int row = index.row();

    // 1) 「本譜へ戻る」行が押された場合：本譜（解決済み行0）を再適用
    if (m_kifuBranchModel->isBackToMainRow(row)) {
        // 直前に見ていた手数（m_currentSelectedPly）を維持して本譜に戻る
        // applyResolvedRowAndSelect は 盤面/棋譜欄/分岐候補/矢印/ハイライト を一括同期します
        applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/m_currentSelectedPly);  // 本譜へ
        return;
    }

    // 2) 通常の分岐候補：m_branchRowMap から解決済み行と手数を取り出してジャンプ
    if (row >= 0 && row < m_branchRowMap.size()) {
        const int resolvedRow = m_branchRowMap[row].first;   // どの「行」（本譜=0/分岐=1..）か
        const int targetPly   = m_branchRowMap[row].second;  // その行での手数(0..N)

        // 盤面・棋譜欄・分岐候補・矢印ボタン・ツリーハイライトまで一括同期
        applyResolvedRowAndSelect(resolvedRow, targetPly);
    }
}

void MainWindow::buildResolvedLinesAfterLoad()
{
    m_resolvedRows.clear();

    // --- 行0 = 本譜 ---
    ResolvedRow mainRow;
    mainRow.startPly = 1;
    mainRow.disp     = m_dispMain;   // 表示用
    mainRow.sfen     = m_sfenMain;   // 0..N（本譜の再生済み）
    mainRow.gm       = m_gmMain;     // USI列
    mainRow.varIndex = -1;
    m_resolvedRows.push_back(mainRow);

    // --- 行1.. = 分岐（後勝ちプレフィクス + 自身） ---
    for (int vi = 0; vi < m_variationsSeq.size(); ++vi) {
        const KifLine& v = m_variationsSeq.at(vi);
        if (v.disp.isEmpty()) continue;

        const int start = qMax(1, v.startPly);

        // 1) プレフィクス(1..start-1) を本譜で初期化
        QList<KifDisplayItem> prefixDisp = m_dispMain.mid(0, start - 1); // 手目は1始まり→indexは0始まり
        QVector<ShogiMove>    prefixGm   = m_gmMain.mid(0, start - 1);
        QStringList           prefixSfen = m_sfenMain.mid(0, start);     // 0..start-1 の局面

        // 2) それ以前に登場した分岐で “後勝ち” 上書き
        //    disp / gm に加え、sfen も u.sfenList で該当範囲を上書きする
        for (int uj = 0; uj < vi; ++uj) {
            const KifLine& u = m_variationsSeq.at(uj);
            if (u.disp.isEmpty()) continue;

            const int uStart = qMax(1, u.startPly);
            const int uEnd   = uStart + u.disp.size() - 1;
            const int endCol = qMin(start - 1, uEnd);  // プレフィクス内で上書き可能な終端
            if (endCol < uStart) continue;

            // sfenList は「基底（uStart-1手目後）」が index=0、以降の各手後が +1 で入っている想定
            // まず基底（uStart-1手目後）を安全に反映
            if (!u.sfenList.isEmpty() && uStart - 1 < prefixSfen.size()) {
                prefixSfen[uStart - 1] = u.sfenList.at(0);
            }

            for (int col = uStart; col <= endCol; ++col) {
                const int idx = col - 1;          // disp/gm のインデックス
                const int off = col - uStart;     // u 内のオフセット（0始まり）

                if (0 <= idx && idx < prefixDisp.size() && 0 <= off && off < u.disp.size())
                    prefixDisp[idx] = u.disp.at(off);

                if (0 <= idx && idx < prefixGm.size() && 0 <= off && off < u.gameMoves.size())
                    prefixGm[idx] = u.gameMoves.at(off);

                // sfen：col 手目後 = u.sfenList[off+1]
                const int sfenIdx = off + 1;
                if (col < prefixSfen.size() && 0 <= sfenIdx && sfenIdx < u.sfenList.size())
                    prefixSfen[col] = u.sfenList.at(sfenIdx);
            }
        }

        // 3) 分岐本体と連結して “行” を確定
        ResolvedRow row;
        row.startPly = start;
        row.varIndex = vi;

        row.disp = prefixDisp;
        row.disp += v.disp;

        row.gm = prefixGm;
        row.gm += v.gameMoves;

        row.sfen = prefixSfen;                 // 0..start-1 は上で“後勝ち”反映済み
        if (v.sfenList.size() >= 2)            // v.sfenList[0] は基底（start-1手目後）
            row.sfen += v.sfenList.mid(1);     // start.. の各手後局面を接続

        m_resolvedRows.push_back(row);
    }

#ifdef SHOGIBOARDQ_DEBUG_KIF
    qDebug().noquote() << "[RESOLVED] lines=" << m_resolvedRows.size();
    for (int i=0;i<m_resolvedRows.size();++i) {
        const auto& r = m_resolvedRows[i];
        QStringList pm; pm.reserve(r.disp.size());
        for (const auto& d : r.disp) pm << d.prettyMove;
        qDebug().noquote()
            << "  [" << (i==0?"Main":QString("Var%1").arg(r.varIndex)) << "]"
            << " start=" << r.startPly
            << " ndisp=" << r.disp.size()
            << " nsfen=" << r.sfen.size()
            << " seq=「 " << pm.join(" / ") << " 」";
    }
#endif
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
                    // 念のためモデル一致を確認（違っていたら何もしない）
                    if (view->model() == m_kifuRecordModel) {
                        view->setCurrentIndex(idx);
                        view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                    }
                }
            }
        }
    }

    // 分岐候補・盤面ハイライト・矢印ボタン
    populateBranchListForPly(m_activePly);
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

Qt::ConnectionType MainWindow::connTypeFor(QObject* obj) const
{
    return (obj && obj->thread() == this->thread()) ? Qt::DirectConnection : Qt::AutoConnection;
}

void MainWindow::resetGameFlags()
{
    m_gameIsOver = false;
    m_gameoverMoveAppended = false;
    m_hasLastGameOver = false;
}

void MainWindow::syncClockTurnAndEpoch()
{
    m_shogiClock->stopClock();
    const int cur = (m_gameController->currentPlayer() == ShogiGameController::Player2) ? 2 : 1;
    updateTurnStatus(cur);
    m_shogiClock->startClock();

    // 現在手番側のターン・エポックを開始
    const qint64 now = ShogiUtils::nowMs();
    m_turnEpochP1Ms = m_turnEpochP2Ms = -1;
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        m_turnEpochP1Ms = now;
    } else {
        m_turnEpochP2Ms = now;
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

void MainWindow::wireResignToArbiter(Usi* engine, bool asP1)
{
    if (!engine) return;
    QObject::disconnect(engine, &Usi::bestMoveResignReceived, this, &MainWindow::onEngine1Resigns);
    QObject::disconnect(engine, &Usi::bestMoveResignReceived, this, &MainWindow::onEngine2Resigns);
    const auto t = connTypeFor(engine);
    if (asP1) {
        QObject::connect(engine, &Usi::bestMoveResignReceived, this, &MainWindow::onEngine1Resigns, t);
    } else {
        QObject::connect(engine, &Usi::bestMoveResignReceived, this, &MainWindow::onEngine2Resigns, t);
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

bool MainWindow::engineThinkApplyMove(Usi* engine, QString& positionStr, QString& ponderStr, QPoint* outFrom, QPoint* outTo)
{
    if (!engine) return false;

    // go パラメータを最新化
    refreshGoTimes();

    // 旗落ち判定用に「指す前の手番（＝この手を指す側）」と上限を確定
    const bool moverWasP1 = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    const int  budgetMs   = computeMoveBudgetMsForCurrentTurn();

    // 思考要求
    QPoint from(-1, -1), to(-1, -1);
    m_gameController->setPromote(false);
    engine->handleEngineVsHumanOrEngineMatchCommunication(
        positionStr, ponderStr,
        from, to,
        m_byoyomiMilliSec1, m_bTime, m_wTime,
        m_addEachMoveMiliSec1, m_addEachMoveMiliSec2, m_useByoyomi
    );

    // 投了で終局したらここで終了
    if (m_gameIsOver) {
        updateTurnAndTimekeepingDisplay();
        return false;
    }

    // 着手適用
    bool ok = false;
    try {
        ok = m_gameController->validateAndMove(
            from, to, m_lastMove, m_playMode,
            m_currentMoveIndex, m_sfenRecord, m_gameMoves
        );
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
        return false;
    }

    if (m_gameIsOver) {
        updateTurnAndTimekeepingDisplay();
        return false;
    }
    if (!ok) return false;

    // 旗落ち判定と思考時間反映
    const qint64 thinkMs = engine->lastBestmoveElapsedMs();
    if (thinkMs > budgetMs + kFlagFallGraceMs) {
        qDebug().nospace()
            << "[GUI] Flag-fall (engine move) "
            << "think=" << thinkMs << "ms "
            << "budget=" << budgetMs << "ms "
            << "grace=" << kFlagFallGraceMs << "ms";
        handleFlagFallForMover(moverWasP1);
        return false;
    }

    setConsiderationForJustMoved(thinkMs);
    updateTurnAndTimekeepingDisplay();

    if (outFrom) *outFrom = from;
    if (outTo)   *outTo   = to;
    return true;
}

// ======================== ヘルパ実装 ========================
void MainWindow::destroyEngine(Usi*& e)
{
    if (e) { delete e; e = nullptr; }
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

    resetGameFlags();
}


// EvE：m_usi1 / m_usi2 を用意して共通初期化
void MainWindow::initEnginesForEvE()
{
    destroyEngine(m_usi1);
    destroyEngine(m_usi2);
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode, this);
    m_usi2 = new Usi(m_lineEditModel2, m_modelThinking2, m_gameController, m_playMode, this);

    m_usi1->resetResignNotified(); m_usi1->clearHardTimeout();
    m_usi2->resetResignNotified(); m_usi2->clearHardTimeout();

    wireResignToArbiter(m_usi1, /*asP1=*/true);
    wireResignToArbiter(m_usi2, /*asP1=*/false);

    m_usi1->setLogIdentity("[E1]", "P1", m_startGameDialog->engineName1());
    m_usi2->setLogIdentity("[E2]", "P2", m_startGameDialog->engineName2());

    m_usi1->setSquelchResignLogging(false);
    m_usi2->setSquelchResignLogging(false);

    resetGameFlags();
}

// 初期 position（共通） ※PvE でも EvE でも両方の ponder を同値で初期化して問題なし
void MainWindow::setupInitialPositionStrings()
{
    m_positionStr1 = "position " + m_startPosStr + " moves";
    m_positionStrList.append(m_positionStr1);
    m_positionPonder1 = m_positionStr1;
    m_positionPonder2 = m_positionStr1;
}

// 時計・手番同期と対局エポック開始
void MainWindow::syncAndEpoch(const QString& title)
{
    syncClockTurnAndEpoch();
    startMatchEpoch(title);
}

// いま手番が人間か？
bool MainWindow::isHumanTurnNow(bool engineIsP1) const
{
    const auto cur = m_gameController->currentPlayer();
    const auto engineSide = engineIsP1 ? ShogiGameController::Player1
                                       : ShogiGameController::Player2;
    return (cur != engineSide);
}

// エンジンに1手指させ、ハイライトと評価グラフを更新（共通）
bool MainWindow::engineMoveOnce(Usi* eng,
                                QString& positionStr,
                                QString& ponderStr,
                                bool /*useSelectedField2*/,
                                int engineIndex,
                                QPoint* outTo)
{
    QPoint from, to;
    if (!engineThinkApplyMove(eng, positionStr, ponderStr, &from, &to))
        return false;

    // ★ ハイライトはコントローラに一元化
    if (m_boardController)
        m_boardController->showMoveHighlights(from, to);   // 赤=from, 黄=to

    if (engineIndex == 1) redrawEngine1EvaluationGraph();
    else                  redrawEngine2EvaluationGraph();

    if (outTo) *outTo = to;
    return true;
}

bool MainWindow::playOneEngineTurn(Usi* mover, Usi* receiver,
                                   QString& positionStr,
                                   QString& ponderStr,
                                   int engineIndex)
{
    // EvEではどちらも m_selectedField を使っていた既存挙動を踏襲
    QPoint to;
    if (!engineMoveOnce(mover, positionStr, ponderStr,
                        /*useSelectedField2=*/false, engineIndex, &to)) {
        return false; // 投了・エラー等
    }

    // 次手のヒント（直前に動いた"to"を相手側へ伝える）
    receiver->setPreviousFileTo(to.x());
    receiver->setPreviousRankTo(to.y());

    // engineMoveOnce 内で終局になった可能性もある
    if (m_gameIsOver) return false;

    return true;
}

void MainWindow::assignSidesHumanVsEngine()
{
    // 平手: 後手エンジン / 駒落ち: 先手エンジン
    if (m_playMode == EvenHumanVsEngine)
        initializeAndStartPlayer2WithEngine1(); // 後手エンジン
    else
        initializeAndStartPlayer1WithEngine1(); // 先手エンジン
}

void MainWindow::assignSidesEngineVsHuman()
{
    // 平手: 先手エンジン / 駒落ち: 後手エンジン
    if (m_playMode == EvenEngineVsHuman)
        initializeAndStartPlayer1WithEngine1(); // 先手エンジン
    else
        initializeAndStartPlayer2WithEngine1(); // 後手エンジン
}

void MainWindow::assignEnginesEngineVsEngine()
{
    // 平手: (P1=Engine1, P2=Engine2) / 駒落ち: (P1=Engine2, P2=Engine1)
    if (m_playMode == EvenEngineVsEngine) {
        initializeAndStartPlayer1WithEngine1();
        initializeAndStartPlayer2WithEngine2();
    } else { // HandicapEngineVsEngine
        initializeAndStartPlayer2WithEngine1();
        initializeAndStartPlayer1WithEngine2();
    }
}

inline void pumpUi() {
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5); // 最大5ms程度
}

// 平手、駒落ち Player1: Human, Player2: Human
void MainWindow::startHumanVsHumanGame()
{
    if (m_boardController)
           m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);

    // ← 追加：シングルエンジンではないので必ず false に
    m_engine1IsP1 = false;

    // 将棋クロックとUI手番の同期
    syncClockTurnAndEpoch();

    // 人間手番のストップウォッチを同時起動
    armTurnTimerIfNeeded();
}

// 平手 Player1: Human, Player2: USI Engine
// 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
void MainWindow::startHumanVsEngineGame()
{
    if (m_boardController)
         m_boardController->setMode(BoardInteractionController::Mode::HumanVsEngine);

    if (!m_modelThinking1) m_modelThinking1 = new ShogiEngineThinkingModel(this);
    if (!m_lineEditModel1) m_lineEditModel1 = new UsiCommLogModel(this);

    if (m_analysisTab) {
        m_analysisTab->setEngine1ThinkingModel(m_modelThinking1);
        m_analysisTab->setDualEngineVisible(false);
    }

    const bool engineIsP1 = (m_playMode == HandicapEngineVsHuman); // 駒落ち=先手エンジン

    m_engine1IsP1 = engineIsP1;   // ← 追加：以後の“次手は人間？”判定に使う

    initSingleEnginePvE(engineIsP1);

    // 担当割り当て
    assignSidesHumanVsEngine();

    // 初期 position
    setupInitialPositionStrings();

    // 時計同期＋エポック
    syncAndEpoch(QStringLiteral("Human vs Engine"));

    // 人間が初手なら go は送らず UI だけ整える
    if (isHumanTurnNow(engineIsP1)) {
        QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
        updateTurnAndTimekeepingDisplay();
        return;
    }

    // --- エンジン初手（HvE は m_selectedField2 を使用していた挙動を踏襲） ---
    if (!engineMoveOnce(m_usi1, m_positionStr1, m_positionPonder1,
                        /*useSelectedField2=*/true, /*engineIndex=*/1))
        return;

    // 次は人間手番
    m_shogiView->setMouseClickMode(true);
    QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
    pumpUi();
}

// 平手 Player1: USI Engine（先手）, Player2: Human（後手）
// 駒落ち Player1: Human（下手）,  Player2: USI Engine（上手）
void MainWindow::startEngineVsHumanGame()
{
    if (m_boardController)
            m_boardController->setMode(BoardInteractionController::Mode::HumanVsEngine);

    if (!m_modelThinking1) m_modelThinking1 = new ShogiEngineThinkingModel(this);
    if (!m_lineEditModel1) m_lineEditModel1 = new UsiCommLogModel(this);

    // 変更後
    if (m_analysisTab) {
        m_analysisTab->setEngine1ThinkingModel(m_modelThinking1);
        m_analysisTab->setDualEngineVisible(false);
    }

    const bool engineIsP1 = (m_playMode == EvenEngineVsHuman); // 平手=先手エンジン

    m_engine1IsP1 = engineIsP1;   // ← 追加

    initSingleEnginePvE(engineIsP1);

    // 担当割り当て
    assignSidesEngineVsHuman();

    // 初期 position とクリックハンドラ
    setupInitialPositionStrings();

    // 時計同期＋エポック
    syncAndEpoch(QStringLiteral("Engine vs Human"));

    // 人間が初手なら UI 調整のみ
    if (isHumanTurnNow(engineIsP1)) {
        QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
        updateTurnAndTimekeepingDisplay();
        return;
    }

    // --- エンジン初手（EvH は m_selectedField を使用していた挙動を踏襲） ---
    if (!engineMoveOnce(m_usi1, m_positionStr1, m_positionPonder1,
                        /*useSelectedField2=*/false, /*engineIndex=*/1))
        return;

    // 次は人間手番
    m_shogiView->setMouseClickMode(true);
    QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
    pumpUi();
}

// 平手、駒落ち Player1: USI Engine, Player2: USI Engine
void MainWindow::startEngineVsEngineGame()
{
    m_shogiView->setMouseClickMode(false);

    if (!m_modelThinking1) m_modelThinking1 = new ShogiEngineThinkingModel(this);
    if (!m_modelThinking2) m_modelThinking2 = new ShogiEngineThinkingModel(this);
    if (!m_lineEditModel1) m_lineEditModel1 = new UsiCommLogModel(this);
    if (!m_lineEditModel2) m_lineEditModel2 = new UsiCommLogModel(this);

    // 変更後
    if (m_analysisTab) {
        m_analysisTab->setEngine1ThinkingModel(m_modelThinking1);
        m_analysisTab->setEngine2ThinkingModel(m_modelThinking2);
        m_analysisTab->setDualEngineVisible(true);
    }

    // ← 追加：シングルエンジンではない（両方エンジン）ので false にしておく
    m_engine1IsP1 = false;

    initEnginesForEvE();

    // エンジン割り当て
    assignEnginesEngineVsEngine();

    // 初期 position（双方 ponder）
    setupInitialPositionStrings();

    // 時計同期＋エポック
    syncAndEpoch(QStringLiteral("Engine vs Engine"));

    // 指し合いループ（1手単位の共通処理でスリム化）
    while (!m_gameIsOver) {
        if (!playOneEngineTurn(m_usi1, m_usi2, m_positionStr1, m_positionPonder1, /*engineIndex=*/1))
            break;

        // 1手目の描画を確実に反映させたいなら入れる
        pumpUi();

        if (!playOneEngineTurn(m_usi2, m_usi1, m_positionStr1, m_positionPonder2, /*engineIndex=*/2))
            break;

        // ここは従来どおり保険として残してOK（なくても大差はない）
        pumpUi();
    }

    updateTurnAndTimekeepingDisplay();
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

int MainWindow::maxPlyAtRow(int row) const {
    // r.disp.size() と同義
    const int clamped = qBound(0, row, m_resolvedRows.size() > 0 ? m_resolvedRows.size() - 1 : 0);
    return m_resolvedRows[clamped].disp.size();
}

int MainWindow::currentPly() const
{
    // すでにナビ用に追跡している値があればそれを返す
    if (m_activePly >= 0) return m_activePly;

    // フォールバック：RecordPane の棋譜ビューの選択行
    const QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (view) {
        const QModelIndex cur = view->currentIndex();
        if (cur.isValid()) return qMax(0, cur.row());
    }
    return 0;
}

void MainWindow::applySelect(int row, int ply) {
    applyResolvedRowAndSelect(row, ply); // 既存の“統一適用”関数へ委譲
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
        m_recordPane = new RecordPane(this);

        // RecordPane → MainWindow 通知
        connect(m_recordPane, &RecordPane::mainRowChanged,
                this, &MainWindow::onMainMoveRowChanged);
        connect(m_recordPane, &RecordPane::branchActivated,
                this, &MainWindow::onBranchCandidateActivated);

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
}

// どこかの初期化パスで（例：initializeComponents 内やコンストラクタ末尾）
void MainWindow::setupEngineAnalysisTab()
{
    if (m_analysisTab) return;

    m_analysisTab = new EngineAnalysisTab(this);
    // モデルを渡す（既存ポインタをそのまま利用）
    m_analysisTab->setModels(m_modelThinking1, m_modelThinking2,
                             m_lineEditModel1, m_lineEditModel2);

    m_analysisTab->setDualEngineVisible(false);

    // 既存の m_tab を差し替えたい場合は以下のように
    m_tab = m_analysisTab->tab();

    // ツリー上のクリック → 解決済み行/手数を適用
    connect(m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
            this, [this](int row, int ply){
                applyResolvedRowAndSelect(row, ply);
            });

    connect(m_analysisTab, &EngineAnalysisTab::requestApplyStart,
            this, [this]{
                applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/0);
                if (m_kifuBranchModel) m_kifuBranchModel->clearBranchCandidates();
                if (auto* br = m_recordPane ? m_recordPane->branchView() : nullptr)
                    br->setEnabled(false);
                enableArrowButtons();
            });

    connect(m_analysisTab, &EngineAnalysisTab::requestApplyMainAtPly,
            this, [this](int ply){
                applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/qMax(0, ply));
                populateBranchListForPly(qMax(0, ply));
                enableArrowButtons();
            });

    connect(m_analysisTab, &EngineAnalysisTab::requestApplyVariation,
            this, [this](int startPly, int bucketIndex){
                applyVariationByKey(startPly, bucketIndex);
                populateBranchListForPly(startPly);
                enableArrowButtons();
            });
}

void MainWindow::onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex&)
{
    const int row = cur.isValid() ? cur.row() : 0;

    QString text;
    if (row >= 0 && row < m_commentsByRow.size())
        text = m_commentsByRow[row].trimmed();

    if (m_analysisTab)
        m_analysisTab->setCommentText(text.isEmpty() ? tr("コメントなし") : text);
}

void MainWindow::wireBoardInteractionController()
{
    // 既存があれば入れ替え
    if (m_boardController) {
        m_boardController->deleteLater();
        m_boardController = nullptr;
    }

    // コントローラ生成
    m_boardController = new BoardInteractionController(m_shogiView, m_gameController, this);

    // 盤クリックを全面的にコントローラへ
    QObject::connect(m_shogiView, &ShogiView::clicked,
                     m_boardController, &BoardInteractionController::onLeftClick,
                     Qt::UniqueConnection);
    QObject::connect(m_shogiView, &ShogiView::rightClicked,
                     m_boardController, &BoardInteractionController::onRightClick,
                     Qt::UniqueConnection);

    // コントローラ → Main（合法判定＆適用）
    QObject::connect(m_boardController, &BoardInteractionController::moveRequested,
                     this, [this](const QPoint& from, const QPoint& to)
    {
        // ★ 着手前の手番（＝この手を指す側）を控えておく
        const auto moverBefore = m_gameController->currentPlayer();

        // validateAndMove が非常参照パラメータの場合に備えてローカルコピー
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

        // 適用結果をコントローラへ通知（ハイライト／ドラッグ状態の確定）
        if (m_boardController) m_boardController->onMoveApplied(from, to, ok);
        if (!ok) return;

        // 人間の着手ハイライト（コントローラに集約）
        if (m_boardController)
            m_boardController->showMoveHighlights(hFrom, hTo);

        // ── ここから成功時の後続処理 ─────────────────────────────────────

        switch (m_playMode) {

        // ▼ 人間 vs 人間
        case HumanVsHuman: {
            // この手を指した側（着手前の手番）の考慮時間を確定
            finishTurnTimerAndSetConsiderationFor(moverBefore);

            // 時計・手番表示を確定
            updateTurnAndTimekeepingDisplay();
            pumpUi();

            // 次手の人間用（共通）ストップウォッチ
            QTimer::singleShot(0, this, [this]{ armTurnTimerIfNeeded(); });
            m_shogiView->setMouseClickMode(true);
            break;
        }

        // ▼ 人間とエンジン（先後どちらでもOK）
        case EvenHumanVsEngine:
        case HandicapHumanVsEngine:
        case EvenEngineVsHuman:
        case HandicapEngineVsHuman: {
            // 人間の手が確定 → 人間側ストップウォッチを確定
            finishHumanTimerAndSetConsideration();

            // “同○” 判定用（従来踏襲）
            if (m_usi1) {
                m_usi1->setPreviousFileTo(hTo.x());
                m_usi1->setPreviousRankTo(hTo.y());
            }

            // まず UI/時計を確定（byoyomi / increment の適用もここで）
            updateTurnAndTimekeepingDisplay();

            // 人の操作は一旦不可に（エンジン思考中）
            m_shogiView->setMouseClickMode(false);

            // go パラメータを最新化
            refreshGoTimes();

            // この対局で “エンジンが先手か？”
            const bool engineIsP1 =
                (m_playMode == EvenEngineVsHuman) || (m_playMode == HandicapEngineVsHuman);

            // 直後の手番がエンジンなら 1手だけ指させる
            if (!isHumanTurnNow(engineIsP1)) {
                // ★ ここがポイント：
                // handleHumanVsEngineCommunication は「引数の from/to に
                //   人間の着手を渡し、戻り時にエンジンの着手に上書き」する設計。
                QPoint eFrom = hFrom;   // 人間の着手を渡す（必須）
                QPoint eTo   = hTo;     // 人間の着手を渡す（必須）

                const int engineByoyomiMs = engineIsP1 ? m_byoyomiMilliSec1 : m_byoyomiMilliSec2;

                try {
                    m_usi1->handleHumanVsEngineCommunication(
                        m_positionStr1, m_positionPonder1,
                        eFrom, eTo,                      // ← 人間の着手で渡し、戻りはエンジン着手
                        engineByoyomiMs,
                        m_bTime, m_wTime,
                        m_positionStrList,
                        m_addEachMoveMiliSec1, m_addEachMoveMiliSec2,
                        m_useByoyomi
                    );
                } catch (const std::exception& e) {
                    displayErrorMessage(e.what());
                    m_shogiView->setMouseClickMode(true);
                    return;
                }

                // 受け取った bestmove（eFrom,eTo）を適用
                bool ok2 = false;
                try {
                    ok2 = m_gameController->validateAndMove(
                        eFrom, eTo, m_lastMove, m_playMode,
                        m_currentMoveIndex, m_sfenRecord, m_gameMoves
                    );
                } catch (const std::exception& e) {
                    displayErrorMessage(e.what());
                    m_shogiView->setMouseClickMode(true);
                    return;
                }

                if (ok2) {
                    // エンジン着手ハイライト
                    if (m_boardController)
                        m_boardController->showMoveHighlights(eFrom, eTo);

                    // エンジンの思考時間を考慮時間へ反映
                    const qint64 thinkMs = m_usi1 ? m_usi1->lastBestmoveElapsedMs() : 0;
                    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
                        // 今の手番は先手 → 直前に指したのは後手（エンジン）
                        m_shogiClock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
                    } else {
                        // 今の手番は後手 → 直前に指したのは先手（エンジン）
                        m_shogiClock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
                    }

                    // 時計・手番表示と評価値グラフ
                    updateTurnAndTimekeepingDisplay();
                    m_positionStrList.append(m_positionStr1);
                    redrawEngine1EvaluationGraph();
                    pumpUi();
                }
            }

            // 終局でなければ次は人間手番：クリック受付と人間用タイマーを整える
            if (!m_gameIsOver) {
                m_shogiView->setMouseClickMode(true);
                QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
            }
            break;
        }

        // ▼ エンジン vs エンジン / 解析系など
        default:
            // ここでは何もしない（エンジン側ループや既存処理に委ねる）
            break;
        }
    });

    // 既定モード（必要に応じて開始時に上書き）
    m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);
}
