#ifndef USIPROTOCOLHANDLER_H
#define USIPROTOCOLHANDLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QPoint>
#include <QElapsedTimer>
#include <QPointer>

class EngineProcessManager;
class ThinkingInfoPresenter;
class ShogiGameController;

/**
 * @brief USIプロトコルのコマンド送受信を管理するクラス
 *
 * 責務:
 * - USIコマンドの生成と送信
 * - USIレスポンスの解析
 * - 対局フローの制御（go, ponder, bestmove等）
 * - 待機処理（usiok, readyok, bestmove）
 *
 * 単一責任原則（SRP）に基づき、USIプロトコルの処理のみを担当する。
 * プロセス管理はEngineProcessManagerに、GUI更新はThinkingInfoPresenterに委譲する。
 */
class UsiProtocolHandler : public QObject
{
    Q_OBJECT

public:
    // === 定数定義 ===
    
    static constexpr int SENTE_HAND_FILE = 10;
    static constexpr int GOTE_HAND_FILE = 11;
    static constexpr int BOARD_SIZE = 9;
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

    /// 詰み探索結果
    struct TsumeResult {
        enum Kind { Solved, NoMate, NotImplemented, Unknown } kind = Unknown;
        QStringList pvMoves;
    };

    /// 思考フェーズ
    enum class SearchPhase { Idle, Main, Ponder };

    explicit UsiProtocolHandler(QObject* parent = nullptr);
    ~UsiProtocolHandler() override;

    // === 依存関係設定 ===
    
    void setProcessManager(EngineProcessManager* manager);
    void setPresenter(ThinkingInfoPresenter* presenter);
    void setGameController(ShogiGameController* controller);

    // === 初期化 ===
    
    /// エンジン初期化シーケンスを実行
    bool initializeEngine(const QString& engineName);
    
    /// 設定ファイルからオプションを読み込み
    void loadEngineOptions(const QString& engineName);

    // === USIコマンド送信 ===
    
    void sendUsi();
    void sendIsReady();
    void sendUsiNewGame();
    void sendPosition(const QString& positionStr);
    void sendGo(int byoyomiMs, const QString& btime, const QString& wtime,
                int bincMs, int wincMs, bool useByoyomi);
    void sendGoPonder();
    void sendGoMate(int timeMs, bool infinite = false);
    void sendGoDepth(int depth);              ///< go depth <x> - 指定深さまで探索
    void sendGoNodes(qint64 nodes);           ///< go nodes <x> - 指定ノード数まで探索
    void sendGoMovetime(int timeMs);          ///< go movetime <x> - 指定時間(ms)探索
    void sendStop();
    void sendPonderHit();
    void sendGameOver(const QString& result);
    void sendQuit();
    void sendSetOption(const QString& name, const QString& value);
    void sendSetOption(const QString& name, const QString& value, const QString& type);
    void sendRaw(const QString& command);

    // === 待機メソッド ===
    
    bool waitForUsiOk(int timeoutMs = 5000);
    bool waitForReadyOk(int timeoutMs = 5000);
    bool waitForBestMove(int timeoutMs);
    bool waitForBestMoveWithGrace(int budgetMs, int graceMs);
    bool keepWaitingForBestMove();
    void waitForStopOrPonderhit();

    // === 指し手処理 ===
    
