#ifndef USI_H
#define USI_H

#include <QPlainTextEdit>
#include <QProcess>
#include <QTableView>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <memory>

#include "shogienginethinkingmodel.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "usicommlogmodel.h"
#include "playmode.h"

/**
 * @brief USIプロトコル通信を管理するクラス
 *
 * 将棋エンジンとのUSIプロトコルによる通信を行い、
 * 思考情報をGUIに表示するための処理を行う。
 *
 * 設計上の注意:
 * - シグナル/スロットによる非同期通信を基本とする
 * - QEventLoopを使用した待機パターンでGUIの応答性を維持
 * - View層への直接操作を最小限に抑える
 */
class Usi : public QObject
{
    Q_OBJECT

public:
    // === 定数定義 ===
    
    /// 先手の持ち駒を示す筋番号
    static constexpr int SENTE_HAND_FILE = 10;
    
    /// 後手の持ち駒を示す筋番号
    static constexpr int GOTE_HAND_FILE = 11;
    
    /// 盤面の1辺のマス数
    static constexpr int BOARD_SIZE = 9;
    
    /// 将棋盤のマス数
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

    // === コンストラクタ・デストラクタ ===
    
    explicit Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking,
                 ShogiGameController* algorithm, PlayMode& playMode, QObject* parent = nullptr);
    ~Usi();

    // === 公開インターフェース ===
    
    /// 評価値の文字列を返す
    QString scoreStr() const;

    /// GUIがbestmove resignを受信したかどうかのフラグを返す
    bool isResignMove() const;

    /// 一番最後に受信した指し手の評価値を返す
    int lastScoreCp() const;

    /// 漢字の指し手に変換したpv文字列を返す
    QString pvKanjiStr() const;

    /// 漢字の指し手に変換したpv文字列を設定する
    void setPvKanjiStr(const QString& newPvKanjiStr);

    /// 先手が持ち駒を打つときの駒を文字列に変換する
    QString convertFirstPlayerPieceNumberToSymbol(int rankFrom) const;

    /// 後手が持ち駒を打つときの駒を文字列に変換する
    QString convertSecondPlayerPieceNumberToSymbol(int rankFrom) const;

    /// 将棋エンジンがbestmove文字列で返した最善手から移動元の筋と段、移動先の筋と段を取得する
    void parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo);

    /// 将棋エンジンプロセスを起動し、対局を開始するUSIコマンドを送受信する
    void initializeAndStartEngineCommunication(QString& engineFile, QString& enginename);

    /// 人間対将棋エンジンの対局で将棋エンジンとUSIプロトコル通信を行う
    void handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                          QPoint& outFrom, QPoint& outTo,
                                          int byoyomiMilliSec, const QString& btime,
                                          const QString& wtime, QStringList& positionStrList,
                                          int addEachMoveMiliSec1, int addEachMoveMiliSec2,
                                          bool useByoyomi);

    /// 将棋エンジン対人間および将棋エンジン同士の対局で将棋エンジンとUSIプロトコル通信を行う
    void handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                       QString& positionPonderStr,
                                                       QPoint& outFrom, QPoint& outTo,
                                                       int byoyomiMilliSec, const QString& btime,
                                                       const QString& wtime,
                                                       int addEachMoveMiliSec1,
                                                       int addEachMoveMiliSec2, bool useByoyomi);

    /// gameoverコマンドを将棋エンジンに送信する
    void sendGameOverCommand(const QString& result);

    /// quitコマンドを将棋エンジンに送信する
    void sendQuitCommand();

    /// 将棋の段（1～9）をアルファベット表記（a～i）に変換する
    QChar rankToAlphabet(int rank) const;

    /// GUIで「投了」をクリックした場合の処理を行う
    void sendGameOverWinAndQuitCommands();

    /// stopコマンドを将棋エンジンに送信する
    void sendStopCommand();

    /// GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待つ
    void waitForStopOrPonderhitCommand();

    void executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec);

    /// 1手前に指した移動先の筋を設定する
    void setPreviousFileTo(int newPreviousFileTo);

    /// 1手前に指した移動先の段を設定する
    void setPreviousRankTo(int newPreviousRankTo);

    qint64 lastBestmoveElapsedMs() const { return m_lastGoToBestmoveMs; }

    /// エンジンにgameover loseコマンドとquitコマンドを送信し、手番を変更する
    void sendGameOverLoseAndQuitCommands();

    /// ログ識別子の設定
    void setLogIdentity(const QString& engineTag, const QString& sideTag,
                        const QString& engineName = QString());

    /// タイムアウト確定後に、当該エンジンが出す "bestmove resign" を黙殺する
    void setSquelchResignLogging(bool on);

    void resetResignNotified() { m_resignNotified = false; }
    void markHardTimeout()     { m_timeoutDeclared = true; }
    void clearHardTimeout()    { m_timeoutDeclared = false; }
    bool isIgnoring() const    { return m_shutdownState != ShutdownState::Running; }

    /// goコマンドを将棋エンジンに送信する
    void sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                       int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    /// 任意のUSIコマンドをそのまま送る
    void sendRaw(const QString& command) const;

    /// ThinkingModelを設定する
    void setThinkingModel(ShogiEngineThinkingModel* m) { m_modelThinking = m; }
    
    /// LogModelを設定する
    void setLogModel(UsiCommLogModel* m) { m_model = m; }

    /// 将棋エンジンのプロセスを終了し、プロセスを削除する
    void cleanupEngineProcessAndThread();

    void sendPositionAndGoMate(const QString& sfen, int timeMs, bool infinite);
    void sendStopForMate();

    /// 将棋エンジンを起動し、対局開始に関するコマンドを送信する
    void startAndInitializeEngine(const QString& engineFile, const QString& enginename);

    /// 詰み探索結果
    struct TsumeResult {
        enum Kind { Solved, NoMate, NotImplemented, Unknown } kind = Unknown;
        QStringList pvMoves;
    };

    void executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec);
    void sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr);
    void cancelCurrentOperation();

