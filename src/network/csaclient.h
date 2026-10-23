#ifndef CSACLIENT_H
#define CSACLIENT_H

/// @file csaclient.h
/// @brief CSAプロトコルクライアントクラスの定義


#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QSslSocket>
#include <QTimer>

#include "shogitypes.h"
#include "timecontrol.h"

#include <optional>

/**
 * @brief CSAプロトコルクライアントクラス
 *
 * CSAサーバーとの通信を担当するクラス。
 * CSAサーバープロトコル V1.2.1
 * (https://www.computer-shogi.org/protocol/tcp_ip_server_121.html) を実装し、
 * shogi-server (https://github.com/TadaoYamaoka/shogi-server) との通信対局に使う。
 *
 * 責務:
 * - TCP/IP接続の確立と管理
 * - CSAプロトコルメッセージの送受信
 * - ログイン/ログアウト処理（LOGOUT:completed を待ってから切断）
 * - 対局の合意処理（AGREE/REJECT）
 * - 指し手の送受信（確認待ちの間は二重送信しない）
 * - 対局中のキープアライブ（LF のみの行）
 */
class CsaClient : public QObject
{
    Q_OBJECT

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_csaprotocol.cpp がパーサ内部状態を直接検証する
    friend class Tst_CsaProtocol;
    /// テスト専用: tst_app_game_session.cpp が対局中の手番を直接設定する
    friend class TestAppGameSession;
#endif

public:
    /**
     * @brief 接続状態
     */
    enum class ConnectionState {
        Disconnected,       ///< 未接続
        Connecting,         ///< 接続中
        Connected,          ///< 接続済み（ログイン前）
        LoggedIn,           ///< ログイン済み（対局待ち）
        WaitingForGame,     ///< 対局待ち（Game_Summary受信待ち）
        GameReady,          ///< 対局条件受信済み（AGREE待ち）
        InGame,             ///< 対局中
        GameOver            ///< 対局終了（ログインしたまま次の対局条件を待つ）
    };
    Q_ENUM(ConnectionState)

    /**
     * @brief 対局結果
     */
    enum class GameResult {
        Win,                ///< 勝利
        Lose,               ///< 敗北
        Draw,               ///< 引き分け
        Censored,           ///< 打ち切り
        Chudan,             ///< 中断
        Unknown             ///< 不明
    };
    Q_ENUM(GameResult)

    /**
     * @brief 対局終了の原因
     */
    enum class GameEndCause {
        Resign,             ///< 投了
        TimeUp,             ///< 時間切れ
        IllegalMove,        ///< 反則
        Sennichite,         ///< 千日手
        OuteSennichite,     ///< 連続王手の千日手
        Jishogi,            ///< 入玉宣言
        MaxMoves,           ///< 手数制限
        Chudan,             ///< 中断
        IllegalAction,      ///< 不正行為
        Unknown             ///< 不明
    };
    Q_ENUM(GameEndCause)

    /**
     * @brief 片方の対局者の持時間情報（BEGIN Time / Time+ / Time- の内容）
     *
     * 値は Time_Unit の単位時間で保持し、*Ms() でミリ秒に換算する。
     */
    struct TimeControl {
        QString timeUnit = QStringLiteral("1sec"); ///< 時間単位（1sec/1min/1msec）
        int totalTime = 0;          ///< 持時間の初期値
        int byoyomi = 0;            ///< 秒読み
        int leastTimePerMove = 0;   ///< 最少消費時間
        int increment = 0;          ///< 加算時間
        int delay = 0;              ///< 遅延時間
        bool timeRoundup = false;   ///< 単位時間未満を切り上げるか
        bool specified = false;     ///< Time 階層が記述されたか

        int unitMs() const;                 ///< 時間単位をミリ秒に換算
        int totalTimeMs() const { return totalTime * unitMs(); }
        int byoyomiMs() const { return byoyomi * unitMs(); }
        int incrementMs() const { return increment * unitMs(); }
        int delayMs() const { return delay * unitMs(); }
        int leastTimePerMoveMs() const { return leastTimePerMove * unitMs(); }
        /// 時間無制限か（Time 階層の省略、または全時間要素の省略）
        bool isUnlimited() const
        {
            return !specified || (totalTime == 0 && byoyomi == 0 && increment == 0 && delay == 0);
        }
    };

