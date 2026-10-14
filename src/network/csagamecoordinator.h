#ifndef CSAGAMECOORDINATOR_H
#define CSAGAMECOORDINATOR_H

/// @file csagamecoordinator.h
/// @brief CSA通信対局コーディネータクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPointer>
#include <QPoint>
#include <QTimer>

#include "csaclient.h"
#include "csaenginecontroller.h"
#include "csagameprogress.h"
#include "recordentry.h"
#include "shogimove.h"
#include "usimove.h"

class ShogiGameController;
class ShogiView;
class ShogiClock;
class UsiCommLogModel;
class ShogiEngineThinkingModel;

/**
 * @brief CSA通信対局コーディネータクラス
 *
 * CSAプロトコルを使用した通信対局の進行を管理するクラス。
 * CsaClientを通じてサーバーと通信し、GUI側の
 * ShogiGameController、ShogiView、ShogiClockなどと連携して
 * 対局を進行させる。
 *
 * 対応する対局形態:
 * - 人間 vs サーバー上の対戦相手
 * - エンジン vs サーバー上の対戦相手
 *
 * 責務:
 * - CSAクライアントとGUIコンポーネントの橋渡し
 * - 対局フローの制御（接続→ログイン→対局→終了→次の対局条件待ち）
 * - 指し手の進行（相手の手・自分の手の確認・エンジン思考。csagamecoordinator_moves.cpp）
 * - 時間管理のGUIへの反映
 * - 棋譜行（RecordEntry）の組み立て。着手・終局ごとに recordEntryReady を1回発行する
 *
 * 1局ぶんの手数・持ち時間・指し手列は値型 CsaGameProgress（m_progress）が持ち、
 * 時間の計算や終局行の組み立てはその関数が値で返す。
 *
 * 解釈できない指し手行（不正な書式・盤面に適用できない手）は、サーバーが続けて送る
 * #ILLEGAL_MOVE などの裁定を待つ（仕様 3.4）。裁定が届かなければ中断として終局させる。
 */
class CsaGameCoordinator : public QObject
{
    Q_OBJECT
    friend class TestAppGameSession;  ///< 手番の状態はサーバーとのやり取りでしか変わらないため、テストで直接設定する

public:
    /**
     * @brief 対局状態
     */
    enum class GameState {
        Idle,               ///< 待機中
        Connecting,         ///< 接続中
        LoggingIn,          ///< ログイン中
        WaitingForMatch,    ///< マッチング待ち
        WaitingForAgree,    ///< 対局条件確認中
        InGame,             ///< 対局中
        GameOver,           ///< 対局終了（ログインは続き、次の対局条件を受け付ける）
        Error               ///< エラー状態
    };
    Q_ENUM(GameState)

    /**
     * @brief 対局者タイプ
     */
    enum class PlayerType {
        Human,              ///< 人間
        Engine              ///< エンジン
    };

    /**
     * @brief 依存オブジェクト
     */
    struct Dependencies {
        ShogiGameController* gameController = nullptr;
        ShogiView* view = nullptr;
        ShogiClock* clock = nullptr;
        QStringList* sfenRecord = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;
        UsiCommLogModel* usiCommLog = nullptr;
        ShogiEngineThinkingModel* engineThinking = nullptr;
    };

    /**
     * @brief 対局開始オプション
     */
    struct StartOptions {
        QString host;
        int port = 4081;
        CsaClient::Transport transport = CsaClient::Transport::Plain;
        QString username;
        QString password;
        PlayerType playerType = PlayerType::Human;
        QString enginePath;
        QString engineName;

        // 接続の維持
        bool keepAliveEnabled = true;       ///< 対局中に空行キープアライブを送る
        int keepAliveIntervalMs = 40000;    ///< 空行キープアライブの間隔
        bool tcpKeepAliveEnabled = true;    ///< OS の TCP キープアライブを使う

        // 対局の進め方
        int thinkingMarginMs = 1000;        ///< エンジンに渡す持時間から差し引く通信の余裕
        int gameLimit = 1;                  ///< この接続で行う対局数（0 は無制限）
        bool reconnectAfterGame = false;    ///< 終局後にサーバーが切断したら再接続して次の対局を待つ
    };

    explicit CsaGameCoordinator(QObject* parent = nullptr);
    ~CsaGameCoordinator() override;

    void setDependencies(const Dependencies& deps);
    void startGame(const StartOptions& options);
    void stopGame();