    /// bestmoveの座標を解析
    void parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo);
    
    /// 人間の指し手をUSI形式に変換
    QString convertHumanMoveToUsi(const QPoint& from, const QPoint& to, bool promote);
    
    /// 現在のbestmoveを取得
    QString bestMove() const { return m_bestMove; }
    
    /// 予想手を取得
    QString predictedMove() const { return m_predictedOpponentMove; }

    // === 状態管理 ===
    
    bool isResignMove() const { return m_isResignMove; }
    bool isWinMove() const { return m_isWinMove; }
    bool isPonderEnabled() const { return m_isPonderEnabled; }
    SearchPhase currentPhase() const { return m_phase; }
    qint64 lastBestmoveElapsedMs() const { return m_lastGoToBestmoveMs; }

    void setResignMove(bool value) { m_isResignMove = value; }
    void setWinMove(bool value) { m_isWinMove = value; }
    void resetResignNotified() { m_resignNotified = false; }
    void resetWinNotified() { m_winNotified = false; }
    void markHardTimeout() { m_timeoutDeclared = true; }
    void clearHardTimeout() { m_timeoutDeclared = false; }
    bool isTimeoutDeclared() const { return m_timeoutDeclared; }
    
    /// オペレーションをキャンセル
    void cancelCurrentOperation();

    // === 座標変換ユーティリティ ===
    
    static QChar rankToAlphabet(int rank);
    static int alphabetToRank(QChar c);
    QString convertFirstPlayerPieceSymbol(int rankFrom) const;
    QString convertSecondPlayerPieceSymbol(int rankFrom) const;

signals:
    void usiOkReceived();
    void readyOkReceived();
    void bestMoveReceived();
    void bestMoveResignReceived();
    void bestMoveWinReceived();    ///< bestmove win（入玉宣言勝ち）を受信
    void stopOrPonderhitSent();
    void errorOccurred(const QString& message);
    void infoLineReceived(const QString& line);  // info行受信通知
    
    /// 詰将棋結果シグナル
    void checkmateSolved(const QStringList& pvMoves);
    void checkmateNoMate();
    void checkmateNotImplemented();
    void checkmateUnknown();

public slots:
    /// データ受信ハンドラ
    void onDataReceived(const QString& line);

private:
    /// コマンド送信（内部）
    void sendCommand(const QString& command);

    /// bestmove行の処理
    void handleBestMoveLine(const QString& line);
    
    /// checkmate行の処理
    void handleCheckmateLine(const QString& line);
    
    /// 待機中断判定
    bool shouldAbortWait() const;

    /// 指し手解析ヘルパ
    void parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom);
    void parseMoveTo(const QString& move, int& fileTo, int& rankTo);
    int pieceToRankWhite(QChar c);
    int pieceToRankBlack(QChar c);

    /// オペレーションコンテキスト
    quint64 beginOperationContext();

private:
    /// 依存オブジェクト
    EngineProcessManager* m_processManager = nullptr;
    ThinkingInfoPresenter* m_presenter = nullptr;
    ShogiGameController* m_gameController = nullptr;

    /// USI状態フラグ
    bool m_usiOkReceived = false;
    bool m_readyOkReceived = false;
    bool m_bestMoveReceived = false;
    bool m_isResignMove = false;
    bool m_isWinMove = false;         ///< bestmove win（入玉宣言勝ち）を受信
    bool m_isPonderEnabled = false;

    /// 指し手情報
    QString m_bestMove;
    QString m_predictedOpponentMove;

    /// 設定
    QStringList m_setOptionCommands;

    /// 計測
    QElapsedTimer m_goTimer;
    qint64 m_lastGoToBestmoveMs = 0;
    static constexpr int kBestmoveGraceMs = 250;

    /// フェーズ管理
    SearchPhase m_phase = SearchPhase::Idle;
    int m_ponderSession = 0;

    /// 状態管理
    bool m_resignNotified = false;
    bool m_winNotified = false;       ///< 入玉宣言勝ち通知済みフラグ
    bool m_timeoutDeclared = false;
    bool m_modeTsume = false;

    /// オペレーションコンテキスト
    QPointer<QObject> m_opCtx { nullptr };
    quint64 m_seq { 0 };

    /// 駒変換マップ（関数でアクセス）
    static const QMap<int, QString>& firstPlayerPieceMap();
    static const QMap<int, QString>& secondPlayerPieceMap();
    static const QMap<QChar, int>& pieceRankWhite();
    static const QMap<QChar, int>& pieceRankBlack();
};

#endif // USIPROTOCOLHANDLER_H
