#ifndef MATCHCOORDINATOR_H
#define MATCHCOORDINATOR_H

#include <QObject>
#include <QString>
#include <functional>
#include <QStringList>
#include <QVector>
#include <QDateTime>
#include <QElapsedTimer>

#include "shogigamecontroller.h"
#include "shogimove.h"
#include "playmode.h"

class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiGameController;
class ShogiClock;
class ShogiView;
class Usi;
class KifuRecordListModel;
class BoardInteractionController;

// 対局進行/終局/時計/USI送受のハブ（寿命は Main 側で管理）
class MatchCoordinator : public QObject {
    Q_OBJECT

public:
    enum Player : int { P1 = 1, P2 = 2 };

    // ★ BreakOff を追加（中断終局）
    enum class Cause : int { Resignation = 0, Timeout = 1, BreakOff = 2 };

    struct GoTimes {
        qint64 btime = 0;   // 先手 残り(ms)
        qint64 wtime = 0;   // 後手 残り(ms)
        qint64 byoyomi = 0; // 共通 秒読み(ms)
        qint64 binc = 0;    // 先手 増加(ms)
        qint64 winc = 0;    // 後手 増加(ms)
    };

    struct GameEndInfo {
        Cause  cause = Cause::Resignation;
        Player loser = P1;
    };

    // 直近の終局状態（既存仕様を維持）
    struct GameOverState {
        bool        isOver       = false;
        bool        moveAppended = false;
        bool        hasLast      = false;
        bool        lastLoserIsP1= false;
        GameEndInfo lastInfo;
        QDateTime   when;
    };

    struct Hooks {
        // --- UI/描画系の委譲 ---
        std::function<void(Player cur)> updateTurnDisplay; // 盤面の手番表示/ハイライト
        std::function<void(const QString& p1, const QString& p2)> setPlayersNames;
        std::function<void(const QString& e1, const QString& e2)> setEngineNames;
        std::function<void(bool inProgress)> setGameActions; // NewGame/Resign 等のON/OFF
        std::function<void()> renderBoardFromGc;             // gc→view 反映
        std::function<void(const QString& title, const QString& message)> showGameOverDialog;
        std::function<void(const QString& msg)> log;         // 任意ログ
        // 追加：直前の着手（移動元→先）のハイライト
        std::function<void(const QPoint& from, const QPoint& to)> showMoveHighlights;

        // --- 時計読み出し（ShogiClock API 差異吸収） ---
        std::function<qint64(Player)> remainingMsFor; // 残り時間
        std::function<qint64(Player)> incrementMsFor; // フィッシャー増加
        std::function<qint64()> byoyomiMs;            // 秒読み（共通）

        // HvE: 人間側の手番（P1/P2）を返す（finishHumanTimer...で使用）
        std::function<Player()> humanPlayerSide = nullptr;

        // --- USI 送受（go/stop/任意生コマンド） ---
        std::function<void(Usi* which, const GoTimes& t)> sendGoToEngine;
        std::function<void(Usi* which)> sendStopToEngine;
        std::function<void(Usi* which, const QString& cmd)> sendRawToEngine; // 任意

        // --- 新規対局の初期化（GUI固有処理） ---
        std::function<void(const QString& sfenStart)> initializeNewGame;

        // ★ 棋譜1行追記（例：text="▲７六歩", elapsed="00:03/00:00:06"）
        std::function<void(const QString& text, const QString& elapsed)> appendKifuLine;

        std::function<void()> appendEvalP1; // P1(先手)エンジンが着手確定 → 評価値を1本目に追記
        std::function<void()> appendEvalP2; // P2(後手)エンジンが着手確定 → 評価値を2本目に追記
    };

    struct Deps {
        ShogiGameController* gc = nullptr;
        ShogiClock*          clock = nullptr;
        ShogiView*           view = nullptr;
        Usi*                 usi1 = nullptr;
        Usi*                 usi2 = nullptr;
        UsiCommLogModel*           comm1 = nullptr;
        ShogiEngineThinkingModel*  think1 = nullptr;
        UsiCommLogModel*           comm2 = nullptr;
        ShogiEngineThinkingModel*  think2 = nullptr;
        Hooks                hooks;
    };

    explicit MatchCoordinator(const Deps& deps, QObject* parent=nullptr);
    ~MatchCoordinator() override;