#ifdef QT_DEBUG
    ShogiEngineThinkingModel* debugThinkingModel() const { return m_modelThinking; }
    UsiCommLogModel*          debugLogModel()      const { return m_model; }
#endif

signals:
    /// stopあるいはponderhitコマンドが送信されたことを通知するシグナル
    void stopOrPonderhitCommandSent();

    /// bestmove resignを受信したことを通知するシグナル
    void bestMoveResignReceived();

    void sigTsumeCheckmate(const Usi::TsumeResult& result);

    /// 詰将棋の最終結果シグナル
    void checkmateSolved(const QStringList& pvMoves);
    void checkmateNoMate();
    void checkmateNotImplemented();
    void checkmateUnknown();

    /// エラー発生シグナル
    void errorOccurred(const QString& message);

    /// usiok受信シグナル（待機用）
    void usiOkReceived();

    /// readyok受信シグナル（待機用）
    void readyOkReceived();

    /// bestmove受信シグナル（待機用）
    void bestMoveReceived();

    /// 思考情報更新シグナル（View層への直接依存を減らすため）
    void thinkingInfoUpdated(const QString& time, const QString& depth,
                             const QString& nodes, const QString& score,
                             const QString& pvKanjiStr);

private:
    // === メンバ変数 ===

    /// 一番最後に受信した指し手の評価値
    int m_lastScoreCp = 0;

    /// 漢字の指し手に変換したpv文字列
    QString m_pvKanjiStr;

    /// 評価値の文字列
    QString m_scoreStr;

    /// 棋譜解析モードかどうかのフラグ
    bool m_analysisMode = false;

    /// 1手前の移動先の筋
    int m_previousFileTo = 0;

    /// 1手前の移動先の段
    int m_previousRankTo = 0;

    /// 将棋エンジンの指し手
    QString m_bestMove;

    /// 将棋エンジンが予想した相手の指し手
    QString m_predictedOpponentMove;

    /// 盤面のコピーデータ
    QVector<QChar> m_clonedBoardData;

    /// 将棋エンジンのプロセス
    QProcess* m_process = nullptr;

    /// 将棋エンジンからの受信データを1行ずつ保存するリスト
    QStringList m_lines;

    /// 3桁ごとのカンマ区切り表示に使用するロケール
    QLocale m_locale;

    /// GUIの「USIプロトコル通信ログ」タブのテキスト欄
    QPlainTextEdit* m_txt = nullptr;

    /// usiokを受信したかどうかのフラグ
    bool m_usiOkSignalReceived = false;

    /// readyokを受信したかどうかのフラグ
    bool m_readyOkSignalReceived = false;

    /// infoを受信したかどうかのフラグ
    bool m_infoSignalReceived = false;

    /// bestmoveを受信したかどうかのフラグ
    bool m_bestMoveSignalReceived = false;

    /// 設定ファイルから読み込んだ将棋エンジンオプションのリスト
    QStringList m_setOptionCommandList;

    /// エンジン情報更新用モデル
    UsiCommLogModel* m_model = nullptr;

    /// 将棋エンジンの思考結果表示用モデル
    ShogiEngineThinkingModel* m_modelThinking = nullptr;

    /// 将棋の対局管理クラス
    ShogiGameController* m_gameController = nullptr;

    /// 対局モード
    PlayMode& m_playMode;

    /// ponderモードが有効かどうかのフラグ
    bool m_isPonderEnabled = false;

    /// GUIがbestmove resignを受信したかどうかのフラグ
    bool m_isResignMove = false;

    /// GUIがbestmove winを受信したかどうかのフラグ
    bool m_isWinMove = false;

    // === infoバッファリング用メンバ変数（動的プロパティから正式メンバに変更）===
    
    /// info行のバッファ
    QStringList m_infoBuffer;
    
    /// バッファフラッシュがスケジュール済みかどうか
    bool m_infoFlushScheduled = false;

    // === 計測・状態管理 ===

    QElapsedTimer m_goTimer;
    qint64 m_lastGoToBestmoveMs = 0;

    /// "小さな猶予"の既定値
    static constexpr int kBestmoveGraceMs = 250;

    /// 思考フェーズ
    enum class SearchPhase { Idle, Main, Ponder };

    /// ログ識別用
    QString m_logEngineTag;
    QString m_logSideTag;
    QString m_logEngineName;
    SearchPhase m_phase = SearchPhase::Idle;
    int m_ponderSession = 0;

    /// quit後に許可するinfo stringの行数
    int m_postQuitInfoStringLinesLeft = 0;

    bool m_gameoverSent = false;
    bool m_quitSent = false;
    bool m_squelchResignLogs = false;

    /// シャットダウン状態
    enum class ShutdownState { Running, IgnoreAll, IgnoreAllExceptInfoString };
    ShutdownState m_shutdownState = ShutdownState::Running;

    bool m_resignNotified = false;
    bool m_timeoutDeclared = false;

    /// オペレーション用コンテキスト
    QPointer<QObject> m_opCtx { nullptr };
    quint64 m_seq { 0 };

    /// 詰み探索中フラグ
    bool m_modeTsume = false;

    // === 駒文字変換用定数マップ ===
    static const QMap<int, QString> s_firstPlayerPieceMap;
    static const QMap<int, QString> s_secondPlayerPieceMap;
    static const QMap<QChar, int> s_pieceRankWhite;
    static const QMap<QChar, int> s_pieceRankBlack;

    // === プライベートメソッド ===

    /// ログ用プレフィックス生成
    QString phaseTag() const;
    QString logPrefix() const;

    /// 標準エラーの受信ログ
    void readFromEngineStderr();

    /// quit後のログ許可判定
    bool shouldLogAfterQuit(const QString& line) const;

    /// 待機中断判定
    bool shouldAbortWait() const;

    /// 予算内待機ヘルパ
    bool waitForBestMoveWithGrace(int budgetMs, int graceMs);

    /// オペレーションコンテキスト開始
    quint64 beginOperationContext();

    /// 詰み行処理
    void handleCheckmateLine(const QString& line);

    /// info行を解析する
    int parseEngineOutputAndUpdateInfo(QString& line, ShogiEngineInfoParser& info);

    /// GUIの各欄を更新するヘルパメソッド
    void updateSearchedHand(const ShogiEngineInfoParser* info);
    void updateDepth(const ShogiEngineInfoParser* info);
    void updateNodes(const ShogiEngineInfoParser* info);
    void updateNps(const ShogiEngineInfoParser* info);
    void updateHashfull(const ShogiEngineInfoParser* info);

    /// 評価値計算
    int calculateScoreInt(const ShogiEngineInfoParser* info) const;
    void updateScoreMateAndLastScore(ShogiEngineInfoParser* info, int& scoreint);
    void updateAnalysisModeAndScore(const ShogiEngineInfoParser* info, int& scoreint);
    void updateLastScore(int scoreInt);
    void updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreint);

    /// USIコマンド送信
    void sendUsiCommand();
    void sendIsReadyCommand();
    void sendCommand(const QString& command) const;

    /// 待機メソッド（QEventLoopベース）
    bool waitForUsiOK(int timeoutMilliseconds);
    bool waitForReadyOk(int timeoutMilliseconds);
    bool waitForBestMove(int timeoutMilliseconds);
    bool keepWaitingForBestMove();

    /// エンジン起動・初期化
    void startEngine(const QString& engineFile);
    void sendUsiNewGameCommand();
    void sendPositionCommand(QString& positionStr);
    void generateSetOptionCommands(const QString& engineName);
    void sendSetOptionCommands();

    /// info関連
    void infoRecordClear();
    void sendGoPonderCommand();

    /// 変換ヘルパ
    int alphabetToRank(QChar c);
    int pieceToRankWhite(QChar c);
    int pieceToRankBlack(QChar c);

    /// ponderコマンド
    void sendPonderHitCommand();

    /// 現在の局面（盤内のみ）をコピーする
    void cloneCurrentBoardData();

    /// 将棋エンジンから返された情報をもとに駒配置を更新
    void applyMovesToBoardFromBestMoveAndPonder();

    /// bestmove待機
    void waitAndCheckForBestMove(int byoyomiMilliSec);
    void waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime,
                                              const QString& wtime, bool useByoyomi);

    /// 予想手を考慮した処理
    void startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr);
    void appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr);
    void sendCommandsAndProcess(int byoyomiMilliSec, QString& positionStr,
                                const QString& btime, const QString& wtime,
                                QString& positionPonderStr, int addEachMoveMilliSec1,
                                int addEachMoveMilliSec2, bool useByoyomi);

    /// エンジンレスポンス処理
    void processEngineResponse(QString& positionStr, QString& positionPonderStr,
                               int byoyomiMilliSec, const QString& btime, const QString& wtime,
                               int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);
    void executeEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                    QPoint& outFrom, QPoint& outTo, int byoyomiMilliSec,
                                    const QString& btime, const QString& wtime,
                                    int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                    bool useByoyomi);

    /// 指し手変換
    QString convertBoardMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo,
                                  bool promote) const;
    QString convertDropMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo) const;
    QString convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote);

    /// ディレクトリ変更
    void changeDirectoryToEnginePath(const QString& engineFile);

    /// コマンド送信・待機
    void sendUsiCommandAndWaitForUsiOk();
    void sendIsReadyCommandAndWaitForReadyOk();
    void sendInitialCommands(const QString& enginename);

    /// 指し手解析
    void parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom);
    void parseMoveTo(const QString& move, int& fileTo, int& rankTo);

    /// position/go送信
    void sendPositionAndGoCommands(int byoyomiMilliSec, QString& positionStr);
    void sendGoCommandByAnalysys(int byoyomiMilliSec);

    /// データ受信・処理
    void readFromEngine();
    void clearResponseData();

    /// bestmove/info受信処理
    void onBestMoveReceived(const QString& line);
    void infoReceived(QString& line);

    /// infoバッファのフラッシュ処理
    void flushInfoBuffer();

    /// 単一のinfo行を処理
    void processInfoLine(const QString& line);

    /// デバッグ用盤面表示
    void printShogiBoard(const QVector<QChar>& boardData) const;

private slots:
    /// QProcessエラーハンドラ
    void onProcessError(QProcess::ProcessError error);

    /// QProcess終了ハンドラ
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
};

#endif // USI_H
