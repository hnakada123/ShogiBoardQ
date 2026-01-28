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
class StartGameDialog;

// 対局進行/終局/時計/USI送受のハブ（寿命は Main 側で管理）
class MatchCoordinator : public QObject {
    Q_OBJECT

public:
    enum Player : int { P1 = 1, P2 = 2 };

    // ★ BreakOff を追加（中断終局）、Jishogi を追加（持将棋／最大手数到達）
    // ★ NyugyokuWin を追加（入玉宣言勝ち）、IllegalMove を追加（反則負け：入玉宣言失敗など）
    enum class Cause : int { Resignation = 0, Timeout = 1, BreakOff = 2, Jishogi = 3, NyugyokuWin = 4, IllegalMove = 5 };

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

        // 棋譜自動保存（対局終了時に呼び出し）
        // saveDir: 保存先ディレクトリ, playMode: 対局モード
        // humanName1, humanName2: 人間対局者名
        // engineName1, engineName2: エンジン名
        std::function<void(const QString& saveDir, PlayMode playMode,
                           const QString& humanName1, const QString& humanName2,
                           const QString& engineName1, const QString& engineName2)> autoSaveKifu;
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
        QStringList* sfenRecord = nullptr;
    };

    explicit MatchCoordinator(const Deps& deps, QObject* parent=nullptr);
    ~MatchCoordinator() override;

    // 外部 API
    void handleResign();                 // 人間の投了
    void handleEngineResign(int idx);    // エンジン投了通知(1 or 2)
    void handleEngineWin(int idx);       // エンジン入玉宣言勝ち通知(1 or 2)
    void flipBoard();                    // 盤反転 + 表示更新
    void updateUsiPtrs(Usi* e1, Usi* e2);// エンジン再生成時などに差し替え
    void handleBreakOff();               // ★ 中断（UI→司令塔）
    void handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw); // ★ 入玉宣言

    // ★ 追加: 対局中の指し手リストを取得（CSA出力等で使用）
    const QVector<ShogiMove>& gameMoves() const { return m_gameMoves; }

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

private:
    bool tryRemoveLastItems(QObject* model, int n);
    bool m_isUndoInProgress = false;
    UndoRefs  u_;
    UndoHooks h_;

// --- private: 内部ヘルパ ---
private:
    void initPositionStringsFromSfen(const QString& sfenBase);
    void startInitialEngineMoveFor(Player engineSide);  // 先手/後手どちらでも1手だけ指す

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
    void setGameInProgressActions(bool inProgress);
    void updateTurnDisplay(Player p);

    // 時計/USI 時間計算
    GoTimes computeGoTimes() const;

    // 終局処理
    void displayResultsAndUpdateGui(const GameEndInfo& info);

public:
    // MainWindow からエンジン初期化/開始を委譲するための統一API
    // side: P1 or P2
    void initializeAndStartEngineFor(Player side,
                                     const QString& enginePathIn,
                                     const QString& engineNameIn);

    // エンジン破棄（片方 or 両方）
    void destroyEngine(int idx, bool clearThinking = true);   // idx: 1 or 2
    void destroyEngines(bool clearModels = true);

private:
    // resign/win 配線の実装（内部ユーティリティ）
    void wireResignToArbiter(Usi* engine, bool asP1);
    void wireWinToArbiter(Usi* engine, bool asP1);   ///< 入玉宣言勝ちシグナルを配線

    // 共通ロジック：時計と手番を読んで timeUpdated(...) を発火
    void emitTimeUpdateFromClock();

private slots:
    void onEngine1Resign();
    void onEngine2Resign();
    void onEngine1Win();              ///< エンジン1が入玉宣言勝ち
    void onEngine2Win();              ///< エンジン2が入玉宣言勝ち
    void kickNextEvETurn();  // EvE を1手ずつ進める
    void onClockTick();
    void onUsiError(const QString& msg);

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

private:
    PlayMode                 m_playMode = PlayMode::NotStarted;
    UsiCommLogModel*         m_comm1    = nullptr;
    ShogiEngineThinkingModel*m_think1   = nullptr;
    UsiCommLogModel*         m_comm2    = nullptr;
    ShogiEngineThinkingModel*m_think2   = nullptr;