    // 外部 API
    void startNewGame(const QString& sfenStart);
    void handleResign();                 // 人間の投了
    void handleEngineResign(int idx);    // エンジン投了通知(1 or 2)
    void notifyTimeout(Player loser);    // ★ 時間切れ通知（UI→司令塔）
    void flipBoard();                    // 盤反転 + 表示更新
    void onTurnFinishedAndSwitch();      // 手番切替時（時計/UI更新 + go送信）
    void updateUsiPtrs(Usi* e1, Usi* e2);// エンジン再生成時などに差し替え
    void handleBreakOff();               // ★ 中断（UI→司令塔）

    // === 時間管理（MainWindowから移譲） ===
    struct TimeControl {
        bool useByoyomi    = false;
        int  byoyomiMs1    = 0;
        int  byoyomiMs2    = 0;
        int  incMs1        = 0;
        int  incMs2        = 0;
        bool loseOnTimeout = false;
    };

    void setTimeControlConfig(bool useByoyomi,
                              int byoyomiMs1, int byoyomiMs2,
                              int incMs1,     int incMs2,
                              bool loseOnTimeout);
    const TimeControl& timeControl() const;

    // HvH/HvE: 1手の開始エポック管理（KIF表示用）
    void   markTurnEpochNowFor(Player side, qint64 nowMs = -1);
    qint64 turnEpochFor(Player side) const;
    void   resetTurnEpochs();

    // HvH: ターン計測
    void armTurnTimerIfNeeded();
    void finishTurnTimerAndSetConsiderationFor(Player mover);

    // HvE: 人間の計測
    void armHumanTimerIfNeeded();
    void finishHumanTimerAndSetConsideration();
    void disarmHumanTimerIfNeeded();

    // USI時間
    void computeGoTimesForUSI(qint64& outB, qint64& outW) const;
    void refreshGoTimes();
    int  computeMoveBudgetMsForCurrentTurn() const;

    // --- public: Main からの委譲で呼ばれるAPIを追加 ---
public:
    // MainWindow::initializePositionStringsForMatch_ の置き換え
    void initializePositionStringsForStart(const QString& sfenStart);

    // MainWindow::startInitialEngineMoveIfNeeded_ / startInitialEngineMoveEvH_ の置き換え
    void startInitialEngineMoveIfNeeded();

    void onHumanMove_HvE(const QPoint& humanFrom, const QPoint& humanTo);

    // 追加：人間の整形済み棋譜文字列（prettyMove）を受け取る版
    void onHumanMove_HvE(const QPoint& humanFrom, const QPoint& humanTo, const QString& prettyMove);

public:
    // --- 既存の Deps 等はそのまま ---

    // UNDO に必要な参照（ランタイム状態を司令塔に集約）
    struct UndoRefs {
        KifuRecordListModel*          recordModel      = nullptr;
        QVector<ShogiMove>*           gameMoves        = nullptr;
        QStringList*                  positionStrList  = nullptr;  // 可：null
        QStringList*                  sfenRecord       = nullptr;  // 可：null
        int*                          currentMoveIndex = nullptr;

        ShogiGameController*          gc       = nullptr;
        BoardInteractionController*   boardCtl = nullptr;
        ShogiClock*                   clock    = nullptr;
        ShogiView*                    view     = nullptr;
    };

    // MainWindow に残す UI/雑務（評価値巻戻し/ハイライト/表示更新/ガード）のフック群
    struct UndoHooks {
        std::function<bool()>                         getMainRowGuard;               // 省略可
        std::function<void(bool)>                     setMainRowGuard;               // 省略可
        std::function<void(int /*ply*/)>              updateHighlightsForPly;        // 推奨
        std::function<void()>                         updateTurnAndTimekeepingDisplay; // 推奨
        std::function<void(int /*moveNumber*/)>       handleUndoMove;                // 推奨（評価値等）
        std::function<bool(ShogiGameController::Player)> isHumanSide;                // 必須
        std::function<bool()>                         isHvH;                         // 必須（H2H 共有タイマか）
    };

    // MainWindow から一度セット
    void setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks);

    // ← これが MainWindow::undoLastTwoMoves の移行先
    bool undoTwoPlies();

private slots:
    void armTimerAfterUndo_();

private:
    bool tryRemoveLastItems_(QObject* model, int n);
    bool m_isUndoInProgress = false;
    UndoRefs  u_;
    UndoHooks h_;

// --- private: 内部ヘルパ ---
private:
    void initPositionStringsFromSfen_(const QString& sfenBase);
    void startInitialEngineMoveFor_(Player engineSide);  // 先手/後手どちらでも1手だけ指す