    /**
     * @brief 対局情報を格納する構造体
     */
    struct GameSummary {
        QString protocolVersion;    ///< プロトコルバージョン
        QString protocolMode;       ///< プロトコルモード（Server/Direct）
        QString format;             ///< フォーマット（Shogi 1.0）
        QString declaration;        ///< 宣言ルール（Jishogi 1.1）
        QString gameId;             ///< 対局ID
        QString blackName;          ///< 先手名
        QString whiteName;          ///< 後手名
        std::optional<Turn> myTurn; ///< こちら側の手番（Your_Turn。届かなければ無し）
        Turn toMove = Turn::Black;  ///< 最初の手番（To_Move）
        bool rematchOnDraw;         ///< 引き分け時再対局するか
        int maxMoves;               ///< 最大手数（0=無制限）

        // 持時間情報。共通の Time は両方に入れ、Time+/Time- は該当側だけを上書きする
        TimeControl timeBlack;      ///< 先手の持時間情報
        TimeControl timeWhite;      ///< 後手の持時間情報
        bool hasIndividualTime;     ///< Time+/Time- の個別設定があるか

        // 局面情報（CSA形式）
        QStringList positionLines;  ///< 初期局面（P1-P9, P+, P-, +/-）
        QStringList moves;          ///< 初期局面からの指し手リスト（再開局面）
        QList<int> moveConsumedTimes; ///< moves と同じ長さ。各手の消費時間（単位時間、,T 無しは 0）

        GameSummary();
        void clear();
        /// こちらが先手（下手）か
        bool isBlackSide() const { return myTurn == Turn::Black; }
        /// こちらが最初に指すか
        bool movesFirst() const { return myTurn == toMove; }
        const TimeControl& timeFor(bool black) const { return black ? timeBlack : timeWhite; }
        TimeControl& timeFor(bool black) { return black ? timeBlack : timeWhite; }
        int timeUnitMs(bool black) const { return timeFor(black).unitMs(); }
        int totalTimeMs(bool black) const { return timeFor(black).totalTimeMs(); }
        int byoyomiMs(bool black) const { return timeFor(black).byoyomiMs(); }
        int incrementMs(bool black) const { return timeFor(black).incrementMs(); }
        int delayMs(bool black) const { return timeFor(black).delayMs(); }
        /// 双方とも時間無制限か
        bool isTimeUnlimited() const { return timeBlack.isUnlimited() && timeWhite.isUnlimited(); }
        /// 持ち時間の決まり（ミリ秒）。時間無制限なら値はすべて 0
        ::TimeControl timeControl() const;
        /// Declaration で入玉宣言ルール（Jishogi）が採用されているか
        bool allowsJishogiDeclaration() const { return declaration.startsWith(QStringLiteral("Jishogi")); }
        /// 再開局面の index 手目の消費時間（ミリ秒）。指した側の時間単位で換算する
        int positionMoveConsumedMs(int index) const;
    };

    explicit CsaClient(QObject* parent = nullptr);

    ~CsaClient() override;

    // --- 接続管理 ---

    /// 通常のCSA（平文）を既定とし、TLSも選択可能。
    enum class Transport { Tls, Plain };

    /**
     * @brief サーバーに接続する
     * @param host ホスト名またはIPアドレス
     * @param port ポート番号
     * @param transport 通常のCSA（平文）、またはTLS接続
     *
     * LOGOUT の応答待ちで接続が残っているときは、待たずに切ってから接続する。
     */
    void connectToServer(const QString& host, int port, Transport transport = Transport::Plain);

    /**
     * @brief サーバーから切断する
     *
     * 対局待ち状態なら LOGOUT を送り、LOGOUT:completed（またはタイムアウト）の後に切断する。
     * 対局条件を受け取ったまま同意していなければ REJECT してからログアウトする。
     */
    void disconnectFromServer();

    ConnectionState connectionState() const { return m_connectionState; }

    bool isConnected() const;

    // --- ログイン/ログアウト ---

    /**
     * @brief サーバーにログインする
     * @param username ユーザー名
     * @param password パスワード
     */
    void login(const QString& username, const QString& password);

    /**
     * @brief サーバーからログアウトする（対局待ち状態のみ）
     *
     * LOGOUT を送って応答を待つ。応答を受けるか kLogoutTimeoutMs 経過で切断する。
     */
    void logout();

    // --- 対局制御 ---

