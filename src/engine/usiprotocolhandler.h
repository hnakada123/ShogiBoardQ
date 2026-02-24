#ifndef USIPROTOCOLHANDLER_H
#define USIPROTOCOLHANDLER_H

/// @file usiprotocolhandler.h
/// @brief USIプロトコル送受信管理クラスの定義

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QMap>
#include <QPoint>
#include <QElapsedTimer>
#include <QPointer>
#include <optional>

#include "shogitypes.h"

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
 * プロセス管理はEngineProcessManagerに、GUI更新はThinkingInfoPresenterに委譲する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class UsiProtocolHandler : public QObject
{
    Q_OBJECT

public:
    // --- 定数定義 ---
    
    static constexpr int SENTE_HAND_FILE = 10;
    static constexpr int GOTE_HAND_FILE = 11;
    static constexpr int BOARD_SIZE = 9;
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

    /// 詰み探索結果
    struct TsumeResult {
        enum Kind {
            Solved,          ///< 詰みあり
            NoMate,          ///< 不詰
            NotImplemented,  ///< エンジン未対応
            Unknown          ///< 不明・タイムアウト等
        } kind = Unknown;    ///< 結果種別
        QStringList pvMoves; ///< 詰み手順（Solved時のみ有効）
    };

    /// 思考フェーズ
    enum class SearchPhase {
        Idle,   ///< 待機中
        Main,   ///< 本探索中（go送信後）
        Ponder  ///< 先読み中（go ponder送信後）
    };

    explicit UsiProtocolHandler(QObject* parent = nullptr);
    ~UsiProtocolHandler() override;

    // --- 依存関係設定 ---
    
    void setProcessManager(EngineProcessManager* manager);
    void setPresenter(ThinkingInfoPresenter* presenter);
    void setGameController(ShogiGameController* controller);

    // --- 初期化 ---
    
    /// エンジン初期化シーケンスを実行
    bool initializeEngine(const QString& engineName);
    
    /// 設定ファイルからオプションを読み込み
    void loadEngineOptions(const QString& engineName);

    // --- USIコマンド送信 ---
    
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

    /// go searchmoves <move1> ... <movei> [infinite] - 指定した手のみを探索
    /// @param moves 探索対象の手（USI形式、例: "7g7f", "8h2b+"）
    /// @param infinite trueの場合 "go searchmoves ... infinite" を送信
    void sendGoSearchmoves(const QStringList& moves, bool infinite = true);

    /// go searchmoves <move1> ... <movei> depth <x> - 指定した手のみを指定深さまで探索
    void sendGoSearchmovesDepth(const QStringList& moves, int depth);

    /// go searchmoves <move1> ... <movei> movetime <x> - 指定した手のみを指定時間探索
    void sendGoSearchmovesMovetime(const QStringList& moves, int timeMs);

    void sendStop();
    void sendPonderHit();
    void sendGameOver(GameOverResult result);
    void sendQuit();
    void sendSetOption(const QString& name, const QString& value);
    void sendSetOption(const QString& name, const QString& value, const QString& type);
    void sendRaw(const QString& command);

    // --- 待機メソッド ---
    
    bool waitForUsiOk(int timeoutMs = 5000);
    bool waitForReadyOk(int timeoutMs = 5000);
    bool waitForBestMove(int timeoutMs);
    bool waitForBestMoveWithGrace(int budgetMs, int graceMs);
    bool keepWaitingForBestMove();
    void waitForStopOrPonderhit();

    // --- 指し手処理 ---
    
    /// bestmoveの座標を解析
    void parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo);
    
    /// 人間の指し手をUSI形式に変換
    QString convertHumanMoveToUsi(const QPoint& from, const QPoint& to, bool promote);
    
    /// 現在のbestmoveを取得
    QString bestMove() const { return m_bestMove; }
    
    /// 予想手を取得
    QString predictedMove() const { return m_predictedOpponentMove; }

    // --- 状態管理 ---
    
    bool isResignMove() const { return m_specialMove == SpecialMove::Resign; }
    bool isWinMove() const { return m_specialMove == SpecialMove::Win; }
    SpecialMove specialMove() const { return m_specialMove; }
    bool isPonderEnabled() const { return m_isPonderEnabled; }
    SearchPhase currentPhase() const { return m_phase; }
    qint64 lastBestmoveElapsedMs() const { return m_lastGoToBestmoveMs; }

    void setSpecialMove(SpecialMove sm) { m_specialMove = sm; }
    void resetResignNotified() { m_resignNotified = false; }
    void resetWinNotified() { m_winNotified = false; }
    void markHardTimeout() { m_timeoutDeclared = true; }
    void clearHardTimeout() { m_timeoutDeclared = false; }
    bool isTimeoutDeclared() const { return m_timeoutDeclared; }
    
    /// オペレーションをキャンセル
    void cancelCurrentOperation();

    // --- 座標変換ユーティリティ ---
    
    static QChar rankToAlphabet(int rank);
    static std::optional<int> alphabetToRank(QChar c);
    QString convertFirstPlayerPieceSymbol(int rankFrom) const;
    QString convertSecondPlayerPieceSymbol(int rankFrom) const;

