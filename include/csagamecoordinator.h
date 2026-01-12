#ifndef CSAGAMECOORDINATOR_H
#define CSAGAMECOORDINATOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPointer>

#include "csaclient.h"
#include "shogimove.h"

class ShogiGameController;
class ShogiView;
class ShogiClock;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class BoardInteractionController;
class KifuRecordListModel;

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
        ShogiGameController* gameController = nullptr;
        ShogiView* view = nullptr;
        ShogiClock* clock = nullptr;
        BoardInteractionController* boardController = nullptr;
        KifuRecordListModel* recordModel = nullptr;
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

    /**
     * @brief コンストラクタ
     * @param parent 親オブジェクト
     */
    explicit CsaGameCoordinator(QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
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

    /**
     * @brief 現在の対局状態を取得する
     * @return 対局状態
     */
    GameState gameState() const { return m_gameState; }

    /**
     * @brief 自分の手番かどうか
     * @return 自分の手番の場合true
     */
    bool isMyTurn() const;

    /**
     * @brief 先手側かどうか
     * @return 先手側の場合true
     */
    bool isBlackSide() const;

    /**
     * @brief 人間が操作しているかどうか
     * @return 人間操作の場合true
     */
    bool isHumanPlayer() const { return m_playerType == PlayerType::Human; }

    /**
     * @brief 先手の累計消費時間を取得
     * @return 累計消費時間（ミリ秒）
     */
    int blackTotalTimeMs() const { return m_blackTotalTimeMs; }

    /**
     * @brief 後手の累計消費時間を取得
     * @return 累計消費時間（ミリ秒）
     */
    int whiteTotalTimeMs() const { return m_whiteTotalTimeMs; }

    /**
     * @brief GUI側からCSAサーバーへコマンドを送信する
     * @param command 送信するコマンド
     */
    void sendRawCommand(const QString& command);

    /**
     * @brief ユーザー名（GUI側の対局者名）を取得
     * @return ユーザー名
     */
    QString username() const { return m_options.username; }

signals:
    /**
     * @brief 対局状態が変化した時に発行
     * @param state 新しい状態
     */
    void gameStateChanged(GameState state);

    /**
     * @brief エラーが発生した時に発行
     * @param message エラーメッセージ
     */
    void errorOccurred(const QString& message);

    /**
     * @brief 対局が開始した時に発行
     * @param blackName 先手名
     * @param whiteName 後手名
     */
    void gameStarted(const QString& blackName, const QString& whiteName);

    /**
     * @brief 対局が終了した時に発行
     * @param result 結果（"勝ち", "負け", "引き分け"など）
     * @param cause 終了原因
     * @param consumedTimeMs 終局手の消費時間（ミリ秒）
     */
    void gameEnded(const QString& result, const QString& cause, int consumedTimeMs);

    /**
     * @brief 指し手が確定した時に発行
     * @param csaMove CSA形式の指し手
     * @param usiMove USI形式の指し手
     * @param prettyMove 表示用の指し手
     * @param consumedTimeMs 消費時間（ミリ秒）
     */
    void moveMade(const QString& csaMove, const QString& usiMove,
                  const QString& prettyMove, int consumedTimeMs);

    /**
     * @brief 手番が変わった時に発行
     * @param isMyTurn 自分の手番かどうか
     */
    void turnChanged(bool isMyTurn);

    /**
     * @brief 指し手のハイライト表示を要求
     * @param from 移動元座標
     * @param to 移動先座標
     */
    void moveHighlightRequested(const QPoint& from, const QPoint& to);

    /**
     * @brief 対局情報を受信した時に発行
     * @param summary 対局情報
     */
    void gameSummaryReceived(const CsaClient::GameSummary& summary);

    /**
     * @brief ログメッセージを発行
     * @param message ログメッセージ
     * @param isError エラーかどうか
     */
    void logMessage(const QString& message, bool isError = false);

    /**
     * @brief CSA通信ログを発行（送受信両方）
     * @param line ログ行（"▶ " または "◀ " で始まる）
     */
    void csaCommLogAppended(const QString& line);

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
    // CSAクライアントからのシグナルハンドラ
    void onConnectionStateChanged(CsaClient::ConnectionState state);
    void onClientError(const QString& message);
    void onLoginSucceeded();
    void onLoginFailed(const QString& reason);
    void onLogoutCompleted();
    void onGameSummaryReceived(const CsaClient::GameSummary& summary);
    void onGameStarted(const QString& gameId);
    void onGameRejected(const QString& gameId, const QString& rejector);
    void onMoveReceived(const QString& move, int consumedTimeMs);
    void onMoveConfirmed(const QString& move, int consumedTimeMs);
    void onClientGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs);
    void onGameInterrupted();
    void onRawMessageReceived(const QString& message);
    void onRawMessageSent(const QString& message);

    // エンジンからのシグナルハンドラ
    void onEngineBestMoveReceived_();
    void onEngineResign();

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
    // CSAクライアント
    CsaClient* m_client;

    // 依存オブジェクト
    QPointer<ShogiGameController> m_gameController;
    QPointer<ShogiView> m_view;
    QPointer<ShogiClock> m_clock;
    QPointer<BoardInteractionController> m_boardController;
    QPointer<KifuRecordListModel> m_recordModel;

    // エンジン関連
    Usi* m_engine;
    UsiCommLogModel* m_engineCommLog;           ///< USI通信ログモデル（外部から渡されるか内部で作成）
    ShogiEngineThinkingModel* m_engineThinking; ///< 思考モデル（外部から渡されるか内部で作成）
    bool m_ownsCommLog;      ///< m_engineCommLogの所有権フラグ
    bool m_ownsThinking;     ///< m_engineThinkingの所有権フラグ

    // 状態
    GameState m_gameState;
    PlayerType m_playerType;
    StartOptions m_options;

    // 対局情報
    CsaClient::GameSummary m_gameSummary;
    bool m_isBlackSide;         ///< 先手側かどうか
    bool m_isMyTurn;            ///< 自分の手番かどうか
    int m_moveCount;            ///< 指し手数
    int m_blackTotalTimeMs;     ///< 先手累計消費時間（ミリ秒）
    int m_whiteTotalTimeMs;     ///< 後手累計消費時間（ミリ秒）
    int m_prevToFile;           ///< 前の指し手の移動先筋（「同」判定用）
    int m_prevToRank;           ///< 前の指し手の移動先段（「同」判定用）
    
    // 残り時間追跡（CSA通信対局用）
    int m_initialTimeMs;        ///< 初期持ち時間（ミリ秒）
    int m_blackRemainingMs;     ///< 先手残り時間（ミリ秒）
    int m_whiteRemainingMs;     ///< 後手残り時間（ミリ秒）
    int m_resignConsumedTimeMs; ///< 投了時の消費時間（ミリ秒）

    // USIポジション文字列
    QString m_positionStr;      ///< "position sfen ... moves ..."形式
    QStringList m_usiMoves;     ///< USI形式の指し手リスト

    // SFEN/棋譜記録（外部から渡されたポインタを使用）
    QString m_startSfen;        ///< 開始局面SFEN
    QStringList* m_sfenRecord = nullptr;   ///< 局面SFEN記録（MainWindowと共有）
    QVector<ShogiMove>* m_gameMoves = nullptr; ///< 指し手記録（MainWindowと共有）

    // 定数
    static constexpr int kDefaultPort = 4081;
};

#endif // CSAGAMECOORDINATOR_H
