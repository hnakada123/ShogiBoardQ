#ifndef CSACLIENT_H
#define CSACLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

/**
 * @brief CSAプロトコルクライアントクラス
 *
 * CSAサーバーとの通信を担当するクラス。
 * shogi-server (https://github.com/TadaoYamaoka/shogi-server) との
 * 通信対局を行うためのCSAプロトコル1.2.1を実装。
 *
 * 責務:
 * - TCP/IP接続の確立と管理
 * - CSAプロトコルメッセージの送受信
 * - ログイン/ログアウト処理
 * - 対局の合意処理（AGREE/REJECT）
 * - 指し手の送受信
 */
class CsaClient : public QObject
{
    Q_OBJECT

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
        GameOver            ///< 対局終了
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
     * @brief 対局情報を格納する構造体
     */
    struct GameSummary {
        QString protocolVersion;    ///< プロトコルバージョン
        QString protocolMode;       ///< プロトコルモード（Server/Direct）
        QString format;             ///< フォーマット（Shogi 1.0）
        QString declaration;        ///< 宣言ルール
        QString gameId;             ///< 対局ID
        QString blackName;          ///< 先手名
        QString whiteName;          ///< 後手名
        QString myTurn;             ///< こちら側の手番（+/-）
        QString toMove;             ///< 最初の手番（+/-）
        bool rematchOnDraw;         ///< 引き分け時再対局するか
        int maxMoves;               ///< 最大手数（0=無制限）

        // 時間情報
        QString timeUnit;           ///< 時間単位（1sec/1min/1msec）
        int totalTime;              ///< 持時間（単位時間）
        int byoyomi;                ///< 秒読み（単位時間）
        int leastTimePerMove;       ///< 最少消費時間
        int increment;              ///< 加算時間
        int delay;                  ///< 遅延時間
        bool timeRoundup;           ///< 切り上げ

        // 局面情報（CSA形式）
        QStringList positionLines;  ///< 初期局面（P1-P9, P+, P-, +/-）
        QStringList moves;          ///< 初期局面からの指し手リスト

        // 先手/後手の個別時間（省略時は共通）
        bool hasIndividualTime;     ///< 個別時間設定があるか
        int totalTimeBlack;         ///< 先手持時間
        int totalTimeWhite;         ///< 後手持時間
        int byoyomiBlack;           ///< 先手秒読み
        int byoyomiWhite;           ///< 後手秒読み

        GameSummary();
        void clear();
        bool isBlackTurn() const { return myTurn == QStringLiteral("+"); }
        int timeUnitMs() const;     ///< 時間単位をミリ秒に変換
    };

