#ifndef USI_H
#define USI_H

#include <QPlainTextEdit>
#include <QProcess>
#include <QTableView>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QThread>
#include <QElapsedTimer>

#include "shogienginethinkingmodel.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "usicommlogmodel.h"
#include "playmode.h"

// UsiThreadクラスを前方宣言する。
class UsiThread;

class Usi : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit Usi(UsiCommLogModel* model,  ShogiEngineThinkingModel* modelThinking, ShogiGameController* algorithm, PlayMode& playMode, QObject* parent = 0);

    // デストラクタ
    ~Usi();

    // 評価値の文字列を返す。
    QString scoreStr() const;

    // GUIがbestmove resignを受信したかどうかのフラグを返す。
    bool isResignMove() const;

    // 一番最後に受信した指し手の評価値を返す。
    int lastScoreCp() const;

    // 漢字の指し手に変換したpv文字列を返す。
    QString pvKanjiStr() const;

    // 漢字の指し手に変換したpv文字列を設定する。
    void setPvKanjiStr(const QString& newPvKanjiStr);

    // 先手が持ち駒を打つときの駒を文字列に変換する。
    QString convertFirstPlayerPieceNumberToSymbol(const int rankFrom) const;

    // 後手が持ち駒を打つときの駒を文字列に変換する。
    QString convertSecondPlayerPieceNumberToSymbol(const int rankFrom) const;

    // 将棋エンジンがbestmove文字列で返した最善手から移動元の筋と段、移動先の筋と段を取得する。
    void parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo);

    // 将棋エンジンプロセスを起動し、対局を開始するUSIコマンドを送受信する。
    void initializeAndStartEngineCommunication(QString& engineFile, QString& enginename);

    // 人間対将棋エンジンの対局で将棋エンジンとUSIプロトコル通信を行う。
    void handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr, QPoint& outFrom, QPoint& outTo,
                                          int byoyomiMilliSec, const QString& btime, const QString& wtime, QStringList& positionStrList,
                                          int addEachMoveMiliSec1, int addEachMoveMiliSec2, bool useByoyomi);

    // 将棋エンジン対人間および将棋エンジン同士の対局で将棋エンジンとUSIプロトコル通信を行う。
    void handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr, QString& positionPonderStr, QPoint& outFrom, QPoint& outTo,
                                                       int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                                       int addEachMoveMiliSec1, int addEachMoveMiliSec2, bool useByoyomi);

    // gameoverコマンドを将棋エンジンに送信する。
    void sendGameOverCommand(const QString& result);

    // quitコマンドを将棋エンジンに送信する。
    void sendQuitCommand();

    // 将棋の段（1～9）をアルファベット表記（a～i）に変換する。
    QChar rankToAlphabet(const int rank) const;

    // GUIで「投了」をクリックした場合の処理を行う。
    void sendGameOverWinAndQuitCommands();

    // stopコマンドを将棋エンジンに送信する。
    void sendStopCommand();

    // GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待つ。
    void waitForStopOrPonderhitCommand();

    void executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec);

    // 1手前に指した移動先の筋を設定する。
    void setPreviousFileTo(int newPreviousFileTo);

    // 1手前に指した移動先の段を設定する。
    void setPreviousRankTo(int newPreviousRankTo);

    qint64 lastBestmoveElapsedMs() const { return m_lastGoToBestmoveMs; }

    // エンジンにgameover loseコマンドとquitコマンドを送信し、手番を変更する。
    void sendGameOverLoseAndQuitCommands();

    // ログ識別子の設定（GUI 生成側で E1/E2, P1/P2, Engine名 を渡す）
    void setLogIdentity(const QString& engineTag, const QString& sideTag, const QString& engineName = QString());

    // タイムアウト確定後に、当該エンジンが出す "bestmove resign" を黙殺する
    void setSquelchResignLogging(bool on);

    void resetResignNotified() { m_resignNotified = false; }
    void markHardTimeout()     { m_timeoutDeclared = true; }
    void clearHardTimeout()    { m_timeoutDeclared = false; }
    bool isIgnoring() const    { return m_shutdownState != ShutdownState::Running; }

    // goコマンドを将棋エンジンに送信する。
    void sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                       int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    // 任意の USI コマンドをそのまま送る（ログ出力は sendCommand 側で実施）
    void sendRaw(const QString& command) const;

