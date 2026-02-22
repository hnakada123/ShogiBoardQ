#ifndef CSAGAMECOORDINATOR_H
#define CSAGAMECOORDINATOR_H

/// @file csagamecoordinator.h
/// @brief CSA通信対局コーディネータクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPointer>

#include "csaclient.h"
#include "shogimove.h"
#include "kifurecordlistmodel.h"

class ShogiGameController;
class ShogiView;
class ShogiClock;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class BoardInteractionController;

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
 * - 対局フローの制御（接続→ログイン→対局→終了）
 * - CSA形式とUSI形式の指し手変換
 * - 時間管理のGUIへの反映
 */
class CsaGameCoordinator : public QObject
{
    Q_OBJECT

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
        GameOver,           ///< 対局終了
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
        ShogiGameController* gameController = nullptr; ///< 対局制御（非所有）
        ShogiView* view = nullptr;                         ///< 盤面ビュー（非所有）
        ShogiClock* clock = nullptr;                       ///< 時計（非所有）
        BoardInteractionController* boardController = nullptr; ///< 盤面操作制御（非所有）
        KifuRecordListModel* recordModel = nullptr;        ///< 棋譜リストモデル（非所有）
        QStringList* sfenRecord = nullptr;           ///< SFEN記録（MainWindowと共有）
        QVector<ShogiMove>* gameMoves = nullptr;     ///< 指し手記録（MainWindowと共有）
        UsiCommLogModel* usiCommLog = nullptr;       ///< USI通信ログモデル（MainWindowと共有）
        ShogiEngineThinkingModel* engineThinking = nullptr; ///< エンジン思考モデル（MainWindowと共有）
    };

    /**
     * @brief 対局開始オプション
     */
    struct StartOptions {
        QString host;           ///< サーバーホスト
        int port;               ///< サーバーポート
        QString username;       ///< ユーザー名
        QString password;       ///< パスワード
        QString csaVersion;     ///< CSAプロトコルバージョン
        PlayerType playerType;  ///< こちら側の対局者タイプ
        QString enginePath;     ///< エンジンパス（エンジン使用時）
        QString engineName;     ///< エンジン名（エンジン使用時）
        int engineNumber;       ///< エンジン番号（エンジン使用時）
    };

    explicit CsaGameCoordinator(QObject* parent = nullptr);

    ~CsaGameCoordinator() override;

    /**
     * @brief 依存オブジェクトを設定する
     * @param deps 依存オブジェクト
     */
    void setDependencies(const Dependencies& deps);

    /**
     * @brief CSA通信対局を開始する
     * @param options 対局開始オプション
     */
    void startGame(const StartOptions& options);

    /**
     * @brief 対局を中断する
     */
    void stopGame();

    GameState gameState() const { return m_gameState; }

    bool isMyTurn() const;

    bool isBlackSide() const;

    bool isHumanPlayer() const { return m_playerType == PlayerType::Human; }

    int blackTotalTimeMs() const { return m_blackTotalTimeMs; }

    int whiteTotalTimeMs() const { return m_whiteTotalTimeMs; }

    /**
     * @brief GUI側からCSAサーバーへコマンドを送信する
     * @param command 送信するコマンド
     */
    void sendRawCommand(const QString& command);

    QString username() const { return m_options.username; }

signals:
    /**
     * @brief 対局状態が変化した時に発行（→ CsaGameWiring::onCsaGameStateChanged）
     * @param state 新しい状態
     */
    void gameStateChanged(CsaGameCoordinator::GameState state);

    /**
     * @brief エラーが発生した時に発行（→ CsaGameWiring::onCsaGameError）
     * @param message エラーメッセージ
     */
    void errorOccurred(const QString& message);

    /**
     * @brief 対局が開始した時に発行（→ CsaGameWiring::onCsaGameStarted）
     * @param blackName 先手名
     * @param whiteName 後手名
     */
    void gameStarted(const QString& blackName, const QString& whiteName);

    /**
     * @brief 対局が終了した時に発行（→ CsaGameWiring::onCsaGameEnded）
     * @param result 結果（"勝ち", "負け", "引き分け"など）
     * @param cause 終了原因
     * @param consumedTimeMs 終局手の消費時間（ミリ秒）
     */
    void gameEnded(const QString& result, const QString& cause, int consumedTimeMs);

    /**
     * @brief 指し手が確定した時に発行（→ CsaGameWiring::onCsaMoveMade）
     * @param csaMove CSA形式の指し手
     * @param usiMove USI形式の指し手
     * @param prettyMove 表示用の指し手
     * @param consumedTimeMs 消費時間（ミリ秒）
     */
    void moveMade(const QString& csaMove, const QString& usiMove,
                  const QString& prettyMove, int consumedTimeMs);

    /**
     * @brief 手番が変わった時に発行（→ CsaGameWiring::onCsaTurnChanged）
     * @param isMyTurn 自分の手番かどうか
     */
    void turnChanged(bool isMyTurn);

    /**
     * @brief 指し手のハイライト表示を要求（→ CsaGameWiring::onCsaMoveHighlight）
     * @param from 移動元座標
     * @param to 移動先座標
     */
    void moveHighlightRequested(const QPoint& from, const QPoint& to);

    /**
     * @brief 対局情報を受信した時に発行（→ CsaGameWiring::onCsaGameSummaryReceived）
     * @param summary 対局情報
     */
    void gameSummaryReceived(const CsaClient::GameSummary& summary);

    /**
     * @brief ログメッセージを発行（→ CsaGameWiring::onCsaLogMessage）
     * @param message ログメッセージ
     * @param isError エラーかどうか
     */
    void logMessage(const QString& message, bool isError = false);

    /**
     * @brief CSA通信ログを発行（送受信両方）（→ CsaGameWiring::onCsaCommLogAppended）
     * @param line ログ行（"▶ " または "◀ " で始まる）
     */
    void csaCommLogAppended(const QString& line);

    /**
     * @brief エンジンの評価値が更新された時に発行（→ CsaGameWiring::onCsaEngineScoreUpdated）
     * @param scoreCp 評価値（センチポーン）
     * @param ply 手数
     */
    void engineScoreUpdated(int scoreCp, int ply);