public:
    // 対局開始のためのオプション
    struct StartOptions {
        PlayMode mode = PlayMode::NotStarted;
        QString  sfenStart;

        // エンジン情報（必要なら空でもOK）
        QString engineName1;
        QString enginePath1;
        QString engineName2;
        QString enginePath2;

        // HvE の場合、エンジンがどちら側か
        bool engineIsP1 = false;
        bool engineIsP2 = false;

        // 最大手数（0=無制限）
        int maxMoves = 0;

        // 棋譜自動保存設定
        bool autoSaveKifu = false;
        QString kifuSaveDir;

        // 対局者名（ファイル名生成用）
        QString humanName1;
        QString humanName2;
    };

    // 対局開始フローを一元化
    void configureAndStart(const StartOptions& opt);

    // ==== 追加：検討用オプション ====
    struct AnalysisOptions {
        QString  enginePath;   // 検討に使うエンジン実行ファイル
        QString  engineName;   // 表示用エンジン名
        QString  positionStr;  // "position sfen ... [moves ...]" の完全文字列
        int      byoyomiMs = 0;           // 0=無制限、>0=秒→ms
        int      multiPV   = 1;           // ★ 追加: MultiPV（候補手の数）
        PlayMode mode      = PlayMode::ConsiderationMode; // 既定で検討モード
        ShogiEngineThinkingModel* considerationModel = nullptr;  // ★ 追加: 検討タブ用モデル
        int      previousFileTo = 0;      // ★ 追加: 前回の移動先の筋（1-9, 0=未設定）
        int      previousRankTo = 0;      // ★ 追加: 前回の移動先の段（1-9, 0=未設定）
        QString  lastUsiMove;             // ★ 追加: 開始局面に至った最後の指し手（USI形式）
    };

    // ==== 追加：検討API ====
    void startAnalysis(const AnalysisOptions& opt);

    /// 詰み探索・検討エンジンを終了する
    void stopAnalysisEngine();

    /// 検討中にMultiPVを変更する
    void updateConsiderationMultiPV(int multiPV);

    /// 検討中にポジションを変更する（棋譜欄の別の手を選択したとき）
    /// @return true: 再開処理を開始した, false: 検討モードでないか同じポジションのため無視
    bool updateConsiderationPosition(const QString& newPositionStr,
                                     int previousFileTo = 0, int previousRankTo = 0,
                                     const QString& lastUsiMove = QString());

public:
    Usi* primaryEngine() const;   // HvE/EvH で司令塔が使う主エンジン（これまで m_usi1 に相当）
    Usi* secondaryEngine() const; // ★ 追加

private:
    // 内部ヘルパ
    void startHumanVsHuman(const StartOptions& opt);
    void startHumanVsEngine(const StartOptions& opt, bool engineIsP1);
    void initPositionStringsForEvE(const QString& sfenStart);  // ← 新規ヘルパ
    void startEngineVsEngine(const StartOptions& opt);
    void startEvEFirstMoveByBlack();         // 平手EvE：先手から開始
    void startEvEFirstMoveByWhite();         // 駒落ちEvE：後手（上手）から開始

private:
    // USI "position ... moves" の作業用バッファ
    QString m_positionStr1, m_positionPonder1;
    QString m_positionStr2, m_positionPonder2;

    // 棋譜／SFEN 記録（ShogiGameController::validateAndMove の引数用）
    int m_currentMoveIndex = 0;

    // ★変更：内部保持をやめ、共有ポインタに
    QStringList* m_sfenRecord = nullptr;

    QVector<ShogiMove> m_gameMoves;
    // EvE 専用の棋譜保持（MainWindow から独立）- フォールバック用
    QStringList        m_eveSfenRecord;
    QVector<ShogiMove> m_eveGameMoves;
    int                m_eveMoveIndex = 0;

    // ★ EvE用：共有sfenRecordが設定されていればそれを使い、なければローカルを使う
    QStringList* sfenRecordForEvE() { return m_sfenRecord ? m_sfenRecord : &m_eveSfenRecord; }
    QVector<ShogiMove>& gameMovesForEvE() { return m_sfenRecord ? m_gameMoves : m_eveGameMoves; }

    // ★ HvE/EvH用：u_.gameMoves（MainWindowのポインタ）が設定されていればそれを使う
    QVector<ShogiMove>& gameMovesRef() {
        return u_.gameMoves ? *u_.gameMoves : m_gameMoves;
    }

private:
    // 「その手の開始」エポック（KIFの消費時間計算に使用）
    qint64 m_turnEpochP1Ms = -1;
    qint64 m_turnEpochP2Ms = -1;

    // USI表記用の直近文字列（MainWindow側はラベル更新のみ）
    QString m_bTimeStr, m_wTimeStr;

    // 時間ルール（byoyomi / increment / timeout）
    TimeControl m_tc;

    // 最大手数（0=無制限）
    int m_maxMoves = 0;

    // 棋譜自動保存設定
    bool m_autoSaveKifu = false;
    QString m_kifuSaveDir;

    // 対局者名（ファイル名生成用）
    QString m_humanName1;
    QString m_humanName2;
    QString m_engineNameForSave1;
    QString m_engineNameForSave2;

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
    void wireClock();           // ★追加：時計と onClockTick_ の connect を一元化
    void unwireClock();         // ★追加：既存接続の解除
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

    // ★ 追加: 検討モード終了時に発火
    void considerationModeEnded();

    // ★ 追加: 検討待機開始時に発火（経過タイマー停止用）
    void considerationWaitingStarted();

