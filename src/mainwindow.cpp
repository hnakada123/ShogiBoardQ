#include <QApplication>
#include <QLayout>
#include <QMessageBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <iostream>
#include <QDesktopServices>
#include <QTextStream>
#include <QDir>
#include <QTimer>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QHeaderView>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
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
#include "enginesettingsconstants.h"
#include "shogiclock.h"
#include "shogiutils.h"

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
    m_engine2InfoLayoutWidget(nullptr),
    m_gameCount(0),
    m_gameController(nullptr),
    m_selectedField(nullptr),
    m_selectedField2(nullptr),
    m_movedField(nullptr),
    m_waitingSecondClick(false),
    m_analysisModel(nullptr)
{
    ui->setupUi(this);

    // GUIを構成するWidgetなどのnew生成
    initializeComponents();

    // 両対局者の名前の表示
    createPlayerNameAndMoveDisplay();

    // 残り時間、手数、手番の表示
    setupTimeAndTurnIndicators();

    // 棋譜欄の表示設定
    setupGameRecordView();

    // 矢印ボタンの表示
    setupArrowButtons();

    // 評価値グラフの表示
    createEvaluationChartView();

    // 予想手、探索手などの情報表示
    initializeEngine1InfoDisplay();

    // エンジン1の現在の読み筋とUSIプロトコル通信ログの表示
    initializeEngine1ThoughtTab();

    // 対局者名と残り時間を縦ボックス化
    setupPlayerTimeLayout();

    // 対局者名と残り時間、将棋盤を縦に並べて表示
    setupPlayerAndBoardLayout();

    // 棋譜、矢印ボタン、評価値グラフを縦ボックス化
    setupRecordAndEvaluationLayout();

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

    // info行の予想手、探索手、エンジンの読み筋を縦ボックス化
    setupVerticalEngineInfoDisplay();

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
    connect(ui->actionCopyBoardToClipboard, &QAction::triggered, this, &MainWindow::saveShogiBoardImage);

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

    // 棋譜欄の指し手をクリックするとその局面に将棋盤を更新する。
    connect(m_gameRecordView, &QTableView::clicked, this, &MainWindow::updateBoardFromMoveHistory);

    // 棋譜欄下の矢印ボタン
    // 初期局面を表示する。
    connect(m_playButton1, &QPushButton::clicked, this, &MainWindow::navigateToFirstMove);

    // 現局面から10手戻った局面を表示する。
    connect(m_playButton2, &QPushButton::clicked, this, &MainWindow::navigateBackwardTenMoves);

    // 現局面から1手戻った局面を表示する。
    connect(m_playButton3, &QPushButton::clicked, this, &MainWindow::navigateToPreviousMove);

    // 現局面から1手進んだ局面を表示する。
    connect(m_playButton4, &QPushButton::clicked, this, &MainWindow::navigateToNextMove);

    // 現局面から10手進んだ局面を表示する。
    connect(m_playButton5, &QPushButton::clicked, this, &MainWindow::navigateForwardTenMoves);

    // 最終局面を表示する。
    connect(m_playButton6, &QPushButton::clicked, this, &MainWindow::navigateToLastMove);

    // 将棋盤表示でエラーが発生した場合、エラーメッセージを表示する。
    connect(m_shogiView, &ShogiView::errorOccurred, this, &MainWindow::displayErrorMessage);

    // 将棋盤上での左クリックイベントをハンドリングする。
    connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::onShogiViewClicked);

    // 将棋盤上での右クリックイベントをハンドリングする。
    connect(m_shogiView, &ShogiView::rightClicked, this, &MainWindow::onShogiViewRightClicked);
}

// 将棋盤上での左クリックイベントをハンドリングする。
void MainWindow::onShogiViewClicked(const QPoint &pt)
{
    // 1回めのクリックの場合
    if (!m_waitingSecondClick) {
        // １回目にクリックしたマスの座標を保存する。
        m_firstClick = pt;

        // 駒のドラッグを開始する。
        m_shogiView->startDrag(pt);

        // 2回目のクリックを待っているかどうかを示すフラグを設定する。
        m_waitingSecondClick = true;
    }
    // 2回目のクリックの場合
    else {
        // ドラッグを終了する。駒を移動してカーソルを戻す。
        m_shogiView->endDrag();

        // 2回目のクリックを待っているかどうかを示すフラグをリセットする。
        m_waitingSecondClick = false;
    }
}

// 将棋盤上での右クリックイベントをハンドリングする。
void MainWindow::onShogiViewRightClicked(const QPoint &)
{
    // 2回目のクリックの場合
    if (m_waitingSecondClick) {
        // ドラッグを終了する。駒を移動してカーソルを戻す。
        m_shogiView->endDrag();

        // 2回目のクリックを待っているかどうかを示すフラグをリセットする。
        m_waitingSecondClick = false;

        // 駒の移動元と移動先のマスのハイライトを消去する。
        resetSelectionAndHighlight();
    }
}

