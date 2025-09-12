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
#include "kifreader.h"

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
    m_infoWidget1(nullptr),
    m_infoWidget2(nullptr),
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

    // 棋譜欄の表示設定
    setupKifuRecordView();

    // 棋譜の分岐欄の表示設定
    setupKifuBranchView();

    // 矢印ボタンの表示
    setupArrowButtons();

    // 評価値グラフの表示
    createEvaluationChartView();

    // 予想手、探索手などの情報表示
    initializeEngine1InfoDisplay();

    // エンジン1の現在の読み筋とUSIプロトコル通信ログの表示
    initializeEngine1ThoughtTab();

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

    // 棋譜欄の指し手をクリックするとその局面に将棋盤を更新する。
    connect(m_kifuView, &QTableView::clicked, this, &MainWindow::updateBoardFromMoveHistory);

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

    // 駒のドラッグを終了する。
    connect(m_gameController, &ShogiGameController::endDragSignal, this, &MainWindow::endDrag);
}

// 将棋盤上での左クリックイベントをハンドリングする。
void MainWindow::onShogiViewClicked(const QPoint &pt)
{
    // １回目クリック（つまみ始め）で、かつ持ち駒マスなら枚数をチェックする。
    // 念のため、ShogiView::startDragにもガードを入れている。
    // 持ち駒が1枚も無いマスを左クリックすると駒画像がドラッグされないようにする。
    // このチェックが無いと、持ち駒が1枚も無いマスを左クリックするとドラッグされてしまう。
    if (!m_waitingSecondClick && (pt.x() == 10 || pt.x() == 11)) {
        QChar piece = m_shogiView->board()->getPieceCharacter(pt.x(), pt.y());

        // 持ち駒の枚数が0なら何もしない。
        if (m_shogiView->board()->m_pieceStand.value(piece) <= 0) return;
    }

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
        // 2回目のクリックを待っているかどうかを示すフラグをリセットする。
        m_waitingSecondClick = false;
    }
}

