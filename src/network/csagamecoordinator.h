#ifndef CSAGAMECOORDINATOR_H
#define CSAGAMECOORDINATOR_H

/// @file csagamecoordinator.h
/// @brief CSA通信対局コーディネータクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPointer>
#include <memory>

#include "csaclient.h"
#include "shogimove.h"

class CsaMoveProgressHandler;

class KifuRecordListModel;
class ShogiGameController;
class ShogiView;
class ShogiClock;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class BoardInteractionController;
class CsaEngineController;

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
        int port;
        QString username;
        QString password;
        QString csaVersion;
        PlayerType playerType;
        QString enginePath;
        QString engineName;
        int engineNumber;
    };

    explicit CsaGameCoordinator(QObject* parent = nullptr);
    ~CsaGameCoordinator() override;

    void setDependencies(const Dependencies& deps);
    void startGame(const StartOptions& options);
    void stopGame();

    GameState gameState() const { return m_gameState; }
    bool isMyTurn() const;
    bool isBlackSide() const;
    bool isHumanPlayer() const { return m_playerType == PlayerType::Human; }
    int blackTotalTimeMs() const { return m_blackTotalTimeMs; }
    int whiteTotalTimeMs() const { return m_whiteTotalTimeMs; }

    void sendRawCommand(const QString& command);
    QString username() const { return m_options.username; }

signals:
    void gameStateChanged(CsaGameCoordinator::GameState state);
    void errorOccurred(const QString& message);
    void gameStarted(const QString& blackName, const QString& whiteName);
    void gameEnded(const QString& result, const QString& cause, int consumedTimeMs);
    void moveMade(const QString& csaMove, const QString& usiMove,
                  const QString& prettyMove, int consumedTimeMs);
    void turnChanged(bool isMyTurn);
    void moveHighlightRequested(const QPoint& from, const QPoint& to);
    void logMessage(const QString& message, bool isError = false);
    void csaCommLogAppended(const QString& line);
    void engineScoreUpdated(int scoreCp, int ply);

public slots:
    void onHumanMove(const QPoint& from, const QPoint& to, bool promote);
    void onResign();

private slots:
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
    void onEngineControllerResign();

private:
    void setGameState(GameState state);
    void setupInitialPosition();
    void setupClock();
    void performResign();
    void cleanup();
    void ensureMoveProgressHandler();

    CsaClient* m_client;
    CsaEngineController* m_engineController = nullptr;

    QPointer<ShogiGameController> m_gameController;
    QPointer<ShogiView> m_view;
    QPointer<ShogiClock> m_clock;
    QPointer<BoardInteractionController> m_boardController;
    QPointer<KifuRecordListModel> m_recordModel;

    // エンジン初期化用（CsaEngineControllerに渡す）
    UsiCommLogModel* m_usiCommLog = nullptr;
    ShogiEngineThinkingModel* m_engineThinking = nullptr;

    GameState m_gameState = GameState::Idle;
    PlayerType m_playerType = PlayerType::Human;
    StartOptions m_options;

    CsaClient::GameSummary m_gameSummary;
    bool m_isBlackSide = true;
    bool m_isMyTurn = false;
    int m_moveCount = 0;
    int m_blackTotalTimeMs = 0;
    int m_whiteTotalTimeMs = 0;
    int m_prevToFile = 0;
    int m_prevToRank = 0;

    int m_initialTimeMs = 0;
    int m_blackRemainingMs = 0;
    int m_whiteRemainingMs = 0;
    int m_resignConsumedTimeMs = 0;

    QString m_positionStr;
    QStringList m_usiMoves;

    QString m_startSfen;
    QStringList* m_sfenHistory = nullptr;
    QList<ShogiMove>* m_gameMoves = nullptr;

    std::unique_ptr<CsaMoveProgressHandler> m_moveProgressHandler;

    static constexpr int kDefaultPort = 4081;
};

#endif // CSAGAMECOORDINATOR_H
