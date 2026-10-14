#ifndef USIPROTOCOLHANDLER_H
#define USIPROTOCOLHANDLER_H

/// @file usiprotocolhandler.h
/// @brief USIプロトコル送受信管理クラスの定義

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QHash>
#include <QPair>
#include <QMap>
#include <QPoint>
#include <QElapsedTimer>
#include <QTimer>
#include <optional>

#include "shogitypes.h"
#include "usimove.h"
#include "usigotime.h"
#include "usiinfo.h"

class EngineProcessManager;

/**
 * @brief USIプロトコルのコマンド送受信を管理するクラス
 *
 * 責務:
 * - USIコマンドの生成と送信
 * - USIレスポンスの解析
 * - 対局フローの制御（go, ponder, bestmove等）
 * - 非同期初期化と応答通知（usiok, readyok, bestmove）
 *
 * プロセス管理はEngineProcessManagerに委譲し、受信結果はシグナルで通知する。
 *
 */
class UsiProtocolHandler : public QObject
{
    Q_OBJECT

public:
    /// 思考フェーズ
    enum class SearchPhase {
        Idle,   ///< 待機中
        Main,   ///< 本探索中（go送信後）
        Ponder, ///< 先読み中（go ponder送信後）
        StoppingPonder ///< 先読みのstop送信後、bestmove回収待ち
    };

    /// 現在の探索の最善候補。履歴表示の遅延バッファとは独立して受信時に更新する。
    struct SearchCandidate {
        QString baseSfen;
        QString move;
        QString predictedMove;
        bool active = false;
        bool pondering = false;
    };
    const SearchCandidate& searchCandidate() const { return m_searchCandidate; }

    explicit UsiProtocolHandler(QObject* parent = nullptr);
    ~UsiProtocolHandler() override;

    // --- 依存関係設定 ---
    
    void setProcessManager(EngineProcessManager* manager);
    void setBaseSfen(const QString& sfen);

    // --- 初期化 ---
    
    /// エンジン初期化シーケンスを実行
    
    void initializeEngineAsync(int timeoutMs = 5000);

    /// 設定ファイルからオプションを読み込み
    void loadEngineOptions(const QString& engineName);

    /**
     * @brief この接続だけで送るオプションを設定する（保存済みの設定の後に送り、同名なら上書きする）
     *
     * 登録済みの設定は変えずに、用途に必要な値（詰み探索の最短手数の確認など）を指定する。
     * 保存済みの設定と同じく、エンジンが報告したオプションだけを送る。
     */
    void setSessionOptions(const QList<QPair<QString, QString>>& options) { m_sessionOptions = options; }

    /// usi〜usiok でエンジンが報告した id name（未受信なら空）
    QString reportedEngineName() const { return m_reportedEngineName; }

    /// エンジンが報告したオプションの定義行（"option name ..."。報告がなければ空）
    QString reportedOptionLine(const QString& name) const { return m_reportedOptionLines.value(name); }

    // --- USIコマンド送信 ---
    
    void sendUsi();
    void sendIsReady();
    void sendUsiNewGame();
    void sendPosition(const QString& positionStr);
    void sendGo(const UsiGoTime& goTime);
    void sendGoPonder(const UsiGoTime& goTime);
    void sendGoMate(int timeMs, bool infinite = false);
    void sendGoDepth(int depth);              ///< go depth <x> - 指定深さまで探索
    void sendGoMovetime(int timeMs);          ///< go movetime <x> - 指定時間(ms)探索
    void sendGoInfinite();                    ///< go infinite - stop まで探索（検討・棋譜解析）

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

    /// 利用者が入力した任意のコマンド（USIコンソール）をそのまま送る。アプリの処理は型付きの送信メソッドを使う
    void sendConsoleCommand(const QString& command);

    // --- 指し手処理 ---

    /// 受け取った bestmove を指し手にする。特殊手（投了など）は nullopt、形式が違えばエラーを出して nullopt
    std::optional<UsiMove> parseBestMove();

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

    void setSquelchResignLogging(bool on) { m_squelchResignLogging = on; }
    bool isResignLoggingSquelched() const { return m_squelchResignLogging; }
    void resetResignNotified() { m_resignNotified = false; }
    void resetWinNotified() { m_winNotified = false; }
    void markHardTimeout() { m_timeoutDeclared = true; }
    void clearHardTimeout() { m_timeoutDeclared = false; }
    bool isTimeoutDeclared() const { return m_timeoutDeclared; }
    
    /// オペレーションをキャンセル
    void cancelCurrentOperation();

signals:
    void searchCandidateChanged();
    void searchStarted();
    void positionChanged(const QString& sfen);
    void initializationFinished(bool success);