public slots:
    /**
     * @brief 人間の指し手を処理する
     * @param from 移動元
     * @param to 移動先
     * @param promote 成るかどうか
     */
    void onHumanMove(const QPoint& from, const QPoint& to, bool promote);

    /**
     * @brief 投了する
     */
    void onResign();

private slots:
    /// CsaClient::connectionStateChanged に接続
    void onConnectionStateChanged(CsaClient::ConnectionState state);
    void onClientError(const QString& message);       ///< CsaClient::errorOccurred に接続
    void onLoginSucceeded();                              ///< CsaClient::loginSucceeded に接続
    void onLoginFailed(const QString& reason);             ///< CsaClient::loginFailed に接続
    void onLogoutCompleted();                              ///< CsaClient::logoutCompleted に接続
    void onGameSummaryReceived(const CsaClient::GameSummary& summary); ///< CsaClient::gameSummaryReceived に接続
    void onGameStarted(const QString& gameId);             ///< CsaClient::gameStarted に接続
    void onGameRejected(const QString& gameId, const QString& rejector); ///< CsaClient::gameRejected に接続
    void onMoveReceived(const QString& move, int consumedTimeMs);   ///< CsaClient::moveReceived に接続
    void onMoveConfirmed(const QString& move, int consumedTimeMs);  ///< CsaClient::moveConfirmed に接続
    void onClientGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs); ///< CsaClient::gameEnded に接続
    void onGameInterrupted();                              ///< CsaClient::gameInterrupted に接続
    void onRawMessageReceived(const QString& message);     ///< CsaClient::rawMessageReceived に接続
    void onRawMessageSent(const QString& message);         ///< CsaClient::rawMessageSent に接続

    void onEngineBestMoveReceived();  ///< Usi::bestMoveReceived に接続
    void onEngineResign();             ///< Usi::bestMoveResignReceived に接続

private:
    /**
     * @brief 対局状態を設定する
     * @param state 新しい状態
     */
    void setGameState(GameState state);

    /**
     * @brief CSA形式の指し手をUSI形式に変換する
     * @param csaMove CSA形式の指し手（例: "+7776FU"）
     * @return USI形式の指し手（例: "7g7f"）
     */
    QString csaToUsi(const QString& csaMove) const;

    /**
     * @brief USI形式の指し手をCSA形式に変換する
     * @param usiMove USI形式の指し手（例: "7g7f"）
     * @param isBlack 先手の指し手かどうか
     * @return CSA形式の指し手（例: "+7776FU"）
     */
    QString usiToCsa(const QString& usiMove, bool isBlack) const;

    /**
     * @brief 盤面座標からCSA形式の座標に変換する
     * @param from 移動元
     * @param to 移動先
     * @param promote 成るかどうか
     * @return CSA形式の指し手
     */
    QString boardToCSA(const QPoint& from, const QPoint& to, bool promote) const;

    /**
     * @brief 駒文字（SFEN形式）をCSA形式の駒文字に変換する
     * @param pieceChar 駒文字（P, L, N, S, G, B, R, K, +P等）
     * @param promote 成るかどうか
     * @return CSA形式の駒名（FU, KY, KE, GI, KI, KA, HI, OU, TO等）
     */
    QString pieceCharToCsa(QChar pieceChar, bool promote) const;

    /**
     * @brief CSA形式の指し手を表示用文字列に変換する
     * @param csaMove CSA形式の指し手
     * @param isPromotion 成る手かどうか（デフォルトはfalse）
     * @return 表示用文字列（例: "▲７六歩"）
     */
    QString csaToPretty(const QString& csaMove, bool isPromotion = false) const;

    /**
     * @brief 初期局面をセットアップする
     */
    void setupInitialPosition();

    /**
     * @brief 時計を初期化する
     */
    void setupClock();

    /**
     * @brief エンジンを初期化する
     */
    void initializeEngine();

    /**
     * @brief エンジンに思考を開始させる
     */
    void startEngineThinking();

    /**
     * @brief 指し手を盤面に適用する
     * @param csaMove CSA形式の指し手
     * @return 成功した場合true
     */
    bool applyMoveToBoard(const QString& csaMove);

    /**
     * @brief 駒記号をCSA形式からUSI形式に変換
     * @param csaPiece CSA形式の駒記号（例: "FU"）
     * @return USI形式の駒記号（例: "P"）
     */
    QString csaPieceToUsi(const QString& csaPiece) const;

    /**
     * @brief 駒記号をUSI形式からCSA形式に変換
     * @param usiPiece USI形式の駒記号（例: "P"）
     * @param promoted 成り駒かどうか
     * @return CSA形式の駒記号（例: "FU"）
     */
    QString usiPieceToCsa(const QString& usiPiece, bool promoted) const;

    /**
     * @brief 駒記号を表示用漢字に変換
     * @param csaPiece CSA形式の駒記号
     * @return 漢字表記（例: "歩"）
     */
    QString csaPieceToKanji(const QString& csaPiece) const;

    /**
     * @brief CSA駒種から駒台インデックスを取得
     * @param csaPiece CSA形式の駒記号
     * @return 駒台インデックス（1=歩, 2=香, ...）
     */
    int pieceTypeFromCsa(const QString& csaPiece) const;

    /**
     * @brief CSA駒種をSFEN駒文字に変換
     * @param csaPiece CSA形式の駒記号
     * @param isBlack 先手の駒かどうか
     * @return SFEN形式の駒文字
     */
    QChar csaPieceToSfenChar(const QString& csaPiece, bool isBlack) const;

    /**
     * @brief クリーンアップ処理
     */
    void cleanup();