    /**
     * @brief コンストラクタ
     * @param parent 親オブジェクト
     */
    explicit CsaClient(QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~CsaClient() override;

    // ========== 接続管理 ==========

    /**
     * @brief サーバーに接続する
     * @param host ホスト名またはIPアドレス
     * @param port ポート番号
     */
    void connectToServer(const QString& host, int port);

    /**
     * @brief サーバーから切断する
     */
    void disconnectFromServer();

    /**
     * @brief 現在の接続状態を取得する
     * @return 接続状態
     */
    ConnectionState connectionState() const { return m_connectionState; }

    /**
     * @brief 接続中かどうか
     * @return 接続中の場合true
     */
    bool isConnected() const;

    // ========== ログイン/ログアウト ==========

    /**
     * @brief サーバーにログインする
     * @param username ユーザー名
     * @param password パスワード
     */
    void login(const QString& username, const QString& password);

    /**
     * @brief サーバーからログアウトする
     */
    void logout();

    // ========== 対局制御 ==========

    /**
     * @brief 対局条件に同意する
     * @param gameId 対局ID（省略時は直前の対局）
     */
    void agree(const QString& gameId = QString());

    /**
     * @brief 対局条件を拒否する
     * @param gameId 対局ID（省略時は直前の対局）
     */
    void reject(const QString& gameId = QString());

    /**
     * @brief 指し手を送信する
     * @param move CSA形式の指し手（例: "+7776FU"）
     */
    void sendMove(const QString& move);

    /**
     * @brief 投了する
     */
    void resign();

    /**
     * @brief 勝利宣言する（入玉）
     */
    void declareWin();

    /**
     * @brief 中断を要請する
     */
    void requestChudan();

    /**
     * @brief 現在の対局情報を取得する
     * @return 対局情報
     */
    const GameSummary& gameSummary() const { return m_gameSummary; }

    /**
     * @brief 自分の手番かどうか
     * @return 自分の手番の場合true
     */
    bool isMyTurn() const { return m_isMyTurn; }

    /**
     * @brief CSAプロトコルバージョンを設定する
     * @param version バージョン文字列
     */
    void setCsaVersion(const QString& version) { m_csaVersion = version; }

signals:
    /**
     * @brief 接続状態が変化した時に発行
     * @param state 新しい接続状態
     */
    void connectionStateChanged(ConnectionState state);

    /**
     * @brief エラーが発生した時に発行
     * @param message エラーメッセージ
     */
    void errorOccurred(const QString& message);

    /**
     * @brief ログインに成功した時に発行
     */
    void loginSucceeded();

    /**
     * @brief ログインに失敗した時に発行
     * @param reason 失敗理由
     */
    void loginFailed(const QString& reason);

    /**
     * @brief ログアウトが完了した時に発行
     */
    void logoutCompleted();

    /**
     * @brief 対局条件を受信した時に発行
     * @param summary 対局情報
     */
    void gameSummaryReceived(const GameSummary& summary);

    /**
     * @brief 対局が開始した時に発行
     * @param gameId 対局ID
     */
    void gameStarted(const QString& gameId);

    /**
     * @brief 対局が拒否された時に発行
     * @param gameId 対局ID
     * @param rejector 拒否したプレイヤー名
     */
    void gameRejected(const QString& gameId, const QString& rejector);

    /**
     * @brief 相手の指し手を受信した時に発行
     * @param move CSA形式の指し手（例: "+7776FU"）
     * @param consumedTimeMs 消費時間（ミリ秒）
     */
    void moveReceived(const QString& move, int consumedTimeMs);

    /**
     * @brief 自分の指し手が確認された時に発行
     * @param move CSA形式の指し手
     * @param consumedTimeMs 消費時間（ミリ秒）
     */
    void moveConfirmed(const QString& move, int consumedTimeMs);

    /**
     * @brief 対局が終了した時に発行
     * @param result 対局結果
     * @param cause 終了原因
     */
    void gameEnded(GameResult result, GameEndCause cause);

    /**
     * @brief 中断が通知された時に発行
     */
    void gameInterrupted();

    /**
     * @brief 生のメッセージを受信した時に発行（デバッグ用）
     * @param message 受信メッセージ
     */
    void rawMessageReceived(const QString& message);

    /**
     * @brief 生のメッセージを送信した時に発行（デバッグ用）
     * @param message 送信メッセージ
     */
    void rawMessageSent(const QString& message);

private slots:
    /**
     * @brief ソケット接続時の処理
     */
    void onSocketConnected();

    /**
     * @brief ソケット切断時の処理
     */
    void onSocketDisconnected();

    /**
     * @brief ソケットエラー時の処理
     * @param error エラーコード
     */
    void onSocketError(QAbstractSocket::SocketError error);

    /**
     * @brief データ受信時の処理
     */
    void onReadyRead();

    /**
     * @brief 接続タイムアウト時の処理
     */
    void onConnectionTimeout();

private:
    /**
     * @brief メッセージを送信する
     * @param message 送信するメッセージ
     */
    void sendMessage(const QString& message);

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
     * @brief Game_Summaryを処理する
     * @param line 受信行
     */
    void processGameSummary(const QString& line);

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

    /**
     * @brief 時間文字列を解析する（例: "T12"から12を取得）
     * @param timeStr 時間文字列
     * @return 消費時間（時間単位）
     */
    int parseConsumedTime(const QString& timeStr) const;

private:
    QTcpSocket* m_socket;               ///< TCPソケット
    QTimer* m_connectionTimer;          ///< 接続タイムアウト用タイマー
    ConnectionState m_connectionState;  ///< 接続状態
    QString m_receiveBuffer;            ///< 受信バッファ

    QString m_username;                 ///< ログインユーザー名
    QString m_csaVersion;               ///< CSAプロトコルバージョン

    GameSummary m_gameSummary;          ///< 対局情報
    bool m_isMyTurn;                    ///< 自分の手番かどうか
    bool m_inGameSummary;               ///< Game_Summary解析中フラグ
    bool m_inTimeSection;               ///< Time解析中フラグ
    bool m_inPositionSection;           ///< Position解析中フラグ
    QString m_currentTimeSection;       ///< 現在のTimeセクション名（Time/Time+/Time-）

    QString m_pendingFirstResultLine;   ///< 最初の結果行（#で始まる行）を保持
    int m_moveCount;                    ///< 指し手カウント

    static constexpr int kConnectionTimeoutMs = 10000; ///< 接続タイムアウト（10秒）
};

#endif // CSACLIENT_H