    void usiOkReceived();              ///< usiok受信
    void readyOkReceived();            ///< readyok受信
    void bestMoveReceived();           ///< bestmove受信（Handler → Usi/MatchCoordinator）
    void bestMoveResignReceived();     ///< bestmove resign受信（Handler → MatchCoordinator）
    void bestMoveWinReceived();        ///< bestmove win 入玉宣言勝ち受信（Handler → MatchCoordinator）
    void stopOrPonderhitSent();        ///< stop/ponderhit送信完了
    void errorOccurred(const QString& message); ///< エラー発生（Handler → Usi）
    void infoReceived(const UsiInfo& info);    ///< info行受信（Handler → ThinkingInfoPresenter / AnalysisCoordinator）

    // --- 詰将棋結果 ---
    void checkmateSolved(const QStringList& pvMoves); ///< 詰みあり（Handler → TsumeSearchFlowController）
    void checkmateNoMate();            ///< 不詰（Handler → TsumeSearchFlowController）
    void checkmateNotImplemented();    ///< 未対応（Handler → TsumeSearchFlowController）
    void checkmateUnknown();           ///< 不明・時間切れ（Handler → TsumeSearchFlowController）

public slots:
    /// データ受信ハンドラ
    void onDataReceived(const QString& line);

private slots:
    void onInitializationUsiOk();
    void onInitializationReadyOk();
    void onInitializationTimeout();

private:
    void sendConfiguredOptions();
    enum class Initialization { Idle, UsiOk, ReadyOk };
    Initialization m_initialization = Initialization::Idle;
    QTimer m_initializationTimer;
    int m_initializationTimeoutMs = 5000;

    /// コマンド送信（内部）
    void sendCommand(const QString& command);

    /// bestmove行の処理（有効な行を処理した場合true）
    [[nodiscard]] bool handleBestMoveLine(const QString& line);
    
    /// checkmate行の処理
    void handleCheckmateLine(const QString& line);
    

    /// go本探索の共通前処理
    void beginMainSearch();
    void beginCandidateSearch(bool pondering);
    void invalidateCandidate();
    void updateCandidate(const UsiInfo& info);
    SearchCandidate m_searchCandidate;


    /// オペレーションコンテキスト
    quint64 beginOperationContext();

private:
    // --- 依存オブジェクト ---
    EngineProcessManager* m_processManager = nullptr;  ///< プロセス管理（非所有）
    QString m_baseSfen; ///< 最後に送信した探索局面

    // --- USI状態フラグ ---
    bool m_bestMoveReceived = false;  ///< bestmove受信済み
    SpecialMove m_specialMove = SpecialMove::None; ///< 特殊手（投了/入玉宣言勝ち等）
    bool m_isPonderEnabled = false;   ///< GUI側の先読み許可（オプション報告有無とは独立）

    // --- 指し手情報 ---
    QString m_bestMove;                ///< 最善手（USI形式）
    QString m_predictedOpponentMove;   ///< 予想応手（ponder用）

    // --- 設定 ---
    QStringList m_setOptionCommands;   ///< 初期化時に送信するsetoptionコマンド群
    QSet<QString> m_reportedOptions;   ///< エンジンが報告したオプション名（usi〜usiok間）
    QHash<QString, QString> m_reportedOptionLines; ///< オプション名 → 定義行（usi〜usiok間）
    QString m_reportedEngineName;      ///< エンジンが報告した id name
    QList<QPair<QString, QString>> m_sessionOptions; ///< この接続だけで送るオプション
    bool m_sendUnreportedPonderOption = true; ///< 未報告のUSI_Ponderにも設定を通知

    // --- 計測 ---
    QElapsedTimer m_goTimer;           ///< go送信からbestmoveまでの経過時間計測
    qint64 m_lastGoToBestmoveMs = 0;  ///< 直近のgo→bestmove経過時間(ms)

    // --- フェーズ管理 ---
    SearchPhase m_phase = SearchPhase::Idle; ///< 現在の思考フェーズ

    // --- 状態管理 ---
    bool m_resignNotified = false;     ///< 投了シグナル発行済み（重複防止）
    bool m_winNotified = false;        ///< 入玉宣言勝ちシグナル発行済み（重複防止）
    bool m_squelchResignLogging = false; ///< resign 通知/ログ抑止
    bool m_timeoutDeclared = false;    ///< ハードタイムアウト宣言済み

    // --- オペレーションコンテキスト ---
    quint64 m_seq { 0 };               ///< オペレーション通番（キャンセル検出用）
    quint64 m_activeSearchSeq { 0 };   ///< 直近の go 系探索に対応する通番

};

#endif // USIPROTOCOLHANDLER_H