signals:
    void gameOverShown();
    void boardFlipped(bool nowFlipped);
    void gameStarted();
    void gameEnded(const GameEndInfo& info);

    // MainWindow はこれを受けて m_bTime/m_wTime を表示用に更新するだけ
    void timesForUSIUpdated(qint64 bMs, qint64 wMs);

    // p1ms/p2ms: 両者の残りミリ秒
    // p1turn   : 現在手番が先手なら true
    // urgencyMs: 盤の緊急カラー用 ms（非緊急なら std::numeric_limits<qint64>::max()）
    void timeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);

private:
    // NOTE: QPointer は使わず raw ポインタ（寿命は Main 側で管理）
    ShogiGameController* m_gc   = nullptr;
    ShogiClock*          m_clock= nullptr;
    ShogiView*           m_view = nullptr;
    Usi*                 m_usi1 = nullptr;
    Usi*                 m_usi2 = nullptr;
    Hooks                m_hooks;

    Player m_cur = P1;

private:
    // Main から段階的に移す関数群
    void setPlayersNamesForMode_();
    void setEngineNamesBasedOnMode_();
    void setGameInProgressActions_(bool inProgress);
    void renderShogiBoard_();
    void updateTurnDisplay_(Player p);

    // 時計/USI 時間計算
    GoTimes computeGoTimes_() const;

    // 終局処理
    void stopClockAndSendStops_();
    void displayResultsAndUpdateGui_(const GameEndInfo& info);

public:
    // MainWindow からエンジン初期化/開始を委譲するための統一API
    // side: P1 or P2
    void initializeAndStartEngineFor(Player side,
                                     const QString& enginePathIn,
                                     const QString& engineNameIn);

    // resign シグナルの配線（司令塔→自分の onXxx に直結）
    void wireResignSignals();

    // ゲームオーバー（win+quit）を一方のエンジンに送る
    void sendGameOverWinAndQuitTo(int idx); // idx: 1 or 2

    // エンジン破棄（片方 or 両方）
    void destroyEngine(int idx);   // idx: 1 or 2
    void destroyEngines();

    // （移行ステップ用）ポインタ参照が必要な場合に備えたアクセサ
    Usi* enginePtr(int idx) const; // 1 or 2 → Usi*

private:
    // resign 配線の実装（内部ユーティリティ）
    void wireResignToArbiter_(Usi* engine, bool asP1);

    // 保有エンジンから index を求める（1/2/0）
    int indexForEngine_(const Usi* p) const;

    // 共通ロジック：時計と手番を読んで timeUpdated(...) を発火
    void emitTimeUpdateFromClock_();

private slots:
    void onEngine1Resign();
    void onEngine2Resign();
    void kickNextEvETurn_();  // EvE を1手ずつ進める
    void onClockTick_();

    void onUsiBestmoveDuringTsume_(const QString& bestmove);

    void onUsiError_(const QString& msg);

public slots:
    // 即時に現在値で timeUpdated(...) を発火（UIをすぐ同期させたい時に使う）
    void pokeTimeUpdateNow();

public:
    // ↓↓↓ 追加（PlayMode を司令塔に設定）
    void setPlayMode(PlayMode m);

    // ↓↓↓ 追加（EvE 用のエンジン生成・配線・初期化）
    void initEnginesForEvE(const QString& engineName1,
                           const QString& engineName2);

    // ↓↓↓ 追加（エンジン手進行ロジックの集約）
    bool engineThinkApplyMove(Usi* engine,
                              QString& positionStr,
                              QString& ponderStr,
                              QPoint* outFrom,
                              QPoint* outTo);

    bool engineMoveOnce(Usi* eng,
                        QString& positionStr,
                        QString& ponderStr,
                        bool /*useSelectedField2*/,
                        int engineIndex,
                        QPoint* outTo);

    bool playOneEngineTurn(Usi* mover,
                           Usi* receiver,
                           QString& positionStr,
                           QString& ponderStr,
                           int engineIndex);

    // ↓↓↓ 追加（PlayMode に応じた WIN+QUIT 通知を内包）
    void sendGameOverWinAndQuit();

private:
    PlayMode                 m_playMode = NotStarted;
    UsiCommLogModel*         m_comm1    = nullptr;
    ShogiEngineThinkingModel*m_think1   = nullptr;
    UsiCommLogModel*         m_comm2    = nullptr;
    ShogiEngineThinkingModel*m_think2   = nullptr;