    /**
     * @brief 対局条件に同意する
     * @param gameId 対局ID（省略時は直前の対局）
     */
    void agree(const QString& gameId = QString());

    /**
     * @brief 対局条件を拒否する
     * @param gameId 対局ID（省略時は直前の対局）
     *
     * 送信後は対局待ち状態に戻る（サーバーの REJECT: 通知は待たない）。
     */
    void reject(const QString& gameId = QString());

    /**
     * @brief 指し手を送信する
     * @param move CSA形式の指し手（例: "+7776FU"）
     * @return ソケットに送信して確認待ちに入った場合 true
     *
     * 自分の手番で、直前の送信がサーバーに確認されているときだけ送る。
     * それ以外は commandRejected() を発行する（手番外の送信は #ILLEGAL_ACTION で負けになる）。
     */
    bool sendMove(const QString& move);
    bool awaitingMoveConfirmation() const { return m_awaitingMoveConfirmation; }

    /**
     * @brief 生のコマンドを送信する（デバッグ/テスト用）
     * @param command 送信するコマンド。空文字列は LF のみのキープアライブ
     *
     * 対局中の指し手・%コマンドは sendMove() と同じ手番ガードを通す。
     * shogi-server の拡張コマンド（%%）は手番に関係なく送る。
     */
    void sendRawCommand(const QString& command);

    /**
     * @brief 投了する（自分の手番のみ）
     */
    void resign();

    /**
     * @brief 勝利宣言する（入玉。自分の手番のみ）
     */
    void declareWin();

    /**
     * @brief 中断を要請する（自分の手番のみ）
     */
    void requestChudan();

    const GameSummary& gameSummary() const { return m_gameSummary; }

    bool isMyTurn() const { return m_isMyTurn; }

    /// 送った指し手・意思表示に対するサーバーの確認を待っているか
    bool isAwaitingMoveConfirmation() const { return m_awaitingMoveConfirmation; }

    // --- 接続の維持 ---

    /// 対局中の空行キープアライブを使うか（デフォルト: 使う）
    void setKeepAliveEnabled(bool enabled);
    bool isKeepAliveEnabled() const { return m_keepAliveEnabled; }

    /// 空行キープアライブの間隔（ミリ秒）。仕様の下限 kKeepAliveMinIntervalMs より短くはしない
    void setKeepAliveIntervalMs(int intervalMs);
    int keepAliveIntervalMs() const { return m_keepAliveTimer->interval(); }

    /// OS の TCP キープアライブを使うか（デフォルト: 使う）。接続時にソケットへ設定する
    void setTcpKeepAliveEnabled(bool enabled) { m_tcpKeepAliveEnabled = enabled; }
    bool isTcpKeepAliveEnabled() const { return m_tcpKeepAliveEnabled; }

    /// 対局待ち状態（ログイン済みで対局中でない）か。終局後もここに含む
    static bool isWaitingState(ConnectionState state);

    /**
     * @brief LOGIN に送るパスワード（shogi-server の「ゲーム名[-b|-w],パスワード」）
     * @param gameName 希望するゲーム名（空ならパスワードだけ）
     * @param preferred 希望する手番（-b は先手、-w は後手。無ければ付けない）
     */
    static QString loginPassword(const QString& password, const QString& gameName, std::optional<Turn> preferred);

    /// 対局結果・終局の原因の表示名（「勝ち」「投了」など）
    static QString gameResultText(GameResult result);
    static QString gameEndCauseText(GameEndCause cause);

signals:
    /// 接続状態が変化した時に発行する（→ CsaGameCoordinator::onConnectionStateChanged）
    void connectionStateChanged(CsaClient::ConnectionState state);


    /// エラーが発生した時に発行する（→ CsaGameCoordinator::onClientError）
    void errorOccurred(const QString& message);

    /// 手番や確認待ちのため送信を取りやめた時に発行する（→ CsaGameCoordinator::onCommandRejected）
    void commandRejected(const QString& reason);

    /// ログインに成功した時に発行する（→ CsaGameCoordinator::onLoginSucceeded）
    void loginSucceeded();

    /// ログインに失敗した時に発行する（→ CsaGameCoordinator::onLoginFailed）
    void loginFailed(const QString& reason);

    /// ログアウトが完了した時に発行する（→ CsaGameCoordinator::onLogoutCompleted）
    void logoutCompleted();