// GUIを構成するWidgetなどを生成する。
void MainWindow::initializeComponents()
{
    // 将棋の対局全体を管理し、盤面の初期化、指し手の処理、合法手の検証、対局状態の管理を行うクラスのインスタンス
    m_gameController = new ShogiGameController;

    // 将棋盤と駒台を生成する。
    m_shogiView = new ShogiView;

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

// 将棋盤の画像をファイルとして出力する。
void MainWindow::saveShogiBoardImage()
{
    // ファイルダイアログを通じて、画像の保存パスを取得する。
    QString savePath = QFileDialog::getSaveFileName(this, tr("Output the image"), "", "png(*.png);; tiff(*.tiff *.tif);; jpeg(*.jpg *.jpeg)");

    if (savePath.isEmpty()) {
        // 保存パスが空の場合は何もしない。
        return;
    }

    // 拡張子がない場合は、デフォルトで .png を追加する。
    if (QFileInfo(savePath).suffix().isEmpty()) {
        savePath.append(".png");
    }

    // 現在の将棋盤の状態を画像としてキャプチャし、保存する。
    if (!m_shogiView->grab().toImage().save(savePath)) {
        // 画像の保存が失敗した場合は、エラーメッセージを表示する。
        displayErrorMessage(tr("Failed to save the image."));
    }
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
// info行の予想手、探索手、エンジンの読み筋を縦ボックス化したm_widget3の表示・非表示を切り替える。
void MainWindow::toggleEngineAnalysisVisibility()
{
    // 「表示」の「思考」にチェックが入っている場合
    if (ui->actionToggleEngineAnalysis->isChecked()) {
        m_engine2InfoLayoutWidget->setVisible(true);
    }
    // 「表示」の「思考」にチェックが入っていない場合
    else {
        m_engine2InfoLayoutWidget->setVisible(false);
    }
}

// 駒の移動元と移動先のマスのハイライトを消去する。
void MainWindow::clearMoveHighlights()
{
    // 駒の移動元のマスのハイライトを消去する。
    m_shogiView->removeHighlight(m_selectedField);

    // 駒の移動元のマスを削除
    delete m_selectedField;

    // 駒の移動元のマスのポインタを初期化する。
    m_selectedField = nullptr;

    // 駒の移動元のマス2のハイライトを消去する。
    m_shogiView->removeHighlight(m_selectedField2);

    // 駒の移動元のマス2を削除
    delete m_selectedField2;

    // 駒の移動元のマス2のポインタを初期化する。
    m_selectedField2 = nullptr;

    // 駒の移動先のマスのハイライトを消去する。
    m_shogiView->removeHighlight(m_movedField);

    // 駒の移動先のマスを削除
    delete m_movedField;

    // 駒の移動先のマスのポインタを初期化する。
    m_movedField = nullptr;
}

// 駒の移動元と移動先のマスをそれぞれ別の色でハイライトする。
void MainWindow::addMoveHighlights()
{
    // 駒の移動元のマスを薄い赤色でハイライトするフィールドを生成
    m_selectedField = new ShogiView::FieldHighlight(m_gameMoves.at(m_currentMoveIndex - 1).fromSquare.x() + 1,
                                                    m_gameMoves.at(m_currentMoveIndex - 1).fromSquare.y() + 1,
                                                    QColor(255, 0, 0, 50));

    // 駒の移動元のマスをハイライトする。
    m_shogiView->addHighlight(m_selectedField);

    // 駒の移動元のマスを黄色でハイライトするフィールドを生成
    m_movedField = new ShogiView::FieldHighlight(m_gameMoves.at(m_currentMoveIndex - 1).toSquare.x() + 1,
                                                 m_gameMoves.at(m_currentMoveIndex - 1).toSquare.y() + 1,
                                                 Qt::yellow);

    // 駒の移動先のマスをハイライトする。
    m_shogiView->addHighlight(m_movedField);
}

// 棋譜欄の指し手をクリックするとその局面に将棋盤を更新する。
void MainWindow::updateBoardFromMoveHistory()
{
    // 駒の移動元と移動先のマスのハイライトを消去する。
    clearMoveHighlights();

    // 棋譜欄の現在の指し手のインデックスを取得する。
    QModelIndex index = m_gameRecordView->currentIndex();

    // SFEN文字列リストより現在のインデックス行のSFEN文字列を取り出す。
    QString str = m_sfenRecord->at(index.row());

    // SFEN文字列の盤面を描画する。
    m_gameController->board()->setSfen(str);

    // 現在の手数をセットする。
    m_currentMoveIndex = index.row();

    // 手数表示を更新
    m_totalMovesDisplay->setText(QString::number(m_currentMoveIndex));

    // 最終行のインデックスを取得
    int lastRowIndex = m_gameRecordModel->rowCount() - 1;

    // 指し手が１手以上でかつ移動先が最終行でない場合
    if ((m_currentMoveIndex > 0) && (m_currentMoveIndex != lastRowIndex)) {
        // 駒の移動元と移動先のマスをハイライトする。
        addMoveHighlights();
    }
}

// 棋譜欄下の矢印「1手進む」
// 現局面から1手進んだ局面を表示する。
void MainWindow::navigateToNextMove()
{
    // マスのハイライトを消去する。
    clearMoveHighlights();

    // 現在のインデックスを取得する。
    QModelIndex currentIndex = m_gameRecordView->currentIndex();

    // 現在の行を取得する。
    int currentRow = currentIndex.row();

    // 棋譜欄で次の手（1手進む）に移動する。
    int nextRow = currentRow == -1 ? m_gameRecordModel->rowCount() - 1 : currentRow + 1;

    // 棋譜のリストの範囲を超えていないかチェックする。
    if (nextRow >= m_gameRecordModel->rowCount()) {
        // 最後の手に移動する。
        nextRow = m_gameRecordModel->rowCount() - 1;
    }

    // 移動先のインデックスをセットする。
    QModelIndex nextIndex = m_gameRecordModel->index(nextRow, 0, QModelIndex());

    m_gameRecordView->setCurrentIndex(nextIndex);

    // SFEN文字列を取得し、盤面を更新する。
    QString sfenStr = m_sfenRecord->at(nextRow);

    // 盤面を更新する。
    m_gameController->board()->setSfen(sfenStr);

    // 現在の手数を更新する。
    m_currentMoveIndex = nextRow;

    // 手数表示を更新する。
    m_totalMovesDisplay->setText(QString::number(m_currentMoveIndex));

    // 最終行のインデックスを取得する。
    int lastRowIndex = m_gameRecordModel->rowCount() - 1;

    // 移動先が最終行でない場合
    if (nextRow != lastRowIndex) {
        // 駒の移動元と移動先のマスをハイライトする。
        addMoveHighlights();
    }
}

// 棋譜欄下の矢印「10手進む」
// 現局面から10手進んだ局面を表示する。
void MainWindow::navigateForwardTenMoves()
{
    // マスのハイライトを消去する。
    clearMoveHighlights();

    // 現在のインデックスを取得する。
    QModelIndex currentIndex = m_gameRecordView->currentIndex();

    // 現在の行を取得する。
    int currentRow = currentIndex.row();

    // 棋譜欄で次の10手（10手進む）に移動する。
    int nextRow = currentRow == -1 ? m_gameRecordModel->rowCount() - 1 : currentRow + 10;

    // 棋譜のリストの範囲を超えていないかチェックする。
    if (nextRow >= m_gameRecordModel->rowCount()) {
        // 最後の手に移動する。
        nextRow = m_gameRecordModel->rowCount() - 1;
    }

    // 移動先のインデックスをセットする。
    QModelIndex nextIndex = m_gameRecordModel->index(nextRow, 0, QModelIndex());

    m_gameRecordView->setCurrentIndex(nextIndex);

    // SFEN文字列を取得する。
    QString sfenStr = m_sfenRecord->at(nextRow);

    // 盤面を更新する。
    m_gameController->board()->setSfen(sfenStr);

    // 現在の手数を更新する。
    m_currentMoveIndex = nextRow;

    // 手数表示を更新
    m_totalMovesDisplay->setText(QString::number(m_currentMoveIndex));

    // 最終行のインデックスを取得
    int lastRowIndex = m_gameRecordModel->rowCount() - 1;

    // 移動先が最終行でない場合
    if (nextRow != lastRowIndex) {
        // 駒の移動元と移動先のマスをハイライトする。
        addMoveHighlights();
    }
}

// 棋譜欄下の矢印「最後まで進む」
// 最終局面を表示する。
void MainWindow::navigateToLastMove()
{
    // マスのハイライトを消去
    clearMoveHighlights();

    // 最終行のインデックスを取得
    int lastRowIndex = m_gameRecordModel->rowCount() - 1;

    // 最終行のインデックスが有効な場合のみ処理
    if (lastRowIndex >= 0) {
        // 棋譜欄の最終行のインデックスを現在のインデックスにセット
        QModelIndex lastIndex = m_gameRecordModel->index(lastRowIndex, 0);

        m_gameRecordView->setCurrentIndex(lastIndex);

        // SFEN文字列を取得し、盤面を更新
        QString sfenStr = m_sfenRecord->at(lastRowIndex);

        // 盤面を更新する。
        m_gameController->board()->setSfen(sfenStr);

        // 現在の手数を更新
        m_currentMoveIndex = lastRowIndex;

        // 手数表示を更新
        m_totalMovesDisplay->setText(QString::number(m_currentMoveIndex));
    }
}

// 棋譜欄下の矢印「1手戻る」
// 現局面から1手戻った局面を表示する。
void MainWindow::navigateToPreviousMove()
{
    // 棋譜欄下の矢印「1手戻る」ボタンを押した時の元のインデックスを指す。
    QModelIndex indexBefore = m_gameRecordView->currentIndex();

    // 移動先のインデックスを計算する。
    int rowToMove = indexBefore.row() == -1 ? m_gameRecordModel->rowCount() - 1 : indexBefore.row() - 1;

    // 移動先のインデックスが有効な範囲内か確認する。
    if (rowToMove >= 0 && rowToMove < m_gameRecordModel->rowCount()) {
        // 移動先のインデックスを現在のインデックスにセットする。
        QModelIndex indexAfter = m_gameRecordModel->index(rowToMove, 0, QModelIndex());

        // 移動先のインデックスを現在のインデックスにセットする。
        m_gameRecordView->setCurrentIndex(indexAfter);

        // SFEN文字列リストより移動先のインデックス行のSFEN文字列を取り出す。
        QString str = m_sfenRecord->at(indexAfter.row());

        // SFEN文字列の盤面を描画する。
        m_gameController->board()->setSfen(str);

        // 現在の手数をセットする。
        m_currentMoveIndex = indexAfter.row();

        // 手数表示を更新
        m_totalMovesDisplay->setText(QString::number(m_currentMoveIndex));
    }

    // マスのハイライトを更新する。
    clearMoveHighlights();

    // 1手以上指している時、駒の移動元と移動先のマスをそれぞれ別の色でハイライトする。
    if (m_currentMoveIndex) addMoveHighlights();
}

// 棋譜欄下の矢印「10手戻る」
// 現局面から10手戻った局面を表示する。
void MainWindow::navigateBackwardTenMoves()
{
    // 棋譜欄下の矢印「10手戻る」ボタンを押した時の元のインデックスを指す。
    QModelIndex indexBefore = m_gameRecordView->currentIndex();

    // 現在の行が無効な場合、最後の行に設定する。
    int rowToMove = indexBefore.row() == -1 ? m_gameRecordModel->rowCount() - 1 : indexBefore.row();

    // 10手戻る先の行数を計算する。
    rowToMove -= 10;

    // 移動先のインデックスが有効な範囲内か確認する。無効な場合は最初の手（インデックス0）に移動する。
    rowToMove = (rowToMove < 0) ? 0 : rowToMove;

    QModelIndex indexAfter = m_gameRecordModel->index(rowToMove, 0, QModelIndex());

    // 移動先のインデックスを現在のインデックスにセットする。
    m_gameRecordView->setCurrentIndex(indexAfter);

    // SFEN文字列リストより移動先のインデックス行のSFEN文字列を取り出す。
    QString str = m_sfenRecord->at(indexAfter.row());

    // SFEN文字列の盤面を描画する。
    m_gameController->board()->setSfen(str);

    // 現在の手数をセットする。
    m_currentMoveIndex = indexAfter.row();

    // 手数表示を更新
    m_totalMovesDisplay->setText(QString::number(m_currentMoveIndex));

    // マスのハイライトを更新する。
    clearMoveHighlights();

    // 1手以上指している時、駒の移動元と移動先のマスをそれぞれ別の色でハイライトする。
    if (m_currentMoveIndex) addMoveHighlights();
}

// 棋譜欄下の矢印「最初の局面に戻る」
// 現局面から最初の局面を表示する。
void MainWindow::navigateToFirstMove()
{
    // マスのハイライトを消去
    clearMoveHighlights();

    // 棋譜欄の最初のインデックス（初期状態）をセットする。
    QModelIndex firstIndex = m_gameRecordModel->index(0, 0);

    m_gameRecordView->setCurrentIndex(firstIndex);

    // SFEN文字列リストから最初のインデックス行のSFEN文字列を取得し、盤面を更新
    QString sfenStr = m_sfenRecord->at(0);

    // 盤面を更新する。
    m_gameController->board()->setSfen(sfenStr);

    // 手数表示を更新
    m_totalMovesDisplay->setText("0");
    m_totalMovesDisplay->update();
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
        m_series2->remove(m_currentMoveIndex, m_scoreCp.last());
        break;

    // 平手 Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:
        // 待ったをした時の2手前のposition文字列をセット
        m_positionStr1 = m_positionStrList.at(index);

        // 待ったボタンを押す前の評価値グラフの値を削除
        // (X, Y) = (m_numberOfMoves, m_scoreCp.last())
        m_series1->remove(m_currentMoveIndex, m_scoreCp.last());
        break;

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        // 待ったをした時の2手前のposition文字列をセット
        m_positionStr1 = m_positionStrList.at(index);

        // 待ったボタンを押す前の評価値グラフの値を削除
        // (X, Y) = (m_numberOfMoves, m_scoreCp.last())
        m_series1->remove(m_currentMoveIndex, m_scoreCp.last());
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        // 待ったをした時の2手前のposition文字列をセット
        m_positionStr2 = m_positionStrList.at(index);

        // 待ったボタンを押す前の評価値グラフの値を削除
        // (X, Y) = (m_numberOfMoves, m_scoreCp.last())
        m_series2->remove(m_currentMoveIndex, m_scoreCp.last());
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
    // 2手前の手数
    int moveNumber = m_currentMoveIndex - 2;

    // 2手前が0手（初期状態）以上の場合
    if (moveNumber >= 0) {
        // 2手分の指し手マスデータを削除する。
        m_gameMoves.removeLast();
        m_gameMoves.removeLast();

        // 待ったをした時のposition文字列のセットと評価値グラフの値を削除する。
        // 待ったをする。〔将棋などで〕withdraw one's moves
        handleUndoMove(moveNumber);

        // 待ったをした場合、不要になったposition文字列を削除する。
        m_positionStrList.removeLast();
        m_positionStrList.removeLast();

        // 2手前の手数を現在の手数にセットする。
        m_currentMoveIndex = moveNumber;

        // 2手前の手数のSFEN文字列を取り出す。
        QString str = m_sfenRecord->at(m_currentMoveIndex);

        // 将棋盤の表示を2手前の手数の局面に描画する。
        m_gameController->board()->setSfen(str);

        // 将棋盤上部の手数表示を2手前の手数に設定する。
        m_totalMovesDisplay->setText(QString::number(m_currentMoveIndex));

        // 2手分のSFEN文字列を削除
        m_sfenRecord->removeLast();
        m_sfenRecord->removeLast();

        // マスのハイライトを消去する。
        clearMoveHighlights();

        // 2手分の棋譜欄のデータ削除
        m_gameRecordModel->removeLastItem();
        m_gameRecordModel->removeLastItem();

        // 2手前の残り時間に戻す。
        m_shogiClock->undo();
    }

    // 1手以上の場合、指し手のマスをハイライトする。
    if (m_currentMoveIndex) addMoveHighlights();
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
    switch (m_playMode) {
    // Player1: Human, Player2: Human
    case HumanVsHuman:
        m_playersName1->setText("▲" + m_humanName1);
        m_playersName2->setText("△" + m_humanName2);
        break;

    // Player1: Human, Player2: USI Engine
    case EvenHumanVsEngine:
        m_playersName1->setText("▲" + m_humanName1);
        m_playersName2->setText("△" + m_engineName2);
        break;

    // Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:
        m_playersName1->setText("▲" + m_engineName1);
        m_playersName2->setText("△" + m_humanName2);
        break;

    // Player1: USI Engine, Player2: USI Engine
    case EvenEngineVsEngine:
        m_playersName1->setText("▲" + m_engineName1);
        m_playersName2->setText("△" + m_engineName2);
        break;

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        m_playersName1->setText("▲" + m_humanName1);
        m_playersName2->setText("△" + m_engineName2);
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        m_playersName1->setText("▲" + m_engineName1);
        m_playersName2->setText("△" + m_humanName2);
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine（上手）
    case HandicapEngineVsEngine:
        m_playersName1->setText("▲" + m_engineName1);
        m_playersName2->setText("△" + m_engineName2);
        break;

    // まだ対局を開始していない状態
    case NotStarted:

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
}

// 将棋盤上部の対局者名欄、手数欄を作成し、パックしてウィジェットにまとめる。
void MainWindow::createPlayerNameAndMoveDisplay()
{
    // フォントの指定
    QFont font("Noto Sans CJK JP", 10);

    // 対局者名1（先手）欄を作成する。
    m_playersName1 = new QLineEdit;

    // 対局者名1（先手）にフォントをセットする。
    m_playersName1->setFont(font);

    // 対局者名1（先手）を手動修正できないようにする。
    m_playersName1->setReadOnly(true);

    // 対局者名1の欄の幅を可能な限り、広げる。
    m_playersName1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 対局者名2（後手）欄を作成する。
    m_playersName2 = new QLineEdit;

    // 対局者名2（後手）にフォントをセットする。
    m_playersName2->setFont(font);

    // 対局者名2（後手）を手動修正できないようにする。
    m_playersName2->setReadOnly(true);

    // 対局者名2の欄の幅を可能な限り、広げる。
    m_playersName2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 手数欄の作成
    m_totalMovesDisplay = new QLineEdit;

    // 手数欄にフォントをセットする。
    m_totalMovesDisplay->setFont(font);

    // 手数欄を右寄せにする。
    m_totalMovesDisplay->setAlignment(Qt::AlignRight);

    // 手数欄を手動修正できないようにする。
    m_totalMovesDisplay->setReadOnly(true);

    // 手数欄の幅を60に固定する。
    m_totalMovesDisplay->setFixedWidth(60);

    // 「手目」の欄を作成
    m_moveNumberLabel = new QLabel;

    // 「手目」のフォントをセットする。
    m_moveNumberLabel->setFont(font);

    // 「手目」を記載
    m_moveNumberLabel->setText("手目");

    // スペーサーの作成
    m_spacer1 = new QWidget;

    // スペーサーのポリシーを設定
    // QSizePolicy::Expanding 可能な限りウィジェットを大きく表示する。
    // QSizePolicy::Preferred sizeHint()以内でウィジェットを拡大可能
    m_spacer1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // hboxLayoutに対局者名2、スペーサー、手数欄、「手数」ラベル、スペーサー、対局者名1の
    // 6つのウィジェットを追加
    QHBoxLayout* hboxLayout = new QHBoxLayout;
    hboxLayout->addWidget(m_playersName2);
    //hboxLayout->addWidget(m_spacer1);
    hboxLayout->addWidget(m_totalMovesDisplay);
    hboxLayout->addWidget(m_moveNumberLabel);
    //hboxLayout->addWidget(m_spacer1);
    hboxLayout->addWidget(m_playersName1);

    // 6つのウィジェットをパックしてm_firstLineとする。
    m_firstLine = new QWidget;
    m_firstLine->setLayout(hboxLayout);
}

// 将棋盤上部の残り時間と手番を示す赤丸を設定する。
void MainWindow::setupTimeAndTurnIndicators()
{
    // 手番を示す赤丸画像の設定
    QPixmap pix(":/icons/circle-128.png");

    // 先手の手番画像を表示するラベル作成
    m_turnLabel1 = new QLabel;

    // 先手の手番画像のサイズを設定
    m_turnLabel1->setPixmap(pix.scaled(12, 12, Qt::KeepAspectRatio));

    // 先手の手番画像を対局前なので非表示にする。
    m_turnLabel1->setVisible(false);

    // 後手の手番画像を表示するラベル作成
    m_turnLabel2 = new QLabel;

    // 後手の手番画像のサイズを設定
    m_turnLabel2->setPixmap(pix.scaled(12, 12, Qt::KeepAspectRatio));

    // 後手の手番画像を対局前なので非表示にする。
    m_turnLabel2->setVisible(false);

    // フォントの指定
    QFont font("Noto Sans CJK JP", 20, QFont::Bold);

    // 先手の残り時間を表示するラベル作成
    m_remainTimeText1 = new QLabel;

    // パレットを青色に指定する。
    QPalette palette1 = m_remainTimeText1->palette();
    palette1.setColor(QPalette::WindowText, Qt::blue);

    // 先手の残り時間の文字色を青色に指定
    m_remainTimeText1->setPalette(palette1);

    // 指定したフォントに設定
    m_remainTimeText1->setFont(font);

    // 残り時間の初期値として「00:00:00」を設定
    m_remainTimeText1->setText("00:00:00");

    // 後手の残り時間を表示するラベル作成
    m_remainTimeText2 = new QLabel;

    // パレットを青色に指定する。
    QPalette palette2 = m_remainTimeText2->palette();
    palette2.setColor(QPalette::WindowText, Qt::blue);

    // 後手の残り時間の文字色を青色に指定
    m_remainTimeText2->setPalette(palette2);

    // 指定したフォントに設定
    m_remainTimeText2->setFont(font);

    // 残り時間の初期値として「00:00:00」を設定
    m_remainTimeText2->setText("00:00:00");

    // スペーサーの作成
    m_spacer2 = new QWidget;

    // スペーサーのポリシーを設定
    // QSizePolicy::Expanding 可能な限りウィジェットを大きく表示する。
    // QSizePolicy::Preferred sizeHint()以内でウィジェットを拡大可能
    m_spacer2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // スペーサー、後手の残り時間、後手の手番を示す赤丸、スペーサー、先手の手番を示す赤丸、
    // 先手の残り時間、スペーサーをhboxLayoutに追加
    QHBoxLayout* hboxLayout = new QHBoxLayout;
    hboxLayout->addWidget(m_spacer2);
    hboxLayout->addWidget(m_remainTimeText2);
    hboxLayout->addWidget(m_turnLabel2);
    hboxLayout->addWidget(m_spacer2);
    hboxLayout->addWidget(m_turnLabel1);
    hboxLayout->addWidget(m_remainTimeText1);
    hboxLayout->addWidget(m_spacer2);

    // 7つのウィジェットをパックしてm_secondLineとする。
    m_secondLine = new QWidget;
    m_secondLine->setLayout(hboxLayout);
}

// 駒台を含む将棋盤全体の画像をクリップボードにコピーする。
void MainWindow::copyBoardToClipboard()
{
    QClipboard* clipboard = QApplication::clipboard();

    clipboard->setPixmap(m_shogiView->grab());
}

// 将棋盤を180度回転させる。それに伴って、対局者名、残り時間も入れ替える。
// m_shogiView->flipMode ノーマル:0、反転:1
// flipModeは、0か1かのフラグでm_shogiViewをリペイントする際に、
// 先手が将棋盤の手前か後手が手前になるかが決まる。
void MainWindow::flipBoardAndUpdatePlayerInfo()
{
    // 先手・後手の対局者名を入れ替える。
    QString tmpName = m_playersName1->text();
    m_playersName1->setText(m_playersName2->text());
    m_playersName2->setText(tmpName);

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

    // GUIの残り時間を更新する。
    updateRemainingTimeDisplay();
}

// 棋譜欄の表示を設定する。
void MainWindow::setupGameRecordView()
{
    // 棋譜欄を作成する。
    m_gameRecordView = new QTableView;

    // シングルクリックで選択する。
    m_gameRecordView->setSelectionMode(QAbstractItemView::SingleSelection);

    // 行のみを選択する。
    m_gameRecordView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 垂直方向のヘッダは表示させない。
    m_gameRecordView->verticalHeader()->setVisible(false);

    // 棋譜のListModelを作成する。
    m_gameRecordModel = new KifuRecordListModel;

    // 棋譜のListModelをセットする。
    m_gameRecordView->setModel(m_gameRecordModel);

    // 「指し手」の列を伸縮可能にする。
    m_gameRecordView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    // 「消費時間」の列を伸縮可能にする。
    m_gameRecordView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

// 棋譜欄下の矢印ボタンを作成する。
void MainWindow::setupArrowButtons()
{
    // 6つの矢印ボタンを作成する。
    m_playButton1 = new QPushButton;
    m_playButton2 = new QPushButton;
    m_playButton3 = new QPushButton;
    m_playButton4 = new QPushButton;
    m_playButton5 = new QPushButton;
    m_playButton6 = new QPushButton;

    QPalette pal;

    // パレットを緑色に設定する。
    pal.setColor(QPalette::Button, QColor(79, 146, 114));

    // 6つの矢印ボタンの色を緑に設定する。
    m_playButton1->setPalette(pal);
    m_playButton2->setPalette(pal);
    m_playButton3->setPalette(pal);
    m_playButton4->setPalette(pal);
    m_playButton5->setPalette(pal);
    m_playButton6->setPalette(pal);

    // 矢印ボタンの画像をセットする。
    m_playButton1->setIcon(QIcon(":/icons/gtk-media-next-rtl.png"));
    m_playButton2->setIcon(QIcon(":/icons/gtk-media-forward-rtl.png"));
    m_playButton3->setIcon(QIcon(":/icons/gtk-media-play-rtr.png"));
    m_playButton4->setIcon(QIcon(":/icons/gtk-media-play-ltr.png"));
    m_playButton5->setIcon(QIcon(":/icons/gtk-media-forward-ltr.png"));
    m_playButton6->setIcon(QIcon(":/icons/gtk-media-next-ltr.png"));

    // 矢印ボタンのサイズを指定する。
    m_playButton1->setIconSize(QSize(32, 32));
    m_playButton2->setIconSize(QSize(32, 32));
    m_playButton3->setIconSize(QSize(32, 32));
    m_playButton4->setIconSize(QSize(32, 32));
    m_playButton5->setIconSize(QSize(32, 32));
    m_playButton6->setIconSize(QSize(32, 32));

    // 6つの矢印ボタンを横ボックス化したレイアウトを作成する。
    QHBoxLayout* hboxLayout = new QHBoxLayout;
    hboxLayout->addWidget(m_playButton1);
    hboxLayout->addWidget(m_playButton2);
    hboxLayout->addWidget(m_playButton3);
    hboxLayout->addWidget(m_playButton4);
    hboxLayout->addWidget(m_playButton5);
    hboxLayout->addWidget(m_playButton6);

    // 6つの矢印ボタン用のウィジェットを作成する。
    m_arrows = new QWidget;

    // 6つの矢印ボタンをウィジェットとしてm_arrowsに設定する。
    m_arrows->setLayout(hboxLayout);

    // 6つの矢印ボタンの高さを50に固定する。
    m_arrows->setFixedHeight(50);

    // 6つの矢印ボタン全体の最小幅を600に設定する。
    m_arrows->setMinimumWidth(600);
}

// 評価値グラフを表示する。
void MainWindow::createEvaluationChartView()
{
    // 参考．https://doc.qt.io/qt-5/qtcharts-customchart-example.html
    // フォントの指定
    QFont labelsFont("Noto Sans CJK JP", 6);

    // 横軸「手数」を作成する。
    m_axisX = new QValueAxis;

    // 横軸「手数」の範囲指定。1000手まで表示する。
    m_axisX->setRange(0, 1000);

    // 横軸「手数」の目盛りの数を設定する。
    // 200にすると間隔が5ではなく6になる部分が生じるので201に設定している。
    m_axisX->setTickCount(201);

    // 横軸「手数」の目盛りのフォントを設定する。
    m_axisX->setLabelsFont(labelsFont);

    // 横軸「手数」は整数で表示する。
    m_axisX->setLabelFormat("%i");

    // 縦軸「評価値」を作成する。
    m_axisY = new QValueAxis;

    // 縦軸「評価値」の範囲は、-2000〜2000までに設定する。
    m_axisY->setRange(-2000, 2000);

    // 縦軸「評価値」の目盛りの数を設定する。
    m_axisY->setTickCount(5);

    // 縦軸「評価値」の目盛りのフォントを設定する。
    m_axisY->setLabelsFont(labelsFont);

    // 縦軸「評価値」は整数で表示する。
    m_axisY->setLabelFormat("%i");

    // チャートを作成する。
    m_chart = new QChart;

    // チャートの背面をオセロ盤のような濃い緑色に設定する。
    QColor darkGreen(0, 100, 0); // または QColor darkGreen("#006400");
    m_chart->setBackgroundBrush(QBrush(darkGreen));

    // 目盛りの文字色を明るい灰色に設定する。
    QColor lightGray(192, 192, 192);
    QPen pen(lightGray);
    m_axisX->setLabelsColor(pen.color());
    m_axisY->setLabelsColor(pen.color());

    // チャートの凡例（説明書き）を非表示にする。
    m_chart->legend()->hide();

    // 横軸「手数」の目盛りは、チャートの一番下に表示する。
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    // 縦軸「評価値」の目盛りは、チャートの一番左に表示する。
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    // 先手評価値グラフを作成する。
    m_series1 = new QLineSeries;

    // 先手評価値グラフをチャートに追加する。
    m_chart->addSeries(m_series1);

    // 先手評価値グラフ軸を横軸「手数」、縦軸「評価値」に設定する。
    m_series1->attachAxis(m_axisX);
    m_series1->attachAxis(m_axisY);

    // 後手評価値グラフを作成する。
    m_series2 = new QLineSeries;

    // 後手評価値グラフをチャートに追加する。
    m_chart->addSeries(m_series2);

    // 後手評価値グラフを横軸「手数」、縦軸「評価値」に設定する。
    m_series2->attachAxis(m_axisX);
    m_series2->attachAxis(m_axisY);

    // チャートを作成する。
    m_chartView = new QChartView(m_chart);

    // チャートのレンダリングをアンチエイリアスに設定する。
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // チャートの高さを170に固定する。
    m_chartView->setFixedHeight(170);

    // チャートの幅を10000に固定する。
    m_chartView->setFixedWidth(10000);

    // レイアウトを作成する。
    QHBoxLayout* hboxLayout = new QHBoxLayout;

    // レイアウトにチャートを指定する。
    hboxLayout->addWidget(m_chartView);

    // スクロールエリアを作成する。
    m_scrollArea = new QScrollArea;

    // スクロールエリアの高さを200に設定する。
    m_scrollArea->setFixedHeight(200);

    // スクロールエリアのバーを常に表示する。
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // スクロールエリア用のウィジェットを作成する。
    QWidget* scrollAreaWidget = new QWidget;

    // スクロールエリアのウィジェットのレイアウトにm_layを指定する。
    scrollAreaWidget->setLayout(hboxLayout);

    // スクロールエリアにm_scrollAreaWidgetContentsを指定する。
    m_scrollArea->setWidget(scrollAreaWidget);
}

// 対局モードに応じて将棋盤下部に表示されるエンジン名をセットする。
void MainWindow::setEngineNamesBasedOnMode()
{
    // 対局モード
    switch (m_playMode) {
    // 平手 Player1: Human, Player2: USI Engine
    case EvenHumanVsEngine:

    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    case HandicapHumanVsEngine:
        // エンジン名2をエンジン名欄1に表示する。
        m_engineNameText1->setText(m_engineName2);
        break;

    // 平手 Player1: USI Engine, Player2: Human
    case EvenEngineVsHuman:

    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    case HandicapEngineVsHuman:
        // エンジン名1をエンジン名欄1に表示する。
        m_engineNameText1->setText(m_engineName1);
        break;

    // 平手 Player1: USI Engine, Player2: USI Engine
    case EvenEngineVsEngine:
        // エンジン名1、エンジン名2をエンジン名欄1、エンジン名欄2に表示する。
        m_engineNameText1->setText(m_engineName1);
        m_engineNameText2->setText(m_engineName2);
        break;

    // 駒落ち Player1: USI Engine（下手）, Player2: USI Engine
    case HandicapEngineVsEngine:
        // エンジン名1、エンジン名2をエンジン名欄2、エンジン名欄1に表示する。
        m_engineNameText1->setText(m_engineName2);
        m_engineNameText2->setText(m_engineName1);
        break;

    // まだ対局を開始していない状態
    case NotStarted:

    // 人間同士の対局
    case HumanVsHuman:

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
}

// エンジン1の予想手、探索手などの情報を表示する。
void MainWindow::initializeEngine1InfoDisplay()
{
    // フォントを指定する。
    QFont font("Noto Sans CJK JP", 8);

    // エンジン名1の表示欄を作成する。
    m_engineNameText1 = new QLineEdit;

    // エンジン名1の表示欄のフォントを指定する。
    m_engineNameText1->setFont(font);

    // エンジン名1の表示欄の横幅を300に設定する。
    m_engineNameText1->setFixedWidth(300);

    // エンジン名1の表示欄をリードオンリーにする。
    m_engineNameText1->setReadOnly(true);

    // 予想手1のラベル欄を作成する。
    m_predictiveHandLabel1 = new QLabel;

    // 予想手1のラベル欄に「予想手」と表示する。
    m_predictiveHandLabel1->setText("予想手");

    // 予想手1のラベル欄にフォントを設定する。
    m_predictiveHandLabel1->setFont(font);

    // 予想手1の欄を作成する。
    m_predictiveHandText1 = new QLineEdit;

    // 予想手1の欄をリードオンリーにする。
    m_predictiveHandText1->setReadOnly(true);

    // 予想手1の欄にフォントを設定する。
    m_predictiveHandText1->setFont(font);

    // 探索手1のラベル欄を作成する。
    m_searchedHandLabel1 = new QLabel;

    // 探索手1のラベル欄に「探索手」と表示する。
    m_searchedHandLabel1->setText("探索手");

    // 探索手1のラベル欄にフォントを設定する。
    m_searchedHandLabel1->setFont(font);

    // 探索手1の欄を作成する。
    m_searchedHandText1 = new QLineEdit;

    // 探索手1の欄をリードオンリーにする。
    m_searchedHandText1->setReadOnly(true);

    // 探索手1の欄にフォントを設定する。
    m_searchedHandText1->setFont(font);

    // 深さ1のラベル欄を作成する。
    m_depthLabel1 = new QLabel;

    // 深さ1のラベル欄に「深さ」と表示する。
    m_depthLabel1->setText("深さ");

    // 深さ1のラベル欄にフォントを設定する。
    m_depthLabel1->setFont(font);

    // 探さ1の欄を作成する。
    m_depthText1 = new QLineEdit;

    // 探さ1の欄をリードオンリーにする。
    m_depthText1->setReadOnly(true);

    // 探さ1の欄の表示を右揃えにする。
    m_depthText1->setAlignment(Qt::AlignRight);

    // 探さ1の欄にフォントを設定する。
    m_depthText1->setFont(font);

    // 探さ1の欄の横幅を60に固定する。
    m_depthText1->setFixedWidth(60);

    // ノード数1のラベル欄を作成する。
    m_nodesLabel1 = new QLabel;

    // ノード数1のラベル欄に「ノード数」と表示する。
    m_nodesLabel1->setText("ノード数");

    // ノード数1のラベル欄にフォントを設定する。
    m_nodesLabel1->setFont(font);

    // ノード数1の欄を作成する。
    m_nodesText1 = new QLineEdit;

    // ノード数1の欄をリードオンリーにする。
    m_nodesText1->setReadOnly(true);

    // ノード数1の欄の表示を右揃えにする。
    m_nodesText1->setAlignment(Qt::AlignRight);

    // ノード数1のラベル欄にフォントを設定する。
    m_nodesText1->setFont(font);

    // 探索局面数1のラベル欄の作成する。
    m_npsLabel1 = new QLabel;

    // 探索局面数1のラベル欄に「探索局面数」と表示する。
    m_npsLabel1->setText("探索局面数");

    // 探索局面数1のラベル欄にフォントを設定する。
    m_npsLabel1->setFont(font);

    // 探索局面数1の欄を作成する。
    m_npsText1 = new QLineEdit;

    // 探索局面数1の欄をリードオンリーにする。
    m_npsText1->setReadOnly(true);

    // 探索局面数1の欄の表示を右揃えにする。
    m_npsText1->setAlignment(Qt::AlignRight);

    // 探索局面数1の欄にフォントを設定する。
    m_npsText1->setFont(font);

    // ハッシュ使用率1のラベル欄を作成する。
    m_hashfullLabel1 = new QLabel;

    // ハッシュ使用率1のラベル欄に「ハッシュ使用率」と表示する。
    m_hashfullLabel1->setText("ハッシュ使用率");

    // ハッシュ使用率1のラベル欄にフォントを設定する。
    m_hashfullLabel1->setFont(font);

    // ハッシュ使用率1の欄を作成する。
    m_hashfullText1 = new QLineEdit;

    // ハッシュ使用率1の欄をリードオンリーにする。
    m_hashfullText1->setReadOnly(true);

    // ハッシュ使用率1の欄の表示を右揃えにする。
    m_hashfullText1->setAlignment(Qt::AlignRight);

    // ハッシュ使用率1の欄にフォントを設定する。
    m_hashfullText1->setFont(font);

    // ハッシュ使用率1の欄の幅を60に固定する。
    m_hashfullText1->setFixedWidth(60);

    // エンジン名1、予想手1、探索手1、深さ1、ノード数1、局面探索数1、ハッシュ使用率1を横ボックス化したレイアウト
    QHBoxLayout* hboxLayout = new QHBoxLayout;
    hboxLayout->addWidget(m_engineNameText1);
    hboxLayout->addWidget(m_predictiveHandLabel1);
    hboxLayout->addWidget(m_predictiveHandText1);
    hboxLayout->addWidget(m_searchedHandLabel1);
    hboxLayout->addWidget(m_searchedHandText1);
    hboxLayout->addWidget(m_depthLabel1);
    hboxLayout->addWidget(m_depthText1);
    hboxLayout->addWidget(m_nodesLabel1);
    hboxLayout->addWidget(m_nodesText1);
    hboxLayout->addWidget(m_npsLabel1);
    hboxLayout->addWidget(m_npsText1);
    hboxLayout->addWidget(m_hashfullLabel1);
    hboxLayout->addWidget(m_hashfullText1);

    // 上記ウィジェットを纏めて1つのウィジェットm_infoWidget1に設定する。
    m_infoWidget1 = new QWidget;
    m_infoWidget1->setLayout(hboxLayout);

    // エンジン名が変更された場合、GUIのエンジン名を変更する。
    connect(m_lineEditModel1, &UsiCommLogModel::engineNameChanged, this, &MainWindow::updateEngine1NameDisplay);

    // 予想手が変更された場合、GUIの予想手を変更する。
    connect(m_lineEditModel1, &UsiCommLogModel::predictiveMoveChanged, this, &MainWindow::updateEngine1PredictedMoveDisplay);

    // 探索手が変更された場合、GUIの探索手を変更する。
    connect(m_lineEditModel1, &UsiCommLogModel::searchedMoveChanged, this, &MainWindow::updateEngine1SearchedMoveDisplay);

    // 深さが変更された場合、GUIの深さを変更する。
    connect(m_lineEditModel1, &UsiCommLogModel::searchDepthChanged, this, &MainWindow::updateEngine1DepthDisplay);

    // ノード数が変更された場合、GUIのノード数を変更する。
    connect(m_lineEditModel1, &UsiCommLogModel::nodeCountChanged, this, &MainWindow::updateEngine1NodesCountDisplay);

    // 局面探索数が変更された場合、GUIの局面探索数を変更する。
    connect(m_lineEditModel1, &UsiCommLogModel::nodesPerSecondChanged, this, &MainWindow::updateEngine1NpsDisplay);

    // ハッシュ使用率が変更された場合、GUIのハッシュ使用率を変更する。
    connect(m_lineEditModel1, &UsiCommLogModel::hashUsageChanged, this, &MainWindow::updateEngine1HashUsageDisplay);
}

// GUIのエンジン名欄1を変更する。
void MainWindow::updateEngine1NameDisplay()
{
    m_engineNameText1->setText(m_lineEditModel1->engineName());
}

// GUIの予想手欄1を変更する。
void MainWindow::updateEngine1PredictedMoveDisplay()
{
    m_predictiveHandText1->setText(m_lineEditModel1->predictiveMove());
}

// GUIの探索手欄1を変更する。
void MainWindow::updateEngine1SearchedMoveDisplay()
{
    m_searchedHandText1->setText(m_lineEditModel1->searchedMove());
}

// GUIの深さ欄1を変更する。
void MainWindow::updateEngine1DepthDisplay()
{
    m_depthText1->setText(m_lineEditModel1->searchDepth());
}

// GUIのノード数欄1を変更する。
void MainWindow::updateEngine1NodesCountDisplay()
{
    m_nodesText1->setText(m_lineEditModel1->nodeCount());
}

// GUIの局面探索数欄1を変更する。
void MainWindow::updateEngine1NpsDisplay()
{
    m_npsText1->setText(m_lineEditModel1->nodesPerSecond());
}

// GUIのハッシュ使用率欄1を変更する。
void MainWindow::updateEngine1HashUsageDisplay()
{
    m_hashfullText1->setText(m_lineEditModel1->hashUsage());
}

// エンジン2の予想手、探索手などの情報を表示する。
void MainWindow::initializeEngine2InfoDisplay()
{
    // フォントを指定する。
    QFont font("Noto Sans CJK JP", 8);

    // エンジン名2の表示欄を作成する。
    m_engineNameText2 = new QLineEdit;

    // エンジン名2の表示欄のフォントを指定する。
    m_engineNameText2->setFont(font);

    // エンジン名2の表示欄の横幅を300に設定する。
    m_engineNameText2->setFixedWidth(300);

    // エンジン名2の表示欄をリードオンリーにする。
    m_engineNameText2->setReadOnly(true);

    // 予想手2のラベル欄を作成する。
    m_predictiveHandLabel2 = new QLabel;

    // 予想手2のラベル欄に「予想手」と表示する。
    m_predictiveHandLabel2->setText("予想手");

    // 予想手2のラベル欄にフォントを設定する。
    m_predictiveHandLabel2->setFont(font);

    // 予想手2の欄を作成する。
    m_predictiveHandText2 = new QLineEdit;

    // 予想手2の欄をリードオンリーにする。
    m_predictiveHandText2->setReadOnly(true);

    // 予想手2の欄にフォントを設定する。
    m_predictiveHandText2->setFont(font);

    // 探索手2のラベル欄を作成する。
    m_searchedHandLabel2 = new QLabel;

    // 探索手2のラベル欄に「探索手」と表示する。
    m_searchedHandLabel2->setText("探索手");

    // 探索手2のラベル欄にフォントを設定する。
    m_searchedHandLabel2->setFont(font);

    // 探索手2の欄を作成する。
    m_searchedHandText2 = new QLineEdit;

    // 探索手2の欄をリードオンリーにする。
    m_searchedHandText2->setReadOnly(true);

    // 探索手2の欄にフォントを設定する。
    m_searchedHandText2->setFont(font);

    // 深さ2のラベル欄を作成する。
    m_depthLabel2 = new QLabel;

    // 深さ2のラベル欄に「深さ」と表示する。
    m_depthLabel2->setText("深さ");

    // 深さ2のラベル欄にフォントを設定する。
    m_depthLabel2->setFont(font);

    // 探さ2の欄を作成する。
    m_depthText2 = new QLineEdit;

    // 探さ2の欄をリードオンリーにする。
    m_depthText2->setReadOnly(true);

    // 探さ2の欄の表示を右揃えにする。
    m_depthText2->setAlignment(Qt::AlignRight);

    // 探さ2の欄にフォントを設定する。
    m_depthText2->setFont(font);

    // 探さ2の欄の横幅を60に固定する。
    m_depthText2->setFixedWidth(60);

    // ノード数2のラベル欄を作成する。
    m_nodesLabel2 = new QLabel;

    // ノード数2のラベル欄に「ノード数」と表示する。
    m_nodesLabel2->setText("ノード数");

    // ノード数2のラベル欄にフォントを設定する。
    m_nodesLabel2->setFont(font);

    // ノード数2の欄を作成する。
    m_nodesText2 = new QLineEdit;

    // ノード数2の欄をリードオンリーにする。
    m_nodesText2->setReadOnly(true);

    // ノード数2の欄の表示を右揃えにする。
    m_nodesText2->setAlignment(Qt::AlignRight);

    // ノード数2のラベル欄にフォントを設定する。
    m_nodesText2->setFont(font);

    // 探索局面数2のラベル欄を作成する。
    m_npsLabel2 = new QLabel;

    // 探索局面数2のラベル欄に「探索局面数」と表示する。
    m_npsLabel2->setText("探索局面数");

    // 探索局面数2のラベル欄にフォントを設定する。
    m_npsLabel2->setFont(font);

    // 探索局面数2の欄を作成する。
    m_npsText2 = new QLineEdit;

    // 探索局面数2の欄をリードオンリーにする。
    m_npsText2->setReadOnly(true);

    // 探索局面数2の欄の表示を右揃えにする。
    m_npsText2->setAlignment(Qt::AlignRight);

    // 探索局面数2の欄にフォントを設定する。
    m_npsText2->setFont(font);

    // ハッシュ使用率2のラベル欄を作成する。
    m_hashfullLabel2 = new QLabel;

    // ハッシュ使用率2のラベル欄に「ハッシュ使用率」と表示する。
    m_hashfullLabel2->setText("ハッシュ使用率");

    // ハッシュ使用率2のラベル欄にフォントを設定する。
    m_hashfullLabel2->setFont(font);

    // ハッシュ使用率2の欄を作成する。
    m_hashfullText2 = new QLineEdit;

    // ハッシュ使用率2の欄をリードオンリーにする。
    m_hashfullText2->setReadOnly(true);

    // ハッシュ使用率2の欄の表示を右揃えにする。
    m_hashfullText2->setAlignment(Qt::AlignRight);

    // ハッシュ使用率2の欄にフォントを設定する。
    m_hashfullText2->setFont(font);

    // ハッシュ使用率2の欄の幅を60に固定する。
    m_hashfullText2->setFixedWidth(60);

    // エンジン名2、予想手2、探索手2、深さ2、ノード数2、局面探索数2、ハッシュ使用率2を横ボックス化したレイアウト
    QHBoxLayout* hboxLayout = new QHBoxLayout;
    hboxLayout->addWidget(m_engineNameText2);
    hboxLayout->addWidget(m_predictiveHandLabel2);
    hboxLayout->addWidget(m_predictiveHandText2);
    hboxLayout->addWidget(m_searchedHandLabel2);
    hboxLayout->addWidget(m_searchedHandText2);
    hboxLayout->addWidget(m_depthLabel2);
    hboxLayout->addWidget(m_depthText2);
    hboxLayout->addWidget(m_nodesLabel2);
    hboxLayout->addWidget(m_nodesText2);
    hboxLayout->addWidget(m_npsLabel2);
    hboxLayout->addWidget(m_npsText2);
    hboxLayout->addWidget(m_hashfullLabel2);
    hboxLayout->addWidget(m_hashfullText2);

    // 上記ウィジェットを纏めて2つのウィジェットm_infoWidget2に設定する。
    m_infoWidget2 = new QWidget;
    m_infoWidget2->setLayout(hboxLayout);

    // エンジン名が変更された場合、GUIのエンジン名を変更する。
    connect(m_lineEditModel2, &UsiCommLogModel::engineNameChanged, this, &MainWindow::updateEngine2NameDisplay);

    // 予想手が変更された場合、GUIの予想手を変更する。
    connect(m_lineEditModel2, &UsiCommLogModel::predictiveMoveChanged, this, &MainWindow::updateEngine2PredictedMoveDisplay);

    // 探索手が変更された場合、GUIの探索手を変更する。
    connect(m_lineEditModel2, &UsiCommLogModel::searchedMoveChanged, this, &MainWindow::updateEngine2SearchedMoveDisplay);

    // 深さが変更された場合、GUIの深さを変更する。
    connect(m_lineEditModel2, &UsiCommLogModel::searchDepthChanged, this, &MainWindow::updateEngine2DepthDisplay);

    // ノード数が変更された場合、GUIのノード数を変更する。
    connect(m_lineEditModel2, &UsiCommLogModel::nodeCountChanged, this, &MainWindow::updateEngine2NodesCountDisplay);

    // 局面探索数が変更された場合、GUIの局面探索数を変更する。
    connect(m_lineEditModel2, &UsiCommLogModel::nodesPerSecondChanged, this, &MainWindow::updateEngine2NpsDisplay);

    // ハッシュ使用率が変更された場合、GUIのハッシュ使用率を変更する。
    connect(m_lineEditModel2, &UsiCommLogModel::hashUsageChanged, this, &MainWindow::updateEngine2HashUsageDisplay);
}

// GUIのエンジン名欄2を変更する。
void MainWindow::updateEngine2NameDisplay()
{
    m_engineNameText2->setText(m_lineEditModel2->engineName());
}

// GUIの予想手欄2を変更する。
void MainWindow::updateEngine2PredictedMoveDisplay()
{
    m_predictiveHandText2->setText(m_lineEditModel2->predictiveMove());
}

// GUIの探索手欄2を変更する。
void MainWindow::updateEngine2SearchedMoveDisplay()
{
    m_searchedHandText2->setText(m_lineEditModel2->searchedMove());
}

// GUIの深さ欄2を変更する。
void MainWindow::updateEngine2DepthDisplay()
{
    m_depthText2->setText(m_lineEditModel2->searchDepth());
}

// GUIのノード数欄2を変更する。
void MainWindow::updateEngine2NodesCountDisplay()
{
    m_nodesText2->setText(m_lineEditModel2->nodeCount());
}

// GUIの局面探索数欄2を変更する。
void MainWindow::updateEngine2NpsDisplay()
{
    m_npsText2->setText(m_lineEditModel2->nodesPerSecond());
}

// GUIのハッシュ使用率欄2を変更する。
void MainWindow::updateEngine2HashUsageDisplay()
{
    m_hashfullText2->setText(m_lineEditModel2->hashUsage());
}

// エンジン1の思考タブを作成する。
void MainWindow::initializeEngine1ThoughtTab()
{
    // 将棋エンジン1の思考タブを作成する。
    m_usiView1 = new QTableView;

    // エンジン1の思考結果をGUI上で表示するためのクラスのインスタンス
    m_modelThinking1 = new ShogiEngineThinkingModel;

    // 将棋エンジン1の思考タブにモデルm_modelThinking1を設定する。
    m_usiView1->setModel(m_modelThinking1);

    // 将棋エンジン1の思考タブの横幅を4列目をストレッチする形に設定する。
    m_usiView1->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    // 将棋エンジンのUSIプロトコル通信ログを表示するためのテキストエディタを作成する。
    m_usiCommLogEdit = new QPlainTextEdit;

    // 将棋エンジン1のUSIプロトコル通信ログを表示するためのテキストエディタをリードオンリーにする。
    m_usiCommLogEdit->setReadOnly(true);

    // タブウィジェットを作成する。
    m_tab1 = new QTabWidget;

    // 思考1タブとUSIプロトコル通信ログタブをタブウィジェットに追加する。
    m_tab1->addTab(m_usiView1, "思考1");
    m_tab1->addTab(m_usiCommLogEdit, "USIプロトコル通信ログ");

    // 将棋エンジン1のタブウィジェットの高さを150に設定する。
    m_tab1->setMinimumHeight(150);

    // 将棋エンジン1のUSIプロトコル通信ログを表示するためのシグナル・スロットを設定する。
    connect(m_lineEditModel1, &UsiCommLogModel::usiCommLogChanged, this, [this]() {
        m_usiCommLogEdit->appendPlainText(m_lineEditModel1->usiCommLog());
        });
}

// 将棋エンジン2の思考タブを作成する。
void MainWindow::initializeEngine2ThoughtTab()
{
    // 将棋エンジン2の思考タブを作成する。
    m_usiView2 = new QTableView;

    // エンジン2の思考結果をGUI上で表示するためのクラスのインスタンス
    m_modelThinking2 = new ShogiEngineThinkingModel;

    // 将棋エンジン2の思考タブにモデルm_modelThinking2を設定する。
    m_usiView2->setModel(m_modelThinking2);

    // 将棋エンジン2の思考タブの横幅を4列目をストレッチする形に設定する。
    m_usiView2->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    // 将棋エンジン2のUSIプロトコル通信ログを表示するためのテキストエディタを作成する。
    m_tab2 = new QTabWidget;

    // 将棋エンジン2の思考タブをタブウィジェットに追加する。
    m_tab2->addTab(m_usiView2, "思考2");

    // 将棋エンジン2のタブウィジェットの高さを150に設定する。
    m_tab2->setMinimumHeight(150);

    // 将棋エンジン2のUSIプロトコル通信ログを表示するためのテキストエディタを作成する。
    connect(m_lineEditModel2, &UsiCommLogModel::usiCommLogChanged, this, [this]() {
        m_usiCommLogEdit->appendPlainText(m_lineEditModel2->usiCommLog());
        });
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

// 対局者名と残り時間を縦ボックス化する。
void MainWindow::setupPlayerTimeLayout()
{
    // 縦ボックスレイアウトを作成する。
    QVBoxLayout* vboxLayout = new QVBoxLayout;

    // 対局者名と残り時間を縦ボックス化する。
    vboxLayout->addWidget(m_firstLine);
    vboxLayout->setSpacing(0);
    vboxLayout->addWidget(m_secondLine);

    // 縦ボックスウィジェットを作成する。
    m_playerInfoLayoutWidget = new QWidget;

    // 縦ボックスウィジェットに縦ボックスレイアウトを設定する。
    m_playerInfoLayoutWidget->setLayout(vboxLayout);
}

// 対局者名と残り時間、将棋盤を縦に並べて表示する。
void MainWindow::setupPlayerAndBoardLayout()
{
    // 縦ボックスレイアウトを作成する。
    QVBoxLayout* vboxLayout = new QVBoxLayout;

    // 対局者名と残り時間、将棋盤を縦に並べて表示する。
    vboxLayout->addWidget(m_playerInfoLayoutWidget);
    vboxLayout->setSpacing(0);
    vboxLayout->addWidget(m_shogiView);

    // 縦ボックスウィジェットのサイズを固定にする。
    vboxLayout->setSizeConstraint(QLayout::SetFixedSize);

    // 縦ボックスウィジェットを作成する。
    m_playerAndBoardLayoutWidget = new QWidget;

    // 縦ボックスウィジェットに縦ボックスレイアウトを設定する。
    m_playerAndBoardLayoutWidget->setLayout(vboxLayout);
}

// 棋譜、矢印ボタン、評価値グラフを縦ボックス化する。
void MainWindow::setupRecordAndEvaluationLayout()
{
    // 縦ボックスレイアウトを作成する。
    QVBoxLayout* vboxLayout = new QVBoxLayout;

    // 棋譜、矢印ボタン、評価値グラフを縦ボックス化する。
    vboxLayout->addWidget(m_gameRecordView);
    vboxLayout->addWidget(m_arrows);
    vboxLayout->addWidget(m_scrollArea);

    // 縦ボックスウィジェットを作成する。
    m_gameRecordLayoutWidget = new QWidget;

    // 縦ボックスウィジェットに縦ボックスレイアウトを設定する。
    m_gameRecordLayoutWidget->setLayout(vboxLayout);
}

// 対局者名と残り時間、将棋盤と棋譜、矢印ボタン、評価値グラフのグループを横に並べて表示する。
void MainWindow::setupHorizontalGameLayout()
{
    m_hsplit = new QSplitter(Qt::Horizontal);

    m_hsplit->addWidget(m_playerAndBoardLayoutWidget);
    m_hsplit->addWidget(m_gameRecordLayoutWidget);
}

// info行の予想手、探索手、エンジンの読み筋を縦ボックス化する。
void MainWindow::setupVerticalEngineInfoDisplay()
{
    // 縦ボックスレイアウトを作成する。
    QVBoxLayout* vboxLayout = new QVBoxLayout;

    // エンジン名1、予想手1、探索手1、深さ1、ノード数1、局面探索数1、ハッシュ使用率1、思考1タブをレイアウトに追加する。
    vboxLayout->addWidget(m_infoWidget1);
    vboxLayout->setSpacing(0);
    vboxLayout->addWidget(m_tab1);

    // エンジン2の予想手、探索手などの情報を表示する。
    initializeEngine2InfoDisplay();

    // 将棋エンジン2の思考タブを作成する。
    initializeEngine2ThoughtTab();

    // エンジン名2、予想手2、探索手2、深さ2、ノード数2、局面探索数2、ハッシュ使用率2、思考2タブをレイアウトに追加する。
    vboxLayout->addWidget(m_infoWidget2);
    vboxLayout->addWidget(m_tab2);

    // エンジン名2、予想手2、探索手2、深さ2、ノード数2、局面探索数2、ハッシュ使用率2、思考2タブを非表示にする。
    m_infoWidget2->hide();
    m_tab2->hide();

    // 新しいウィジェットを作成する。
    QWidget* newWidget3 = new QWidget;
    newWidget3->setLayout(vboxLayout);

    // newWidget3の親ウィジェットをthisに設定する。
    // これにより、メモリ管理が簡単になり、親ウィジェットと一緒に表示・非表示が管理されるようになる。
    newWidget3->setParent(this);

    // 古いウィジェットを削除する。
    if (m_engine2InfoLayoutWidget) {
        // 親子関係を解除する。
        m_engine2InfoLayoutWidget->setParent(nullptr);

        // イベントループが次回アイドル状態になったときに削除されるようにする。
        m_engine2InfoLayoutWidget->deleteLater();
    }

    // 新しいウィジェットをm_widget3に設定する。
    m_engine2InfoLayoutWidget = newWidget3;
}

// 対局者名と残り時間、将棋盤、棋譜、矢印ボタン、評価値グラフのウィジェットと
// info行の予想手、探索手、エンジンの読み筋のウィジェットを縦ボックス化して
// セントラルウィジェットにセットする。
void MainWindow::initializeCentralGameDisplay()
{
    // 縦ボックスレイアウトを作成する。
    QVBoxLayout* vboxLayout = new QVBoxLayout;

    // 対局者名と残り時間、将棋盤、棋譜、矢印ボタン、評価値グラフのウィジェットと
    // info行の予想手、探索手、エンジンの読み筋のウィジェットを縦ボックスレイアウトに追加する。
    vboxLayout->addWidget(m_hsplit);
    vboxLayout->setSpacing(0);
    vboxLayout->addWidget(m_engine2InfoLayoutWidget);

    // 新しいウィジェットを作成する。
    QWidget* newWidget = new QWidget;

    // 新しいウィジェットに縦ボックスレイアウトを設定する。
    newWidget->setLayout(vboxLayout);

    // セントラルウィジェットを取得する。
    QWidget* oldWidget = centralWidget();

    // 新しいウィジェットをセントラルウィジェットに設定する。
    setCentralWidget(newWidget);

    // 古いウィジェットを削除する。
    if (oldWidget) {
        // 親子関係を解除する。
        oldWidget->setParent(nullptr);

        // イベントループが次回アイドル状態になったときに削除されるようにする。
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
    // 将棋の駒画像を各駒文字（1文字）にセットする。
    // 駒文字と駒画像をm_piecesに格納する。
    // m_piecesの型はQMap<char, QIcon>
    // m_boardにboardをセットする。
    // 将棋盤データが更新されたら再描画する。
    // 将棋盤と駒台のサイズは固定にする。
    // 将棋盤と駒台のマスのサイズをセットする。
    // 将棋盤と駒台の再描画
    renderShogiBoard();

    // 対局者名の設定
    // 対局モードに応じて将棋盤上部に表示される対局者名をセットする。
    setPlayersNamesForMode();

    // エンジン名の設定
    // 対局モードに応じて将棋盤下部に表示されるエンジン名をセットする。
    setEngineNamesBasedOnMode();
}

// 棋譜欄の下の矢印ボタンを無効にする。
void MainWindow::disableArrowButtons()
{
    m_playButton1->setEnabled(false);
    m_playButton2->setEnabled(false);
    m_playButton3->setEnabled(false);
    m_playButton4->setEnabled(false);
    m_playButton5->setEnabled(false);
    m_playButton6->setEnabled(false);
}

// 棋譜欄の下の矢印ボタンを有効にする。
void MainWindow::enableArrowButtons()
{
    m_playButton1->setEnabled(true);
    m_playButton2->setEnabled(true);
    m_playButton3->setEnabled(true);
    m_playButton4->setEnabled(true);
    m_playButton5->setEnabled(true);
    m_playButton6->setEnabled(true);
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
    m_gameController->changeCurrentPlayer();

    // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
    updateTurnAndTimekeepingDisplay();

    // 手番が対局者1の場合
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // 対局結果を表示する。
        displayGameOutcome(ShogiGameController::Player1Wins);
    }
    // 手番が対局者2の場合
    else {
        // 対局結果を表示する。
        displayGameOutcome(ShogiGameController::Player2Wins);
    }


    // 棋譜欄の下の矢印ボタンを有効にする。
    enableArrowButtons();

    // 棋譜欄をシングルクリックで選択できるようにする。
    m_gameRecordView->setSelectionMode(QAbstractItemView::SingleSelection);
}

// 棋譜欄の最後に表示する投了の文字列を設定する。
// 対局モードが平手のエンジン対エンジンの場合
void MainWindow::setResignationMove(bool isPlayerOneResigning)
{
    // 対局モードが平手のエンジン対エンジンまたは駒落ちのエンジン対エンジンの場合
    if ((m_playMode == EvenHumanVsEngine) || (m_playMode == HandicapHumanVsEngine) || (m_playMode == HandicapEngineVsEngine)) {
        m_lastMove = isPlayerOneResigning ? "△投了" : "▲投了";
    }
    // それ以外の対局モードの場合
    else {
        m_lastMove = isPlayerOneResigning ? "▲投了" : "△投了";
    }
}

// メニューで「投了」をクリックした場合の処理を行う。
void MainWindow::handleResignation()
{
    // 将棋クロックの停止と将棋エンジンへの対局終了コマンド送信処理を行う。
    stopClockAndSendCommands();

    // 棋譜欄の最後に表示する投了の文字列を設定する。
    // 対局モードが平手のエンジン対エンジンの場合
    setResignationMove(false);

    // 対局結果の表示とGUIの更新処理を行う。
    displayResultsAndUpdateGui();
}

// 将棋エンジンが2"bestmove resign"コマンドで投了した場合の処理を行う。
void MainWindow::handleEngineTwoResignation()
{
    // 将棋クロックを停止する。
    m_shogiClock->stopClock();

    // 対局モードが平手のエンジン対エンジンまたは、駒落ちのエンジン対エンジンの場合
    // 将棋エンジン2に対して、gameover winコマンドとquitコマンドを送信する。
    sendCommandsToEngineTwo();

    // 棋譜欄の最後に表示する投了の文字列を設定する。
    // 対局モードが平手のエンジン対エンジンの場合
    setResignationMove(true);

    // 対局結果の表示とGUIの更新処理を行う。
    displayResultsAndUpdateGui();
}

// 将棋エンジン1が"bestmove resign"コマンドで投了した場合の処理を行う。
void MainWindow::handleEngineOneResignation()
{
    m_shogiClock->stopClock();

    // エンジン1にgameover winコマンドとquitコマンドを送信する。
    m_usi1->sendGameOverWinAndQuitCommands();

    // 棋譜欄の最後に表示する投了の文字列を設定する。
    // 対局モードが平手のエンジン対エンジンの場合
    setResignationMove(false);

    // 対局結果の表示とGUIの更新処理を行う。
    displayResultsAndUpdateGui();
}

// 将棋盤と駒台を描画する。
// 将棋の駒画像を各駒文字（1文字）にセットする。
// 駒文字と駒画像をm_piecesに格納する。
// m_piecesの型はQMap<char, QIcon>
// m_boardにboardをセットする。
// 将棋盤データが更新されたら再描画する。
// 将棋盤と駒台のサイズは固定にする。
// 将棋盤と駒台のマスのサイズをセットする。
void MainWindow::renderShogiBoard()
{
    // 将棋の駒画像を各駒文字（1文字）にセットする。
    // 駒文字と駒画像をm_piecesに格納する。
    // m_piecesの型はQMap<char, QIcon>
    m_shogiView->setPieces();

    // m_boardにboardをセットする。
    // 将棋盤データが更新されたら再描画する。
    m_shogiView->setBoard(m_gameController->board());

    // 将棋盤と駒台のサイズは固定にする。
    m_shogiView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // 将棋盤と駒台のマスのサイズをセットする。
    m_shogiView->setFieldSize(QSize(m_shogiView->squareSize(), m_shogiView->squareSize()));

    // 将棋盤と駒台の再描画
    m_shogiView->update();
}

// エンジン1の評価値グラフの再描画を行う。
void MainWindow::redrawEngine1EvaluationGraph()
{
    // シリーズを削除する。
    m_chart->removeSeries(m_series1);

    // 対局モードが駒落ちのエンジン対エンジン、あるいは平手の人間対エンジン、あるいは駒落ちの人間対エンジン、解析モードの場合
    if ((m_playMode == HandicapEngineVsEngine) || (m_playMode == EvenHumanVsEngine) || (m_playMode == HandicapHumanVsEngine)
        || (m_playMode == AnalysisMode)) {
        // エンジン1の評価値グラフの色を白に設定する。
        m_series1->setColor(Qt::white);
    } else {
        // エンジン1の評価値グラフの色を黒に設定する。
        m_series1->setColor(Qt::black);
    }

    // シリーズのペンを取得する。
    QPen pen = m_series1->pen();

    // シリーズのペンの太さを2に設定する。
    pen.setWidth(2);

    // シリーズのペンを設定する。
    m_series1->setPen(pen);

    // シリーズの点を表示する。
    m_series1->setPointsVisible(true);

    // ラベルのフォントを設定する。
    QFont labelsFont("Noto Sans CJK JP", 6);

    // X軸の範囲を設定する。
    m_axisX->setRange(0, 1000);

    // X軸の目盛りの数を設定する。
    m_axisX->setTickCount(201);

    // X軸のラベルのフォントを設定する。
    m_axisX->setLabelsFont(labelsFont);

    // X軸のラベルのフォーマットを設定する。
    m_axisX->setLabelFormat("%i");

    // Y軸の範囲を設定する。
    m_axisY->setRange(-2000, 2000);

    // Y軸の目盛りの数を設定する。
    m_axisY->setTickCount(5);

    // Y軸のラベルのフォントを設定する。
    m_axisY->setLabelsFont(labelsFont);

    // Y軸のラベルのフォーマットを設定する。
    m_axisY->setLabelFormat("%i");

    // 凡例を非表示にする。
    m_chart->legend()->hide();

    // シリーズを追加する。
    m_chart->addSeries(m_series1);

    // X軸を追加する。
    m_series1->attachAxis(m_axisX);

    // Y軸を追加する。
    m_series1->attachAxis(m_axisY);

    // 対局モード
    switch (m_playMode) {
    // 対局モードが解析モード、平手のエンジン対エンジン、駒落ちのエンジン対エンジン、駒落ちの人間対エンジンの場合
    case AnalysisMode:
    case EvenEngineVsHuman:
    case EvenEngineVsEngine:
    case HandicapEngineVsHuman:
        // エンジン1の評価値を追加する。
        m_series1->append(m_currentMoveIndex, m_usi1->lastScoreCp());
        break;

    // 平手の人間対エンジン、駒落ちのエンジン対エンジン、駒落ちの人間対エンジンの場合
    case EvenHumanVsEngine:
    case HandicapEngineVsEngine:
    case HandicapHumanVsEngine:
        // エンジン1の評価値を符号を反転させて追加する。
        m_series1->append(m_currentMoveIndex, - m_usi1->lastScoreCp());
        break;

    default:
        break;
    }

    // 評価値のリストに評価値を追加する。
    m_scoreCp.append(m_usi1->lastScoreCp());

    // 評価値グラフを更新する。
    m_chartView->update();
}

// エンジン2の評価値グラフの再描画を行う。
void MainWindow::redrawEngine2EvaluationGraph()
{
    // シリーズを削除する。
    m_chart->removeSeries(m_series2);

    // 対局モードが駒落ちのエンジン対エンジンの場合
    if (m_playMode == HandicapEngineVsEngine) {
        // エンジン2の評価値グラフの色を黒に設定する。
        m_series2->setColor(Qt::black);
    }
    // それ以外の対局モードの場合
    else {
        // エンジン2の評価値グラフの色を白に設定する。
        m_series2->setColor(Qt::white);
    }

    // シリーズのペンを取得する。
    QPen pen = m_series2->pen();

    // シリーズのペンの太さを2に設定する。
    pen.setWidth(2);

    // シリーズのペンを設定する。
    m_series2->setPen(pen);

    // シリーズの点を表示する。
    m_series2->setPointsVisible(true);

    // ラベルのフォントを設定する。
    QFont labelsFont("Noto Sans CJK JP", 6);

    // X軸の範囲を設定する。
    m_axisX->setRange(0, 1000);

    // X軸の目盛りの数を設定する。
    m_axisX->setTickCount(201);

    // X軸のラベルのフォントを設定する。
    m_axisX->setLabelsFont(labelsFont);

    // X軸のラベルのフォーマットを設定する。
    m_axisX->setLabelFormat("%i");

    // Y軸の範囲を設定する。
    m_axisY->setRange(-2000, 2000);

    // Y軸の目盛りの数を設定する。
    m_axisY->setTickCount(5);

    // Y軸のラベルのフォントを設定する。
    m_axisY->setLabelsFont(labelsFont);

    // Y軸のラベルのフォーマットを設定する。
    m_axisY->setLabelFormat("%i");

    // 凡例を非表示にする。
    m_chart->legend()->hide();

    // シリーズを追加する。
    m_chart->addSeries(m_series2);

    // X軸を追加する。
    m_series2->attachAxis(m_axisX);

    // Y軸を追加する。
    m_series2->attachAxis(m_axisY);

    // 対局モードが駒落ちのエンジン対エンジンの場合
    if (m_playMode == HandicapEngineVsEngine) {
        // エンジン2の評価値をそのまま追加する。
        m_series2->append(m_currentMoveIndex, m_usi2->lastScoreCp());
    }
    // それ以外の対局モードの場合
    else {
        // エンジン2の評価値を符号を反転させて追加する。
        m_series2->append(m_currentMoveIndex, - m_usi2->lastScoreCp());
    }

    // 評価値のリストに評価値を追加する。
    m_scoreCp.append(m_usi2->lastScoreCp());

    // 評価値グラフを更新する。
    m_chartView->update();
}

// 対局者のクリックをリセットし、選択したハイライトを削除する。
void MainWindow::resetSelectionAndHighlight()
{
    // クリックポイントをリセットする。
    m_clickPoint = QPoint();

    // 選択したハイライトを削除する。
    deleteHighlight(m_selectedField);
}

// 指す駒を左クリックで選択した場合、そのマスをオレンジ色にする。
void MainWindow::selectPieceAndHighlight(const QPoint& field)
{
    // マスに駒がある場合
    if (m_shogiView->board()->getPieceCharacter(field.x(), field.y()) != ' ') {
        // 選択したマスをクリックポイントに設定する。
        m_clickPoint = field;

        // 前に選択したマスのハイライトを削除する。
        m_shogiView->removeHighlight(m_selectedField);

        // 選択したマスをオレンジ色にする。
        m_selectedField = new ShogiView::FieldHighlight(field.x(), field.y(), QColor(255, 0, 0, 50));

        // 選択したマスを表示する。
        m_shogiView->addHighlight(m_selectedField);
    }
}

// 既存のハイライトをクリアし、新しいハイライトを作成して表示する。
void MainWindow::updateHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& field, const QColor& color)
{
    // highlightFieldがnullでない場合
    if (highlightField) {
        // 既存のハイライトをクリアする。
        deleteHighlight(highlightField);
    }

    // 新しいハイライトを作成し、それを表示する。
    addNewHighlight(highlightField, field, color);
}

// 既存のハイライトをクリアし（nullでないことが前提）、新しいハイライトを作成して表示する。
void MainWindow::clearAndSetNewHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& field, const QColor& color)
{
    // 既存のハイライトをクリアする。
    deleteHighlight(highlightField);

    // 新しいハイライトを作成し、それを表示する。
    addNewHighlight(highlightField, field, color);
}

// 指定されたハイライトを削除する。
void MainWindow::deleteHighlight(ShogiView::FieldHighlight*& highlightField)
{
    // highlightFieldがnullptrでない場合
    if (highlightField != nullptr) {
        // ハイライトを削除する。
        m_shogiView->removeHighlight(highlightField);

        // 既存のハイライトのポインタをnullに設定する。
        delete highlightField;

        // ハイライトのポインタをnullptrに設定する。
        highlightField = nullptr;
    }
}

// 新しいハイライトを作成し、それを表示する。
void MainWindow::addNewHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& position, const QColor& color)
{
    // 新しいハイライトを作成する。
    highlightField = new ShogiView::FieldHighlight(position.x(), position.y(), color);

    // 指定されたハイライトオブジェクトをハイライトリストに追加し、表示を更新する。
    m_shogiView->addHighlight(highlightField);
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

// 将棋のGUIでクリックされたポイントに基づいて駒の移動を処理する。
// 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
void MainWindow::handleClickForPlayerVsEngine(const QPoint& field)
{
    // 移動先のマスが移動元のマスと同じになってしまった場合
    if (field == m_clickPoint) {
        // 移動元のマスの選択を解除する。
        resetSelectionAndHighlight();

        return;
    }

    // 人間の指した手の移動元のマスをoutFromに設定する。
    QPoint outFrom = m_clickPoint;

    // 人間の指した手の移動先のマスをoutToに設定する。
    QPoint outTo = field;

    // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
    // 将棋エンジンが指した手の移動元のマスがoutFromに、移動先のマスがoutToに上書きされる。
    bool isMoveValid;

    try {
        isMoveValid = m_gameController->validateAndMove(outFrom, outTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves);
    } catch (const std::exception& e) {
        // エラーメッセージを表示する。
        displayErrorMessage(e.what());

        // エラーが発生した場合、処理を終了する。
        return;
    }

    if (isMoveValid) {
        // 現在の駒の移動先の筋と段番号を直前の移動先として保存しておく。
        m_usi1->setPreviousFileTo(outTo.x());
        m_usi1->setPreviousRankTo(outTo.y());

        // 既存の移動先のハイライトをクリアし、新しいハイライトを作成してビューに追加する。
        updateHighlight(m_movedField, outTo, Qt::yellow);

        // 既存の移動元のハイライトをクリアする。
        deleteHighlight(m_selectedField2);

        // 将棋盤内をマウスでクリックしても反応しないようにする。
        m_shogiView->setMouseClickMode(false);

        // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
        updateTurnAndTimekeepingDisplay();

        try {
            // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
            m_usi1->handleHumanVsEngineCommunication(m_positionStr1, m_positionPonder1, outFrom, outTo, m_countDownMilliSec1,
                                                     m_bTime, m_wTime, m_positionStrList, m_addEachMoveMiliSec1, m_addEachMoveMiliSec2,
                                                     m_useByoyomi);
        } catch (const std::exception& e) {
            // エラーメッセージを表示する。
            displayErrorMessage(e.what());

            // エラーが発生した場合、処理を終了する。
            return;
        }

        // 将棋エンジンが投了した場合
        if (m_usi1->isResignMove()) {
            // 将棋エンジンが"bestmove resign"コマンドで投了した場合の処理を行う。
            handleEngineTwoResignation();

            // 処理を終了する。
            return;
        }

        // 移動元のマスをオレンジ色に着色する。
        addNewHighlight(m_selectedField2, outFrom, QColor(255, 0, 0, 50));

        // 将棋エンジンが指そうとした手が合法手だった場合、盤面を更新する。
        bool isMoveValid;

        try {
            isMoveValid = m_gameController->validateAndMove(outFrom, outTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves);
        } catch (const std::exception& e) {
            // エラーメッセージを表示する。
            displayErrorMessage(e.what());

            // エラーが発生した場合、処理を終了する。
            return;
        }

        if (isMoveValid) {
            // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
            updateTurnAndTimekeepingDisplay();

            // 将棋エンジンの指し手が合法手だったのでpositionコマンド文字列をリストに追加する。
            m_positionStrList.append(m_positionStr1);

            // 将棋盤内をマウスでクリックして反応できるように設定を戻す。
            m_shogiView->setMouseClickMode(true);

            // 移動先のマスを黄色に着色する。
            updateHighlight(m_movedField, outTo, Qt::yellow);

            // 評価値グラフを更新する。
            redrawEngine1EvaluationGraph();
        }
        // 将棋エンジンが指そうとした手が合法手でない場合、エラー処理を行う。
        else {
            // エラーメッセージを表示する。
            displayErrorMessage(tr("An error occurred in MainWindow::handleNonNullClickPoint. The move attempted by the shogi engine is not legal."));

            return;
        }
    }

    // クリックをリセットして選択していたハイライトを削除する。
    resetSelectionAndHighlight();

    // 成・不成のフラグを不成に戻しておく。
    m_gameController->setPromote(false);
}

// 人間対エンジンあるいはエンジン対人間の対局で、クリックされたマスに基づいて駒の移動を処理する。
void MainWindow::handlePlayerVsEngineClick(const QPoint& field)
{
    // 移動させる駒をクリックした場合
    if (m_clickPoint.isNull()) {
        // 指す駒を左クリックで選択した時にそのマスをオレンジ色にする。
        selectPieceAndHighlight(field);
    }
    // すでに移動させたい駒を選択している場合
    else {
        // 将棋のGUIでクリックされたポイントに基づいて駒の移動を処理する。
        // 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
        handleClickForPlayerVsEngine(field);
    }
}

// 人間対人間の対局で、クリックされたポイントに基づいて駒の移動を処理する。
// 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
void MainWindow::handleClickForHumanVsHuman(const QPoint& field)
{
    // 移動先のマスが移動元のマスと同じになってしまった場合
    if (field == m_clickPoint) {
        // 移動元のマスの選択を解除する。
        resetSelectionAndHighlight();

        return;
    }

    // 人間の指した手の移動元のマスをoutFromに設定する。
    QPoint outFrom = m_clickPoint;

    // 人間の指した手の移動先のマスをoutToに設定する。
    QPoint outTo = field;

    // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
    // 将棋エンジンが指した手の移動元のマスがoutFromに、移動先のマスがoutToに上書きされる。
    bool isMoveValid;

    try {
        isMoveValid = m_gameController->validateAndMove(outFrom, outTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves);
    } catch (const std::exception& e) {
        // エラーメッセージを表示する。
        displayErrorMessage(e.what());

        // エラーが発生した場合、処理を終了する。
        return;
    }

    if (isMoveValid) {
        // 既存の移動先のハイライトをクリアし、新しいハイライトを作成してビューに追加する。
        updateHighlight(m_movedField, outTo, Qt::yellow);

        // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
        updateTurnAndTimekeepingDisplay();
    }

    // 移動元のマスの選択を解除する。
    resetSelectionAndHighlight();
}

// 将棋クロックの手番を設定し、GUIの手番表示を更新する。
void MainWindow::updateTurnStatus(int currentPlayer, bool visibleLabel1)
{
    // 将棋クロックの手番を設定する。
    m_shogiClock->setCurrentPlayer(currentPlayer);

    // GUIの手番を示す赤丸を表示する。
    m_turnLabel1->setVisible(visibleLabel1);
    m_turnLabel2->setVisible(!visibleLabel1);
}

// 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
void MainWindow::updateTurnDisplay()
{
    // 手番が対局者1の場合
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // 将棋クロックの手番を対局者1に設定し、GUIの手番表示を更新する。
        updateTurnStatus(1, true);
    }
    // 手番が対局者2の場合
    else if (m_gameController->currentPlayer() == ShogiGameController::Player2) {
        // 将棋クロックの手番を対局者2に設定し、GUIの手番表示を更新する。
        updateTurnStatus(2, false);
    }
}

// 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
void MainWindow::updateTurnAndTimekeepingDisplay()
{
    //begin
    qDebug() << "@@@ before MainWindow::updateTurnAndTimekeepingDisplay()";
    qDebug() << "m_bTime: " << m_bTime;
    qDebug() << "m_wTime: " << m_wTime;
    //end

    if (m_useByoyomi) {
        // 「時間切れを負けにする」が有効な場合
        if (m_isLoseOnTimeout) {
            // 対局者1が秒読みに入った場合
            if (m_shogiClock->byoyomi1Applied()) {
                // 対局者1の持ち時間を0にする。
                m_bTime = "0";
            }
            // 対局者1が秒読みに入っていない場合
            else {
                // 対局者1の持ち時間をミリ秒単位に変換する。
                m_bTime = QString::number(m_shogiClock->getPlayer1TimeInt() * 1000);
            }

            // 対局者2が秒読みに入った場合
            if (m_shogiClock->byoyomi2Applied()) {
                // 対局者2の持ち時間を0にする。
                m_wTime = "0";
            }
            // 対局者2が秒読みに入っていない場合
            else {
                // 対局者2の持ち時間をミリ秒単位に変換する。
                m_wTime = QString::number(m_shogiClock->getPlayer2TimeInt() * 1000);
            }
        }
    }
    //
    else {
        // 対局者1の持ち時間をミリ秒単位に変換する。
        m_bTime = QString::number(m_shogiClock->getPlayer1TimeInt() * 1000 - m_addEachMoveMiliSec1);
    }

    //begin
    qDebug() << "@@@ middle MainWindow::updateTurnAndTimekeepingDisplay()";
    qDebug() << "m_bTime: " << m_bTime;
    qDebug() << "m_wTime: " << m_wTime;
    //end

    // 手番が対局者1の場合
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // 対局者2に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
        // 考慮時間を総考慮時間に追加する。
        m_shogiClock->applyByoyomiAndResetConsideration2(m_useByoyomi);

        // 棋譜を更新し、UIの表示も同時に更新する。
        updateGameRecord(m_shogiClock->getPlayer2ConsiderationAndTotalTime());

        // 対局者2の考慮時間を0に設定する。
        m_shogiClock->setPlayer2ConsiderationTime(0);

        // 将棋クロックの手番を対局者1に設定し、GUIの手番表示を更新する。
        updateTurnStatus(1, true);
    }
    // 手番が対局者2の場合
    else if (m_gameController->currentPlayer() == ShogiGameController::Player2) {
        // 対局者1に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
        // 考慮時間を総考慮時間に追加する。
        m_shogiClock->applyByoyomiAndResetConsideration1(m_useByoyomi);

        // 棋譜を更新し、UIの表示も同時に更新する。
        updateGameRecord(m_shogiClock->getPlayer1ConsiderationAndTotalTime());

        // 対局者1の考慮時間を0に設定する。
        m_shogiClock->setPlayer1ConsiderationTime(0);

        // 将棋クロックの手番を対局者2に設定し、GUIの手番表示を更新する。
        updateTurnStatus(2, false);
    }

    //begin
    qDebug() << "@@@ after MainWindow::updateTurnAndTimekeepingDisplay()";
    qDebug() << "m_bTime: " << m_bTime;
    qDebug() << "m_wTime: " << m_wTime;
    //end
}

// 人間対人間の対局で、クリックされたマスに基づいて駒の移動を処理する。
void MainWindow::handleHumanVsHumanClick(const QPoint& field)
{
    // 移動させる駒をクリックした場合
    if (m_clickPoint.isNull()) {
        // 指す駒を左クリックで選択した時にそのマスをオレンジ色にする。
        selectPieceAndHighlight(field);
    }
    // すでに移動させる駒をクリックしている場合
    else {
        // 人間対人間の対局で、クリックされたポイントに基づいて駒の移動を処理する。
        // 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
        handleClickForHumanVsHuman(field);
    }
}

// 局面編集モードで、クリックされたポイントに基づいて駒の移動を処理する。
// 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
void MainWindow::handleClickForEditMode(const QPoint& field)
{
    // 移動先のマスが移動元のマスと同じになってしまった場合
    if (field == m_clickPoint) {
        // 移動元のマスの選択を解除する。
        resetSelectionAndHighlight();

        return;
    }

    // 人間の指した手の移動元のマスをoutFromに設定する。
    QPoint outFrom = m_clickPoint;

    // 人間の指した手の移動先のマスをoutToに設定する。
    QPoint outTo = field;

    // 駒移動を行い、局面を更新した場合
    if (m_gameController->editPosition(outFrom, outTo)) {
        // 移動先のマスを黄色に着色する。
        updateHighlight(m_movedField, outTo, Qt::yellow);
    }

    // 移動元のマスの選択を解除する。
    resetSelectionAndHighlight();
}

// 局面編集モードで、クリックされたマスに基づいて駒の移動を処理する。
void MainWindow::handleEditModeClick(const QPoint& field)
{
    // 移動させる駒をクリックした場合
    if (m_clickPoint.isNull()) {
        // 指す駒を左クリックで選択した時にそのマスをオレンジ色にする。
        selectPieceAndHighlight(field);

        // ハイライトを削除する。
        // 以下の行が無いと赤と黄色が混ざってしまう。
        m_shogiView->removeHighlight(m_movedField);
    }
    // すでに移動させる駒を選択している場合
    else {
        // 局面編集モードで、クリックされたポイントに基づいて駒の移動を処理する。
        // 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
        handleClickForEditMode(field);
    }
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
                resetSelectionAndHighlight();
            }
        }
    }
    // すでに移動させる駒を選択している場合
    else {
        // クリックをリセットして選択していたハイライトを削除する。
        resetSelectionAndHighlight();
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
    QMessageBox::information(this, tr("Game Over"), tr("The game has ended. %1").arg(text));
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

// Webサイトをブラウザで表示する。
void MainWindow::openWebsiteInExternalBrowser()
{
    QDesktopServices::openUrl(QUrl("https://www.yahoo.co.jp/"));
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

// 平手、駒落ち Player1: Human, Player2: Human
void MainWindow::startHumanVsHumanGame()
{
    // マウスでクリックして駒を選択して指す処理を行う。
    connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handleHumanVsHumanClick);
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

// 平手 Player1: Human, Player2: USI Engine
// 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
void MainWindow::startHumanVsEngineGame()
{
    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを削除する。
    if (m_usi1 != nullptr) delete m_usi1;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを生成する。
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode, this);

    // 平手で「人間 対 将棋エンジン」の場合
    if ((m_playMode == EvenHumanVsEngine) || (m_playMode == HandicapHumanVsEngine)) {
        // 対局者2をエンジン1で初期化して対局を開始する。
        initializeAndStartPlayer2WithEngine1();
    }
    // 駒落ちで「将棋エンジン 対 人間」の場合
    else if (m_playMode == HandicapEngineVsHuman) {
        // 対局者1をエンジン1で初期化して対局を開始する。
        initializeAndStartPlayer1WithEngine1();
    }

    // positionコマンド文字列の生成
    m_positionStr1 = "position " + m_startPosStr + " moves";

    // 対局開始の初期状態のpositionコマンド文字列をリストに追加する。
    m_positionStrList.append(m_positionStr1);

    // マウスでクリックして駒を選択して指す処理を行う。
    connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handlePlayerVsEngineClick);
}

