#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QSplitter>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QValueAxis>

#include "kifurecordlistmodel.h"
#include "shogienginethinkingmodel.h"
#include "kifuanalysislistmodel.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
//#include "ui_mainwindow.h"
#include "usi.h"
#include "startgamedialog.h"
#include "kifuanalysisdialog.h"
#include "playmode.h"
#include "shogimove.h"
#include "usicommlogmodel.h"
#include "considarationdialog.h"
#include "tsumeshogisearchdialog.h"
#include "shogiclock.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ShogiGameController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit MainWindow(QWidget* parent = 0);

protected:
     Ui::MainWindow *ui;

private slots:
    // エラーメッセージを表示する。
    void displayErrorMessage(const QString& message);

    // 棋譜ファイルをダイアログから選択し、そのファイルを開く。
    void chooseAndLoadKifuFile();

    // 人間対エンジンあるいはエンジン対人間の対局で、クリックされたマスに基づいて駒の移動を処理する。
    void handlePlayerVsEngineClick(const QPoint& field);

    // 人間対人間の対局で、クリックされたマスに基づいて駒の移動を処理する。
    void handleHumanVsHumanClick(const QPoint& field);

    // 局面編集モードで、クリックされたマスに基づいて駒の移動を処理する。
    void handleEditModeClick(const QPoint& field);

    // 局面編集モードで右クリックした駒を成る・不成の表示に変換する。
    void togglePiecePromotionOnClick(const QPoint& field);

    // 成る・不成の選択ダイアログを起動する。
    void displayPromotionDialog();

    // エンジン設定のダイアログを起動する。
    void displayEngineSettingsDialog();

    // GUIのバージョン情報を表示する。
    void displayVersionInformation();

    // Webサイトをブラウザで表示する。
    void openWebsiteInExternalBrowser();

    // 対局を開始する。
    void initializeGame();

    // 検討ダイアログを表示する。
    void displayConsiderationDialog();

    // 棋譜解析ダイアログを表示する。
    void displayKifuAnalysisDialog();

    // 将棋盤を180度回転させる。それに伴って、対局者名、残り時間も入れ替える。
    void flipBoardAndUpdatePlayerInfo();

    // 駒台を含む将棋盤全体の画像をクリップボードにコピーする。
    void copyBoardToClipboard();

    // 現局面から1手進んだ局面を表示する。
    void navigateToNextMove();

    // 現局面から1手戻った局面を表示する。
    void navigateToPreviousMove();

    // 現局面から10手進んだ局面を表示する。
    void navigateForwardTenMoves();

    // 現局面から10手戻った局面を表示する。
    void navigateBackwardTenMoves();

    // 最終局面を表示する。
    void navigateToLastMove();

    // 現局面から最初の局面を表示する。
    void navigateToFirstMove();

    // info行の予想手、探索手、エンジンの読み筋を縦ボックス化したm_widget3の表示・非表示を切り替える。
    void toggleEngineAnalysisVisibility();

    // 待ったボタンを押すと、2手戻る。
    void undoLastTwoMoves();

    // 将棋盤の画像をファイルとして出力する。
    void saveShogiBoardImage();

    // 設定ファイルにGUI全体のウィンドウサイズを書き込む。
    // また、将棋盤のマスサイズも書き込む。その後、GUIを終了する。
    void saveSettingsAndClose();

    // GUIを初期画面表示に戻す。
    void resetToInitialState();

    // 対局結果を表示する。
    void displayGameOutcome(ShogiGameController::Result);

    // メニューで「投了」をクリックした場合の処理を行う。
    void handleResignation();

    // 将棋エンジン2が"bestmove resign"コマンドで投了した場合の処理を行う。
    void handleEngineTwoResignation();

    // 将棋エンジン1が"bestmove resign"コマンドで投了した場合の処理を行う。
    void handleEngineOneResignation();

    // 将棋エンジン1に対して、gameover winコマンドとquitコマンドを送信する。
    void sendCommandsToEngineOne();

    // 将棋エンジン2に対して、gameover winコマンドとquitコマンドを送信する。
    void sendCommandsToEngineTwo();

    // 投了時の棋譜欄に表示する文字列を設定する。
    void setResignationMove(bool isPlayerOneResigning);

    // 将棋クロックの停止と将棋エンジンへの対局終了コマンド送信処理を行う。
    void stopClockAndSendCommands();

    // 対局結果の表示とGUIの更新処理を行う。
    void displayResultsAndUpdateGui();

    // 棋譜欄の指し手をクリックするとその局面に将棋盤を更新する。
    void updateBoardFromMoveHistory();

    // 棋譜欄の下の矢印ボタンを無効にする。
    void disableArrowButtons();

    // 棋譜欄の下の矢印ボタンを有効にする。
    void enableArrowButtons();

    // 局面編集を開始する。
    void beginPositionEditing();

    // 局面編集を終了した場合の処理を行う。
    void finishPositionEditing();

    // 「全ての駒を駒台へ」をクリックした時に実行される。
    // 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
    void resetPiecesToStand();

    // 手番を変更する。
    void switchTurns();

    // 平手初期局面に盤面を初期化する。
    void setStandardStartPosition();

    // 詰将棋の初期局面に盤面を初期化する。
    void setTsumeShogiStartPosition();

    // 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
    void swapBoardSides();

    // 盤面のサイズを拡大する。
    void enlargeBoard();

    // 盤面のサイズを縮小する。
    void reduceBoardSize();

    // 棋譜をファイルに保存する。
    void saveKifuToFile();

    // 棋譜をファイルに上書き保存する。
    void overwriteKifuFile();

    // GUIのエンジン名欄1を変更する。
    void updateEngine1NameDisplay();

    // GUIの予想手欄1を変更する。
    void updateEngine1PredictedMoveDisplay();

    // GUIの探索手欄1を変更する。
    void updateEngine1SearchedMoveDisplay();

    // GUIの探索深さ欄1を変更する。
    void updateEngine1DepthDisplay();

    // GUIのノード数欄1を変更する。
    void updateEngine1NodesCountDisplay();

    // GUIの局面探索数欄1を変更する。
    void updateEngine1NpsDisplay();

    // GUIのハッシュ使用率欄1を変更する。
    void updateEngine1HashUsageDisplay();

    // GUIのエンジン名欄2を変更する。
    void updateEngine2NameDisplay();

    // GUIの予想手欄2を変更する。
    void updateEngine2PredictedMoveDisplay();

    // GUIの探索手欄2を変更する。
    void updateEngine2SearchedMoveDisplay();

    // GUIの探索深さ欄2を変更する。
    void updateEngine2DepthDisplay();

    // GUIのノード数欄2を変更する。
    void updateEngine2NodesCountDisplay();

    // GUIの局面探索数欄2を変更する。
    void updateEngine2NpsDisplay();

    // GUIのハッシュ使用率欄2を変更する。
    void updateEngine2HashUsageDisplay();

    // gameover resignコマンドを受信した場合の終了処理を行う。
    void processResignCommand();

    // 対局者1の残り時間の文字色を赤色に指定する。
    void setPlayer1TimeTextToRed();

    // 対局者2の残り時間の文字色を赤色に指定する。
    void setPlayer2TimeTextToRed();

    // 将棋盤上での左クリックイベントをハンドリングする。
    void onShogiViewClicked(const QPoint &pt);

    // 将棋盤上での右クリックイベントをハンドリングする。
    void onShogiViewRightClicked(const QPoint &);

    // ドラッグを終了する。駒を移動してカーソルを戻す。
    void endDrag();