signals:
    void usiOkReceived();              ///< usiok受信（Handler → waitForUsiOk）
    void readyOkReceived();            ///< readyok受信（Handler → waitForReadyOk）
    void bestMoveReceived();           ///< bestmove受信（Handler → Usi/MatchCoordinator）
    void bestMoveResignReceived();     ///< bestmove resign受信（Handler → MatchCoordinator）
    void bestMoveWinReceived();        ///< bestmove win 入玉宣言勝ち受信（Handler → MatchCoordinator）
    void stopOrPonderhitSent();        ///< stop/ponderhit送信完了（Handler → waitForStopOrPonderhit）
    void errorOccurred(const QString& message); ///< エラー発生（Handler → Usi）
    void infoLineReceived(const QString& line); ///< info行受信（Handler → AnalysisCoordinator）

    // --- 詰将棋結果 ---
    void checkmateSolved(const QStringList& pvMoves); ///< 詰みあり（Handler → TsumeSearchFlowController）
    void checkmateNoMate();            ///< 不詰（Handler → TsumeSearchFlowController）
    void checkmateNotImplemented();    ///< 未対応（Handler → TsumeSearchFlowController）
    void checkmateUnknown();           ///< 不明（Handler → TsumeSearchFlowController）

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
    // --- 依存オブジェクト ---
    EngineProcessManager* m_processManager = nullptr;  ///< プロセス管理（非所有）
    ThinkingInfoPresenter* m_presenter = nullptr;      ///< 思考情報表示（非所有）
    ShogiGameController* m_gameController = nullptr;   ///< 対局制御（非所有）

    // --- USI状態フラグ ---
    bool m_usiOkReceived = false;     ///< usiok受信済み
    bool m_readyOkReceived = false;   ///< readyok受信済み
    bool m_bestMoveReceived = false;  ///< bestmove受信済み
    SpecialMove m_specialMove = SpecialMove::None; ///< 特殊手（投了/入玉宣言勝ち等）
    bool m_isPonderEnabled = false;   ///< USI_Ponderが有効

    // --- 指し手情報 ---
    QString m_bestMove;                ///< 最善手（USI形式）
    QString m_predictedOpponentMove;   ///< 予想応手（ponder用）

    // --- 設定 ---
    QStringList m_setOptionCommands;   ///< 初期化時に送信するsetoptionコマンド群
    QSet<QString> m_reportedOptions;   ///< エンジンが報告したオプション名（usi〜usiok間）

    // --- 計測 ---
    QElapsedTimer m_goTimer;           ///< go送信からbestmoveまでの経過時間計測
    qint64 m_lastGoToBestmoveMs = 0;  ///< 直近のgo→bestmove経過時間(ms)
    static constexpr int kBestmoveGraceMs = 250; ///< bestmove待ちの猶予時間(ms)

    // --- フェーズ管理 ---
    SearchPhase m_phase = SearchPhase::Idle; ///< 現在の思考フェーズ
    int m_ponderSession = 0;           ///< ponderセッション番号（重複防止用）

    // --- 状態管理 ---
    bool m_resignNotified = false;     ///< 投了シグナル発行済み（重複防止）
    bool m_winNotified = false;        ///< 入玉宣言勝ちシグナル発行済み（重複防止）
    bool m_timeoutDeclared = false;    ///< ハードタイムアウト宣言済み
    bool m_modeTsume = false;          ///< 詰将棋探索モード

    // --- オペレーションコンテキスト ---
    QPointer<QObject> m_opCtx { nullptr }; ///< 現在のオペレーション（所有、キャンセル時にdelete）
    quint64 m_seq { 0 };               ///< オペレーション通番（キャンセル検出用）

    // --- 駒変換マップ（関数でアクセス） ---
    static const QMap<int, QString>& firstPlayerPieceMap();
    static const QMap<int, QString>& secondPlayerPieceMap();
    static const QMap<QChar, int>& pieceRankWhite();
    static const QMap<QChar, int>& pieceRankBlack();
};

#endif // USIPROTOCOLHANDLER_H