// 平手 Player1: USI Engine（先手）, Player2: Human（後手）
// 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
void MainWindow::startEngineVsHumanGame()
{
    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを削除する。
    if (m_usi1 != nullptr) delete m_usi1;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを生成する。
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode,this);

    // 平手で「将棋エンジン 対 人間」の場合
    if (m_playMode == EvenEngineVsHuman) {
        // 対局者1をエンジン1で初期化して対局を開始する。
        initializeAndStartPlayer1WithEngine1();
    }
    // 駒落ちで「人間 対 将棋エンジン」の場合
    else if (m_playMode == HandicapHumanVsEngine) {
        // 対局者2をエンジン1で初期化して対局を開始する。
        initializeAndStartPlayer2WithEngine1();
    }

    // positionコマンド文字列の生成
    m_positionStr1 = "position " + m_startPosStr + " moves";

    // 対局開始の初期状態のpositionコマンド文字列をリストに追加する。
    m_positionStrList.append(m_positionStr1);

    // ponder使用時に使う文字列の生成
    m_positionPonder1 = m_positionStr1;

    // 駒を成る・不成のフラグを不成に設定
    m_gameController->setPromote(false);

    // 駒を指した場合の移動元と移動先のマスの宣言
    QPoint outFrom;
    QPoint outTo;

    // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
    m_usi1->handleEngineVsHumanOrEngineMatchCommunication(m_positionStr1, m_positionPonder1, outFrom, outTo, m_countDownMilliSec1,
                                                          m_bTime, m_wTime, m_addEachMoveMiliSec1, m_addEachMoveMiliSec2, m_useByoyomi);

    // 将棋エンジンが投了した場合
    if (m_usi1->isResignMove()) {
        // 将棋エンジンが"bestmove resign"コマンドで投了した場合の処理を行う。
        handleEngineTwoResignation();

        return;
    }

    // 移動元のマスをオレンジ色に着色する。
    addNewHighlight(m_selectedField2, outFrom, QColor(255, 0, 0, 50));

    // 指そうとした手が合法手だった場合、盤面を更新する。
    bool isMoveValid;

    try {
        isMoveValid = m_gameController->validateAndMove(outFrom, outTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves);
    } catch (const std::exception& e) {
        // エラーメッセージを表示する。
        displayErrorMessage(e.what());

        // エラーが発生した場合、処理を終了する。
        return;
    }

    if (isMoveValid) {
        // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
        updateTurnAndTimekeepingDisplay();

        // 移動先のマスを黄色に着色する。
        updateHighlight(m_movedField, outTo, Qt::yellow);

        // 評価値グラフを更新する。
        redrawEngine1EvaluationGraph();
    }

    // マウスでクリックして駒を選択して指す処理を行う。
    connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handlePlayerVsEngineClick);
}