public:
    // ★ 追加：後からThinking/Logモデルを挿し直すためのsetter
    void setThinkingModel(ShogiEngineThinkingModel* m) { m_modelThinking = m; }
    void setLogModel(UsiCommLogModel* m) { m_model = m; }

    // 将棋エンジンのプロセスを終了し、プロセスとスレッドを削除する。
    void cleanupEngineProcessAndThread();

#ifdef QT_DEBUG
    // ★ 追加：デバッグ用ダンプ
    ShogiEngineThinkingModel* debugThinkingModel() const { return m_modelThinking; }
    UsiCommLogModel*          debugLogModel()      const { return m_model; }
#endif

signals:
    // stopあるいはponderhitコマンドが送信されたことを通知するシグナル
    void stopOrPonderhitCommandSent();

    // bestmove resignを受信したことを通知するシグナル
    void bestMoveResignReceived();

private:
    // 盤面の1辺のマス数（列数と段数）
    static constexpr int BOARD_SIZE = 9;

    // 将棋盤のマス数
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

    // 一番最後に受信した指し手の評価値
    int m_lastScoreCp;

    // 漢字の指し手に変換したpv文字列
    QString m_pvKanjiStr;

    // 評価値の文字列
    QString m_scoreStr;

    // 棋譜解析モードかどうかのフラグ
    bool m_analysisMode;

    // 1手前の移動先の段
    int m_previousFileTo;

    // 1手前の移動先の筋
    int m_previousRankTo;

    // 将棋エンジンの指し手
    QString m_bestMove;

    // 将棋エンジンが予想した相手の指し手
    QString m_predictedOpponentMove;

    // 盤面のコピーデータ
    QVector<QChar> m_clonedBoardData;

    // 将棋エンジンのプロセス
    QProcess* m_process;

    // 将棋エンジンからの受信データを1行ずつ保存するリスト
    QStringList m_lines;

    // 3桁ごとのカンマ区切り表示に使用するロケール
    QLocale m_locale;

    // GUIの「USIプロトコル通信ログ」タブのテキスト欄
    QPlainTextEdit* m_txt;

    // usiokを受信したかどうかのフラグ
    bool m_usiOkSignalReceived;

    // readyokを受信したかどうかのフラグ
    bool m_readyOkSignalReceived;

    // infoを受信したかどうかのフラグ
    bool m_infoSignalReceived;

    // bestmoveを受信したかどうかのフラグ
    bool m_bestMoveSignalReceived;

    // 設定ファイルから読み込んだ将棋エンジンオプションのリスト
    QStringList m_setOptionCommandList;

    // エンジン名、予想手、探索手、深さ、ノード数、局面探索数、ハッシュ使用率の更新に関するクラス
    UsiCommLogModel* m_model;

    // 将棋エンジンの思考結果をGUI上で表示するためのクラス
    ShogiEngineThinkingModel* m_modelThinking;

    // 将棋の対局全体を管理し、盤面の初期化、指し手の処理、合法手の検証、対局状態の管理を行うクラス
    ShogiGameController* m_gameController;

    // 対局モード
    PlayMode& m_playMode;

    // USIプロトコルに基づく通信を処理するための別スレッド
    // GUIのメインスレッドとは別のスレッドでgo ponderコマンド受信後の将棋エンジンの処理を行う。
    // GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待ち、GUIの思考欄に読み筋を出力する。
    UsiThread* m_usiThread;

    // ponderモードが有効かどうかのフラグ
    bool m_isPonderEnabled;

    // GUIがbestmove resignを受信したかどうかのフラグ
    bool m_isResignMove;

    // GUIがbestmove winを受信したかどうかのフラグ
    bool m_isWinMove;

    // 将棋エンジンからデータを受信して保管した行リストをクリアする。
    void clearResponseData();

    // 将棋エンジンにコマンドを送信する。
    void sendCommand(const QString& command) const;

    // 将棋エンジンを起動する。
    void startEngine(const QString& engineFile);

    // usinewgameコマンドを将棋エンジンに送信する。
    void sendUsiNewGameCommand();

    // positionコマンドを将棋エンジンに送信する。
    void sendPositionCommand(QString& positionStr);

    // 設定ファイルShogibanQ.iniから将棋エンジンオプションを読み込み、
    // setoptionコマンドの文字列を生成する。
    void generateSetOptionCommands(const QString& engineName);

    // setoptionコマンドを将棋エンジンに送信する。
    void sendSetOptionCommands();

    // info行を全て削除する。
    void infoRecordClear();

    // go ponderコマンドを将棋エンジンに送信する。
    void sendGoPonderCommand();

    // 将棋のアルファベット表記（a～i）を段（1～9）に変換する。
    int alphabetToRank(QChar c);

    // 白（後手）の駒を表すアルファベットから持ち駒の段に変換する。
    int pieceToRankWhite(QChar c);

    // 黒（先手）の駒を表すアルファベットから持ち駒の段に変換する。
    int pieceToRankBlack(QChar c);

    // ponderhitコマンドを将棋エンジンに送信する。
    void sendPonderHitCommand();

    // info行を解析する。
    int parseEngineOutputAndUpdateInfo(QString& line, ShogiEngineInfoParser& info);

    // GUIの「探索手」欄を更新する。
    void updateSearchedHand(const ShogiEngineInfoParser* info);

    // GUIの「深さ」欄を更新する。
    void updateDepth(const ShogiEngineInfoParser* info);

    // GUIの「ノード数」欄を更新する。
    void updateNodes(const ShogiEngineInfoParser* info);

    // GUIの「ノード数」欄を更新する。
    void updateNps(const ShogiEngineInfoParser* info);

    // GUIの「ハッシュ使用率」欄を更新する。
    void updateHashfull(const ShogiEngineInfoParser* info);

    // 評価値を計算する。
    int calculateScoreInt(const ShogiEngineInfoParser* info) const;

    // 詰み手数（scoremate）と最終評価値（lastScore）の更新を行う。
    void updateScoreMateAndLastScore(ShogiEngineInfoParser* info, int& scoreint);

    // 棋譜解析モードにより、評価値の処理を行う。
    void updateAnalysisModeAndScore(const ShogiEngineInfoParser* info, int& scoreint);

    // 入力された評価値（scoreint）を範囲内（-2000〜2000）に制限して、その値をm_lastScorecpに設定する。
    void updateLastScore(const int scoreInt);

    // 現在の評価値（scorecp）が存在するかどうかに基づき、詰み手数（scoremate）と最終評価値（lastScore）を更新する。
    // scorecpが存在する場合は、棋譜解析モードに基づいた評価値の処理を行い、その他の情報（時間、深さ、ノード数、評価値、読み筋）をセットする。
    // さらに、評価値を範囲内（-2000〜2000）に制限した上で、最終評価値を更新する。
    void updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreint);

    // usiコマンドを将棋エンジンに送信する。
    void sendUsiCommand();

    // 将棋エンジンからusiokを受信するまで待つ。
    bool waitForUsiOK(const int timeoutMilliseconds);

    // isreadyコマンドを将棋エンジンに送信する。
    void sendIsReadyCommand();

    // 将棋エンジンからreadyokを受信するまで待つ。
    bool waitForReadyOk(const int timeoutMilliseconds);

    // 将棋エンジンからbestmoveを受信するまで待つ。
    bool waitForBestMove(const int timeoutMilliseconds);

     // 将棋エンジンからbestmoveを受信するまで待ち続ける。
    bool keepWaitingForBestMove();

    // 新しいスレッドを生成し、GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待つ。
    // start関数が実行されることでrun関数が実行され、その中でwaitForStopOrPonderhitCommand関数が実行される。
    void startUsiThread();

    // 現在の局面（盤内のみ）をコピーする。
    void cloneCurrentBoardData();

    // 将棋エンジンから返された「bestmove A ponder B」の情報をもとに、将棋盤内の駒配置を更新する。
    void applyMovesToBoardFromBestMoveAndPonder();

    // bestmoveを指定した時間内に受信するまで待機する。
    // 指定した時間内に受信できなかった場合、エラーメッセージを表示して例外をスローする。
    // @param byoyomiMilliSec bestmoveを待機する時間（ミリ秒単位）
    // @return true: bestmoveを受信した場合, false: タイムアウトした場合
    void waitAndCheckForBestMove(const int byoyomiMilliSec);

    // 将棋エンジンからの「bestmove」受信後に予想手を考慮した以下の処理を開始する。
    // 1. 「bestmove」と「ponder」の情報をもとに駒配置を更新。
    // 2. 予想される相手の指し手をposition文字列に追加。
    // 3. 更新したpositionコマンドをエンジンに送信。
    // 4. ponderモードをセット。
    // 5. エンジンに「go ponder」コマンドを送信。
    // 6. "stop"または"ponderhit"コマンドの待機のため新しいスレッドを生成。
    void startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr);

    // 将棋エンジンからの最善手をposition文字列に追加し、予想手を考慮した処理を開始する。
    // @param positionStr position文字列
    // @param positionPonderStr position文字列に予想手を追加したもの
    void appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr);

    // positionコマンドを送信し、ponderモードをオフにして、goコマンドを送信し、
    // bestmoveを受信して予想手を考慮した処理を開始する。
    // @param positionStr positionコマンドの文字列
    // @param byoyomiMilliSec bestmoveを待機する時間（ミリ秒単位）
    // @param btime 黒の残り時間
    // @param wtime 白の残り時間
    // @param positionPonderStr position文字列に予想手を追加したもの
    void sendCommandsAndProcess(int byoyomiMilliSec, QString& positionStr, const QString& btime, const QString& wtime,
                                QString& positionPonderStr, int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    // 将棋エンジンからのレスポンスに基づいて、適切なコマンドを送信し、必要に応じて処理を行う。
    void processEngineResponse(QString& positionStr, QString& positionPonderStr, int byoyomiMilliSec, const QString& btime, const QString& wtime,
                               int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    // 将棋エンジンとのUSIプロトコルに基づく通信を処理するための共通関数
    // 人間対エンジン、エンジン対人間、およびエンジン同士の対局で共通して使用される。
    void executeEngineCommunication(QString& positionStr, QString& positionPonderStr, QPoint& outFrom,
                                    QPoint& outTo, int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                    int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    // 盤上の駒を動かす場合の指し手をUSI形式に変換する。
    QString convertBoardMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo, bool promote) const;

    // 持ち駒を打つ場合の指し手をUSI形式に変換する。
    QString convertDropMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo) const;

    // 人間の指し手をUSI形式の指し手に直す。
    QString convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote);

    // カレントディレクトリをエンジンファイルのあるディレクトリに移動する。
    void changeDirectoryToEnginePath(const QString& engineFile);

    // usiコマンドを将棋エンジンに送り、usiokを待機する。
    void sendUsiCommandAndWaitForUsiOk();

    // isreadyコマンドを将棋エンジンに送り、readyokを待機する。
    void sendIsReadyCommandAndWaitForReadyOk();

    // 将棋エンジンに対局開始に関するコマンドを送信する。
    void sendInitialCommands(const QString& enginename);

    // 将棋エンジンを起動し、対局開始に関するコマンドを送信する。
    void startAndInitializeEngine(const QString& engineFile, const QString& enginename);

    // bestmove文字列（例: "7g7f", "P*5e"など）から移動元の座標（盤上の駒の場合）または持ち駒の種類（持ち駒を打つ場合）を解析する。
    void parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom);

    // bestmove文字列（例: "7g7f", "P*5e"など）から移動先の座標を解析する。
    void parseMoveTo(const QString& move, int& fileTo, int& rankTo);

    // positionコマンドとgoコマンドを送信し、bestmoveを受信するまで待機する。
    void sendPositionAndGoCommands(int byoyomiMilliSec, QString& positionStr);

    // 棋譜解析モードでgoコマンドを将棋エンジンに送信する。
    void sendGoCommandByAnalysys(int byoyomiMilliSec);

    // 将棋エンジンから受信したデータを1行ごとにm_linesに貯え、GUIに受信データをログ出力する。
    void readFromEngine();

    // 将棋エンジンからbestmoveを受信した時に最善手を取得する。
    void bestMoveReceived(const QString& line);

    // 将棋エンジンからinfoを受信した時にinfo行を解析し、GUIの「思考」タブに表示する。
    void infoReceived(QString& line);

    // QProcessのエラーが発生したときに呼び出されるスロット
    void onProcessError(QProcess::ProcessError error);

    // 盤面データを9x9のマスに表示する。
    void printShogiBoard(const QVector<QChar>& boardData) const;

    // 残り時間になるまでbestmoveを待機する。
    void waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime, const QString& wtime, bool useByoyomi);

    QElapsedTimer m_goTimer;               // go or ponderhit を送った時刻
    qint64        m_lastGoToBestmoveMs=0;  // 直近の go/ponderhit → bestmove 経過ms

    // === 追記: ヘッダ（privateセクション）======================================
    // ★ 追加(2)(3): 予算内待機 + 小さな猶予で再度待つヘルパ
    bool waitForBestMoveWithGrace(int budgetMs, int graceMs);

    // ★ 追加(2): “小さな猶予”の既定値（OSスケジューリング/パイプ遅延用）
    static constexpr int kBestmoveGraceMs = 250; // 200〜300ms がおすすめ

    // 思考フェーズ
    enum class SearchPhase { Idle, Main, Ponder };

    // ログ識別用
    QString m_logEngineTag;   // 例: "[E1]" / "[E2]"
    QString m_logSideTag;     // 例: "P1" / "P2"
    QString m_logEngineName;  // 例: "YaneuraOu"
    SearchPhase m_phase = SearchPhase::Idle;
    int m_ponderSession = 0;  // ぽんだーセッション番号

    // ログ用プレフィックス生成
    QString phaseTag() const;            // 例: "" / "[MAIN]" / "[PONDER#3]"
    QString logPrefix() const;           // 例: "[E1/P1 YaneuraOu] [PONDER#3]"

    // ★ 標準エラーの受信ログ（なければ新規で宣言）
    void readFromEngineStderr();

    // quit後に "info string ..." を何行までログに残すか（0で許可なし）
    int m_postQuitInfoStringLinesLeft = 0; // NEW

    // "info string ..." を許可するか判定する（quit後専用）
    bool shouldLogAfterQuit(const QString& line) const; // NEW

    bool m_gameoverSent = false; // このインスタンスに gameover を送ったか
    bool m_quitSent     = false; // このインスタンスに quit を送ったか

    bool m_squelchResignLogs = false; // NEW: true なら "bestmove resign" をログ/処理ともに捨てる  

    enum class ShutdownState { Running, IgnoreAll, IgnoreAllExceptInfoString };

    ShutdownState m_shutdownState = ShutdownState::Running;
    bool m_resignNotified  = false;  // resign の多重 emit 防止
    bool m_timeoutDeclared = false;  // ★ GUIにより旗落ち確定（この局の bestmove は受け付けない）

    bool shouldAbortWait() const;  // タイムアウト/終了状態なら true

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
};

// GUIのメインスレッドとは別のスレッドでgo ponderコマンド受信後の将棋エンジンの処理を行う。
// GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待ち、GUIの思考欄に読み筋を出力する。
class UsiThread : public QThread
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit UsiThread(Usi* usi, QObject* parent = nullptr)
        : QThread(parent), m_usi(usi) {}

protected:
    // スレッドを実行する。
    void run() override {
        qDebug() << "Running in a separate thread...";

        // GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待つ。
        m_usi->waitForStopOrPonderhitCommand();
    }

private:
    // Usiクラスのポインタ
    Usi* m_usi;
};

#endif // USI_H