    /// 対局条件（Game_Summary）を受信した時に発行する（→ CsaGameCoordinator::onGameSummaryReceived）
    void gameSummaryReceived(const CsaClient::GameSummary& summary);

    /// 対局が開始した時（START:受信時）に発行する（→ CsaGameCoordinator::onGameStarted）
    void gameStarted(const QString& gameId);

    /// 対局が拒否された時に発行する（→ CsaGameCoordinator::onGameRejected）
    void gameRejected(const QString& gameId, const QString& rejector);

    /// 相手の指し手を受信した時に発行する（→ CsaGameCoordinator::onMoveReceived）
    void moveReceived(const QString& move, int consumedTimeMs);

    /// 自分の指し手がサーバーで確認された時に発行する（→ CsaGameCoordinator::onMoveConfirmed）
    void moveConfirmed(const QString& move, int consumedTimeMs);

    /// 対局が終了した時に発行する（→ CsaGameCoordinator::onClientGameEnded）
    void gameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);

    /// 中断が通知された時に発行する（→ CsaGameCoordinator::onGameInterrupted）
    void gameInterrupted();

    /// 生のメッセージを受信した時に発行する（→ CsaGameCoordinator::onRawMessageReceived）
    void rawMessageReceived(const QString& message);

    /// 生のメッセージを送信した時に発行する（→ CsaGameCoordinator::onRawMessageSent）
    void rawMessageSent(const QString& message);

private slots:
    /// ソケット接続時の処理（QTcpSocket::connected に接続）
    void onSocketConnected();

    /// ソケット切断時の処理（QTcpSocket::disconnected に接続）
    void onSocketDisconnected();

    /// ソケットエラー時の処理（QTcpSocket::errorOccurred に接続）
    void onSocketError(QAbstractSocket::SocketError error);

    /// データ受信時の処理（QTcpSocket::readyRead に接続）
    void onReadyRead();

    /// 接続タイムアウト時の処理（m_connectionTimer::timeout に接続）
    void onConnectionTimeout();

    /// LOGOUT:completed が届かなかった時の処理（m_logoutTimer::timeout に接続）
    void onLogoutTimeout();

    /// 対局中のキープアライブ送信（m_keepAliveTimer::timeout に接続）
    void onKeepAliveTimeout();

private:
    /**
     * @brief メッセージを送信する
     * @param message 送信するメッセージ
     * @return 書き込めたら true
     */
    bool sendMessage(const QString& message);

    /**
     * @brief 1行を書き込む（LF を付ける）。送信時刻とキープアライブの起点を更新する
     * @param line 送信する行（空文字列なら LF のみ）
     * @return 書き込めたら true
     */
    bool writeLine(const QString& line);

    /**
     * @brief 受信した1行を処理する
     * @param line 受信行
     */
    void processLine(const QString& line);

    /**
     * @brief ログイン応答を処理する
     * @param line 受信行
     */
    void processLoginResponse(const QString& line);

    /**
     * @brief 対局待ち状態の受信行を処理する（Game_Summary の開始、LOGOUT 応答）
     * @param line 受信行
     */
    void processWaitingLine(const QString& line);

    /**
     * @brief Game_Summaryを処理する
     * @param line 受信行
     */
    void processGameSummary(const QString& line);
    bool handleGameSummarySectionBoundary(const QString& line);
    bool handleGameSummaryTimeSectionLine(const QString& line);
    bool handleGameSummaryPositionSectionLine(const QString& line);
    void processGameSummaryGeneralInfoLine(const QString& line);

    /**
     * @brief 対局中のメッセージを処理する
     * @param line 受信行
     */
    void processGameMessage(const QString& line);

    /**
     * @brief 結果行（#で始まる行）を処理する
     * @param line 受信行
     */
    void processResultLine(const QString& line);

    /**
     * @brief 指し手行を処理する
     * @param line 受信行
     */
    void processMoveLine(const QString& line);

    /**
     * @brief 接続状態を設定する
     * @param state 新しい状態
     */
    void setConnectionState(ConnectionState state);

    /// 接続セッションに紐づく受信/解析状態を初期化する
    void resetSessionState();

    /// 対局の進行状態（手番・確認待ち・終局手の消費時間）を初期化する
    void resetGameProgressState();

    /// ソケットを閉じる（送信済みデータは送り終えてから）
    void closeSocket();

    /// 応答を待たずに接続を破棄する。すぐ再接続する前提で Disconnected は通知しない
    void abortConnection();
    void rejectOversizedInput();

    /// LOGOUT 応答を受けた時の処理
    void handleLogoutResponse();

    /// 対局中に自分の手番でだけ送れるコマンド（指し手・%TORYO など）か
    static bool isTurnBoundCommand(const QString& command);

    /// 接続直後にソケットへ Nagle 無効化と TCP キープアライブを設定する
    void applySocketOptions();

    /**
     * @brief 自分の手番で送れる状態か。送れないときは commandRejected() を発行する
     * @param what 送ろうとした内容（通知文に使う）
     */
    bool ensureCanSendOnMyTurn(const QString& what);

    /**
     * @brief 時間文字列を解析する（例: "T12"から12を取得）
     * @param timeStr 時間文字列
     * @return 消費時間（時間単位）
     */
    int parseConsumedTime(const QString& timeStr) const;

    /// 「,T12」のような消費時間部分から単位時間の値を取り出す。無ければ 0
    int consumedTimeUnitsAfterComma(const QString& line, qsizetype commaPos) const;

