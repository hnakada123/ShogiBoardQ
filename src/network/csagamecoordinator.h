#ifndef CSAGAMECOORDINATOR_H
#define CSAGAMECOORDINATOR_H

/// @file csagamecoordinator.h
/// @brief CSA通信対局コーディネータクラスの定義


#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPointer>
#include <QTimer>

#include "csaclient.h"
#include "csagameprogress.h"
#include "matchautosave.h"
#include "matchtimesource.h"
#include "recordentry.h"
#include "recordline.h"
#include "shogimove.h"
#include "usimatchhandler.h"
#include "usimove.h"

class ShogiGameController;
class ShogiClock;
class EngineSlots;
class MatchEngineSeat;
class Usi;

/**
 * @brief CSA通信対局コーディネータクラス
 *
 * CSAプロトコルを使用した通信対局の進行を管理するクラス。
 * CsaClientを通じてサーバーと通信し、GUI側の
 * ShogiGameController（盤と手番）、ShogiClock（時計）と連携して
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
 * 時間の計算や終局行の組み立てはその関数が値で返す。自分のエンジンが読む持ち時間（MatchTimeSource）も
 * この残り時間から返す（表示用の ShogiClock はその写しで、計算には使わない）。
 *
 * 解釈できない指し手行（不正な書式・盤面に適用できない手）は、サーバーが続けて送る
 * #ILLEGAL_MOVE などの裁定を待つ（仕様 3.4）。裁定が届かなければ中断として終局させる。
 */
