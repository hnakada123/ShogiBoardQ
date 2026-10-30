#ifndef USIPROTOCOLHANDLER_H
#define USIPROTOCOLHANDLER_H

/// @file usiprotocolhandler.h
/// @brief USI エンジン1台の状態とコマンドの送受信

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <optional>

#include "shogitypes.h"
#include "usigotime.h"
#include "engineprofilestore.h"
#include "usiinfo.h"
#include "usimessages.h"
#include "usiposition.h"

class EngineProcessManager;

/**
 * @brief USI エンジン1台の状態機械
 *
 * エンジンの状態（起動待ち → 初期化（usi〜readyok）→ 待機 → 探索 → 停止待ち）をこの1か所で持ち、
 * 状態に合うコマンドだけを送る。受信行は UsiLineParser で型付きの値にしてシグナルで知らせる。
 * bestmove・checkmate には、その探索の文脈（経過時間・最後の評価値・先読みか・stop の後か）を添える。
 *
 * 取り消した探索（cancelCurrentOperation・abandonSearch の後）に遅れて届いた結果と info は知らせない。
 * プロセスの起動と終了と、探索の切り替え（前の探索を止めてから次を始める）は UsiEngineSession が行う。
 */
class UsiProtocolHandler : public QObject
{
    Q_OBJECT

public:
    /// エンジンの状態
    enum class State {
        NotStarted,   ///< プロセスがない（終了した・終了させた・初期化に失敗した）
        Starting,     ///< プロセスの起動を待っている
        Handshaking,  ///< usi〜usiok〜setoption〜isready〜readyok
        Ready,        ///< 探索していない
        Searching,    ///< go を送り、結果（bestmove・checkmate）を待っている
        Stopping,     ///< stop を送り、結果を待っている
    };

    /// 探索の種類（Searching・Stopping のとき）
    enum class SearchKind { Main, Ponder, Mate };

    explicit UsiProtocolHandler(EngineProcessManager* process = nullptr, QObject* parent = nullptr);
    ~UsiProtocolHandler() override;

    // --- 状態 ---

    State state() const { return m_state; }
    SearchKind searchKind() const { return m_searchKind; }
    /// 起動待ちか初期化中か（探索の要求は初期化が終わるまで待たせる）
    bool isInitializing() const { return m_state == State::Starting || m_state == State::Handshaking; }
    /// 探索の結果を待っているか（stop を送った後も含む）
    bool isSearching() const { return m_state == State::Searching || m_state == State::Stopping; }

    // --- 起動と初期化（UsiEngineSession から使う） ---

    /// 初期化で送るオプションと先読みの設定（保存した設定は EngineProfileStore::launchOptions が読む）
    void setLaunchOptions(const EngineProfileStore::LaunchOptions& options) { m_launchOptions = options; }

    /**
     * @brief この接続だけで送るオプションを設定する（保存済みの設定の後に送り、同名なら上書きする）
     *
     * 登録済みの設定は変えずに、用途に必要な値（詰み探索の最短手数の確認など）を指定する。
     * 保存済みの設定と同じく、エンジンが報告したオプションだけを送る。
     */
    void setSessionOptions(const QList<QPair<QString, QString>>& options) { m_sessionOptions = options; }

    /// プロセスの起動を待つ状態にする
    void beginStartup();
    /**
     * @brief usi を送って初期化を始める（応答を timeoutMs ずつ待つ）
     * @param untilUsiOk usiok で終える（エンジンの登録で名前とオプションだけを聞く）。false なら readyok まで
     */
    void initializeEngineAsync(int timeoutMs = 5000, bool untilUsiOk = false);
    /// プロセスを止めた（状態を NotStarted に戻す）
    void markStopped();

    /// usi〜usiok でエンジンが報告した id name・id author（未受信なら空）
    QString reportedEngineName() const { return m_reportedEngineName; }
    QString reportedEngineAuthor() const { return m_reportedEngineAuthor; }
    /// usi〜usiok でエンジンが報告した option 行（報告の順。読めない行も含む）
    QStringList reportedOptionLines() const { return m_reportedOptionLines; }
    /// エンジンが報告したオプションの定義（報告がなければ nullopt）
    std::optional<EngineOption> reportedOption(const QString& name) const;
    /// GUI 側で先読みを許しているか（エンジンのオプションの報告とは別）
    bool isPonderEnabled() const { return m_launchOptions.ponderEnabled; }

    // --- 探索 ---