private:
    // 平手初期局面のSFEN文字列
    QString m_startSfenStr;

    // 現在の局面のSFEN文字列
    QString m_currentSfenStr;

    // エラーが発生したかどうかを示すフラグ
    bool m_errorOccurred;

    // 先手あるいは下手の残り時間
    QString m_bTime;

    // 後手あるいは上手の残り時間
    QString m_wTime;

    // 持ち時間の制限があるかどうかを示すフラグ
    bool m_isLoseOnTimeout;

    // 現在の手数
    int m_currentMoveIndex;

    // 棋譜欄最後の指し手
    QString m_lastMove;

    // USIプロトコル通信により、将棋エンジン1と通信を行うためのオブジェクト
    Usi* m_usi1;

    // USIプロトコル通信により、将棋エンジン2と通信を行うためのオブジェクト
    Usi* m_usi2;

    // エンジン1のpositionコマンド文字列
    QString m_positionStr1;

    // エンジン2のpositionコマンド文字列
    QString m_positionStr2;

    // positionコマンド文字列のリスト
    QStringList m_positionStrList;

    // ponderを受信した後のエンジン1のpositionコマンド文字列
    QString m_positionPonder1;

    // 対局モード
    PlayMode m_playMode;

    // エンジン1の実行ファイル名
    QString m_engineFile1;

    // エンジン2の実行ファイル名
    QString m_engineFile2;

    // 対局者1の秒読み時間（単位はミリ秒）
    int m_byoyomiMilliSec1;

    // 対局者2の秒読み時間（単位はミリ秒）
    int m_byoyomiMilliSec2;

    // 対局者1の1手ごとの時間加算（単位はミリ秒）
    int m_addEachMoveMiliSec1;

    // 対局者2の1手ごとの時間加算（単位はミリ秒）
    int m_addEachMoveMiliSec2;

    // 秒読みを行うかどうかのフラグ
    bool m_useByoyomi;

    // 全手数
    int m_totalMove;

    // 将棋盤に関するビュー
    ShogiView* m_shogiView;

    // 将棋の対局全体を管理し、盤面の初期化、指し手の処理、合法手の検証、対局状態の管理を行うオブジェクト
    ShogiGameController* m_gameController;

    // クリックされたマスポイント
    QPoint m_clickPoint;

    // 移動元のマスのハイライト
    ShogiView::FieldHighlight* m_selectedField = nullptr;

    // 移動元のマスのハイライト2
    ShogiView::FieldHighlight* m_selectedField2;

    // 移動先のマスのハイライト
    ShogiView::FieldHighlight* m_movedField;

    // 対局初期状態のpositionコマンド文字列
    QString m_startPosStr;

    // 対局ダイアログ
    StartGameDialog* m_startGameDialog;

    // 検討ダイアログ
    ConsidarationDialog* m_considarationDialog;

    // 検討ダイアログ
    TsumeShogiSearchDialog* m_tsumeShogiSearchDialog;

    // 棋譜解析ダイアログ
    KifuAnalysisDialog* m_analyzeGameRecordDialog;

    // 棋譜欄を表示するクラス
    KifuRecordListModel* m_gameRecordModel;

    // エンジン1の思考結果をGUI上で表示するためのクラスのインスタンス
    ShogiEngineThinkingModel* m_modelThinking1;

    // エンジン2の思考結果をGUI上で表示するためのクラスのインスタンス
    ShogiEngineThinkingModel* m_modelThinking2;

    // 棋譜解析結果をGUI上で表示するためのクラス
    KifuAnalysisListModel* m_analysisModel = nullptr;

    // エンジンのUSIプロトコル通信ログ表示欄
    QPlainTextEdit* m_usiCommLogEdit;

    // GUIの思考1タブ
    QTabWidget* m_tab1;

    // GUIの思考2タブ
    QTabWidget* m_tab2;

    // GUIの思考1ビュー
    QTableView* m_usiView1;

    // GUIの思考2ビュー
    QTableView* m_usiView2;

    // GUIの棋譜ビュー
    QTableView* m_gameRecordView;

    // GUIの棋譜解析結果ビュー
    QTableView* m_analysisResultsView;

    // エンジン1の評価値チャートのデータ
    QLineSeries* m_series1;

    // エンジン2の評価値チャートのデータ
    QLineSeries* m_series2;

    // 評価値チャート
    QChart* m_chart;

    // 評価値チャートビュー
    QChartView* m_chartView;

    // 6つの矢印ボタン用のウィジェット
    QWidget* m_arrows;

    // 棋譜を前後に進める6つの矢印ボタン
    QPushButton* m_playButton1;
    QPushButton* m_playButton2;
    QPushButton* m_playButton3;
    QPushButton* m_playButton4;
    QPushButton* m_playButton5;
    QPushButton* m_playButton6;

    // 予想手1のラベル欄
    QLabel* m_predictiveHandLabel1;

    // 予想手2のラベル欄
    QLabel* m_predictiveHandLabel2;

    // 探索手1のラベル欄
    QLabel* m_searchedHandLabel1;

    // 探索手2のラベル欄
    QLabel* m_searchedHandLabel2;

    // 探索深さ1のラベル欄
    QLabel* m_depthLabel1;

    // 探索深さ2のラベル欄
    QLabel* m_depthLabel2;

    // ノード数1のラベル欄
    QLabel* m_nodesLabel1;

    // ノード数2のラベル欄
    QLabel* m_nodesLabel2;

    // 探索局面数1のラベル欄
    QLabel* m_npsLabel1;

    // 探索局面数2のラベル欄
    QLabel* m_npsLabel2;

    // ハッシュ使用率1のラベル欄
    QLabel* m_hashfullLabel1;

    // ハッシュ使用率2のラベル欄
    QLabel* m_hashfullLabel2;

    // エンジン名1、予想手1、探索手1、深さ1、ノード数1、局面探索数1、ハッシュ使用率1を横ボックス化したウィジェット
    QWidget* m_infoWidget1;

    // エンジン名2、予想手2、探索手2、深さ2、ノード数2、局面探索数2、ハッシュ使用率2を横ボックス化したウィジェット
    QWidget* m_infoWidget2;

    // エンジン名1、予想手1、探索手1、深さ1、ノード数1、局面探索数1、ハッシュ使用率1の更新に関するオブジェクト
    UsiCommLogModel* m_lineEditModel1;

    // エンジン名2、予想手2、探索手2、深さ2、ノード数2、局面探索数2、ハッシュ使用率2の更新に関するオブジェクト
    UsiCommLogModel* m_lineEditModel2;

    // エンジン名1の表示欄
    QLineEdit* m_engineNameText1;

    // エンジン名2の表示欄
    QLineEdit* m_engineNameText2;

    // 棋譜、矢印ボタン、評価値グラフを縦ボックス化したウィジェット
    QWidget* m_gameRecordLayoutWidget;

    // エンジン名2、予想手2、探索手2、深さ2、ノード数2、局面探索数2、ハッシュ使用率2、思考2タブを縦ボックス化したウィジェット
    QWidget* m_engine2InfoLayoutWidget;

    // // 対局者名と残り時間、将棋盤を縦ボックス化したウィジェット
    QWidget* m_playerAndBoardLayoutWidget;

    // 対局者名と残り時間、将棋盤と棋譜、矢印ボタン、評価値グラフのグループを横に並べて表示する。
    // リサイズできる複数のセクションにエリアを分割するために使用されるウィジェット。
    QSplitter* m_hsplit;

    // 予想手1の表示欄
    QLineEdit* m_predictiveHandText1;

    // 予想手2の表示欄
    QLineEdit* m_predictiveHandText2;

    // 探索手1の表示欄
    QLineEdit* m_searchedHandText1;

    // 探索手2の表示欄
    QLineEdit* m_searchedHandText2;

    // 探索深さ1の表示欄
    QLineEdit* m_depthText1;

    // 探索深さ2の表示欄
    QLineEdit* m_depthText2;

    // ノード数1の表示欄
    QLineEdit* m_nodesText1;

    // ノード数2の表示欄
    QLineEdit* m_nodesText2;

    // 局面探索数1の表示欄
    QLineEdit* m_npsText1;

    // 局面探索数2の表示欄
    QLineEdit* m_npsText2;

    // ハッシュ使用率1の表示欄
    QLineEdit* m_hashfullText1;

    // ハッシュ使用率2の表示欄
    QLineEdit* m_hashfullText2;

    // 評価値チャートのスクロールエリア
    QScrollArea* m_scrollArea;

    // エンジンによる現在の評価値リスト
    QList<int> m_scoreCp;

    // 対局者が人間の場合の対局者名1
    QString m_humanName1;

    // 対局者が人間の場合の対局者名2
    QString m_humanName2;

    // 対局者がエンジンの場合の対局者名1
    QString m_engineName1;

    // 対局者がエンジンの場合の対局者名2
    QString m_engineName2;

    // 評価値チャートの横軸「手数」
    QValueAxis* m_axisX;

    // 評価値チャートの縦軸「評価値」
    QValueAxis* m_axisY;

    // SFEN文字列のリスト
    QStringList* m_sfenRecord;

    // 棋譜データのリスト
    QList<KifuDisplay *>* m_moveRecords;

    // 対局数
    int m_gameCount;

    // KIF形式の棋譜データリスト
    QStringList m_kifuDataList;

    // 棋譜のデフォルト保存ファイル名
    QString defaultSaveFileName;

    // 棋譜の保存ファイル名
    QString kifuSaveFileName;

    // 指し手のリスト
    QVector<ShogiMove> m_gameMoves;

    // 将棋クロック
    ShogiClock* m_shogiClock;

    // 2回目のクリックを待っているかどうかを示すフラグ
    bool m_waitingSecondClick;

    // 1回目のクリック位置
    QPoint m_firstClick;

    // 将棋盤、駒台を初期化（何も駒がない）し、入力のSFEN文字列の配置に将棋盤、駒台の駒を
    // 配置し、対局結果を結果なし、現在の手番がどちらでもない状態に設定する。
    void startNewShogiGame(QString& startSfenStr);

    // 棋譜を更新し、GUIの表示も同時に更新する。
    void updateGameRecord(const QString& elapsedTime);

    // 対局ダイアログの設定を出力する。
    void printGameDialogSettings(StartGameDialog* m_gamedlg);

    // 待ったをした場合、position文字列のセットと評価値グラフの値を削除する。
    void handleUndoMove(int index);

    // 将棋盤上部の対局者名欄、手数欄を作成し、パックしてウィジェットにまとめる。
    void createPlayerNameAndMoveDisplay();

    // 対局モードに応じて将棋盤上部に表示される対局者名をセットする。
    void setPlayersNamesForMode();

    // 将棋盤上部の残り時間と手番を示す赤丸を設定する。
    void setupTimeAndTurnIndicators();

    // 将棋盤と駒台を描画する。
    void renderShogiBoard();

    // 棋譜欄の表示を設定する。
    void setupGameRecordView();

    // 棋譜解析結果を表示するためのテーブルビューを設定および表示する。
    void displayAnalysisResults();

    // 棋譜欄下の矢印ボタンを作成する。
    void setupArrowButtons();

    // 評価値グラフを表示する。
    void createEvaluationChartView();

    // エンジン1の予想手、探索手などの情報を表示する。
    void initializeEngine1InfoDisplay();

    // エンジン2の予想手、探索手などの情報を表示する。
    void initializeEngine2InfoDisplay();

    // エンジン1の思考タブを作成する。
    void initializeEngine1ThoughtTab();

    // エンジン2の思考タブを作成する。
    void initializeEngine2ThoughtTab();

    // 新規対局の準備をする。
    void initializeNewGame(QString& startSfenStr);

    // "sfen 〜"で始まる文字列startpositionstrを入力して"sfen "を削除したSFEN文字列を返す。
    QString parseStartPositionToSfen(QString startPositionStr);

    // 平手、駒落ち Player1: Human, Player2: Human
    void startHumanVsHumanGame();

    // 平手 Player1: Human, Player2: USI Engine
    // 駒落ち Player1: USI Engine（下手）, Player2: Human（上手）
    void startHumanVsEngineGame();

    // 平手 Player1: USI Engine（先手）, Player2: Human（後手）
    // 駒落ち Player1: Human（下手）, Player2: USI Engine（上手）
    void startEngineVsHumanGame();

    // 平手、駒落ち Player1: USI Engine, Player2: USI Engine
    void startEngineVsEngineGame();

    // 検討を開始する。
    void startConsidaration();

    // 棋譜解析を開始する。
    void analyzeGameRecord();

    // 対局モード（人間対エンジンなど）に応じて対局処理を開始する。
    void startGameBasedOnMode();

    // エンジン1の評価値グラフの再描画を行う。
    void redrawEngine1EvaluationGraph();

    // エンジン2の評価値グラフの再描画を行う。
    void redrawEngine2EvaluationGraph();

    // 駒の移動元と移動先のマスのハイライトを消去する。
    void clearMoveHighlights();

    // 駒の移動元と移動先のマスをそれぞれ別の色でハイライトする。
    void addMoveHighlights();

    // GUIの残り時間を更新する。
    void updateRemainingTimeDisplay();

    // 対局のメニュー表示を一部隠す。
    void hideGameActions();

    // 対局中のメニュー表示に変更する。
    void setGameInProgressActions();

    // GUIを構成するWidgetなどを生成する。
    void initializeComponents();

    // 棋譜、矢印ボタン、評価値グラフを縦ボックス化する。
    void setupRecordAndEvaluationLayout();

    // 対局者名と残り時間、将棋盤と棋譜、矢印ボタン、評価値グラフのグループを横に並べて表示する。
    void setupHorizontalGameLayout();

    // info行の予想手、探索手、エンジンの読み筋を縦ボックス化する。
    void setupVerticalEngineInfoDisplay();

    // 対局者名と残り時間、将棋盤、棋譜、矢印ボタン、評価値グラフのウィジェットと
    // info行の予想手、探索手、エンジンの読み筋のウィジェットを縦ボックス化して
    // セントラルウィジェットにセットする。
    void initializeCentralGameDisplay();

    // 対局モードに応じて将棋盤下部に表示されるエンジン名をセットする。
    void setEngineNamesBasedOnMode();

    // 設定ファイルにGUI全体のウィンドウサイズを書き込む。
    // また、将棋盤のマスサイズも書き込む。
    void saveWindowAndBoardSettings();

    // ShogibanQ全体のウィンドウサイズを読み込む。
    // 前回起動したウィンドウサイズに設定する。
    void loadWindowSettings();

    // ウィンドウを閉じる処理を行う。
    void closeEvent(QCloseEvent* event);

    // 局面編集中のメニュー表示に変更する。
    void displayPositionEditMenu();

    // 局面編集メニュー後のメニュー表示に変更する。
    void hidePositionEditMenu();

    // 棋譜ファイルのヘッダー部分を作成する。
    void makeKifuFileHeader();

    // 棋譜ファイルのヘッダー部分の対局者名を作成する。
    void getPlayersName(QString& playersName1, QString& playersName2);

    // 保存ファイルのデフォルト名を作成する。
    void makeDefaultSaveFileName();

    // 棋譜欄に「指し手」と「消費時間」のデータを表示させる。
    void displayGameRecord(QStringList& kifuMove, QStringList& kifuMoveConsumptionTime, const QString& handicapType);

    // 対局者の残り時間と秒読みを設定する。
    void setRemainingTimeAndCountDown();

    // 対局ダイアログから各オプションを取得する。
    void getOptionFromStartGameDialog();

    // 現在の局面で開始する場合に必要なListデータなどを用意する。
    void prepareDataCurrentPosition();

    // 初期局面からの対局する場合の準備を行う。
    void prepareInitialPosition();

    // 残り時間をセットしてタイマーを開始する。
    void setTimerAndStart();

    // 現在の手番を設定する。
    void setCurrentTurn();

    // 対局モードを決定する。
    PlayMode determinePlayMode(const int initPositionNumber, const bool isPlayer1Human, const bool isPlayer2Human) const;

    // 対局モードを決定する。
    PlayMode setPlayMode();

    // 指す駒を左クリックで選択した場合、そのマスをオレンジ色にする。
    void selectPieceAndHighlight(const QPoint& field);

    // 将棋のGUIでクリックされたポイントに基づいて駒の移動を処理する。
    // 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
    void handleClickForPlayerVsEngine(const QPoint& field);

    // 人間対人間の対局で、クリックされたポイントに基づいて駒の移動を処理する。
    // 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
    void handleClickForHumanVsHuman(const QPoint& field);

    // 局面編集モードで、クリックされたポイントに基づいて駒の移動を処理する。
    // 対局者が将棋盤上で駒をクリックして移動させる際の一連の処理を担当する。
    void handleClickForEditMode(const QPoint &field);

    // ユーザーのクリックをリセットし、選択したハイライトを削除する。
    void resetSelectionAndHighlight();

    // 既存のフィールドハイライトを更新し、新しいフィールドハイライトを作成してビューに追加する。
    void updateHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& field, const QColor& color);

    // 既存のハイライトをクリアし（nullでないことが前提）、新しいハイライトを作成して表示する。
    void clearAndSetNewHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& to, const QColor& color);

    // 指定されたハイライトを削除する。
    void deleteHighlight(ShogiView::FieldHighlight*& highlightField);

    // 新しいハイライトを作成し、それを表示する。
    void addNewHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& position, const QColor& color);

    // 棋譜を更新し、GUIの表示も同時に更新する。
    void updateGameRecordAndGUI();

    // 「すぐ指させる」
    // エンジンにstopコマンドを送る。
    // エンジンに対し思考停止を命令するコマンド。エンジンはstopを受信したら、できるだけすぐ思考を中断し、
    // bestmoveで指し手を返す。
    void movePieceImmediately();

    // KIF形式の棋譜ファイルを読み込む。
    // 参考．http://kakinoki.o.oo7.jp/kif_format.html
    void loadKifuFromFile(const QString &filePath);

    // positionコマンド文字列を生成し、リストに格納する。
    void createPositionCommands();

    // ファイルからテキストの行を読み込み、QStringListとして返す。
    QStringList loadTextLinesFromFile(const QString& filePath);

    // 対局者2をエンジン1で初期化して対局を開始する。
    void initializeAndStartPlayer2WithEngine1();

    // 対局者1をエンジン1で初期化して対局を開始する。
    void initializeAndStartPlayer1WithEngine1();

    // 対局者2をエンジン2で初期化して対局を開始する。
    void initializeAndStartPlayer2WithEngine2();

    // 対局者1をエンジン2で初期化して対局を開始する。
    void initializeAndStartPlayer1WithEngine2();

    // 手番に応じて将棋クロックの手番変更およびGUIの手番表示を更新する。
    void updateTurnAndTimekeepingDisplay();

    // GUIの残り時間を更新する。
    void updateRemainingTimeDisplayHumanVsHuman();

    // 手番に応じて将棋クロックの手番変更する。
    void updateTurnDisplay();

    // 将棋クロックの手番を設定する。
    void updateTurnStatus(int currentPlayer);

    // 対局者1と対局者2の持ち時間を設定する。
    void setPlayerTimeLimits();

    // 詰み探索ダイアログを表示する。
    void displayTsumeShogiSearchDialog();

    // 詰み探索を開始する。
    void startTsumiSearch();

    // ドラッグ操作のリセットを行う。
    void finalizeDrag();

    QElapsedTimer m_humanTurnTimer;
    bool m_humanTimerArmed = false;

    ShogiGameController::Player humanPlayerSide() const;
    bool isHumanTurn() const;
    void armHumanTimerIfNeeded();         // 人間手番になったら一度だけ start
    void finishHumanTimerAndSetConsideration(); // 合法手確定時に ShogiClock へ反映

    QElapsedTimer m_turnTimer;     // 人間用：手番スタート→合法手確定まで
    bool          m_turnTimerArmed = false;

    void armTurnTimerIfNeeded(); // 現在手番に対して未アームなら start
    void finishTurnTimerAndSetConsiderationFor(ShogiGameController::Player mover); // mover に加算

    void disarmHumanTimerIfNeeded();

    QTimer* m_uiClockTimer = nullptr;
    void refreshClockLabels();
    static QString formatMs(int ms);

    // MainWindow のメンバに追加
    bool m_p1HasMoved = false;
    bool m_p2HasMoved = false;

    void computeGoTimesForUSI(qint64& outB, qint64& outW);
    void refreshGoTimes();
};

#endif // MAINWINDOW_H