class CsaGameCoordinator : public QObject, public MatchTimeSource
{
    Q_OBJECT
    friend class TestAppGameSession;  ///< 手番の状態はサーバーとのやり取りでしか変わらないため、テストで直接設定する

public:
    /**
     * @brief 通信対局の進み具合（司令塔から見た状態）
     *
     * 持たずに求める（gameState()）。接続の段階（接続中・ログイン中・対局待ち・対局条件の確認中）は
     * 接続の状態（CsaClient::ConnectionState）から、対局中・終局後・失敗は司令塔の段階（Phase）から決める。
     * 接続が切れても、終局後は再接続の判断のために GameOver のまま、ログイン失敗・不正な局面・
     * 対局中でない切断は Error として残す。
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
    struct Deps {
        ShogiGameController* gameController = nullptr;
        ShogiClock* clock = nullptr;
        EngineSlots* engines = nullptr;  ///< エンジンで指すとき、エンジンを置く枠
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
        MatchAutoSave autoSave;             ///< 終局した棋譜の自動保存（棋譜の受け手が行う）
    };

    /// 依存が無ければ盤・時計を動かさない（通信の手順だけを試すテスト用）
    explicit CsaGameCoordinator(const Deps& deps, QObject* parent = nullptr);
    CsaGameCoordinator() : CsaGameCoordinator(Deps{}) {}
    ~CsaGameCoordinator() override;
    void startGame(const StartOptions& options);
    void stopGame();

    GameState gameState() const;
    bool isConnected() const
    {
        // RemoteHostClosedErrorではソケットの状態が切り替わる前に切断通知が届く。
        return m_client->connectionState() != CsaClient::ConnectionState::Disconnected
            && m_client->connectionState() != CsaClient::ConnectionState::Connecting
            && m_client->isConnected();
    }
    enum class TurnState { MyTurn, AwaitingConfirmation, OpponentTurn };
    TurnState turnState() const;
    bool isMyTurn() const;
    bool isBlackSide() const;
    bool isHumanPlayer() const { return m_options.playerType == PlayerType::Human; }
    int blackTotalTimeMs() const { return m_progress.blackTotalMs; }
    int whiteTotalTimeMs() const { return m_progress.whiteTotalMs; }
    int blackRemainingMs() const { return m_progress.blackRemainingMs; }
    int whiteRemainingMs() const { return m_progress.whiteRemainingMs; }
    /// 1局ぶんの進行（手数・持ち時間・指し手列）
    const CsaGameProgress& progress() const { return m_progress; }
    /// 受信した対局条件（Game_Summary）。持ち時間は CsaClient::GameSummary の各関数で単位換算する
    const CsaClient::GameSummary& gameSummary() const { return m_progress.summary; }
    /// 対局の開始局面（SFEN）
    QString startSfen() const { return m_progress.record.startSfen; }
    void requestChudan();
    void declareWin();
    /// エンジンが自分の手番で思考中なら、すぐ指させる
    void forceImmediateMove();

    void sendRawCommand(const QString& command);
    QString username() const { return m_options.username; }
    /// 始めた対局の接続と進め方
    const StartOptions& options() const { return m_options; }
    /// この対局の手順（開始局面と指した手）
    const RecordLine& record() const { return m_progress.record; }

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

    // --- MatchTimeSource（自分のエンジンの持ち時間。サーバーが示した残り時間・遅延・余裕） ---
    MatchSearchTiming snapshot(Turn side, bool pondering) const override;
    /// side の今の手番の考慮時間（サーバーが消費時間を知らせないときの投了の消費時間にも使う）
    qint64 elapsedThisTurn(Turn side) const override;

signals:
    void connectionStateChanged(bool connected);
    void gameStateChanged(CsaGameCoordinator::GameState state);
    void errorOccurred(const QString& message);
    /// 対局が始まった。initialRecords は Game_Summary に含まれていた既存手順の棋譜行
    void gameStarted(const QString& blackName, const QString& whiteName,
                     const QList<RecordEntry>& initialRecords);
    /// 対局が終わった。終局行は直前の recordEntryReady で通知済み
    void gameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    /// 棋譜行が確定した（対局中の着手ごと・終局時に1回ずつ）。手順（record()）は更新済み
    void recordEntryReady(const RecordEntry& entry);
    void turnChanged(bool isMyTurn);
    void logMessage(const QString& message, bool isError = false);
    void csaCommLogAppended(const QString& line);
    /// 自分のエンジンの評価値（自分から見た値）。ply は指した後の手数
    void engineScoreUpdated(const UsiScore& score, int ply);

public slots:
    /// 人間が盤で選んだ手をサーバーへ送る（盤は確認を受けてから動かす）
    void onHumanMove(const UsiMove& move);
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
    void onEngineResigned();
    void onEngineDeclaredWin();
    void onEngineInitialized();
    void onEngineError(const QString& message);
    void onEngineMoveReady(const UsiMatchMove& move);
    void onVerdictTimeout();

private:
    /// 司令塔だけが知る段階（接続の段階は CsaClient が持つ）
    enum class Phase {
        Off,        ///< 始めていない・止めた・ログアウトした
        Active,     ///< 接続してから対局が始まるまで（再接続の後も）
        Playing,    ///< 対局が始まってから終局を記録するまで
        Ended,      ///< 終局を記録した後（同じ接続で次の対局条件を待つ。サーバーが切っても GameOver のまま）
        Failed      ///< 続けられない失敗（ログイン失敗・不正な局面・エンジンの失敗）
    };
    void setPhase(Phase phase);
    /// gameState() が前に知らせた値から変わっていれば gameStateChanged を出す
    void publishGameState();
    /// 開始局面と再開局面の手を盤に並べる。手を指せなければ失敗にして false
    bool setupInitialPosition();
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
    QString validateMoveText(const QString& move) const;
    /// 解釈できない指し手行を記録し、サーバーの裁定を待つ
    void deferVerdict(const QString& logText, const QString& failureText);
    /// 裁定待ちのまま次の行が来たら、続行できないとして終局させる
    bool failIfVerdictPending();
    /// 解釈できなかった指し手の裁定待ちを取り消す（終局・切断時）
    void clearPendingVerdict();
    /// CSA の指し手で盤を進める（盤と合わなければ nullopt）。表記の「同」は CSA の棋譜の直前の手で決める
    std::optional<RecordEntry> playCsaMove(const QString& csaMove);
    /// 着手を記録して通知する（手数・手番・棋譜行。盤のハイライトは棋譜の表示中の手から出る）
    void finishMove(const RecordEntry& played, int consumedTimeMs, bool nowMyTurn);
    /// 同じ確定経路で人間・エンジン・相手の着手を記録する。
    bool applyConfirmedMove(const QString& move, int consumedTimeMs, bool nowMyTurn);

    CsaClient* m_client;
    MatchEngineSeat* m_engineSeat = nullptr;   ///< 自分の手を指すエンジン（枠 E1）

    QPointer<ShogiGameController> m_gameController;
    QPointer<ShogiClock> m_clock;
    EngineSlots* m_engines = nullptr;  ///< 非所有

    Phase m_phase = Phase::Off;
    GameState m_publishedState = GameState::Idle;  ///< 最後に gameStateChanged で知らせた値
    bool m_lastGameEndedByServer = false; ///< 直前の終局がサーバーの裁定によるもの（再接続の条件）
    int m_completedGames = 0;            ///< この接続（再接続を含む）で終えた対局数
    QTimer* m_reconnectTimer;            ///< 終局後の再接続までの待ち
    StartOptions m_options;

    CsaGameProgress m_progress;          ///< 1局ぶんの進行（対局条件の受信で作り直す）


    QTimer* m_verdictTimer;              ///< 裁定待ちのタイムアウト
    QElapsedTimer m_turnTimer;           ///< 今の手番（m_progress.sideToMove()）が始まってからの時間
    QString m_pendingFailureText;        ///< 裁定が届かなかったときの終局理由（空なら待ちなし）

    static constexpr int kReconnectDelayMs = 3000; ///< 終局後の再接続までの待ち時間
};

#endif // CSAGAMECOORDINATOR_H