public:
    // 対局開始のためのオプション
    struct StartOptions {
        PlayMode mode = NotStarted;
        QString  sfenStart;

        // エンジン情報（必要なら空でもOK）
        QString engineName1;
        QString enginePath1;
        QString engineName2;
        QString enginePath2;

        // HvE の場合、エンジンがどちら側か
        bool engineIsP1 = false;
        bool engineIsP2 = false;
    };

    // 対局開始フローを一元化
    void configureAndStart(const StartOptions& opt);

    // ==== 追加：検討用オプション ====
    struct AnalysisOptions {
        QString  enginePath;   // 検討に使うエンジン実行ファイル
        QString  engineName;   // 表示用エンジン名
        QString  positionStr;  // "position sfen ... [moves ...]" の完全文字列
        int      byoyomiMs = 0;           // 0=無制限、>0=秒→ms
        PlayMode mode      = ConsidarationMode; // 既定で検討モード
    };

    // ==== 追加：検討API ====
    void startAnalysis(const AnalysisOptions& opt);
    void stopAnalysis();
    bool isAnalysisActive() const;

    // ==== 追加：棋譜解析 継続API ====
    // 既に startAnalysis() 済みの単発エンジンを使い回し、次の position を送って解析を継続する（途中では quit しない）
    void continueAnalysis(const QString& positionStr, int byoyomiMs);

public:
    Usi* primaryEngine() const;   // HvE/EvH で司令塔が使う主エンジン（これまで m_usi1 に相当）
    Usi* secondaryEngine() const; // ★ 追加

private:
    // 内部ヘルパ
    void startHumanVsHuman_(const StartOptions& opt);
    void startHumanVsEngine_(const StartOptions& opt, bool engineIsP1);
    void initPositionStringsForEvE_();        // ← 新規ヘルパ
    void startEngineVsEngine_(const StartOptions& /*opt*/);

private:
    // USI "position ... moves" の作業用バッファ
    QString m_positionStr1, m_positionPonder1;
    QString m_positionStr2, m_positionPonder2;

    // 棋譜／SFEN 記録（ShogiGameController::validateAndMove の引数用）
    int m_currentMoveIndex = 0;
    QStringList m_sfenRecord;
    QVector<ShogiMove> m_gameMoves;
    // EvE 専用の棋譜保持（MainWindow から独立）
    QStringList        m_eveSfenRecord;
    QVector<ShogiMove> m_eveGameMoves;
    int                m_eveMoveIndex = 0;

private:
    // 「その手の開始」エポック（KIFの消費時間計算に使用）
    qint64 m_turnEpochP1Ms = -1;
    qint64 m_turnEpochP2Ms = -1;

    // USI表記用の直近文字列（MainWindow側はラベル更新のみ）
    QString m_bTimeStr, m_wTimeStr;

    // 時間ルール（byoyomi / increment / timeout）
    TimeControl m_tc;
    // === ここから MainWindow から移した状態 ===
    // ターン計測（HvH）
    QElapsedTimer m_turnTimer;
    bool          m_turnTimerArmed  = false;

    // 人間側の計測（HvE）
    QElapsedTimer m_humanTurnTimer;
    bool          m_humanTimerArmed = false;

public:
    // 既存 Deps に clock が入ってこない場合に備えて、後から差し替え可能に
    void setClock(ShogiClock* clock);

private:
    void wireClock_();           // ★追加：時計と onClockTick_ の connect を一元化
    void unwireClock_();         // ★追加：既存接続の解除
    QMetaObject::Connection m_clockConn; // ★追加：接続ハンドル保持

public:
    // ---- [GameOver 統合API] ----
    const GameOverState& gameOverState() const { return m_gameOver; }
    void clearGameOverState();  // 対局開始/局面編集開始などで呼ぶ
    void setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce = true);
    void markGameOverMoveAppended(); // MainWindow が棋譜に一意追記後に通知

signals:
    // 既存: gameEnded(const GameEndInfo& info) はそのまま活用
    void gameOverStateChanged(const GameOverState& st);      // 参照側UI向け
    void requestAppendGameOverMove(const GameEndInfo& info); // 必要なら司令塔から一意追記をリクエスト

private:
    // ...（既存）
    GameOverState m_gameOver;

    void startTsumeSearch(const QString& sfen, int timeMs, bool infinite);
    void stopTsumeSearch();

public:
    // ==== 追加：検討の強制終了（quit送信→エンジン破棄） ====
    void handleBreakOffConsidaration();

private slots:
    void onCheckmateSolved_(const QStringList& pv);
    void onCheckmateNoMate_();
    void onCheckmateNotImplemented_();
    void onCheckmateUnknown_();
};

#endif // MATCHCOORDINATOR_H