    GameState gameState() const { return m_gameState; }
    bool isConnected() const
    {
        // RemoteHostClosedErrorではソケットの状態が切り替わる前に切断通知が届く。
        return m_client->connectionState() != CsaClient::ConnectionState::Disconnected
            && m_client->connectionState() != CsaClient::ConnectionState::Connecting
            && m_client->isConnected();
    }
    bool isMyTurn() const;
    bool isBlackSide() const;
    bool isHumanPlayer() const { return m_playerType == PlayerType::Human; }
    int blackTotalTimeMs() const { return m_progress.blackTotalMs; }
    int whiteTotalTimeMs() const { return m_progress.whiteTotalMs; }
    int blackRemainingMs() const { return m_progress.blackRemainingMs; }
    int whiteRemainingMs() const { return m_progress.whiteRemainingMs; }
    /// 1局ぶんの進行（手数・持ち時間・指し手列）
    const CsaGameProgress& progress() const { return m_progress; }
    /// 受信した対局条件（Game_Summary）。持ち時間は CsaClient::GameSummary の各関数で単位換算する
    const CsaClient::GameSummary& gameSummary() const { return m_progress.summary; }
    /// 対局の開始局面（SFEN）
    QString startSfen() const { return m_progress.startSfen; }
    void requestChudan();
    void declareWin();
    /// エンジンが自分の手番で思考中なら、すぐ指させる
    void forceImmediateMove();

    void sendRawCommand(const QString& command);
    QString username() const { return m_options.username; }
    const QStringList& usiMoves() const { return m_progress.usiMoves; }

    /// この接続でまだ対局を続けるか（対局数の上限に達していない）
    bool moreGamesWanted() const;
    int completedGames() const { return m_completedGames; }
    int gameLimit() const { return m_options.gameLimit; }

    /**
     * @brief 受け入れられない対局条件なら、その理由を返す（受け入れるなら空）
     * @param summary 受信した対局条件
     * @param playerType こちらの対局者タイプ
     *
     * Format が将棋以外、またはエンジン対局で時間無制限のときは REJECT する。
     */
    static QString unsupportedConditionReason(const CsaClient::GameSummary& summary, PlayerType playerType);

    /**
     * @brief 対局開始時に表示用時計を CSA の持時間に合わせる
     *
     * 双方とも時間無制限なら消費時間を数える時計にする。以後は着手ごとに残り時間だけを合わせる。
     */
    static void startDisplayClock(ShogiClock* clock, const CsaClient::GameSummary& summary,
                                  qint64 blackRemainingMs, qint64 whiteRemainingMs);

    /// 裁定が届かないときに中断とみなすまでの時間
    static constexpr int kVerdictTimeoutMs = 5000;

signals:
    void connectionStateChanged(bool connected);
    void gameStateChanged(CsaGameCoordinator::GameState state);
    void errorOccurred(const QString& message);
    /// 対局が始まった。initialRecords は Game_Summary に含まれていた既存手順の棋譜行
    void gameStarted(const QString& blackName, const QString& whiteName,
                     const QList<RecordEntry>& initialRecords);
    /// 対局が終わった。終局行は直前の recordEntryReady で通知済み
    void gameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    /// 棋譜行が確定した（対局中の着手ごと・終局時に1回ずつ）。局面列（sfenRecord）は更新済み
    void recordEntryReady(const RecordEntry& entry);
    void turnChanged(bool isMyTurn);
    void moveHighlightRequested(const QPoint& from, const QPoint& to);
    /// 盤面に着手が反映された（相手の指し手・自エンジンの指し手。人間の指し手は
    /// ShogiGameController::moveCommitted で通知される）
    void moveAppliedToBoard();
    void logMessage(const QString& message, bool isError = false);
    void csaCommLogAppended(const QString& line);
    void engineScoreUpdated(int scoreCp, int ply, const QString& mate);

public slots:
    void onHumanMove(const QPoint& from, const QPoint& to, bool promote);
    void onResign();

private slots:
    void onConnectionStateChanged(CsaClient::ConnectionState state);
    void onClientError(const QString& message);
    void onCommandRejected(const QString& reason);
    void onLoginSucceeded();
    void onLoginFailed(const QString& reason);
    void onLogoutCompleted();
    void onGameSummaryReceived(const CsaClient::GameSummary& summary);
    void onGameStarted(const QString& gameId);
    void onGameRejected(const QString& gameId, const QString& rejector);
    void onMoveReceived(const QString& move, int consumedTimeMs);
    void onMoveConfirmed(const QString& move, int consumedTimeMs);
    void onClientGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    /// サーバーの裁定で終局した（CsaClient::gameEnded）。切断や内部エラーによる終局と区別する
    void onServerGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    void onGameInterrupted();
    /// 終局後にサーバーが切断したとき、少し待ってから次の対局のために接続し直す
    void reconnectForNextGame();
    void onRawMessageReceived(const QString& message);
    void onRawMessageSent(const QString& message);
    void onEngineControllerResign();
    void onEngineControllerWinDeclaration();
    void onEngineControllerInitialized();
    void onEngineControllerError(const QString& message);
    void onEngineThinkingFinished(const CsaEngineController::ThinkingResult& result);
    void onVerdictTimeout();

private:
    void setGameState(GameState state);
    void setupInitialPosition();
    void setupClock();
    void performResign();
    void failGame(const QString& message);
    void cleanup();
    /// エンジンを起動し、readyok まで AGREE を保留する
    void startEngine();
    /// 受信した対局条件の持時間・手数制限・宣言ルールをログに出す
    void logGameConditions(const CsaClient::GameSummary& summary);
    /// 現在のオプションでサーバーへ接続し、ログイン以降の流れを始める
    void connectForGame();
    /// 終局後の対局数の扱い（上限に達したらログアウト、残っていれば次を待つ）
    void handleGameCountAfterEnd();