    /**
     * @brief 局面を送って探索を始める（全種類の go の入口）
     * @return 局面が空なら何も送らず false
     *
     * 探索の文脈（経過時間・評価値）をここで新しくする。先読み（UsiGo::ponder）の局面は、予想した相手の手を
     * 指した局面を渡す（UsiMatchHandler が作る）。
     * 探索中に呼ばない（切り替えは UsiEngineSession::search が前の探索の結果を待ってから行う）。
     */
    bool startSearch(const UsiPosition& position, const UsiGo& go);
    /// 探索中なら stop を送る（停止待ちの間はもう一度は送らない）
    void sendStop();
    /// 今の探索をやめる。探索中なら stop を送り、その結果は知らせない（停止待ちなら知らせないだけにする）
    void abandonSearch();
    /// 先読みが当たった。先読みの探索を本探索にする（停止待ち・回収済みなら送らない）
    void sendPonderHit();

    // --- そのほかのコマンド ---

    void sendUsi();
    void sendIsReady();
    void sendUsiNewGame();
    void sendGameOver(GameOverResult result);
    void sendSetOption(const QString& name, const QString& value);
    /// 利用者が入力した任意のコマンド（USIコンソール）をそのまま送る。アプリの処理は型付きの送信メソッドを使う
    void sendConsoleCommand(const QString& command);

    /// 取り消す。探索・初期化の結果を待つのをやめ、遅れて届いた結果は知らせない
    void cancelCurrentOperation();

signals:
    /// 探索を始めた（思考情報を消す）
    void searchStarted();
    /// 読み筋を表示する局面（探索の局面と、そこに至った直前の手）が変わった
    void positionChanged(const UsiPosition& position);
    /// 初期化（readyok まで）が終わった。失敗したときは先に errorOccurred が届く
    void initializationFinished(bool success);

    void bestMoveReceived(const UsiBestMove& result);
    void checkmateReceived(const UsiCheckmate& result);
    /// 探索の結果（知らせなかった取り消した探索の結果も）を受け、探索していない状態に戻った。
    /// 結果の知らせ（bestMoveReceived・checkmateReceived）の後に届く
    void searchEnded();
    void infoReceived(const UsiInfo& info);
    void errorOccurred(const QString& message);

public slots:
    /// エンジンからの1行
    void onDataReceived(const QString& line);

private slots:
    void onInitializationTimeout();

private:
    void sendCommand(const QString& command);
    void sendConfiguredOptions();
    void setState(State state);
    void handleUsiOk();
    void handleReadyOk();
    void handleBestMove(const QString& line);
    void handleCheckmate(const UsiCheckmate& result);
    /// 結果が取り消した探索のものか（取り消した後に遅れて届いた）
    bool isStaleResult() const;
    /// 探索が終わった（結果を受け取った）
    void finishSearch();
    /// 最善の読み筋（multipv 1）の評価値を覚える（bestmove に添える）
    void updateSearch(const UsiInfo& info);

    EngineProcessManager* m_process = nullptr;  ///< 非所有

    State m_state = State::NotStarted;
    SearchKind m_searchKind = SearchKind::Main;
    enum class HandshakeStep { UsiOk, ReadyOk };
    HandshakeStep m_handshakeStep = HandshakeStep::UsiOk;
    bool m_untilUsiOk = false;        ///< usiok で初期化を終える
    QTimer m_initializationTimer;
    int m_initializationTimeoutMs = 5000;

    // --- 探索の文脈 ---
    QString m_baseSfen;               ///< 最後に送った探索局面
    QElapsedTimer m_goTimer;          ///< go（先読みの的中は ponderhit）からの経過時間
    UsiScore m_searchScore;           ///< この探索の最善の読み筋の最後の評価値

    // --- 取り消し ---
    quint64 m_seq = 0;                ///< 操作の通番（取り消すと進む）
    quint64 m_activeSearchSeq = 0;    ///< 今の探索を始めたときの通番

    // --- 初期化で送るオプションとエンジンの報告 ---
    EngineProfileStore::LaunchOptions m_launchOptions;   ///< 保存した設定で送るオプションと先読みの設定
    QList<QPair<QString, QString>> m_sessionOptions;
    QHash<QString, EngineOption> m_reportedOptions;
    QStringList m_reportedOptionLines;
    QString m_reportedEngineName;
    QString m_reportedEngineAuthor;
};

#endif // USIPROTOCOLHANDLER_H