private:
    CsaClient* m_client;                              ///< CSAプロトコルクライアント（thisが親、所有）

    QPointer<ShogiGameController> m_gameController;   ///< 対局制御（非所有）
    QPointer<ShogiView> m_view;                          ///< 盤面ビュー（非所有）
    QPointer<ShogiClock> m_clock;                        ///< 時計（非所有）
    QPointer<BoardInteractionController> m_boardController; ///< 盤面操作制御（非所有）
    QPointer<KifuRecordListModel> m_recordModel;          ///< 棋譜リストモデル（非所有）

    Usi* m_engine = nullptr;                               ///< USIエンジン（thisが親、所有）
    UsiCommLogModel* m_engineCommLog = nullptr;     ///< USI通信ログモデル（外部から渡されるか内部で作成）
    ShogiEngineThinkingModel* m_engineThinking = nullptr; ///< 思考モデル（外部から渡されるか内部で作成）
    bool m_ownsCommLog = false;  ///< m_engineCommLogの所有権フラグ
    bool m_ownsThinking = false; ///< m_engineThinkingの所有権フラグ

    GameState m_gameState = GameState::Idle;            ///< 現在の対局状態
    PlayerType m_playerType = PlayerType::Human;        ///< こちら側の対局者タイプ
    StartOptions m_options;                             ///< 対局開始時のオプション

    CsaClient::GameSummary m_gameSummary;              ///< サーバーから受信した対局情報
    bool m_isBlackSide = true;      ///< 先手側かどうか
    bool m_isMyTurn = false;        ///< 自分の手番かどうか
    int m_moveCount = 0;            ///< 指し手数
    int m_blackTotalTimeMs = 0;     ///< 先手累計消費時間（ミリ秒）
    int m_whiteTotalTimeMs = 0;     ///< 後手累計消費時間（ミリ秒）
    int m_prevToFile = 0;           ///< 前の指し手の移動先筋（「同」判定用）
    int m_prevToRank = 0;           ///< 前の指し手の移動先段（「同」判定用）

    // --- 残り時間追跡（CSA通信対局用） ---
    int m_initialTimeMs = 0;        ///< 初期持ち時間（ミリ秒）
    int m_blackRemainingMs = 0;     ///< 先手残り時間（ミリ秒）
    int m_whiteRemainingMs = 0;     ///< 後手残り時間（ミリ秒）
    int m_resignConsumedTimeMs = 0; ///< 投了時の消費時間（ミリ秒）

    // --- USIポジション文字列 ---
    QString m_positionStr;      ///< "position sfen ... moves ..."形式
    QStringList m_usiMoves;     ///< USI形式の指し手リスト

    // --- SFEN/棋譜記録 ---
    QString m_startSfen;        ///< 開始局面SFEN
    QStringList* m_sfenHistory = nullptr;   ///< 局面SFEN記録（MainWindowと共有）
    QVector<ShogiMove>* m_gameMoves = nullptr; ///< 指し手記録（MainWindowと共有）

    static constexpr int kDefaultPort = 4081;          ///< CSAサーバーのデフォルトポート
};

#endif // CSAGAMECOORDINATOR_H