// 平手、駒落ち Player1: USI Engine, Player2: USI Engine
void MainWindow::startEngineVsEngineGame()
{
    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを削除する。
    if (m_usi1 != nullptr) delete m_usi1;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを生成する。
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode, this);

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを削除する。
    if (m_usi2 != nullptr) delete m_usi2;

    // 将棋エンジンとUSIプロトコルに関するクラスのインスタンスを生成する。
    m_usi2 = new Usi(m_lineEditModel2, m_modelThinking2, m_gameController, m_playMode, this);

    if (m_playMode == EvenEngineVsEngine) {
        // 対局者1をエンジン1で初期化して対局を開始する。
        initializeAndStartPlayer1WithEngine1();

        // 対局者2をエンジン2で初期化して対局を開始する。
        initializeAndStartPlayer2WithEngine2();
    }
    else if (m_playMode == HandicapEngineVsEngine) {
        // 対局者1をエンジン2で初期化して対局を開始する。
        initializeAndStartPlayer2WithEngine1();

        // 対局者2をエンジン2で初期化して対局を開始する。
        initializeAndStartPlayer1WithEngine2();
    }

    // positionコマンド文字列を生成する。
    m_positionStr1 = "position " + m_startPosStr + " moves";

    // 対局開始の初期状態のpositionコマンド文字列をリストに追加する。
    m_positionStrList.append(m_positionStr1);

    // ponder使用時に使う文字列を生成する。
    m_positionPonder1 = m_positionStr1;

    // 駒を指した場合の移動元と移動先のマスの宣言
    QPoint outFrom;
    QPoint outTo;

    while (1) {
        // 駒を成る・不成のフラグを不成に設定する。
        m_gameController->setPromote(false);

        // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
        m_usi1->handleEngineVsHumanOrEngineMatchCommunication(m_positionStr1, m_positionPonder1, outFrom, outTo, m_countDownMilliSec1,
                                                              m_bTime, m_wTime, m_addEachMoveMiliSec1, m_addEachMoveMiliSec2, m_useByoyomi);

        // 将棋エンジンが投了した場合
        if (m_usi1->isResignMove()) {
            // 将棋エンジンが"bestmove resign"コマンドで投了した場合の処理を行う。
            handleEngineTwoResignation();

            return;
        }

        // 移動元のマスをオレンジ色に着色する。
        updateHighlight(m_selectedField, outFrom, QColor(255, 0, 0, 50));

        // 指そうとした手が合法手だった場合、盤面を更新する。
        bool isMoveValid1;

        try {
            isMoveValid1 = m_gameController->validateAndMove(outFrom, outTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves);
        } catch (const std::exception& e) {
            // エラーメッセージを表示する。
            displayErrorMessage(e.what());

            // エラーが発生した場合、処理を終了する。
            return;
        }

        if (isMoveValid1) {
            // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
            updateTurnAndTimekeepingDisplay();

            // 現在の駒の移動先の筋と段番号を直前の移動先として保存しておく。
            m_usi2->setPreviousFileTo(outTo.x());
            m_usi2->setPreviousRankTo(outTo.y());

            // 移動先のマスを黄色に着色する。
            updateHighlight(m_movedField, outTo, Qt::yellow);

            // 評価値グラフを更新する。
            redrawEngine1EvaluationGraph();
        }

        // 駒を成る・不成のフラグを不成に設定する。
        m_gameController->setPromote(false);

        // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
        m_usi2->handleEngineVsHumanOrEngineMatchCommunication(m_positionStr1, m_positionPonder1, outFrom, outTo, m_countDownMilliSec1,
                                                              m_bTime, m_wTime, m_addEachMoveMiliSec1, m_addEachMoveMiliSec2, m_useByoyomi);

        // 将棋エンジンが投了した場合
        if (m_usi2->isResignMove()) {
            // 将棋エンジンが"bestmove resign"コマンドで投了した場合の処理を行う。
            handleEngineOneResignation();

            return;
        }

        // 移動元のマスをオレンジ色に着色する。
        updateHighlight(m_selectedField, outFrom, QColor(255, 0, 0, 50));

        // 指そうとした手が合法手だった場合、盤面を更新する。
        bool isMoveValid2;

        try {
            isMoveValid2 = m_gameController->validateAndMove(outFrom, outTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves);
        } catch (const std::exception& e) {
            // エラーメッセージを表示する。
            displayErrorMessage(e.what());

            // エラーが発生した場合、処理を終了する。
            return;
        }

        if (isMoveValid2) {
            // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
            updateTurnAndTimekeepingDisplay();

            // 現在の駒の移動先の筋と段番号を直前の移動先として保存しておく。
            m_usi1->setPreviousFileTo(outTo.x());
            m_usi1->setPreviousRankTo(outTo.y());

            // 移動先のマスを黄色に着色する。
            updateHighlight(m_movedField, outTo, Qt::yellow);

            // 評価値グラフを更新する。
            redrawEngine2EvaluationGraph();
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
    // 既存のインスタンスが存在する場合
    if (m_analysisModel) {
        // 既存のインスタンスを削除する。
        delete m_analysisModel;

        // 安全のためにnullptrを代入する。
        m_analysisModel = nullptr;
    }

    // 棋譜解析結果をGUI上で表示するためのクラスのインスタンスを生成する。
    m_analysisModel = new KifuAnalysisListModel;

    // 既存のインスタンスが存在する場合
    if (m_analysisResultsView) {
        // 既存のインスタンスを削除する。
        delete m_analysisResultsView;

        // 安全のためにnullptrを代入する。
        m_analysisResultsView = nullptr;
    }

    // 新しいテーブルビューを生成する。
    m_analysisResultsView = new QTableView;

    // 選択モードをシングルセレクションに設定する。
    m_analysisResultsView->setSelectionMode(QAbstractItemView::SingleSelection);

    // 選択動作を行ごとに設定する。
    m_analysisResultsView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 垂直ヘッダーを非表示に設定する。
    m_analysisResultsView->verticalHeader()->setVisible(false);

    // ビューにモデルを設定する。
    m_analysisResultsView->setModel(m_analysisModel);

    // 水平ヘッダーのリサイズモードを設定する。（3番目のセクションをストレッチ）
    m_analysisResultsView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    // 列の幅を200ピクセルに設定（1列目）
    m_analysisResultsView->setColumnWidth(0, 200);

    // 列の幅を200ピクセルに設定（2列目）
    m_analysisResultsView->setColumnWidth(1, 200);

    // 列の幅を200ピクセルに設定（3列目）
    m_analysisResultsView->setColumnWidth(2, 200);

    // 列の幅を200ピクセルに設定（4列目）
    m_analysisResultsView->setColumnWidth(3, 200);

    // ビューの最小幅を1000ピクセルに設定する。
    m_analysisResultsView->setMinimumWidth(1000);

    // ビューの最小高さを600ピクセルに設定する。
    m_analysisResultsView->setMinimumHeight(600);

    // ウィンドウタイトルを「棋譜解析結果」と設定する。
    m_analysisResultsView->setWindowTitle("棋譜解析結果");

    // 棋譜解析結果のテーブルビューを表示する。
    m_analysisResultsView->show();
}

// 検討ダイアログを表示する。
void MainWindow::displayConsiderationDialog()
{
    // 検討ダイアログを生成する。
    m_considarationDialog = new ConsidarationDialog(this);

    // 検討ダイアログを表示後にユーザーがOKボタンを押した場合
    if (m_considarationDialog->exec() == QDialog::Accepted) {
        // 検討を開始する。
        startConsidaration();
    }

    // 検討ダイアログを削除する。
    delete m_considarationDialog;
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
    // 棋譜解析ダイアログを生成する。
    m_analyzeGameRecordDialog = new KifuAnalysisDialog;

    // 棋譜解析ダイアログを表示後にユーザーがOKボタンを押した場合
    if (m_analyzeGameRecordDialog->exec() == QDialog::Accepted) {
        // 棋譜解析結果を表示するためのテーブルビューを設定および表示する。
        displayAnalysisResults();

        // GUIの手番を示す赤丸を対局者1側に表示する。
        m_turnLabel1->setVisible(true);

        try {
            // 棋譜解析を開始する。
            analyzeGameRecord();
        } catch (const std::exception& e) {
            // 既存のインスタンスが存在する場合
            if (m_analysisResultsView) {
                // 既存のインスタンスを削除する。
                delete m_analysisResultsView;

                // 安全のためにnullptrを代入する。
                m_analysisResultsView = nullptr;
            }

            // 既存のインスタンスが存在する場合
            if (m_analysisModel) {
                // 既存のインスタンスを削除する。
                delete m_analysisModel;

                // 安全のためにnullptrを代入する。
                m_analysisModel = nullptr;
            }

            // エラーメッセージを表示する。
            displayErrorMessage(e.what());
        }
    }

    // 棋譜解析ダイアログを削除する。
    delete m_analyzeGameRecordDialog;
}

// 対局者1と対局者2の持ち時間を設定する。
void MainWindow::setPlayerTimeLimits()
{
    // 時間切れが有効の場合
    if (m_isLoseOnTimeout) {
        // 対局者1の持ち時間（単位はミリ秒）を計算する。
        m_bTime = QString::number((m_startGameDialog->basicTimeHour1() * 60
                                   + m_startGameDialog->basicTimeMinutes1())
                                  * 60000);

        // 対局者2の持ち時間（単位はミリ秒）を計算する。
        m_wTime = QString::number((m_startGameDialog->basicTimeHour2() * 60
                                   + m_startGameDialog->basicTimeMinutes2())
                                  * 60000);
    }
    // 時間切れが無効の場合
    else {
        // 対局者1の持ち時間に0を設定する。
        m_bTime = "0";

        // 対局者2の持ち時間に0を設定する。
        m_wTime = "0";
    }
}

// 対局者の残り時間と秒読み時間を設定する。
void MainWindow::setRemainingTimeAndCountDown()
{
    // 対局ダイアログから時間切れを有効にするかどうか
    // 「時間切れを負けにする」を設定するかのフラグ
    m_isLoseOnTimeout = m_startGameDialog->isLoseOnTimeout();

    int byoyomiSec1 = m_startGameDialog->byoyomiSec1();
    int byoyomiSec2 = m_startGameDialog->byoyomiSec2();

    int addEachMoveSec1 = m_startGameDialog->addEachMoveSec1();
    int addEachMoveSec2 = m_startGameDialog->addEachMoveSec2();

    // いずれかの対局者の秒読み時間が0より大きい場合
    if ((byoyomiSec1 > 0) || (byoyomiSec2 > 0)) {
        // 秒読みを使用する。
        m_useByoyomi = true;

        // 対局者1の秒読み時間を設定（単位はミリ秒）
        m_countDownMilliSec1 = m_startGameDialog->byoyomiSec1() * 1000;

        // 対局者2の秒読み時間を設定（単位はミリ秒）
        m_countDownMilliSec2 = m_startGameDialog->byoyomiSec2() * 1000;

        // 対局者1の1手ごとの時間加算（単位はミリ秒）
        m_addEachMoveMiliSec1 = 0;

        // 対局者2の1手ごとの時間加算（単位はミリ秒）
        m_addEachMoveMiliSec2 = 0;
    }
    // いずれの対局者の秒読み時間も0の場合
    else {
        // いずれかの対局者の1手ごとの時間加算が0より大きい場合
        if ((addEachMoveSec1 > 0) || (addEachMoveSec2 > 0)) {
            // 秒読みを使用しない。
            m_useByoyomi = false;

            // 対局者1の秒読み時間を設定（単位はミリ秒）
            m_countDownMilliSec1 = 0;

            // 対局者2の秒読み時間を設定（単位はミリ秒）
            m_countDownMilliSec2 = 0;

            // 対局者1の1手ごとの時間加算（単位はミリ秒）
            m_addEachMoveMiliSec1 = m_startGameDialog->addEachMoveSec1() * 1000;

            // 対局者2の1手ごとの時間加算（単位はミリ秒）
            m_addEachMoveMiliSec2 = m_startGameDialog->addEachMoveSec2() * 1000;
        }
        // いずれの対局者の秒読み時間も0であり、1手ごとの時間加算も0の場合
        else {
            // 秒読みを使用する。
            m_useByoyomi = true;

            // 対局者1の秒読み時間を設定（単位はミリ秒）
            m_countDownMilliSec1 = 0;

            // 対局者2の秒読み時間を設定（単位はミリ秒）
            m_countDownMilliSec2 = 0;

            // 対局者1の1手ごとの時間加算（単位はミリ秒）
            m_addEachMoveMiliSec1 = 0;

            // 対局者2の1手ごとの時間加算（単位はミリ秒）
            m_addEachMoveMiliSec2 = 0;
        }
    }

    // 対局者1と対局者2の持ち時間を設定する。
    setPlayerTimeLimits();

    //begin
    qDebug() << "---- setRemainingTimeAndCountDown() ---";
    qDebug() << "m_isLoseOnTimeout = " << m_isLoseOnTimeout;
    qDebug() << "m_bTime = " << m_bTime;
    qDebug() << "m_wTime = " << m_wTime;
    qDebug() << "m_useByoyomi = " << m_useByoyomi;
    qDebug() << "m_countDownMilliSec1 = " << m_countDownMilliSec1;
    qDebug() << "m_countDownMilliSec2 = " << m_countDownMilliSec2;
    qDebug() << "m_addEachMoveMiliSec1 = " << m_addEachMoveMiliSec1;
    qDebug() << "m_addEachMoveMiliSec2 = " << m_addEachMoveMiliSec2;
    //end
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
    m_gameRecordModel->clearAllItems();

    // 前対局で選択した途中の局面まで棋譜欄に棋譜データを1行ずつ格納する。
    for (int i = 0; i < m_currentMoveIndex; i++) {
        m_gameRecordModel->appendItem(m_moveRecords->at(i));
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
    m_gameRecordModel->appendItem(new KifuDisplay("=== 開始局面 ===", "（１手 / 合計）"));

    // "sfen "を削除したSFEN文字列を格納する。
    m_sfenRecord->append(m_startSfenStr);
}

// 対局者1の残り時間の文字色を赤色に指定する。
void MainWindow::setPlayer1TimeTextToRed()
{
    // パレットを赤色に指定する。
    QPalette palette = m_remainTimeText1->palette();
    palette.setColor(QPalette::WindowText, Qt::red);

    // 対局者1の残り時間の文字色を赤色に指定する。
    m_remainTimeText1->setPalette(palette);

    // 対局者1の残り時間を0にする。
    m_bTime = "0";
}

// 対局者2の残り時間の文字色を赤色に指定する。
void MainWindow::setPlayer2TimeTextToRed()
{
    // パレットを赤色に指定する。
    QPalette palette = m_remainTimeText2->palette();
    palette.setColor(QPalette::WindowText, Qt::red);

    // 対局者2の残り時間の文字色を赤色に指定する。
    m_remainTimeText2->setPalette(palette);

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

    // 対局者の持ち時間が0になった場合、投了の処理を行う。
    connect(m_shogiClock, &ShogiClock::resignationTriggered, this, &MainWindow::handleResignation);

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
    int remainingTime1 = basicTimeHour1 * 3600 + basicTimeMinutes1 * 60 + binc;

    // 対局者2の残り時間を秒に変換する。
    int remainingTime2 = basicTimeHour2 * 3600 + basicTimeMinutes2 * 60 + winc;

    // 各対局者の残り時間を設定する。
    m_shogiClock->setPlayerTimes(remainingTime1, remainingTime2, byoyomi1, byoyomi2, binc, winc);
    //m_shogiClock->setPlayerTimes(10, 10, byoyomi1, byoyomi2, binc, winc);

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
    //begin
    qDebug() << "---- setCurrentTurn() ---";
    qDebug() << "m_shogiView->board()->currentPlayer() = " << m_shogiView->board()->currentPlayer();
    //end

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
        if ((m_playMode == EvenEngineVsEngine) || (m_playMode == HandicapEngineVsEngine)) {
            m_infoWidget2->show();
            m_tab2->show();
        } else {
            m_infoWidget2->hide();
            m_tab2->hide();
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
            // 棋譜欄の下の矢印ボタンを無効にする。
            disableArrowButtons();

            // 棋譜欄の行をクリックしても選択できないようにする。
            m_gameRecordView->setSelectionMode(QAbstractItemView::NoSelection);
        }

        // 現在の手番を設定
        setCurrentTurn();

        // 残り時間をセットしてタイマーを開始する。
        setTimerAndStart();

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
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // 将棋盤が反転モードになっている場合
        if (m_shogiView->getFlipMode()) {
            // 対局者の残り時間を入れ替えて表示する。
            m_remainTimeText1->setText(m_shogiClock->getPlayer2Time());
            m_remainTimeText2->setText(m_shogiClock->getPlayer1Time());

            // 手番を示す赤丸を反対側に表示する。
            if ((m_turnLabel1->isVisible()) && (!m_turnLabel2->isVisible())) {
                m_turnLabel1->setVisible(false);
                m_turnLabel2->setVisible(true);
            }
        }
        // 将棋盤が通常モードになっている場合
        else {
            // 対局者の残り時間をそのまま表示する。
            m_remainTimeText1->setText(m_shogiClock->getPlayer1Time());
            m_remainTimeText2->setText(m_shogiClock->getPlayer2Time());

            // 手番を示す赤丸をそのまま表示する。
            if ((!m_turnLabel1->isVisible()) && (m_turnLabel2->isVisible())) {
                m_turnLabel1->setVisible(true);
                m_turnLabel2->setVisible(false);
            }
        }
    }
    else {
        // 将棋盤が反転モードになっている場合
        if (m_shogiView->getFlipMode()) {
            // 対局者の残り時間を入れ替えて表示する。
            m_remainTimeText1->setText(m_shogiClock->getPlayer2Time());
            m_remainTimeText2->setText(m_shogiClock->getPlayer1Time());

            // 手番を示す赤丸を反対側に表示する。
            if ((!m_turnLabel1->isVisible()) && (m_turnLabel2->isVisible())) {
                m_turnLabel1->setVisible(true);
                m_turnLabel2->setVisible(false);
            }
        }
        // 将棋盤が通常モードになっている場合
        else {
            // 対局者の残り時間をそのまま表示する。
            m_remainTimeText1->setText(m_shogiClock->getPlayer1Time());
            m_remainTimeText2->setText(m_shogiClock->getPlayer2Time());

            // 手番を示す赤丸をそのまま表示する。
            if ((m_turnLabel1->isVisible()) && (!m_turnLabel2->isVisible())) {
                m_turnLabel1->setVisible(false);
                m_turnLabel2->setVisible(true);
            }
        }
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
    // 将棋盤の駒の選択、ハイライト、シグナル・スロットを解除する。
    m_shogiView->disconnect();

    // 選択されたマスの座標を初期化する。
    m_clickPoint = QPoint();

    // 全てのマスのハイライトを削除する。
    m_shogiView->removeHighlightAllData();

    // 両対局者の名前欄を初期化する。
    m_playersName1->setText("▲");
    m_playersName2->setText("△");

    // 残り時間、手数、手番を初期化する。
    m_remainTimeText1->setText("00:00:00");
    m_remainTimeText2->setText("00:00:00");
    m_totalMovesDisplay->setText("");
    m_turnLabel1->setVisible(false);
    m_turnLabel2->setVisible(false);

    // 棋譜欄を初期化する。
    m_gameRecordModel->clearAllItems();

    // 評価値グラフを初期化する。
    m_series1->clear();
    m_series2->clear();

    // 将棋盤を平手の初期配置に戻す。
    startNewShogiGame(m_startSfenStr);

    // エンジン1の読み筋を初期化する。
    m_modelThinking1->clearAllItems();

    // エンジン2の読み筋を初期化する。
    m_modelThinking2->clearAllItems();

    // USIプロトコル通信ログを初期化する。
    m_usiCommLogEdit->clear();
    m_tab1->setCurrentWidget(m_usiView1);

    // エンジン1の予想手、探索手などの情報を初期化する。
    m_engineNameText1->setText("");
    m_predictiveHandText1->setText("");
    m_searchedHandText1->setText("");
    m_depthText1->setText("");
    m_nodesText1->setText("");
    m_npsText1->setText("");
    m_hashfullText1->setText("");

    // 対局モードが平手のエンジン対エンジンの場合、あるいは駒落ちのエンジン対エンジンの場合
    if ((m_playMode == EvenEngineVsEngine) || (m_playMode == HandicapEngineVsEngine)) {
        // エンジン2の予想手、探索手などの情報を初期化する。
        m_engineNameText2->setText("");
        m_predictiveHandText2->setText("");
        m_searchedHandText2->setText("");
        m_depthText2->setText("");
        m_nodesText2->setText("");
        m_npsText2->setText("");
        m_hashfullText2->setText("");
    }
}

// 棋譜欄に「指し手」と「消費時間」のデータを表示させる。
void MainWindow::displayGameRecord(QStringList& kifuMoves, QStringList& kifuMovesConsumptionTime, const QString& handicapType)
{
    // 棋譜欄のデータのクリア
    m_moveRecords->clear();

    // 棋譜欄の項目にヘッダを付け加える。
    m_gameRecordModel->appendItem(new KifuDisplay("=== 開始局面 ===", "（１手 / 合計）"));

    for (int i = 0; i < kifuMoves.size(); i++) {
        if (handicapType == "平手") {
            // 後手
            if (i % 2) {
                m_lastMove = "△" + kifuMoves.at(i);
            }
            // 先手
            else {
                m_lastMove = "▲" + kifuMoves.at(i);
            }
        } else {
            // 下手
            if (i % 2) {
                m_lastMove = "▲" + kifuMoves.at(i);
            }
            // 上手
            else {
                m_lastMove = "△" + kifuMoves.at(i);
            }
        }

        // 手数を設定する。0から始まっているのでupdateGameRecord関数でインクリメントして現在の手数に直している。
        m_currentMoveIndex = i;

        // 棋譜を更新し、GUIの表示も同時に更新する。
        updateGameRecord(kifuMovesConsumptionTime.at(i));
    }
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

// KIF形式の棋譜ファイルを読み込む。
// 参考．http://kakinoki.o.oo7.jp/kif_format.html
void MainWindow::loadKifuFromFile(const QString &filePath)
{
    // 棋譜ファイルのテキストデータを1行ずつ読み込む。
    QStringList kifuTextLines = loadTextLinesFromFile(filePath);

    // SFENクラスのオブジェクト作成
    KifToSfenConverter converter;

    // KIF形式の棋譜データから指し手部分を取り出してkifuMoveに格納する。
    QStringList kifuMoves;

    // 指し手部分の消費時間
    QStringList kifuMovesConsumptionTime;

    // 対局者名
    QString sentePlayerName = "";
    QString gotePlayerName = "";
    QString uwatePlayerName = "";
    QString shitatePlayerName = "";
    QString handicapType = "";

    // KIF形式の棋譜データをSFEN形式に変換する。
    converter.convertKifToSfen(kifuTextLines, kifuMoves, kifuMovesConsumptionTime, m_sfenRecord, m_gameMoves, sentePlayerName, gotePlayerName,
                               uwatePlayerName, shitatePlayerName, handicapType);

    // GUIの対局者名欄に表示するデータを設定する。
    // 手合割が平手の場合
    if (handicapType == "平手") {
        m_playersName1->setText("▲" + sentePlayerName);
        m_playersName2->setText("△" + gotePlayerName);
    }
    // 手合割が駒落ちの場合
    else {
        if (uwatePlayerName.isEmpty() && shitatePlayerName.isEmpty()) {
            m_playersName1->setText("▲" + sentePlayerName);
            m_playersName2->setText("△" + gotePlayerName);
        } else {
            m_playersName1->setText("△" + uwatePlayerName);
            m_playersName2->setText("▲" + shitatePlayerName);
        }
    }

    // 棋譜欄に「指し手」と「消費時間」のデータを表示させる。
    displayGameRecord(kifuMoves, kifuMovesConsumptionTime, handicapType);

    // 棋譜欄の指し手を始めに戻して表示させる。
    navigateToFirstMove();

    // 棋譜欄の下の矢印ボタンを有効にする。
    enableArrowButtons();

    // 棋譜欄をシングルクリックで選択できるようにする。
    m_gameRecordView->setSelectionMode(QAbstractItemView::SingleSelection);

    // positionコマンド文字列を生成しリストに格納する。
    createPositionCommands();
}

// 棋譜を更新し、GUIの表示も同時に更新する。
// elapsedTimeは指し手にかかった時間を表す文字列
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    // 手数をインクリメントし、文字列に変換する。
    QString moveNumberStr = QString::number(++m_currentMoveIndex);

    // 手数表示のための空白を算出し、生成する。
    QString spaceStr = QString(qMax(0, 4 - moveNumberStr.length()), ' ');

    // 表示用レコードを生成する。（手数と最後の手）
    QString recordLine = spaceStr + moveNumberStr + " " + m_lastMove;

    // KIF形式レコードを生成する。（消費時間含む）
    QString kifuLine = recordLine + " ( " + elapsedTime + ")";
    kifuLine.remove("▲");
    kifuLine.remove("△");

    // KIF形式のデータを保存する。
    m_kifuDataList.append(kifuLine);

    // 指し手のレコードを生成し、保存する。
    m_moveRecords->append(new KifuDisplay(recordLine, elapsedTime));

    // 最後の指し手のレコードを表示用モデルに追加する。
    m_gameRecordModel->appendItem(m_moveRecords->last());

    // 手数表示を更新する。
    m_totalMovesDisplay->setText(moveNumberStr);
    m_totalMovesDisplay->update();

    // 棋譜表示を最下部へスクロールする。
    m_gameRecordView->scrollToBottom();
    m_gameRecordView->update();
}

// bestmove resignコマンドを受信した場合の終了処理を行う。
void MainWindow::processResignCommand()
{
    // エンジンに対してgameover winコマンドを送る。
    // エンジン同士の対局の場合
    if ((m_playMode == EvenEngineVsEngine) || (m_playMode == HandicapEngineVsEngine)) {
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            m_usi1->sendGameOverWinAndQuitCommands();
            m_lastMove = "△投了";
        } else {
            m_usi2->sendGameOverWinAndQuitCommands();
            m_lastMove = "▲投了";
        }
    }

    // 棋譜欄の下の矢印ボタンを有効にする。
    enableArrowButtons();

    // 棋譜欄をシングルクリックで選択できるようにする。
    m_gameRecordView->setSelectionMode(QAbstractItemView::SingleSelection);
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
    int engineNumber1 = m_considarationDialog->getEngineNumber();

    // 将棋エンジン実行ファイル名を設定する。
    m_engineFile1 = m_considarationDialog->getEngineList().at(engineNumber1).path;

    // 将棋エンジン名を取得する。
    QString engineName1 = m_considarationDialog->getEngineName();

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
    if (m_considarationDialog->unlimitedTimeFlag()) {
        m_countDownMilliSec1 = 0;
    }
    // 時間制限の場合
    else {
        // 秒読み時間を設定（単位はミリ秒）
        m_countDownMilliSec1 = m_considarationDialog->getByoyomiSec() * 1000;
    }

    // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
    m_usi1->executeAnalysisCommunication(m_positionStr1, m_countDownMilliSec1);
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

    try {
        // 将棋エンジンを起動し、対局開始までのコマンドを実行する。
        m_usi1->initializeAndStartEngineCommunication(m_engineFile1, engineName1);
    } catch (const std::exception& e) {
        // エラーを再スローし、エラーメッセージを表示する。
        throw;
    }

    /*
    // 手番を設定する。
    if (m_gameMoves.at(m_currentMoveIndex).movingPiece.isUpper()) {
        m_gameController->setCurrentPlayer(ShogiGameController::Player1);
    } else {
        m_gameController->setCurrentPlayer(ShogiGameController::Player2);
    }

    // positionコマンド文字列の生成
    m_positionStr1 = m_positionStrList.at(m_currentMoveIndex);

    // 時間無制限の場合
    if (m_considarationDialog->unlimitedTimeFlag()) {
        m_countDownMilliSec1 = 0;
    }
    // 時間制限の場合
    else {
        // 秒読み時間を設定（単位はミリ秒）
        m_countDownMilliSec1 = m_considarationDialog->getByoyomiSec() * 1000;
    }

    // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
    m_usi1->executeAnalysisCommunication(m_positionStr1, m_countDownMilliSec1);
    */
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
    m_countDownMilliSec1 = m_analyzeGameRecordDialog->byoyomiSec() * 1000;

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

        m_usi1->executeAnalysisCommunication(m_positionStr1, m_countDownMilliSec1);

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

        // 現局面から1手進んだ局面を表示する。
        navigateToNextMove();

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
void MainWindow::beginPositionEditing()
{
    // ShogiBoardQを初期画面表示に戻す。
    resetToInitialState();

    // 0手目に戻す。
    m_currentMoveIndex = 0;

    // 手数を0に戻す。
    m_totalMove = 0;

    // マウスの左クリックで指す駒を選択したマスのハイライトのポインタを初期化する。
    m_selectedField = nullptr;
    m_selectedField2 = nullptr;

    // 指した先のマスのハイライトのポインタを初期化する。
    m_movedField = nullptr;

    // 現局面のSFEN文字列を取り出す。
    m_startSfenStr = parseStartPositionToSfen(m_currentSfenStr);

    // 局面編集モードのフラグ
    bool positionEditMode = true;

    // 局面編集モードのフラグを更新する。
    m_shogiView->setPositionEditMode(positionEditMode);

    // 将棋盤と駒台を再描画する。
    m_shogiView->update();

    // 局面編集中のメニュー表示に変更する。
    displayPositionEditMenu();

    // 対局中のメニュー表示に変更する。
    setGameInProgressActions();

    // SFEN文字列の棋譜データを初期化（データ削除）する。
    m_sfenRecord->clear();

    // 棋譜欄データの初期化（データ削除）する。
    m_moveRecords->clear();

    // 棋譜欄に「=== 開始局面 ===」「（１手 / 合計）」のデータを追加する。
    m_gameRecordModel->appendItem(new KifuDisplay("=== 開始局面 ===", "（１手 / 合計）"));

    // 初期SFEN文字列をm_sfenRecordに格納しておく。
    m_sfenRecord->append(m_startSfenStr);

    // 棋譜欄の下の矢印ボタンを無効にする。
    disableArrowButtons();

    // 棋譜欄の行をクリックしても選択できないようにする。
    m_gameRecordView->setSelectionMode(QAbstractItemView::NoSelection);

    // 見直し必要
    // 手番表示の赤丸を先手側に表示させる。（仮の手番設定）
    // この手番表示は、「手番変更」で変更可能。
    m_turnLabel1->setVisible(true);
    m_turnLabel2->setVisible(false);

    // 手番が先手あるいは下手の場合
    if (m_shogiView->board()->currentPlayer() == "b") {
      // 手番を先手あるいは下手に設定する。
      m_gameController->setCurrentPlayer(ShogiGameController::Player1);

      // 手番表示の赤丸を先手あるいは下手側に表示させる。
      m_turnLabel1->setVisible(true);
      m_turnLabel2->setVisible(false);
    }
    // 手番が後手あるいは上手の場合
    else if (m_shogiView->board()->currentPlayer() == "w") {
      // 手番を後手あるいは上手に設定する。
      m_gameController->setCurrentPlayer(ShogiGameController::Player2);

      // 手番表示の赤丸を後手あるいは上手側に表示させる。
      m_turnLabel1->setVisible(false);
      m_turnLabel2->setVisible(true);
    }

    // 局面編集モードで、クリックされたマスに基づいて駒の移動を処理する。
    connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handleEditModeClick);

    // 局面編集モードで右クリックした駒を成る・不成の表示に変換する。
    connect(m_shogiView, &ShogiView::rightClicked, this, &MainWindow::togglePiecePromotionOnClick);

    // 全ての駒を駒台に置く。
    connect(ui->returnAllPiecesOnStand, &QAction::triggered, this, &MainWindow::resetPiecesToStand);

    // 手番を変更する。
    connect(ui->turnaround, &QAction::triggered, this, &MainWindow::switchTurns);

    // 平手初期局面に盤面を初期化する。
    connect(ui->flatHandInitialPosition, &QAction::triggered, this, &MainWindow::setStandardStartPosition);

    // 詰将棋の初期局面に盤面を初期化する。
    connect(ui->shogiProblemInitialPosition, &QAction::triggered, this, &MainWindow::setTsumeShogiStartPosition);

    // 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
    connect(ui->reversal, &QAction::triggered, this, &MainWindow::swapBoardSides);
}

// 局面編集を終了した場合の処理を行う。
void MainWindow::finishPositionEditing()
{
    // SFEN文字列の棋譜データを初期化（データ削除）する。
    m_sfenRecord->clear();

    // 局面編集後の局面をSFEN形式に変換し、リストに追加する。
    m_gameController->updateSfenRecordAfterEdit(m_sfenRecord);

    // 局面編集モードで、クリックされたマスに基づいて駒の移動を処理するシグナルとスロットの接続を解除する。
    disconnect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handleEditModeClick);

    // 局面編集モードで右クリックした駒を成る・不成の表示に変換するシグナルとスロットの接続を解除する。
    disconnect(m_shogiView, &ShogiView::rightClicked, this, &MainWindow::togglePiecePromotionOnClick);

    // 全ての駒を駒台に置くシグナルとスロットの接続を解除する。
    disconnect(ui->returnAllPiecesOnStand, &QAction::triggered, this, &MainWindow::resetPiecesToStand);

    // 手番を変更するシグナルとスロットの接続を解除する。
    disconnect(ui->turnaround, &QAction::triggered, this, &MainWindow::switchTurns);

    // 平手初期局面に盤面を初期化するシグナルとスロットの接続を解除する。
    disconnect(ui->flatHandInitialPosition, &QAction::triggered, this, &MainWindow::setStandardStartPosition);

    // 詰将棋の初期局面に盤面を初期化するシグナルとスロットの接続を解除する。
    disconnect(ui->shogiProblemInitialPosition, &QAction::triggered, this, &MainWindow::setTsumeShogiStartPosition);

    // 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更するシグナルとスロットの接続を解除する
    disconnect(ui->reversal, &QAction::triggered, this, &MainWindow::swapBoardSides);

    // 局面編集メニュー後のメニュー表示に変更する。
    hidePositionEditMenu();

    // ハイライトリストから全てのデータを削除し、表示を更新する。
    m_shogiView->removeHighlightAllData();

    // 局面編集モードかどうかのフラグ
    bool positionEditMode = false;

    // 局面編集モードのフラグを更新する。
    m_shogiView->setPositionEditMode(positionEditMode);

    // 将棋盤と駒台を再描画する。
    m_shogiView->update();
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
        m_turnLabel1->setVisible(false);
        m_turnLabel2->setVisible(true);
        m_gameController->setCurrentPlayer(ShogiGameController::Player2);
    }
    // 手番が後手あるいは上手の場合
    else {
        // 手番を先手あるいは下手に設定する。
        m_turnLabel1->setVisible(true);
        m_turnLabel2->setVisible(false);
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