private:
    // ...（既存）
    GameOverState m_gameOver;
    bool m_inTsumeSearchMode = false;  ///< 詰み探索モード中かどうか
    bool m_inConsiderationMode = false;  ///< 検討モード中かどうか

    // ★ 検討モードの状態（MultiPV変更時・ポジション変更時の再開用）
    QString m_considerationPositionStr;     ///< 検討中の position 文字列
    int m_considerationByoyomiMs = 0;       ///< 検討の時間制限 (ms)
    int m_considerationMultiPV = 1;         ///< 検討の候補手数
    ShogiEngineThinkingModel* m_considerationModelPtr = nullptr;  ///< 検討タブ用モデル
    bool m_considerationRestartPending = false;  ///< 検討再開待ちフラグ
    bool m_considerationRestartInProgress = false;  ///< 検討再開処理中フラグ（再入防止）
    bool m_considerationWaiting = false;  ///< 検討待機中フラグ（時間切れ後、次の局面選択待ち）
    QString m_considerationEnginePath;      ///< 検討中のエンジンパス
    QString m_considerationEngineName;      ///< 検討中のエンジン名
    int m_considerationPreviousFileTo = 0;  ///< 検討中の前回の移動先の筋
    int m_considerationPreviousRankTo = 0;  ///< 検討中の前回の移動先の段
    QString m_considerationLastUsiMove;     ///< 検討中の最後の指し手（USI形式）

    bool m_engineShutdownInProgress = false;  ///< エンジン破棄中フラグ（再入防止用）

public:
    /// エンジン破棄中かどうかを返す
    bool isEngineShutdownInProgress() const { return m_engineShutdownInProgress; }

private slots:
    void onCheckmateSolved(const QStringList& pv);
    void onCheckmateNoMate();
    void onCheckmateNotImplemented();
    void onCheckmateUnknown();
    void onTsumeBestMoveReceived();  ///< 詰み探索中に bestmove を受信
    void onConsiderationBestMoveReceived();  ///< 検討モード中に bestmove を受信
    void restartConsiderationDeferred();  ///< 検討再開（遅延実行用）

public:
    StartOptions buildStartOptions(PlayMode mode,
                                   const QString& startSfenStr,
                                   const QStringList* sfenRecord,
                                   const StartGameDialog* dlg) const;

    void ensureHumanAtBottomIfApplicable(const StartGameDialog* dlg, bool bottomIsP1);

    // ★新規：準備→開始→必要なら初手 go までを一括実行
    void prepareAndStartGame(PlayMode mode,
                             const QString& startSfenStr,
                             const QStringList* sfenRecord,
                             const StartGameDialog* dlg,
                             bool bottomIsP1);

    // ★ 今回追加：時間/手番・終局の処理（MainWindowから移管）
    void handleTimeUpdated();
    void handlePlayerTimeOut(int player); // 1 or 2
    void handleGameEnded();

    // ★ 開始直後のタイマー起動や初手 go 判定を1本化
    void startMatchTimingAndMaybeInitialGo();

signals:
    // UIへは最小限の“文字列/色”等だけ通知（Presenterで受ける前提でもOK）
    void uiUpdateTurnAndClock(const QString& turnText,
                              const QString& p1Text,
                              const QString& p2Text);
    void uiNotifyTimeout(int player);
    void uiNotifyResign();
    void uiNotifyGameEnded();

    // 司令塔内のタイムティックが進んだ際（MainWindowはこの1本でUI更新に反応）
    void timeTick();

private:
    // 内部の計算ヘルパ（UI非依存）
    void recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const;

public:
    // 現在のプレイモードを外部に公開
    PlayMode playMode() const;

    // 終局1回だけの棋譜追記（投了/時間切れ）と重複防止を司令塔で一括処理
    void appendGameOverLineAndMark(Cause cause, Player loser);

    // HvH：人間が指した直後の後処理（時計の消費設定／次手番開始など）を司令塔で一括実行
    void onHumanMove_HvH(ShogiGameController::Player moverBefore);

public:
    // 現在の状況に応じて適切なエンジンへ stop を送り、即時bestmoveを促す
    void forceImmediateMove();

private:
    void sendRawTo(Usi* which, const QString& cmd);

    // USI "position ... moves" の履歴（UNDO等で使う）。SFENとは混ぜないこと！
    QStringList m_positionStrHistory;

    // 過去の対局履歴（途中局面からの再開判定に使用）
    // 最大履歴数を制限してメモリリークを防止
    static constexpr int kMaxGameHistories = 10;
    QVector<QStringList> m_allGameHistories;

public:
    // --- USI 送受の実体（UI 依存なし） ---
    void sendGoToEngine(Usi* which, const GoTimes& t);
    void sendStopToEngine(Usi* which);
    void sendRawToEngine(Usi* which, const QString& cmd);

    // BreakOff（中断）時の棋譜1行追記＋二重追記ブロック確定
    void appendBreakOffLineAndMark();

    // 持将棋（最大手数到達）時の棋譜1行追記＋終局処理
    void handleMaxMovesJishogi();

    // 最大手数を取得する
    int maxMoves() const { return m_maxMoves; }

    // ShogiClock 取得アクセサ
    ShogiClock*       clock();       // 非const版
    const ShogiClock* clock() const; // const版
};

#endif // MATCHCOORDINATOR_H