    // --- 指し手の進行（csagamecoordinator_moves.cpp） ---
    void startEngineThinking();
    /// 着手後に表示用時計をサーバーが示した残り時間に合わせ、次の手番の時計を動かす
    void syncClockAfterMove(bool startMyTurnClock);
    /// 指し手行の書式・座標を確認し、問題があれば通知文を返す（問題なければ空）
    QString validateMoveText(const QString& move, QPoint* from, QPoint* to) const;
    /// 解釈できない指し手行を記録し、サーバーの裁定を待つ
    void deferVerdict(const QString& logText, const QString& failureText);
    /// 裁定待ちのまま次の行が来たら、続行できないとして終局させる
    bool failIfVerdictPending();
    /// 解釈できなかった指し手の裁定待ちを取り消す（終局・切断時）
    void clearPendingVerdict();
    /// 着手を記録して通知する（手数・手番・ハイライト・棋譜行）
    void finishMove(const QString& csaMove, const QString& usiMove, bool isPromotion,
                    int consumedTimeMs, const QPoint& from, const QPoint& to, bool nowMyTurn);
    /// 局面列の末尾2つ（指す前・指した後）から棋譜行の盤面の値を埋める
    void fillRecordPosition(RecordEntry& entry, const QString& usiMove) const;

    CsaClient* m_client;
    CsaEngineController* m_engineController = nullptr;

    QPointer<ShogiGameController> m_gameController;
    QPointer<ShogiView> m_view;
    QPointer<ShogiClock> m_clock;

    // エンジン初期化用（CsaEngineControllerに渡す）
    UsiCommLogModel* m_usiCommLog = nullptr;
    ShogiEngineThinkingModel* m_engineThinking = nullptr;

    GameState m_gameState = GameState::Idle;
    PlayerType m_playerType = PlayerType::Human;
    bool m_initializingEngine = false;
    bool m_engineNeedsRestart = false;   ///< 終局で gameover/quit を送ったので次局は起動し直す
    bool m_lastGameEndedByServer = false; ///< 直前の終局がサーバーの裁定によるもの（再接続の条件）
    int m_completedGames = 0;            ///< この接続（再接続を含む）で終えた対局数
    int m_thinkingMarginMs = 0;          ///< エンジンに渡す持時間から差し引く通信の余裕
    QTimer* m_reconnectTimer;            ///< 終局後の再接続までの待ち
    StartOptions m_options;

    CsaGameProgress m_progress;          ///< 1局ぶんの進行（対局条件の受信で作り直す）
    bool m_isMyTurn = false;
    QList<RecordEntry> m_initialRecords; ///< 再開局面の既存手順（gameStarted で渡す）

    QStringList* m_sfenHistory = nullptr;
    QList<ShogiMove>* m_gameMoves = nullptr;

    QTimer* m_verdictTimer;              ///< 裁定待ちのタイムアウト
    QString m_pendingFailureText;        ///< 裁定が届かなかったときの終局理由（空なら待ちなし）
    UsiMove m_sentMove;                  ///< 最後にサーバーへ送った自分の指し手

    static constexpr int kReconnectDelayMs = 3000; ///< 終局後の再接続までの待ち時間
};

#endif // CSAGAMECOORDINATOR_H