// 将棋盤上での右クリックイベントをハンドリングする。
void MainWindow::onShogiViewRightClicked(const QPoint &)
{
    // 2回目のクリックの場合
    if (m_waitingSecondClick) {
        // ドラッグ操作のリセットを行う。
        finalizeDrag();
    }
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

// 将棋盤の画像をファイルとして出力する。
// 保存（grabベース、可用フォーマットを動的に列挙）
void MainWindow::saveShogiBoardImage()
{
    // 1) 実行環境で書き出し可能な形式を収集（小文字）
    QSet<QString> fmts;
    for (const QByteArray& f : QImageWriter::supportedImageFormats()) {
        fmts.insert(QString::fromLatin1(f).toLower());
    }

    // 2) 表示順とフィルタ文言（必要なら追加/順序変更OK）
    struct F { QString key; QString filter; QString extForNewFile; };
    QVector<F> candidates = {
        {"png",  "PNG (*.png)",                    "png"},
        {"tiff", "TIFF (*.tiff *.tif)",            "tiff"},
        {"jpeg", "JPEG (*.jpg *.jpeg)",            "jpg"},  // 拡張子はjpgに寄せる
        {"webp", "WebP (*.webp)",                  "webp"},
        {"bmp",  "BMP (*.bmp)",                    "bmp"},
        {"ppm",  "PPM (*.ppm)",                    "ppm"},
        {"pgm",  "PGM (*.pgm)",                    "pgm"},
        {"pbm",  "PBM (*.pbm)",                    "pbm"},
        {"xbm",  "XBM (*.xbm)",                    "xbm"},
        {"xpm",  "XPM (*.xpm)",                    "xpm"},
        // {"ico", "ICO (*.ico)",                  "ico"},   // プラグインがあれば
        // {"tga", "TGA (*.tga)",                  "tga"},   // 環境依存
    };

    QStringList filterList;
    QMap<QString, F> filterToFormat; // クリックされたフィルタ→形式
    for (const auto& c : candidates) {
        if (hasFormat(fmts, c.key)) {
            filterList << c.filter;
            filterToFormat.insert(c.filter, c);
        }
    }
    if (filterList.isEmpty()) {
        displayErrorMessage(tr("No writable image formats are available."));
        return;
    }

    // 3) 既定名（日時入り）とデフォ拡張子（PNGがあればPNG、なければ先頭）
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString base   = QStringLiteral("ShogiBoard_%1").arg(stamp);
    const QString picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString defaultExt  = hasFormat(fmts, "png") ? "png"
                              : filterToFormat[filterList.first()].extForNewFile;
    const QString initialPath = QDir(picturesDir).filePath(base + "." + defaultExt);

    // 4) ダイアログ表示
    QString selectedFilter = filterList.first();
    const QString filters = filterList.join(";;");
    QString path = QFileDialog::getSaveFileName(
        this, tr("Output the image"),
        initialPath,
        filters,
        &selectedFilter
    );
    if (path.isEmpty()) return;

    // 5) 拡張子補完＆保存形式決定
    QString suffix = QFileInfo(path).suffix().toLower();
    QString format; // QImageWriterに渡す形式名

    if (suffix.isEmpty()) {
        const F fmt = filterToFormat.value(selectedFilter);
        path += "." + fmt.extForNewFile;
        format = fmt.key; // "jpeg" は内部キーとして使うが "jpg" もOK
    } else {
        // 入力された拡張子から推定（jpeg/jpg統合処理）
        if (suffix == "jpg") suffix = "jpeg";
        format = suffix;
        if (!hasFormat(fmts, format)) {
            displayErrorMessage(tr("This image format is not available: %1").arg(suffix));
            return;
        }
    }

    // 6) 画像取得（grab→Image）
    const QImage img = m_shogiView->grab().toImage();

    // 7) QImageWriterで明示的に書き出し（品質等の指定も可能）
    QImageWriter writer(path, format.toLatin1());
    if (format == "jpeg" || format == "webp") {
        writer.setQuality(95); // 0-100（可逆でない形式の画質）
    }
    // 例：PNG圧縮レベル（0=圧縮なし, 9=最大）※環境により無視されることも
    // if (format == "png") writer.setCompression(9);

    if (!writer.write(img)) {
        // 具体的なエラー内容を表示
        displayErrorMessage(tr("Failed to save the image: %1").arg(writer.errorString()));
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
void MainWindow::toggleEngineAnalysisVisibility()
{
    // 「表示」の「思考」にチェックが入っている場合
    if (ui->actionToggleEngineAnalysis->isChecked()) {
    }
    // 「表示」の「思考」にチェックが入っていない場合
    else {
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

    // 駒の移動先のマスを黄色でハイライトするフィールドを生成
    m_movedField = new ShogiView::FieldHighlight(m_gameMoves.at(m_currentMoveIndex - 1).toSquare.x() + 1,
                                                 m_gameMoves.at(m_currentMoveIndex - 1).toSquare.y() + 1,
                                                 Qt::yellow);

    // 駒の移動先のマスをハイライトする。
    m_shogiView->addHighlight(m_movedField);
}

void MainWindow::updateBoardFromMoveHistory()
{
    // 必須チェック
    if (!m_kifuView || !m_kifuRecordModel || m_resolvedRows.isEmpty())
        return;

    // クリックされた棋譜行（無効なら 0）
    const QModelIndex idx = m_kifuView->currentIndex();
    int selPly = idx.isValid() ? idx.row() : 0;

    // 現在の“行”（本譜/分岐）は m_activeResolvedRow を使う
    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);

    // 盤面・棋譜欄・分岐候補・矢印・分岐ツリーハイライトまで一括同期
    applyResolvedRowAndSelect(row, selPly);
}

// 棋譜欄下の矢印「1手進む」
// 現局面から1手進んだ局面を表示する。
void MainWindow::navigateToNextMove()
{
    if (m_resolvedRows.isEmpty()) return;

    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[row];

    // 0..N の範囲に丸めた現在手数から +1
    const int maxPly = r.disp.size();      // 末尾手数
    const int cur    = qBound(0, m_activePly, maxPly);
    if (cur >= maxPly) {
        // もう末尾。必要ならビープ等
        return;
    }
    const int nextPly = cur + 1;

    // これだけで：盤面・棋譜欄・分岐候補欄・矢印ボタン・ツリーハイライトまで一括同期
    applyResolvedRowAndSelect(row, nextPly);
}

// 棋譜欄下の矢印「10手進む」
// 現局面から10手進んだ局面を表示する。
void MainWindow::navigateForwardTenMoves()
{
    if (m_resolvedRows.isEmpty()) return;

    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[row];

    // 0..N の範囲に丸めた現在手数から +10（末尾を超えない）
    const int maxPly = r.disp.size();              // 最終手
    const int cur    = qBound(0, m_activePly, maxPly);
    if (cur >= maxPly) return;                     // 既に末尾

    int nextPly = cur + 10;
    if (nextPly > maxPly) nextPly = maxPly;

    // 盤面・棋譜欄・分岐候補欄・矢印ボタン・ツリーハイライトまで一括同期
    applyResolvedRowAndSelect(row, nextPly);
}

// 棋譜欄下の矢印「最後まで進む」
// 最終局面を表示する。
void MainWindow::navigateToLastMove()
{
    if (m_resolvedRows.isEmpty()) return;

    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[row];

    // 最終手（0..N の N）
    const int lastPly = r.disp.size();

    // 盤面・棋譜欄・分岐候補欄・矢印ボタン・ツリーハイライトまで一括同期
    applyResolvedRowAndSelect(row, lastPly);
}

// 棋譜欄下の矢印「1手戻る」
// 現局面から1手戻った局面を表示する。
void MainWindow::navigateToPreviousMove()
{
    if (m_resolvedRows.isEmpty()) return;

    // 現在の“行”は分岐も含めて m_activeResolvedRow を信頼
    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[row];

    // 現在手数（m_activePly を優先。未設定なら棋譜ビューの選択をフォールバック）
    int curPly = (m_activePly >= 0)
                 ? m_activePly
                 : (m_kifuRecordModel && m_kifuView ? m_kifuView->currentIndex().row() : 0);
    curPly = qBound(0, curPly, r.disp.size());

    // 1手戻す（先頭で止める）
    const int prevPly = qMax(0, curPly - 1);

    // 盤面・棋譜欄・分岐候補・矢印ボタン・ツリーハイライトを一括同期
    applyResolvedRowAndSelect(row, prevPly);
}

// 棋譜欄下の矢印「10手戻る」
// 現局面から10手戻った局面を表示する。
void MainWindow::navigateBackwardTenMoves()
{
    if (m_resolvedRows.isEmpty()) return;

    // 現在の“行”は分岐も含めて m_activeResolvedRow を使用
    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[row];

    // 現在手数（m_activePly を優先。未設定なら棋譜ビュー選択へフォールバック）
    int curPly = (m_activePly >= 0)
                 ? m_activePly
                 : (m_kifuRecordModel && m_kifuView ? m_kifuView->currentIndex().row() : 0);
    curPly = qBound(0, curPly, r.disp.size());

    // 10手戻す（0でクランプ）
    const int targetPly = qMax(0, curPly - 10);

    // 盤面・棋譜欄・分岐候補・矢印ボタン・ツリーハイライトを一括同期
    applyResolvedRowAndSelect(row, targetPly);
}

// 棋譜欄下の矢印「最初の局面に戻る」
// 現局面から最初の局面を表示する。
void MainWindow::navigateToFirstMove()
{
    if (m_resolvedRows.isEmpty()) return;

    // 現在の“行”（本譜/分岐）は m_activeResolvedRow を採用
    const int row = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);

    // 先頭手数(=0)へ。盤面・棋譜欄・分岐候補・ツリーハイライトまで一括同期
    applyResolvedRowAndSelect(row, /*selPly=*/0);
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

// 棋譜欄の表示を設定する。
void MainWindow::setupKifuRecordView()
{
    // 棋譜欄を作成する。
    m_kifuView = new QTableView;

    // シングルクリックで選択する。
    m_kifuView->setSelectionMode(QAbstractItemView::SingleSelection);

    // 行のみを選択する。
    m_kifuView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 垂直方向のヘッダは表示させない。
    m_kifuView->verticalHeader()->setVisible(false);

    // 棋譜のListModelを作成する。
    m_kifuRecordModel = new KifuRecordListModel;

    // 棋譜のListModelをセットする。
    m_kifuView->setModel(m_kifuRecordModel);

    // 「指し手」の列を伸縮可能にする。
    m_kifuView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    // 「消費時間」の列を伸縮可能にする。
    m_kifuView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    // 追加：行選択変化でコールバック
    connect(m_kifuView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::onMainMoveRowChanged);
}

void MainWindow::setupKifuBranchView()
{
    m_kifuBranchView = new QTableView;
    m_kifuBranchView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_kifuBranchView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifuBranchView->verticalHeader()->setVisible(false);

    m_kifuBranchModel = new KifuBranchListModel;
    m_kifuBranchView->setModel(m_kifuBranchModel);
    m_kifuBranchView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    // クリック/Enter 両方とも onBranchCandidateActivated へ
    connect(m_kifuBranchView, &QTableView::activated,
            this, &MainWindow::onBranchCandidateActivated, Qt::UniqueConnection);
    connect(m_kifuBranchView, &QTableView::clicked,
            this, &MainWindow::onBranchCandidateActivated, Qt::UniqueConnection);
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

void MainWindow::initializeEngine1ThoughtTab()
{
    m_usiView1 = new QTableView;
    m_modelThinking1 = new ShogiEngineThinkingModel;
    m_usiView1->setModel(m_modelThinking1);
    m_usiView1->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    m_usiView2 = new QTableView;
    m_modelThinking2 = new ShogiEngineThinkingModel;
    m_usiView2->setModel(m_modelThinking2);
    m_usiView2->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    m_usiCommLogEdit = new QPlainTextEdit;
    m_usiCommLogEdit->setReadOnly(true);

    m_tab = new QTabWidget;

    // ★ ここから：思考1タブのページ（縦並び）
    if (!m_infoWidget1) {
        // まだ生成していない場合に備えて（あなたの初期化順に合わせて調整可）
        initializeEngine1InfoDisplay();
    }

    if (!m_infoWidget2) {
        // まだ生成していない場合に備えて（あなたの初期化順に合わせて調整可）
        initializeEngine2InfoDisplay();
    }

    QWidget* page1 = new QWidget(m_tab);
    auto* v1 = new QVBoxLayout(page1);
    v1->setContentsMargins(4,4,4,4);
    v1->setSpacing(4);
    v1->addWidget(m_infoWidget1);   // 1：情報ウィジェット
    v1->addWidget(m_usiView1);      // 2：思考表
    v1->addWidget(m_infoWidget2);   // 3：エンジン2の情報ウィジェット（あってもなくても良い）
    v1->addWidget(m_usiView2);      // 4：エンジン2の思考表（あってもなくても良い）
    v1->setStretch(0, 0);
    v1->setStretch(1, 1);

    m_infoWidget2->setVisible(false); // 初期状態では非表示
    m_usiView2->setVisible(false);    // 初期状態では非表示

    m_tab->addTab(page1, tr("思考"));

    // 既存のログ／コメントタブはそのまま
    m_tab->addTab(m_usiCommLogEdit, tr("USI通信ログ"));

    // 対局情報タブを「棋譜コメント」の前に差し込む
    addGameInfoTabIfMissing();

    m_branchTextInTab1 = new QTextBrowser(m_tab);
    m_branchTextInTab1->setOpenExternalLinks(true);
    m_branchTextInTab1->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_branchTextInTab1->setPlaceholderText(tr("コメントを表示"));
    m_tab->addTab(m_branchTextInTab1, tr("棋譜コメント"));
    m_tab->setMinimumHeight(150);

    connect(m_lineEditModel1, &UsiCommLogModel::usiCommLogChanged, this, [this]() {
        m_usiCommLogEdit->appendPlainText(m_lineEditModel1->usiCommLog());
    });

    connect(m_lineEditModel2, &UsiCommLogModel::usiCommLogChanged, this, [this]() {
        // もともと m_usiCommLogEdit（1側）へ追記していた仕様を維持
        m_usiCommLogEdit->appendPlainText(m_lineEditModel2->usiCommLog());
    });

    if (!m_branchTreeView) {
        m_branchTreeView = new QGraphicsView(m_tab);
        m_branchTreeView->setRenderHint(QPainter::Antialiasing, true);
        m_branchTreeView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_branchTreeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_branchTreeView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_tab->addTab(m_branchTreeView, tr("分岐ツリー"));

        // ★ ノードクリック拾い
        m_branchTreeView->viewport()->installEventFilter(this);
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

void MainWindow::setupBranchTextWidget()
{
    auto* tb = new QTextBrowser(this);
    tb->setReadOnly(true); // 既定でreadOnlyですが明示でもOK
    tb->setOpenExternalLinks(true);  // クリックで外部ブラウザを開く
    tb->setOpenLinks(true);          // 既定true（falseでも外部は開くがtrueで無難）
    tb->setPlaceholderText(tr("コメントを表示"));
    tb->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_branchText = tb;
}

void MainWindow::setupRecordAndEvaluationLayout()
{
    // 左（棋譜 + 矢印）
    auto* vboxKifuLayout = new QVBoxLayout;
    vboxKifuLayout->setContentsMargins(0, 0, 0, 0);
    vboxKifuLayout->setSpacing(6);
    vboxKifuLayout->addWidget(m_kifuView);
    vboxKifuLayout->addWidget(m_arrows);

    auto* kifuWidget = new QWidget;
    kifuWidget->setLayout(vboxKifuLayout);
    kifuWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 右（分岐 + テキスト）※縦に並べる
    setupBranchTextWidget();

    auto* rightSplitter = new QSplitter(Qt::Vertical);
    rightSplitter->addWidget(m_kifuBranchView); // 上：分岐一覧
    rightSplitter->addWidget(m_branchText);     // 下：テキスト表示
    rightSplitter->setChildrenCollapsible(false);
    rightSplitter->setSizes({300, 200});        // 初期比率（お好みで）

    // 左右に配置
    auto* lrSplitter = new QSplitter(Qt::Horizontal);
    lrSplitter->addWidget(kifuWidget);      // 左
    lrSplitter->addWidget(rightSplitter);   // 右
    lrSplitter->setChildrenCollapsible(false);
    lrSplitter->setSizes({600, 400});       // 初期比率（お好みで）

    // 下に評価値グラフ（スクロールエリア）
    auto* vboxLayout = new QVBoxLayout;
    vboxLayout->setContentsMargins(0, 0, 0, 0);
    vboxLayout->setSpacing(8);
    vboxLayout->addWidget(lrSplitter);
    vboxLayout->addWidget(m_scrollArea);

    m_gameRecordLayoutWidget = new QWidget;
    m_gameRecordLayoutWidget->setLayout(vboxLayout);
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
    // 縦ボックスレイアウトを作成する。
    QVBoxLayout* vboxLayout = new QVBoxLayout;

    // 対局者名と残り時間、将棋盤、棋譜、矢印ボタン、評価値グラフのウィジェットと
    // info行の予想手、探索手、エンジンの読み筋のウィジェットを縦ボックスレイアウトに追加する。
    vboxLayout->addWidget(m_hsplit);
    vboxLayout->setSpacing(0);
    vboxLayout->addWidget(m_tab);

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

    // ★ 追加：開局時は両者とも未着手
    m_p1HasMoved = false;
    m_p2HasMoved = false;
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
    // --- 終局では人間用ストップウォッチを無効化 ---
    if (m_turnTimerArmed) { m_turnTimerArmed = false; m_turnTimer.invalidate(); }
    if (m_humanTimerArmed){ m_humanTimerArmed = false; m_humanTurnTimer.invalidate(); }

    // 残時間と考慮msを確定
    m_shogiClock->stopClock();

    // 盤操作を無効化
    m_shogiView->setMouseClickMode(false);

    auto showOutcomeDeferred = [this](ShogiGameController::Result res) {
        updateRemainingTimeDisplay();
        QTimer::singleShot(0, this, [this, res]() { displayGameOutcome(res); });
    };

    // ★ 投了/時間切れ：現在手番の側の考慮時間を記録する
    if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
        // 先手が投了/時間切れ
        m_shogiClock->applyByoyomiAndResetConsideration1(); // ※ gameOver時は加算せず考慮だけ確定
        updateGameRecord(m_shogiClock->getPlayer1ConsiderationAndTotalTime());
        showOutcomeDeferred(ShogiGameController::Player2Wins);
    } else {
        // 後手が投了/時間切れ
        m_shogiClock->applyByoyomiAndResetConsideration2();
        updateGameRecord(m_shogiClock->getPlayer2ConsiderationAndTotalTime());
        showOutcomeDeferred(ShogiGameController::Player1Wins);
    }

    enableArrowButtons();
    m_kifuView->setSelectionMode(QAbstractItemView::SingleSelection);


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

// ドラッグ操作のリセットを行う。
void MainWindow::finalizeDrag()
{
    // ドラッグを終了する。駒を移動してカーソルを戻す。
    endDrag();

    // 2回目のクリックを待っているかどうかを示すフラグをリセットする。
    m_waitingSecondClick = false;

    // 駒の移動元と移動先のマスのハイライトを消去する。
    resetSelectionAndHighlight();
}

// 将棋のGUIでクリックされたポイントに基づいて駒の移動を処理する。
// 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
void MainWindow::handleClickForPlayerVsEngine(const QPoint& field)
{
    // 移動先のマスが移動元のマスと同じになってしまった場合
    if (field == m_clickPoint) {
        // ドラッグ操作のリセットを行う。
        finalizeDrag();

        // 何もしない。
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
        // ★ この手の「考慮時間」を ShogiClock に反映（合法手が確定した瞬間）
        finishHumanTimerAndSetConsideration();

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
            // 人間が指してエンジンに返すとき（Human vs Engine）
            refreshGoTimes();

            // 将棋エンジンにpositionコマンドを送信し、指し手を受信する。
            m_usi1->handleHumanVsEngineCommunication(m_positionStr1, m_positionPonder1, outFrom, outTo, m_byoyomiMilliSec2,
                                                     m_bTime, m_wTime, m_positionStrList, m_addEachMoveMiliSec1, m_addEachMoveMiliSec2,
                                                     m_useByoyomi);
        } catch (const std::exception& e) {
            // エラーメッセージを表示する。
            displayErrorMessage(e.what());

            // エラーが発生した場合、処理を終了する。
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
            const qint64 thinkMs = m_usi1->lastBestmoveElapsedMs();

            // validateAndMove 後は手番が“人間側”に切り替わっているので
            // 「非手番 = 直前に指した側（エンジン）」の考慮時間にセット
            if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
                // 今の手番は先手 → さっき指したのは後手（エンジン）
                m_shogiClock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            } else {
                // 今の手番は後手 → さっき指したのは先手（エンジン）
                m_shogiClock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            }

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

            // ★ ここで“人間手番開始”のストップウォッチを起動
            armHumanTimerIfNeeded();
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
    // 同一マス → 何もしない
    if (field == m_clickPoint) {
        finalizeDrag();
        return;
    }

    // ★ この着手をするのは “今の手番”
    const auto mover = m_gameController->currentPlayer();

    QPoint outFrom = m_clickPoint;
    QPoint outTo   = field;

    bool isMoveValid = false;
    try {
        isMoveValid = m_gameController->validateAndMove(
            outFrom, outTo, m_lastMove, m_playMode, m_currentMoveIndex, m_sfenRecord, m_gameMoves
        );
    } catch (const std::exception& e) {
        displayErrorMessage(e.what());
        return;
    }

    if (isMoveValid) {
        // ★ 合法手が“確定した瞬間”にこの手の考慮時間を確定（UI処理より先）
        finishTurnTimerAndSetConsiderationFor(mover);

        // UI：ハイライトなど（これは考慮時間に含めない）
        updateHighlight(m_movedField, outTo, Qt::yellow);

        // ★ ShogiClock 側の集計・秒読み/インクリメント適用・手番表示など
        updateTurnAndTimekeepingDisplay();

        // 次の人の操作準備
        // （描画負荷を含めたくなければ singleShot(0) で次フレームに回す）
        // QTimer::singleShot(0, this, [this](){ armTurnTimerIfNeeded(); });
        armTurnTimerIfNeeded();
    }

    // クリック状態のリセット
    resetSelectionAndHighlight();
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
    //begin
    qDebug() << "in MainWindow::handleClickForEditMode";
    qDebug() << "field: " << field;
    qDebug() << "m_clickPoint: " << m_clickPoint;
    //end
    // 移動先のマスが移動元のマスと同じになってしまった場合
    if (field == m_clickPoint) {
        // ドラッグ操作のリセットを行う。
        finalizeDrag();

        // 何もしない。
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
    //begin
    qDebug() << "in MainWindow::handleEditModeClick";
    qDebug() << "m_clickPoint: " << m_clickPoint;
    //end

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

        // ドラッグ操作のリセットを行う。
        endDrag();
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
    // 棋譜解析ダイアログを生成する。
    m_analyzeGameRecordDialog = new KifuAnalysisDialog;

    // 棋譜解析ダイアログを表示後にユーザーがOKボタンを押した場合
    if (m_analyzeGameRecordDialog->exec() == QDialog::Accepted) {
        // 棋譜解析結果を表示するためのテーブルビューを設定および表示する。
        displayAnalysisResults();

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
    // 対局者1の持ち時間（単位はミリ秒）を計算する。
    m_bTime = QString::number((m_startGameDialog->basicTimeHour1() * 60
                               + m_startGameDialog->basicTimeMinutes1())
                              * 60000);

    // 対局者2の持ち時間（単位はミリ秒）を計算する。
    m_wTime = QString::number((m_startGameDialog->basicTimeHour2() * 60
                               + m_startGameDialog->basicTimeMinutes2())
                              * 60000);
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
        m_byoyomiMilliSec1 = m_startGameDialog->byoyomiSec1() * 1000;

        // 対局者2の秒読み時間を設定（単位はミリ秒）
        m_byoyomiMilliSec2 = m_startGameDialog->byoyomiSec2() * 1000;

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
            m_byoyomiMilliSec1 = 0;

            // 対局者2の秒読み時間を設定（単位はミリ秒）
            m_byoyomiMilliSec2 = 0;

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
            m_byoyomiMilliSec1 = 0;

            // 対局者2の秒読み時間を設定（単位はミリ秒）
            m_byoyomiMilliSec2 = 0;

            // 対局者1の1手ごとの時間加算（単位はミリ秒）
            m_addEachMoveMiliSec1 = 0;

            // 対局者2の1手ごとの時間加算（単位はミリ秒）
            m_addEachMoveMiliSec2 = 0;
        }
    }

    // 対局者1と対局者2の持ち時間を設定する。
    setPlayerTimeLimits();

    // m_shogiView->blackClockLabel()->setText(m_shogiClock->getPlayer1TimeString());
    // m_shogiView->whiteClockLabel()->setText(m_shogiClock->getPlayer2TimeString());
    // m_shogiView->update();

    //begin
    qDebug() << "---- setRemainingTimeAndCountDown() ---";
    qDebug() << "m_isLoseOnTimeout = " << m_isLoseOnTimeout;
    qDebug() << "m_bTime = " << m_bTime;
    qDebug() << "m_wTime = " << m_wTime;
    qDebug() << "m_useByoyomi = " << m_useByoyomi;
    qDebug() << "m_countDownMilliSec1 = " << m_byoyomiMilliSec1;
    qDebug() << "m_countDownMilliSec2 = " << m_byoyomiMilliSec2;
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

        m_infoWidget2->setVisible(engineVsEngine);
        m_usiView2->setVisible(engineVsEngine);

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
            m_kifuView->setSelectionMode(QAbstractItemView::NoSelection);
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
    // 将棋盤の駒の選択、ハイライト、シグナル・スロットを解除する。
    //m_shogiView->disconnect();

    // 選択されたマスの座標を初期化する。
    m_clickPoint = QPoint();

    // 全てのマスのハイライトを削除する。
    m_shogiView->removeHighlightAllData();

    // 残り時間を初期化する。
    m_shogiView->blackClockLabel()->setText("00:00:00");
    m_shogiView->whiteClockLabel()->setText("00:00:00");

    // 棋譜欄を初期化する。
    m_kifuRecordModel->clearAllItems();

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
    m_tab->setCurrentWidget(m_usiView1);

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
    // 0) リセット
    m_commentsByRow.clear();
    if (m_branchText) m_branchText->clear();

    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    const QString initialSfen = prepareInitialSfen(filePath, teaiLabel);

    // 保存＆KIF再生モード
    m_initialSfen   = initialSfen;
    m_isKifuReplay  = true;

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

    // 終局/中断判定
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

    // 4) スナップショット退避（本譜）
    m_dispMain = disp;
    m_sfenMain = *m_sfenRecord;
    m_gmMain   = m_gameMoves;

    // 5) 分岐データの構築（m_variationsByPly / m_variationsSeq など）
    m_variationsByPly.clear();
    m_branchablePlies.clear();
    m_variationsSeq.clear();   // ツリー接続用：ファイル登場順に保持

    // 5-0) 本文から「＋」と「変化：n手」見出しを収集
    QString usedEnc2, readErr2;
    QStringList kLines;
    if (!KifReader::readLinesAuto(filePath, kLines, &usedEnc2, &readErr2)) {
        qWarning().noquote() << "[VAR] readLinesAuto failed:" << readErr2;
    }

    static const QRegularExpression s_varHdrCap(
        QStringLiteral(R"(^\s*変化[：:]\s*([0-9０-９]+)\s*手)"));
    static const QRegularExpression s_mainHdr(QStringLiteral(R"(^\s*本譜\s*$)"));
    static const QRegularExpression s_moveHead(QStringLiteral("^\\s*([0-9０-９]+)\\s"));
    static const QRegularExpression s_plusAtEnd(QStringLiteral("\\+\\s*$"));

    auto flexDigitsToInt = [](const QString& s)->int {
        int v = 0;
        for (QChar ch : s) {
            int d = -1;
            const ushort u = ch.unicode();
            if (u >= '0' && u <= '9') d = (u - '0');
            else {
                static const QString z = QStringLiteral("０１２３４５６７８９");
                int idx = z.indexOf(ch);
                if (idx >= 0) d = idx;
            }
            if (d >= 0) v = v * 10 + d;
        }
        return v;
    };

    QList<int> plusStarts;    // 本譜末尾「+」マーク由来
    QList<int> headerStarts;  // 見出し「変化：n手」由来
    {
        bool inVariation = false;
        for (const QString& raw : kLines) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            if (s_mainHdr.match(line).hasMatch()) {
                inVariation = false;
                continue;
            }
            auto vh = s_varHdrCap.match(line);
            if (vh.hasMatch()) {
                inVariation = true;
                headerStarts << flexDigitsToInt(vh.captured(1));
                continue;
            }
            if (inVariation) continue;

            auto mh = s_moveHead.match(line);
            if (!mh.hasMatch()) continue;
            if (!s_plusAtEnd.match(line).hasMatch()) continue;

            const int ply = flexDigitsToInt(mh.captured(1));
            if (ply > 0) plusStarts << ply;
        }
    }
    qDebug() << "[VAR] plus markers   =" << plusStarts;
    qDebug() << "[VAR] header markers =" << headerStarts;

    // 5-1) 手目→baseSFEN をキャッシュ
    QHash<int, QString> baseByPly;
    auto getBaseForPly = [&](int startPly)->QString {
        if (baseByPly.contains(startPly)) return baseByPly[startPly];
        SfenPositionTracer tr; tr.setFromSfen(initialSfen);
        const int parentMoves = qMin(startPly - 1, m_usiMoves.size());
        for (int i = 0; i < parentMoves; ++i) tr.applyUsiMove(m_usiMoves[i]);
        const QString base = tr.toSfenString();
        baseByPly.insert(startPly, base);
        return base;
    };

    // 5-2) 変化を1本ずつ KifLine 化（startPly は「＋」→見出し→既定の順で決定）
    const int mainN = m_usiMoves.size();
    int vi = 0;
    for (const KifVariation& v0 : res.variations) {
        KifLine v;

        int sp = v0.line.startPly;                    // 解析既定
        if (vi < plusStarts.size()) {
            sp = plusStarts[vi];                      // 末尾「+」優先
        } else if (vi < headerStarts.size()) {
            sp = headerStarts[vi];                    // 見出し「変化：n手」
        }
        // サニティ（1..mainN にクランプ）
        if (sp < 1) sp = 1;
        if (mainN > 0 && sp > mainN) sp = mainN;

        v.startPly = sp;
        v.usiMoves = v0.line.usiMoves;
        v.disp     = v0.line.disp;
        v.endsWithTerminal = v0.line.endsWithTerminal;
        v.baseSfen = getBaseForPly(v.startPly);

        m_branchablePlies.insert(v.startPly);

        // v.sfenList / v.gameMoves 生成
        {
            SfenPositionTracer tr; tr.setFromSfen(v.baseSfen);
            v.sfenList.clear();
            v.sfenList << tr.toSfenString();

            auto rankLetterToNum = [](QChar r)->int {
                ushort u = r.toLower().unicode();
                return (u<'a'||u>'i') ? -1 : int(u-'a')+1;
            };
            auto tokenToOneChar = [](const QString& tok)->QChar {
                if (tok.isEmpty()) return QLatin1Char(' ');
                if (tok.size()==1) return tok.at(0);
                static const QHash<QString,QChar> map = {
                    {"+P",'Q'},{"+L",'M'},{"+N",'O'},{"+S",'T'},{"+B",'C'},{"+R",'U'},
                    {"+p",'q'},{"+l",'m'},{"+n",'o'},{"+s",'t'},{"+b",'c'},{"+r",'u'},
                };
                auto it = map.find(tok);
                return it==map.end()? QLatin1Char(' '): *it;
            };

            v.gameMoves.clear();
            for (const QString& usi : v.usiMoves) {
                if (usi.size()>=2 && usi[1]==QLatin1Char('*')) {
                    const bool black = tr.blackToMove();
                    const QChar up = usi.at(0).toUpper();
                    const int f = usi.at(2).toLatin1()-'0';
                    const int r = rankLetterToNum(usi.at(3));
                    const QPoint from(black ? 9 : 10,
                                      black ? (up=='P'?0:up=='L'?1:up=='N'?2:up=='S'?3:up=='G'?4:up=='B'?5:6)
                                            : (up=='P'?8:up=='L'?7:up=='N'?6:up=='S'?5:up=='G'?4:up=='B'?3:2));
                    const QPoint to(f-1, r-1);
                    const QChar moving = black ? up : up.toLower();
                    v.gameMoves.push_back(ShogiMove(from, to, moving, QLatin1Char(' '), false));
                    tr.applyUsiMove(usi);
                    v.sfenList << tr.toSfenString();
                    continue;
                }

                const int ff = usi.at(0).toLatin1()-'0';
                const int rf = rankLetterToNum(usi.at(1));
                const int ft = usi.at(2).toLatin1()-'0';
                const int rt = rankLetterToNum(usi.at(3));
                const bool prom = (usi.size()>=5 && usi.at(4)==QLatin1Char('+'));

                const QString fromTok = tr.tokenAtFileRank(ff, usi.at(1));
                const QString toTok   = tr.tokenAtFileRank(ft, usi.at(3));
                const QPoint from(ff-1, rf-1);
                const QPoint to  (ft-1, rt-1);
                const QChar moving   = tokenToOneChar(fromTok);
                const QChar captured = tokenToOneChar(toTok);

                v.gameMoves.push_back(ShogiMove(from, to, moving, captured, prom));
                tr.applyUsiMove(usi);
                v.sfenList << tr.toSfenString();
            }
        }

        // ツリー接続用：ファイル登場順で保存
        m_variationsSeq.push_back(v);
        // バケツにも格納（候補欄用）
        m_variationsByPly[v.startPly].push_back(v);

        ++vi;
    }

    // 5-3) キー確認・ログ
    {
        QList<int> keys = m_variationsByPly.keys();
        std::sort(keys.begin(), keys.end());
        qDebug().noquote() << "[VAR] keys(after remap) =" << keys;
        qDebug() << "[VAR] branchable set =" << m_branchablePlies;
    }

    // 6) ログ（任意）
    logImportSummary(filePath, m_usiMoves, disp, teaiLabel, parseWarn, QString());
    {
        qDebug().noquote() << "== Variations (変化, after remap) ==";
        QList<int> keys = m_variationsByPly.keys();
        std::sort(keys.begin(), keys.end());
        if (keys.isEmpty()) {
            qDebug().noquote() << "  (none)";
        } else {
            for (int startPly : keys) {
                const auto& bucket = m_variationsByPly[startPly];
                int vidx = 0;
                for (const auto& v : bucket) {
                    qDebug().noquote()
                        << QStringLiteral("[変化%1] start=%2, USI=%3手, 表示=%4手")
                              .arg(++vidx).arg(v.startPly)
                              .arg(v.usiMoves.size()).arg(v.disp.size());
                    const int preview = qMin(6, v.disp.size());
                    for (int i = 0; i < preview; ++i) {
                        const auto& it = v.disp.at(i);
                        qDebug().noquote()
                            << QStringLiteral("   ・「%1」「%2」")
                                  .arg(it.prettyMove,
                                       it.timeText.isEmpty()
                                           ? QStringLiteral("00:00/00:00:00")
                                           : it.timeText);
                    }
                    if (v.disp.size() > preview) {
                        qDebug().noquote()
                            << QStringLiteral("   ……（以下 %1 手）")
                                  .arg(v.disp.size() - preview);
                    }
                }
            }
        }
    }

    // 7) ★ 「後勝ち」前計算：行（本譜＋分岐）を一括解決
    buildResolvedLinesAfterLoad();

    // ★これを必ず追加
    buildBranchCandidateIndex();

    // 8) GUI初期表示は「解決済み 本譜 行・0手目」を適用するだけ
    applyResolvedRowAndSelect(/*resolvedRow=*/0, /*selectPly=*/0);

    // 9) 棋譜欄の選択ハンドラ（既存）
    if (m_kifuView) {
        m_kifuView->setSelectionMode(QAbstractItemView::SingleSelection);
        if (m_kifuView->selectionModel()) {
            connect(m_kifuView->selectionModel(),
                    &QItemSelectionModel::currentRowChanged,
                    this,
                    &MainWindow::onMainMoveRowChanged,
                    Qt::UniqueConnection);
        }
    }

    // 10) 分岐ツリーを再描画（クリック時は applyResolvedRowAndSelect を使用）
    rebuildBranchTreeView();

    // 11) そのほか UI の整合
    enableArrowButtons();
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
    // クリア
    m_moveRecords->clear();
    m_currentMoveIndex = 0;
    m_kifuDataList.clear();
    m_moveRecords->clear();
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
    //   行番号: row = i+1
    //   表示コメント: disp[i+1].comment（= その手の後に書かれるコメント）
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

    // 初期表示を開始局面に合わせておく（任意）
    if (m_kifuView)
        m_kifuView->setCurrentIndex(m_kifuRecordModel->index(0, 0));
    if (m_branchText) {
        const QString head = (0 < m_commentsByRow.size() ? m_commentsByRow[0].trimmed()
                                                         : QString());
        m_branchText->setPlainText(head.isEmpty() ? tr("コメントなし") : head);
    }

    // 行選択が変わったらコメントも更新
    connect(m_kifuView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& cur, const QModelIndex&) {
                updateBranchTextForRow(cur.row());
            });
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

QString MainWindow::makeBranchHtml(const QString& text) const
{
    if (text.isEmpty())
        return QString();

    // エスケープ → URL をリンク化（簡易）
    QString esc = Qt::convertFromPlainText(text); // 改行を <br> に、& などをエスケープ
    static const QRegularExpression re(R"((https?://[^\s<]+))");
    esc.replace(re, R"(<a href="\1">\1</a>)");
    return esc;
}

void MainWindow::updateBranchTextForRow(int row)
{
    QString raw;
    if (row >= 0 && row < m_commentsByRow.size())
        raw = m_commentsByRow.at(row).trimmed();

    const QString html = makeBranchHtml(raw);

    // 元のコメントパネルを更新
    if (m_branchText) {
        m_branchText->setHtml(html);
    }

    // ★ 追加：m_tab1 のコメントタブも更新
    if (m_branchTextInTab1) {
        m_branchTextInTab1->setHtml(html);
    }
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

    // 棋譜表示を最下部へスクロールする。
    if (m_kifuView) {
        m_kifuView->scrollToBottom();
        m_kifuView->update();
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
    m_kifuView->setSelectionMode(QAbstractItemView::SingleSelection);
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
    m_kifuRecordModel->appendItem(new KifuDisplay("=== 開始局面 ===", "（１手 / 合計）"));

    // 初期SFEN文字列をm_sfenRecordに格納しておく。
    m_sfenRecord->append(m_startSfenStr);

    // 棋譜欄の下の矢印ボタンを無効にする。
    disableArrowButtons();

    // 棋譜欄の行をクリックしても選択できないようにする。
    m_kifuView->setSelectionMode(QAbstractItemView::NoSelection);

    // 手番が先手あるいは下手の場合
    if (m_shogiView->board()->currentPlayer() == "b") {
      // 手番を先手あるいは下手に設定する。
      m_gameController->setCurrentPlayer(ShogiGameController::Player1);
    }
    // 手番が後手あるいは上手の場合
    else if (m_shogiView->board()->currentPlayer() == "w") {
      // 手番を後手あるいは上手に設定する。
      m_gameController->setCurrentPlayer(ShogiGameController::Player2);
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

    // 将棋盤上での左クリックイベントをハンドリングする。
    //connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::onShogiViewClicked);

    // 将棋盤上での右クリックイベントをハンドリングする。
    //connect(m_shogiView, &ShogiView::rightClicked, this, &MainWindow::onShogiViewRightClicked);

    // 駒のドラッグを終了する。
    connect(m_gameController, &ShogiGameController::endDragSignal, this, &MainWindow::endDrag);
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

    // もしドック内に入っていたら取り外して破棄（二重親防止）
    if (m_gameInfoDock && m_gameInfoDock->widget() == m_gameInfoTable) {
        m_gameInfoDock->setWidget(nullptr);
        m_gameInfoDock->deleteLater();
        m_gameInfoDock = nullptr;
    }

    if (m_tab->indexOf(m_gameInfoTable) == -1) {
        const int commentsIdx = m_tab->indexOf(m_branchTextInTab1);
        const int insertPos   = (commentsIdx >= 0) ? commentsIdx + 1 : m_tab->count();
        m_tab->insertTab(insertPos, m_gameInfoTable, tr("対局情報"));
        // お好みで：m_tab->setCurrentIndex(insertPos); // 追加直後に選択したい場合
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

void MainWindow::onMainMoveRowChanged(const QModelIndex& current, const QModelIndex&)
{
    const int row = current.isValid() ? current.row() : -1;
    // 行0 = 「=== 開始局面 ===」、行n = n手目 という前提
    m_currentSelectedPly = (row <= 0) ? 0 : row;

    qDebug() << "[SEL] rowChanged row=" << row
             << " -> ply=" << m_currentSelectedPly
             << " branchable?=" << m_branchablePlies.contains(m_currentSelectedPly);

    populateBranchListForPly(m_currentSelectedPly);
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

void MainWindow::applyVariation(int parentPly, int branchIndex)
{
    if (!m_variationsByPly.contains(parentPly)) return;
    const VariationBucket& bucket = m_variationsByPly[parentPly];
    if (branchIndex < 0 || branchIndex >= bucket.size()) return;

    const KifLine& v = bucket.at(branchIndex);

    // 1) 表示用（disp）を合成：本譜の 1..N-1 手 + 分岐ライン
    QList<KifDisplayItem> dispCombined;
    for (int i = 0; i < parentPly - 1 && i < m_dispMain.size(); ++i)
        dispCombined.push_back(m_dispMain.at(i));
    for (const auto& x : v.disp)
        dispCombined.push_back(x);

    // 2) sfen を合成：本譜の 0..N-1 局面 + 分岐 1..end
    QStringList sfenCombined;
    for (int i = 0; i < parentPly && i < m_sfenMain.size(); ++i)
        sfenCombined << m_sfenMain.at(i);
    for (int i = 1; i < v.sfenList.size(); ++i)  // v.sfenList[0] は base
        sfenCombined << v.sfenList.at(i);

    // 3) gameMoves を合成：本譜の 1..N-1 + 分岐
    QVector<ShogiMove> gmCombined;
    for (int i = 0; i < parentPly - 1 && i < m_gmMain.size(); ++i)
        gmCombined.push_back(m_gmMain.at(i));
    for (const auto& mv : v.gameMoves)
        gmCombined.push_back(mv);

    // 4) 反映
    *m_sfenRecord = sfenCombined;
    m_gameMoves   = gmCombined;
    displayGameRecord(dispCombined);

    // （任意）分岐欄は選択を残す or クリアする
    // m_kifuBranchModel->setItems({});
    // m_kifuBranchView->setEnabled(false);

    // 再生位置や矢印ボタンの初期化
    navigateToFirstMove();
    enableArrowButtons();
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

void MainWindow::onBranchRowClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    const int ply = m_currentSelectedPly;
    if (ply <= 0) return;

    auto it = m_variationsByPly.constFind(ply);
    if (it == m_variationsByPly.cend()) return;
    const VariationBucket& bucket = it.value();

    const int row = index.row();

    // 末尾「本譜へ戻る」
    if (m_kifuBranchModel->isBackToMainRow(row)) {
        *m_sfenRecord = m_sfenMain;
        m_gameMoves   = m_gmMain;
        showRecordAtPly(m_dispMain, /*selectPly=*/ply);
        syncBoardAndHighlightsAtRow(ply);
        populateBranchListForPly(ply);
        enableArrowButtons();
        return;
    }

    // 行0 = 「本譜のその手」（同手に戻す）
    if (row == 0) {
        *m_sfenRecord = m_sfenMain;
        m_gameMoves   = m_gmMain;
        showRecordAtPly(m_dispMain, /*selectPly=*/ply);
        syncBoardAndHighlightsAtRow(ply);
        populateBranchListForPly(ply);
        enableArrowButtons();
        return;
    }

    // 1行目以降が分岐
    const int vIdx = row - 1;
    if (vIdx < 0 || vIdx >= bucket.size()) return;
    const KifLine& v = bucket[vIdx];

    // ★★★ ここが肝：合成の土台を“本譜”ではなく「現在の経路」にする ★★★
    // prefix は「現在表示している経路」の v.startPly-1 手まで
    QList<KifDisplayItem> mergedDisp;
    if (v.startPly > 1)
        mergedDisp = m_dispCurrent.mid(0, v.startPly - 1);
    mergedDisp += v.disp;

    // SFEN も「現在の経路」の v.startPly 手目の局面までで prefix を作る
    QStringList mergedSfen = m_sfenRecord->mid(0, v.startPly);
    mergedSfen += v.sfenList.mid(1); // 先頭は基底と重複するので除外

    // m_gameMoves も同様に合成
    QVector<ShogiMove> mergedGm;
    if (v.startPly > 1)
        mergedGm = m_gameMoves.mid(0, v.startPly - 1);
    mergedGm += v.gameMoves;

    // 反映
    *m_sfenRecord = mergedSfen;
    m_gameMoves   = mergedGm;

    // 分岐の最初の手（= startPly）を選択状態で表示
    showRecordAtPly(mergedDisp, /*selectPly=*/v.startPly);
    m_currentSelectedPly = v.startPly;

    // 見た目の選択も同期
    if (m_kifuRecordModel && m_kifuView) {
        const QModelIndex idx2 = m_kifuRecordModel->index(m_currentSelectedPly, 0);
        m_kifuView->setCurrentIndex(idx2);
        m_kifuView->scrollTo(idx2, QAbstractItemView::PositionAtCenter);
    }

    // 盤面＆ハイライトを即同期
    syncBoardAndHighlightsAtRow(m_currentSelectedPly);

    // 同じ手目の分岐候補は維持（末尾「本譜へ戻る」も残る）
    populateBranchListForPly(v.startPly);
    enableArrowButtons();
}

// 全角/半角の数字を int に
static int flexDigitsToInt(const QString& s) {
    int v = 0;
    for (const QChar &ch : s) {
        ushort u = ch.unicode();
        if (u >= u'0' && u <= u'9')        v = v*10 + (u - u'0');
        else if (u >= 0xFF10 && u <= 0xFF19) v = v*10 + (u - 0xFF10); // 全角0..9
    }
    return v;
}

// KIF を1回なめて「変化：N手」「本譜行末の +」を拾う
static QList<int> scanVariationStarts(const QString& kifPath, QSet<int>& outBranchablePlies)
{
    outBranchablePlies.clear();
    QList<int> starts; // 変化ブロックの開始手を出現順に

    QString usedEnc, err;
    QStringList lines;
    if (!KifReader::readLinesAuto(kifPath, lines, &usedEnc, &err)) return starts;

    static const QRegularExpression reVar(
        QStringLiteral(R"(変化[：:]\s*([0-9０-９]+)\s*手)"));
    static const QRegularExpression reMoveHead(
        QStringLiteral(R"(^\s*([0-9０-９]+)\s)"));      // 本譜の手数頭
    static const QRegularExpression rePlusAtEnd(
        QStringLiteral(R"(\+\s*$)"));                   // 行末 '+'

    bool inVariation = false; // 変化ブロック内の + は無視したい
    int moveIndex = 0;        // 本譜の現在手数を数える

    for (const QString& raw : lines) {
        const QString line = raw.trimmed();

        // 変化ヘッダ
        if (const auto m = reVar.match(line); m.hasMatch()) {
            const int ply = flexDigitsToInt(m.captured(1));
            if (ply > 0) { starts << ply; outBranchablePlies.insert(ply); }
            inVariation = true;
            continue;
        }

        // 本譜の手数行か？
        if (const auto mh = reMoveHead.match(line); mh.hasMatch()) {
            moveIndex = flexDigitsToInt(mh.captured(1));
            if (!inVariation && rePlusAtEnd.match(line).hasMatch()) {
                if (moveIndex > 0) outBranchablePlies.insert(moveIndex);
            }
            continue;
        }

        // 非手行。シンプルに：空行や「本譜」等で変化抜け扱い
        if (line.isEmpty() || line.contains(QStringLiteral("本譜"))) {
            inVariation = false;
        }
    }
    return starts;
}

void MainWindow::showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly)
{
    // ★ これを保持しておくと、分岐クリック時に「現在の経路」を前提に合成できる
    m_dispCurrent = disp;

    // 既存のテーブル更新
    displayGameRecord(disp);

    // 「=== 開始局面 ===」が row=0、1手目が row=1
    const int rc  = m_kifuRecordModel->rowCount();
    const int row = qBound(0, selectPly, rc > 0 ? rc - 1 : 0);

    const QModelIndex idx = m_kifuRecordModel->index(row, 0);
    if (idx.isValid()) {
        m_kifuView->setCurrentIndex(idx);
        m_kifuView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }
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

void MainWindow::refreshBoardAndHighlightsForRow(int row)
{
    if (!m_sfenRecord || m_sfenRecord->isEmpty()) return;
    if (row < 0) row = 0;
    if (row >= m_sfenRecord->size()) row = m_sfenRecord->size() - 1;

    // “現在手数”として内部状態を揃える（既存実装に合わせて両方更新）
    m_currentSelectedPly = row;
    m_currentMoveIndex   = row;

    // 盤面をこの手数のSFENへ
    applySfenAtCurrentPly();

    // ハイライトの更新
    clearMoveHighlights();
    // 開始局面(row==0)ではハイライト不要。m_gameMovesは手数分だけ入っている想定。
    if (row > 0 && row <= m_gameMoves.size()) {
        addMoveHighlights();
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

    QStringList mergedSfen = m_sfenMain.mid(0, v.startPly);   // [0..startPly-1] まで本譜
    mergedSfen += v.sfenList.mid(1);                           // 先頭(基底)は重複するので除外

    QVector<ShogiMove> mergedGm;
    if (v.startPly > 1)
        mergedGm = m_gmMain.mid(0, v.startPly - 1);
    mergedGm += v.gameMoves;

    // --- 現在の表示・局面ソースに反映 ---
    *m_sfenRecord = mergedSfen;
    m_gameMoves   = mergedGm;

    // 分岐の最初の手（= startPly手目）を選択
    const int selPly = v.startPly;
    showRecordAtPly(mergedDisp, /*selectPly=*/selPly);
    m_currentSelectedPly = selPly;

    // 棋譜欄の見た目も合わせる（明示的に選択＆スクロール）
    if (m_kifuRecordModel && m_kifuView) {
        const QModelIndex idx = m_kifuRecordModel->index(selPly, 0);
        m_kifuView->setCurrentIndex(idx);
        m_kifuView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
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

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (m_branchTreeView && obj == m_branchTreeView->viewport()
        && ev->type() == QEvent::MouseButtonRelease)
    {
        auto* me = static_cast<QMouseEvent*>(ev);
        const QPointF sp = m_branchTreeView->mapToScene(me->pos());

        QGraphicsItem* hit = m_branchTreeView->scene()
                                 ? m_branchTreeView->scene()->itemAt(sp, QTransform())
                                 : nullptr;
        // 子(Text)が当たることがあるので親まで遡ってデータを探す
        while (hit && !hit->data(BR_ROLE_KIND).isValid())
            hit = hit->parentItem();
        if (!hit) return false;

        const int kind = hit->data(BR_ROLE_KIND).toInt();

        // 以降はハイライトを“必ず”更新する流れで統一
        auto selectRowInView = [&](int row){
            if (m_kifuRecordModel && m_kifuView) {
                const QModelIndex idx = m_kifuRecordModel->index(row, 0);
                m_kifuView->setCurrentIndex(idx);
                m_kifuView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
            }
        };

        switch (kind) {
        case BNK_Start: {
            // 開始局面へ
            *m_sfenRecord = m_sfenMain;
            m_gameMoves   = m_gmMain;
            showRecordAtPly(m_dispMain, /*selectPly=*/0);
            m_currentSelectedPly = 0;
            selectRowInView(0);

            // 分岐候補は空＆無効
            if (m_kifuBranchModel) m_kifuBranchModel->clearBranchCandidates();
            if (m_kifuBranchView)  m_kifuBranchView->setEnabled(false);

            // 盤・ハイライト
            syncBoardAndHighlightsAtRow(0);
            enableArrowButtons();
            return true;
        }
        case BNK_Main: {
            const int ply = hit->data(BR_ROLE_PLY).toInt();
            *m_sfenRecord = m_sfenMain;
            m_gameMoves   = m_gmMain;
            showRecordAtPly(m_dispMain, /*selectPly=*/ply);
            m_currentSelectedPly = ply;
            selectRowInView(ply);

            // この手での分岐候補
            populateBranchListForPly(ply);

            // 盤・ハイライト
            syncBoardAndHighlightsAtRow(ply);
            enableArrowButtons();
            return true;
        }
        case BNK_Var: {
            const int startPly = hit->data(BR_ROLE_STARTPLY).toInt();
            const int bIdx     = hit->data(BR_ROLE_BUCKET).toInt();

            // 既存のヘルパを活用（棋譜差し替え＋選択まで）
            applyVariationByKey(startPly, bIdx);

            // 念のため盤・ハイライトを明示同期
            syncBoardAndHighlightsAtRow(startPly);
            enableArrowButtons();
            return true;
        }
        default:
            break;
        }
    }
    return QObject::eventFilter(obj, ev);
}

void MainWindow::rebuildBranchTreeView()
{
    if (!m_branchTreeView) return;

    // 3-1) 索引クリア
    m_branchNodeIndex.clear();

    // 既存シーン破棄→新規作成
    if (auto* old = m_branchTreeView->scene()) old->deleteLater();
    auto* sc = new QGraphicsScene(m_branchTreeView);
    m_branchTreeView->setScene(sc);
    m_branchTreeView->setRenderHint(QPainter::Antialiasing, true);

    // ---- レイアウト定数 ----
    const qreal COL_W   = 168;
    const qreal NODE_W  = 132;
    const qreal NODE_H  = 30;
    const qreal TOP_Y   = 48;
    const qreal VAR_Y0  = TOP_Y + 70;
    const qreal ROW_H   = 64;
    const qreal RADIUS  = 8.0;

    // ---- ロール ----
    enum { RoleKind=0, RolePly=1, RoleRow=2,
           RoleBasePen=10, RoleBasePenW=11, RoleBaseBrush=12 };
    enum { NodeRoot=0, NodeMain=1, NodeVar=2 };

    // ---- 色 ----
    const QPen   penRoot (QColor(160,160,160), 1.1);
    const QBrush brRoot  (QColor(245,245,245));
    const QPen   penBlack(QColor( 72,120,170), 1.2);
    const QBrush brBlack(QColor(232,245,255));   // ▲奇数手
    const QPen   penWhite(QColor(190,110,120), 1.2);
    const QBrush brWhite(QColor(255,236,242));   // △偶数手
    const QPen   penSel  (QColor(255,200,0), 3.0);

    // ---- ユーティリティ ----
    auto makePath = [&](const QSizeF& sz){
        QPainterPath p; p.addRoundedRect(QRectF(QPointF(0,0), sz), RADIUS, RADIUS); return p;
    };
    auto storeBaseStyle = [&](QGraphicsPathItem* it, const QPen& p, const QBrush& b){
        it->setPen(p); it->setBrush(b);
        it->setData(RoleBasePen,  p.color());
        it->setData(RoleBasePenW, p.widthF());
        it->setData(RoleBaseBrush,b.color());
    };
    auto restoreBaseStyle = [&](QGraphicsPathItem* it){
        if (!it) return;
        const QColor pc = it->data(RoleBasePen).value<QColor>();
        const qreal  pw = it->data(RoleBasePenW).toDouble();
        const QColor bc = it->data(RoleBaseBrush).value<QColor>();
        it->setPen(QPen(pc, pw));
        it->setBrush(QBrush(bc));
    };
    auto colX = [&](int ply)->qreal { return 20 + COL_W * ply; };
    auto rightCenter = [](const QRectF& rc){ return QPointF(rc.right(), rc.center().y()); };
    auto leftCenter  = [](const QRectF& rc){ return QPointF(rc.left(),  rc.center().y()); };

    auto addNode = [&](const QPointF& posScene, const QString& text,
                       int role, int ply, int row,
                       const QPen& p, const QBrush& b) -> QGraphicsPathItem*
    {
        auto* it = new QGraphicsPathItem;
        it->setPath(makePath(QSizeF(NODE_W, NODE_H)));
        it->setPos(posScene);
        it->setZValue(10);
        it->setData(RoleKind, role);
        it->setData(RolePly,  ply);
        it->setData(RoleRow,  row);
        it->setFlag(QGraphicsItem::ItemIsSelectable, true);
        it->setAcceptedMouseButtons(Qt::LeftButton);
        it->setCursor(Qt::PointingHandCursor);
        storeBaseStyle(it, p, b);
        sc->addItem(it);

        auto* t = new QGraphicsTextItem(text, it);
        QFont f = t->font(); f.setPointSizeF(f.pointSizeF()*0.95); t->setFont(f);
        const QRectF br = t->boundingRect();
        t->setPos(QPointF(NODE_W/2.0 - br.width()/2.0, NODE_H/2.0 - br.height()/2.0));
        t->setAcceptedMouseButtons(Qt::NoButton);
        t->setFlag(QGraphicsItem::ItemIsSelectable, false);
        t->setZValue(11);
        return it;
    };

    auto addEdge = [&](const QPointF& a, const QPointF& b){
        auto* ln = sc->addLine(QLineF(a,b), QPen(QColor(140,140,140),1.0));
        ln->setZValue(-10); return ln;
    };

    // === 最大手数の見積り ===
    int maxPly = 0;
    if (!m_resolvedRows.isEmpty()) {
        maxPly = m_resolvedRows.first().disp.size();           // 本譜の最終手
        for (int r = 1; r < m_resolvedRows.size(); ++r)
            maxPly = qMax(maxPly, m_resolvedRows[r].disp.size());
    } else {
        maxPly = m_dispMain.size();
    }

    // 列ヘッダ
    for (int i=1; i<=maxPly; ++i) {
        auto* h = sc->addText(QStringLiteral("%1手目").arg(i));
        const QRectF br = h->boundingRect();
        h->setPos(colX(i) + NODE_W/2.0 - br.width()/2.0, TOP_Y - 30);
        h->setZValue(100);
    }

    // 親アンカー
    QHash<int,QRectF> lastRectAtCol;

    // S ノード（3-2 索引登録）
    QRectF rcRoot(QPointF(colX(0), TOP_Y), QSizeF(NODE_W, NODE_H));
    auto* itRoot = addNode(rcRoot.topLeft(), QStringLiteral("S"), NodeRoot, 0, 0, penRoot, brRoot);
    m_branchNodeIndex.insert(qMakePair(0, 0), itRoot);
    lastRectAtCol[0] = rcRoot;

    // --- 行0：本譜 ---
    if (!m_resolvedRows.isEmpty()) {
        const auto& R0 = m_resolvedRows.first();
        QRectF prev = rcRoot;
        for (int i=1; i<=R0.disp.size(); ++i) {
            const bool black = (i % 2 == 1);
            QRectF rc(QPointF(colX(i), TOP_Y), QSizeF(NODE_W, NODE_H));
            auto* itMain = addNode(rc.topLeft(), R0.disp.at(i-1).prettyMove,
                                   NodeMain, i, 0,
                                   black ? penBlack : penWhite,
                                   black ? brBlack : brWhite);
            // 3-2 索引登録
            m_branchNodeIndex.insert(qMakePair(0, i), itMain);

            addEdge(rightCenter(prev), leftCenter(rc));
            prev = rc;
            lastRectAtCol[i] = rc;
        }
    } else {
        // フォールバック
        QRectF prev = rcRoot;
        for (int i=1; i<=m_dispMain.size(); ++i) {
            const bool black = (i % 2 == 1);
            QRectF rc(QPointF(colX(i), TOP_Y), QSizeF(NODE_W, NODE_H));
            auto* itMain = addNode(rc.topLeft(), m_dispMain.at(i-1).prettyMove,
                                   NodeMain, i, 0,
                                   black ? penBlack : penWhite,
                                   black ? brBlack : brWhite);
            // 3-2 索引登録
            m_branchNodeIndex.insert(qMakePair(0, i), itMain);

            addEdge(rightCenter(prev), leftCenter(rc));
            prev = rc;
            lastRectAtCol[i] = rc;
        }
    }

    // --- 行1..：分岐行 ---
    for (int row = 1; row < m_resolvedRows.size(); ++row) {
        const auto& RL = m_resolvedRows[row];
        if (RL.disp.isEmpty()) continue;

        const int start = qMax(1, RL.startPly);
        const QList<KifDisplayItem> seq = RL.disp.mid(start - 1);

        QRectF prevVar;
        for (int k = 0; k < seq.size(); ++k) {
            const int col = start + k;               // 実際の手数列
            const bool black = (col % 2 == 1);
            const qreal y = VAR_Y0 + (row-1)*ROW_H;  // 行ごとの段
            QRectF rc(QPointF(colX(col), y), QSizeF(NODE_W, NODE_H));

            auto* itVar = addNode(rc.topLeft(), seq.at(k).prettyMove,
                                  NodeVar, col, row,
                                  black ? penBlack : penWhite,
                                  black ? brBlack : brWhite);
            // 3-2 索引登録
            m_branchNodeIndex.insert(qMakePair(row, col), itVar);

            if (k == 0) {
                const QRectF parent = lastRectAtCol.value(col-1, rcRoot);
                addEdge(rightCenter(parent), leftCenter(rc));
            } else {
                addEdge(rightCenter(prevVar), leftCenter(rc));
            }

            prevVar = rc;
            lastRectAtCol[col] = rc;                 // この列の“直近ノード”を更新
        }
    }

    sc->setSceneRect(sc->itemsBoundingRect().adjusted(-20,-20, 200, 60));

    // ---- 選択ハンドラ（3-3 再入抑止付き）----
    QObject::disconnect(sc, nullptr, this, nullptr);
    QObject::connect(sc, &QGraphicsScene::selectionChanged, this,
        [this, sc, penSel, restoreBaseStyle]()
    {
        for (auto* item : sc->items()) {
            if (auto* it = qgraphicsitem_cast<QGraphicsPathItem*>(item)) restoreBaseStyle(it);
        }
        const auto sel = sc->selectedItems();
        if (sel.isEmpty()) return;
        auto* it = qgraphicsitem_cast<QGraphicsPathItem*>(sel.first());
        if (!it) return;

        it->setPen(penSel);

        const int role = it->data(RoleKind).toInt();
        const int ply  = qMax(0, it->data(RolePly).toInt());
        const int row  = qMax(0, it->data(RoleRow).toInt());

        // ★ プログラム側からの選択変更時はここで打ち切り
        if (m_branchTreeSelectGuard) return;

        if (role == NodeRoot) {
            applyResolvedRowAndSelect(0, 0);
        } else {
            applyResolvedRowAndSelect(row, ply);     // 行・手数で一元遷移
        }
    });
}

#ifdef SHOGIBOARDQ_DEBUG_KIF
static QString dbgFlatMoves(const QList<KifDisplayItem>& disp, int startPly)
{
    QStringList out;
    out.reserve(disp.size());
    for (int i = 0; i < disp.size(); ++i) {
        const int ply = startPly + i;
        out << QStringLiteral("「%1%2」")
                 .arg(ply)
                 .arg(disp[i].prettyMove);
    }
    return out.join(QStringLiteral(" "));
}

void MainWindow::dbgDumpMoves(const QList<KifDisplayItem>& disp,
                              const QString& tag, int startPly) const
{
    qDebug().noquote() << tag
                       << QStringLiteral("(n=%1)").arg(disp.size())
                       << dbgFlatMoves(disp, startPly);
}

void MainWindow::dbgCompareGraphVsKifu(const QList<KifDisplayItem>& graphDisp,
                                       const QList<KifDisplayItem>& kifuDisp,
                                       int selPly, const QString& where) const
{
    qDebug().noquote() << "==== [DBG] Compare @" << where << " selPly=" << selPly << "====";
    dbgDumpMoves(graphDisp,  QStringLiteral("[graph]"), 1);
    dbgDumpMoves(kifuDisp,   QStringLiteral("[kifu ]"),  1);

    const int n = qMin(graphDisp.size(), kifuDisp.size());
    int diff = -1;
    for (int i = 0; i < n; ++i) {
        if (graphDisp[i].prettyMove != kifuDisp[i].prettyMove) { diff = i; break; }
    }
    if (diff < 0 && graphDisp.size() != kifuDisp.size()) diff = n; // 長さ違い

    if (diff < 0) {
        qDebug().noquote() << "[OK] graph と kifu は一致しました。";
    } else {
        const QString g = (diff < graphDisp.size() ? graphDisp[diff].prettyMove : QStringLiteral("(なし)"));
        const QString k = (diff < kifuDisp.size()  ? kifuDisp[diff].prettyMove  : QStringLiteral("(なし)"));
        qWarning().noquote()
            << QStringLiteral("[NG] 最初の不一致: %1手目 graph=「%2」 kifu=「%3」")
                  .arg(diff+1).arg(g, k);
    }
    qDebug().noquote() << "===============================================";
}
#endif

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

    // 棋譜欄へ反映
    showRecordAtPly(r.disp, m_activePly);
    m_currentSelectedPly = m_activePly;

    if (m_kifuRecordModel && m_kifuView) {
        const QModelIndex idx = m_kifuRecordModel->index(m_activePly, 0);
        m_kifuView->setCurrentIndex(idx);
        m_kifuView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }

    // 分岐候補・盤面ハイライト・矢印ボタン
    populateBranchListForPly(m_activePly);
    syncBoardAndHighlightsAtRow(m_activePly);
    enableArrowButtons();

    qDebug().noquote() << "[APPLY] row=" << row
                       << " ply=" << m_activePly
                       << " rows=" << m_resolvedRows.size()
                       << " dispSz=" << r.disp.size();

    // 分岐ツリー側の黄色ハイライト同期（centerOnはお好みで）
    highlightBranchTreeAt(/*row*/m_activeResolvedRow, /*ply*/m_activePly, /*centerOn*/false);

    // ★ 棋譜欄の「分岐あり」行をオレンジでマーク
    updateKifuBranchMarkersForActiveRow();
}

void MainWindow::buildResolvedRowsAfterLoad()
{
    QVector<ResolvedRow> rows;
    rows.reserve(1 + m_variationsSeq.size());

    // 行0 = 本譜
    {
        ResolvedRow r;
        r.startPly  = 1;
        r.disp  = m_dispMain;
        r.sfen  = m_sfenMain;
        r.gm = m_gmMain;
        rows.push_back(std::move(r));
    }

    // 行1.. = 分岐（後勝ちで prefix を上書き）
    for (int vi = 0; vi < m_variationsSeq.size(); ++vi) {
        const KifLine& v = m_variationsSeq.at(vi);
        if (v.disp.isEmpty()) continue;

        QList<KifDisplayItem> prefixDisp  = m_dispMain.mid(0, qMax(0, v.startPly - 1));
        QStringList           prefixSfen  = m_sfenMain.mid(0, qMax(0, v.startPly));
        QVector<ShogiMove>    prefixMoves = m_gmMain.mid(0, qMax(0, v.startPly - 1));

        // それ以前に登場した分岐で上書き
        for (int u = 0; u < vi; ++u) {
            const KifLine& up = m_variationsSeq.at(u);
            if (up.disp.isEmpty()) continue;
            const int s = qMax(1, up.startPly);
            const int e = qMin(v.startPly - 1, up.startPly + up.disp.size() - 1);
            for (int col = s; col <= e; ++col) {
                const int idx = col - 1;
                if (idx >= 0 && idx < prefixDisp.size())
                    prefixDisp[idx] = up.disp.at(idx - (up.startPly - 1));
                if (col >= 1 && col < prefixSfen.size())
                    prefixSfen[col] = up.sfenList.at(col - (up.startPly - 1));
                if (idx >= 0 && idx < prefixMoves.size())
                    prefixMoves[idx] = up.gameMoves.at(idx - (up.startPly - 1));
            }
        }

        ResolvedRow r;
        r.startPly  = qMax(1, v.startPly);
        r.disp  = prefixDisp;  r.disp += v.disp;
        r.sfen  = prefixSfen;  {
            QStringList body = v.sfenList;
            if (!body.isEmpty()) body.pop_front();
            r.sfen += body;
        }
        r.gm = prefixMoves; r.gm += v.gameMoves;

        rows.push_back(std::move(r));
    }

    // ★ここがキモ：メンバに保存（これが残らないと rows=0 になります）
    m_resolvedRows = std::move(rows);
}

void MainWindow::buildBranchCandidateIndex()
{
    m_branchIndex.clear();

    if (m_resolvedRows.isEmpty()) {
        qDebug().noquote() << "[BIDX] no resolved rows";
        return;
    }

    auto norm = [](QString s)->QString {
        static const QRegularExpression reHead(QStringLiteral(R"(^\s*[0-9０-９]+\s+)"));
        s.remove(reHead);
        return s.trimmed();
    };

    // 最大手数
    int maxPly = 0;
    for (const auto& r : m_resolvedRows)
        maxPly = qMax(maxPly, r.disp.size());

    // 各手数ごとに「基底SFEN単位」で候補を集約
    for (int ply = 1; ply <= maxPly; ++ply) {
        QHash<QString, QList<BranchCandidate>> groups;  // baseSFEN -> candidates
        QHash<QString, QSet<QString>> seenByBase;       // 重複排除（同じ文字の手）

        const int idx = ply - 1;

        for (int row = 0; row < m_resolvedRows.size(); ++row) {
            const auto& R = m_resolvedRows[row];
            if (idx < 0 || idx >= R.disp.size() || idx >= R.sfen.size())
                continue;

            const QString base = R.sfen.at(idx);                   // 基底SFEN（ply-1局面）
            const QString text = norm(R.disp.at(idx).prettyMove);  // その手の表示
            if (text.isEmpty()) continue;

            if (!seenByBase[base].contains(text)) {
                groups[base].append(BranchCandidate{ text, row, ply });
                seenByBase[base].insert(text);
            }
        }

        // 分岐になっていない（候補1件以下）の基底は捨てる
        for (auto it = groups.begin(); it != groups.end(); ) {
            if (it->size() <= 1) it = groups.erase(it);
            else                 ++it;
        }

        if (!groups.isEmpty()) {
            m_branchIndex.insert(ply, groups);

            // デバッグ
            for (auto g = groups.constBegin(); g != groups.constEnd(); ++g) {
                QStringList ls;
                for (const auto& c : g.value()) ls << c.text;
                qDebug().noquote() << "[BIDX] ply=" << ply
                                   << " base=" << (g.key().left(24) + "...")
                                   << " cand=" << ls;
            }
        }
    }

    qDebug().noquote() << "[BIDX] built ply keys =" << m_branchIndex.keys();

    // デバッグ出力などの後
    updateKifuBranchMarkersForActiveRow();
}

QGraphicsPathItem* MainWindow::branchNodeFor(int row, int ply) const
{
    const auto it = m_branchNodeIndex.constFind(qMakePair(row, ply));
    return (it == m_branchNodeIndex.constEnd()) ? nullptr : it.value();
}

void MainWindow::highlightBranchTreeAt(int row, int ply, bool centerOn /*=false*/)
{
    if (!m_branchTreeView || !m_branchTreeView->scene()) return;

    // まずは「その行のその手」を探す
    QGraphicsPathItem* it = branchNodeFor(row, ply);
    // 無ければ本譜同手へフォールバック（分岐行のプレフィクス部分など）
    if (!it && row != 0) it = branchNodeFor(/*main*/0, ply);
    // さらに無ければルートへ
    if (!it && ply == 0) it = branchNodeFor(/*row*/0, /*ply*/0);

    if (!it) return;

    // selectionChanged の中で applyResolvedRowAndSelect が呼ばれないようにガード
    m_branchTreeSelectGuard = true;
    auto* sc = m_branchTreeView->scene();
    sc->clearSelection();      // 既存選択解除 → selectionChanged が走る
    it->setSelected(true);     // ここで selectionChanged が走り、黄色枠の描画はされる
    if (centerOn) m_branchTreeView->centerOn(it);
    m_branchTreeSelectGuard = false;
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

// ====== デリゲートの取り付け（初回のみ） ======
void MainWindow::ensureBranchRowDelegateInstalled()
{
    if (!m_kifuView) return;
    if (!m_branchRowDelegate) {
        m_branchRowDelegate = new BranchRowDelegate(m_kifuView);
        // ビュー全体に適用（列単位にしたければ setItemDelegateForColumn(列) を使ってください）
        m_kifuView->setItemDelegate(m_branchRowDelegate);
    }
    m_branchRowDelegate->setMarkers(&m_branchablePlySet);
}

// ====== アクティブ行に対する「分岐あり手」の再計算 → ビュー再描画 ======
void MainWindow::updateKifuBranchMarkersForActiveRow()
{
    m_branchablePlySet.clear();

    if (m_resolvedRows.isEmpty()) {
        if (m_kifuView) m_kifuView->viewport()->update();
        return;
    }

    const int active = qBound(0, m_activeResolvedRow, m_resolvedRows.size()-1);
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

        if (itBase->size() >= 2) {
            m_branchablePlySet.insert(ply);      // 候補が2件以上 → 分岐点
        }
    }

    ensureBranchRowDelegateInstalled();

    if (m_kifuView) m_kifuView->viewport()->update();

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

// 平手、駒落ち Player1: Human, Player2: Human
void MainWindow::startHumanVsHumanGame()
{
    // 盤クリック受付（人対人）
    QObject::disconnect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handlePlayerVsEngineClick);
    QObject::disconnect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handleHumanVsHumanClick);
    QObject::connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handleHumanVsHumanClick);

    // 将棋クロックとUI手番の同期
    syncClockTurnAndEpoch();

    // 人間手番のストップウォッチを同時起動
    armTurnTimerIfNeeded();
}

// 平手 Player1: Human, Player2: USI Engine
// 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
void MainWindow::startHumanVsEngineGame()
{
    // 既存エンジン掃除＆作成（このモードでは m_usi1 を使用）
    if (m_usi1) { delete m_usi1; m_usi1 = nullptr; }
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode, this);

    // 初期化
    m_usi1->resetResignNotified();
    m_usi1->clearHardTimeout();
    if (m_usi2) { m_usi2->resetResignNotified(); m_usi2->clearHardTimeout(); }
    const bool engineIsP1 = (m_playMode == HandicapEngineVsHuman); // 駒落ち=先手エンジン
    wireResignToArbiter(m_usi1, engineIsP1);
    m_usi1->setLogIdentity(engineIsP1 ? "[E1]" : "[E2]",
                           engineIsP1 ? "P1"   : "P2",
                           m_startGameDialog->engineName1());
    m_usi1->setSquelchResignLogging(false);
    resetGameFlags();

    // 担当割り当て
    if (m_playMode == EvenHumanVsEngine)       initializeAndStartPlayer2WithEngine1(); // 後手エンジン
    else /*HandicapEngineVsHuman*/            initializeAndStartPlayer1WithEngine1(); // 先手エンジン

    // 初期 position（ponder も）
    m_positionStr1 = "position " + m_startPosStr + " moves";
    m_positionStrList.append(m_positionStr1);
    m_positionPonder1 = m_positionStr1;

    // クリックは PvE ハンドラへ
    QObject::disconnect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handleHumanVsHumanClick);
    QObject::disconnect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handlePlayerVsEngineClick);
    QObject::connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handlePlayerVsEngineClick);

    // 時計・UI同期＋エポック
    syncClockTurnAndEpoch();
    startMatchEpoch("Human vs Engine");

    // 初手が人間なら go 送らず整えるだけ
    const bool humanTurnNow =
        (m_gameController->currentPlayer() ==
            (engineIsP1 ? ShogiGameController::Player2 : ShogiGameController::Player1));
    if (humanTurnNow) {
        QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
        updateTurnAndTimekeepingDisplay();
        return;
    }

    // --- エンジン初手 ---
    QPoint from, to;
    if (!engineThinkApplyMove(m_usi1, m_positionStr1, m_positionPonder1, &from, &to)) {
        return; // 投了・旗落ち・エラー等
    }

    // ハイライト＆UI後処理
    updateHighlight(m_selectedField2, from, QColor(255, 0, 0, 50));
    updateHighlight(m_movedField, to, Qt::yellow);
    redrawEngine1EvaluationGraph();

    // 次は人間手番：描画後にストップウォッチをアーム
    m_shogiView->setMouseClickMode(true);
    QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
    qApp->processEvents();
}

// 平手 Player1: USI Engine（先手）, Player2: Human（後手）
// 駒落ち Player1: Human（下手）,  Player2: USI Engine（上手）
void MainWindow::startEngineVsHumanGame()
{
    // 既存エンジン掃除＆作成（このモードでも m_usi1 を使用）
    if (m_usi1) { delete m_usi1; m_usi1 = nullptr; }
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode, this);

    // 初期化
    m_usi1->resetResignNotified();
    m_usi1->clearHardTimeout();
    const bool engineIsP1 = (m_playMode == EvenEngineVsHuman); // 平手=先手エンジン
    wireResignToArbiter(m_usi1, engineIsP1);
    m_usi1->setLogIdentity(engineIsP1 ? "[E1]" : "[E2]",
                           engineIsP1 ? "P1"   : "P2",
                           m_startGameDialog->engineName1());
    m_usi1->setSquelchResignLogging(false);
    resetGameFlags();

    // 担当割り当て
    if (m_playMode == EvenEngineVsHuman)      initializeAndStartPlayer1WithEngine1(); // 先手エンジン
    else /*HandicapHumanVsEngine*/           initializeAndStartPlayer2WithEngine1(); // 後手エンジン

    // 初期 position
    m_positionStr1 = "position " + m_startPosStr + " moves";
    m_positionStrList.append(m_positionStr1);
    m_positionPonder1 = m_positionStr1;

    // クリックは PvE ハンドラへ
    QObject::disconnect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handleHumanVsHumanClick);
    QObject::disconnect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handlePlayerVsEngineClick);
    QObject::connect(m_shogiView, &ShogiView::clicked, this, &MainWindow::handlePlayerVsEngineClick);

    // 時計・UI同期＋エポック
    syncClockTurnAndEpoch();
    startMatchEpoch("Engine vs Human");

    // 初手が人間なら go 送らず整えるだけ
    const bool engineTurnNow =
        (m_gameController->currentPlayer() ==
            (engineIsP1 ? ShogiGameController::Player1 : ShogiGameController::Player2));
    if (!engineTurnNow) {
        QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
        updateTurnAndTimekeepingDisplay();
        return;
    }

    // --- エンジン初手 ---
    QPoint from, to;
    if (!engineThinkApplyMove(m_usi1, m_positionStr1, m_positionPonder1, &from, &to)) {
        return;
    }

    updateHighlight(m_selectedField, from, QColor(255, 0, 0, 50));
    updateHighlight(m_movedField, to, Qt::yellow);
    redrawEngine1EvaluationGraph();

    m_shogiView->setMouseClickMode(true);
    QTimer::singleShot(0, this, [this]{ armHumanTimerIfNeeded(); });
}

// 平手、駒落ち Player1: USI Engine, Player2: USI Engine
void MainWindow::startEngineVsEngineGame()
{
    // 既存エンジン掃除＆作成
    if (m_usi1) { delete m_usi1; m_usi1 = nullptr; }
    if (m_usi2) { delete m_usi2; m_usi2 = nullptr; }
    m_usi1 = new Usi(m_lineEditModel1, m_modelThinking1, m_gameController, m_playMode, this);
    m_usi2 = new Usi(m_lineEditModel2, m_modelThinking2, m_gameController, m_playMode, this);

    // 初期化
    m_usi1->resetResignNotified(); m_usi1->clearHardTimeout();
    m_usi2->resetResignNotified(); m_usi2->clearHardTimeout();
    wireResignToArbiter(m_usi1, /*asP1=*/true);
    wireResignToArbiter(m_usi2, /*asP1=*/false);
    m_usi1->setLogIdentity("[E1]", "P1", m_startGameDialog->engineName1());
    m_usi2->setLogIdentity("[E2]", "P2", m_startGameDialog->engineName2());
    m_usi1->setSquelchResignLogging(false);
    m_usi2->setSquelchResignLogging(false);
    resetGameFlags();

    // エンジン割り当て
    if (m_playMode == EvenEngineVsEngine) {
        initializeAndStartPlayer1WithEngine1();
        initializeAndStartPlayer2WithEngine2();
    } else { // HandicapEngineVsEngine
        initializeAndStartPlayer2WithEngine1();
        initializeAndStartPlayer1WithEngine2();
    }

    // 初期 position（双方の ponder を保持）
    m_positionStr1 = "position " + m_startPosStr + " moves";
    m_positionStrList.append(m_positionStr1);
    m_positionPonder1 = m_positionStr1;
    m_positionPonder2 = m_positionStr1;

    // 時計・UI同期＋エポック
    syncClockTurnAndEpoch();
    startMatchEpoch("Engine vs Engine");

    // 指し合いループ
    while (!m_gameIsOver) {
        // --- Engine1 の手 ---
        QPoint from1, to1;
        if (!engineThinkApplyMove(m_usi1, m_positionStr1, m_positionPonder1, &from1, &to1)) break;

        updateHighlight(m_selectedField, from1, QColor(255, 0, 0, 50));
        updateHighlight(m_movedField, to1, Qt::yellow);
        redrawEngine1EvaluationGraph();

        // 次手のヒント
        m_usi2->setPreviousFileTo(to1.x());
        m_usi2->setPreviousRankTo(to1.y());

        if (m_gameIsOver) break;

        // --- Engine2 の手 ---
        QPoint from2, to2;
        if (!engineThinkApplyMove(m_usi2, m_positionStr1, m_positionPonder2, &from2, &to2)) break;

        updateHighlight(m_selectedField, from2, QColor(255, 0, 0, 50));
        updateHighlight(m_movedField, to2, Qt::yellow);
        redrawEngine2EvaluationGraph();

        // 次手のヒント
        m_usi1->setPreviousFileTo(to2.x());
        m_usi1->setPreviousRankTo(to2.y());

        // UI 応答性の保険
        qApp->processEvents();
    }

    updateTurnAndTimekeepingDisplay();
}