private:
    QSslSocket* m_socket;               ///< TLS対応ソケット
    QTimer* m_connectionTimer;          ///< 接続タイムアウト用タイマー
    QTimer* m_logoutTimer;              ///< LOGOUT:completed 待ちタイマー
    QTimer* m_keepAliveTimer;           ///< 対局中のキープアライブタイマー（送信ごとに再起動）
    QElapsedTimer m_lastSendTimer;      ///< 直前の送信からの経過時間（手動キープアライブの間隔規定用）
    ConnectionState m_connectionState = ConnectionState::Disconnected; ///< 接続状態
    QByteArray m_receiveBuffer;         ///< UTF-8デコード前の受信バッファ

    Transport m_transport = Transport::Plain;
    qsizetype m_summaryBytes = 0;
    int m_summaryLines = 0;
    static constexpr qsizetype kMaxLineBytes = 64 * 1024;
    static constexpr qsizetype kMaxSummaryBytes = 2 * 1024 * 1024;
    static constexpr int kMaxSummaryLines = 20000;
    GameSummary m_gameSummary;          ///< 対局情報
    bool m_isMyTurn = false;            ///< 自分の手番かどうか
    bool m_awaitingMoveConfirmation = false; ///< 送った指し手・意思表示の確認待ち
    bool m_logoutPending = false;       ///< LOGOUT 送信済みで応答待ち
    bool m_keepAliveEnabled = true;     ///< 対局中に空行キープアライブを送るか
    bool m_tcpKeepAliveEnabled = true;  ///< OS の TCP キープアライブを使うか
    /// Game_Summary のどの階層を読んでいるか
    enum class SummarySection {
        Outside,    ///< Game_Summary の外
        TopLevel,   ///< Game_Summary の直下
        Time,       ///< BEGIN Time（双方の持時間）
        TimeBlack,  ///< BEGIN Time+（先手の持時間）
        TimeWhite,  ///< BEGIN Time-（後手の持時間）
        Position    ///< BEGIN Position
    };
    SummarySection m_summarySection = SummarySection::Outside;

    QString m_pendingFirstResultLine;   ///< 最初の結果行（#で始まる行）を保持
    int m_endMoveConsumedTimeMs = 0;    ///< 終局手の消費時間（ミリ秒）

    static constexpr int kConnectionTimeoutMs = 10000; ///< 接続タイムアウト（10秒）
    static constexpr int kLogoutTimeoutMs = 3000;      ///< LOGOUT:completed を待つ上限
    /// 空行キープアライブの既定の間隔。仕様は直前の送信から30秒以上空けることを求める
    static constexpr int kKeepAliveIntervalMs = 40000;
    static constexpr int kKeepAliveMinIntervalMs = 30000; ///< キープアライブの最短間隔（仕様 3.4）
    /// OS の TCP キープアライブ: 無通信がこの時間続いたら探査を始め、応答が無ければ間隔ごとに送る
    static constexpr int kTcpKeepAliveIdleSec = 30;
    static constexpr int kTcpKeepAliveIntervalSec = 10;
    static constexpr int kTcpKeepAliveProbeCount = 5;
};

#endif // CSACLIENT_H
