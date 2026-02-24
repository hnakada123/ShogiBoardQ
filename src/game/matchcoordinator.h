#ifndef MATCHCOORDINATOR_H
#define MATCHCOORDINATOR_H

/// @file matchcoordinator.h
/// @brief 対局進行コーディネータ（司令塔）クラスの定義

#include <QObject>
#include <QString>
#include <functional>
#include <memory>
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
class GameModeStrategy;
class EngineVsEngineStrategy;
class ShogiView;
class Usi;
class KifuRecordListModel;
class BoardInteractionController;
class StartGameDialog;
class AnalysisSessionHandler;

/**
 * @brief 対局進行/終局/時計/USI送受のハブとなる司令塔クラス
 *
 * 対局全体のライフサイクルを管理し、エンジン通信・時計管理・
 * 棋譜記録・終局判定を統括する。寿命はMainWindow側で管理する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class MatchCoordinator : public QObject {
    Q_OBJECT

public:
    class StrategyContext;
    StrategyContext& strategyCtx();
    // --- 型定義 ---

    /// 対局者を表す列挙値
    enum Player : int {
        P1 = 1,  ///< 先手
        P2 = 2   ///< 後手
    };

    /// 終局原因
    enum class Cause : int {
        Resignation    = 0,  ///< 投了
        Timeout        = 1,  ///< 時間切れ
        BreakOff       = 2,  ///< 中断
        Jishogi        = 3,  ///< 持将棋／最大手数到達
        NyugyokuWin    = 4,  ///< 入玉宣言勝ち
        IllegalMove    = 5,  ///< 反則負け（入玉宣言失敗など）
        Sennichite     = 6,  ///< 千日手（引き分け）
        OuteSennichite = 7   ///< 連続王手の千日手（反則負け）
    };

    /// USI goコマンドに渡す時間パラメータ（ミリ秒）
    struct GoTimes {
        qint64 btime = 0;    ///< 先手残り時間（ms）
        qint64 wtime = 0;    ///< 後手残り時間（ms）
        qint64 byoyomi = 0;  ///< 共通秒読み（ms）
        qint64 binc = 0;     ///< 先手フィッシャー加算（ms）
        qint64 winc = 0;     ///< 後手フィッシャー加算（ms）
    };

    /// 終局情報
    struct GameEndInfo {
        Cause  cause = Cause::Resignation;  ///< 終局原因
        Player loser = P1;                  ///< 敗者
    };

    /// 直近の終局状態
    struct GameOverState {
        bool        isOver       = false;   ///< 終局済みか
        bool        moveAppended = false;   ///< 棋譜に終局手を追記済みか
        bool        hasLast      = false;   ///< 直近の終局情報があるか
        bool        lastLoserIsP1= false;   ///< 直近の敗者が先手か
        GameEndInfo lastInfo;               ///< 直近の終局情報
        QDateTime   when;                   ///< 終局日時
    };

    /// UI/描画系の委譲コールバック群
    struct Hooks {
        // --- UI/描画系 ---
        std::function<void(Player cur)> updateTurnDisplay;      ///< 手番表示/ハイライト更新
        std::function<void(const QString& p1, const QString& p2)> setPlayersNames; ///< 対局者名設定
        std::function<void(const QString& e1, const QString& e2)> setEngineNames;  ///< エンジン名設定
        std::function<void(bool inProgress)> setGameActions;    ///< NewGame/Resign等のON/OFF
        std::function<void()> renderBoardFromGc;                ///< gc→view反映
        std::function<void(const QString& title, const QString& message)> showGameOverDialog; ///< 終局ダイアログ表示
        std::function<void(const QString& msg)> log;            ///< ログ出力
        std::function<void(const QPoint& from, const QPoint& to)> showMoveHighlights; ///< 着手ハイライト

        // --- 時計読み出し（ShogiClock API差異吸収） ---
        std::function<qint64(Player)> remainingMsFor;           ///< 残り時間（ms）
        std::function<qint64(Player)> incrementMsFor;           ///< フィッシャー加算（ms）
        std::function<qint64()> byoyomiMs;                      ///< 秒読み（共通、ms）

        /// HvE: 人間側の手番（P1/P2）を返す
        std::function<Player()> humanPlayerSide = nullptr;

        // --- USI送受 ---
        std::function<void(Usi* which, const GoTimes& t)> sendGoToEngine;    ///< go送信
        std::function<void(Usi* which)> sendStopToEngine;                    ///< stop送信
        std::function<void(Usi* which, const QString& cmd)> sendRawToEngine; ///< 任意コマンド送信

        // --- 新規対局の初期化 ---
        std::function<void(const QString& sfenStart)> initializeNewGame;     ///< GUI固有の初期化処理

        /// 棋譜1行追記（text="▲７六歩", elapsed="00:03/00:00:06"）
        std::function<void(const QString& text, const QString& elapsed)> appendKifuLine;

        std::function<void()> appendEvalP1;  ///< P1着手確定→評価値を1本目に追記
        std::function<void()> appendEvalP2;  ///< P2着手確定→評価値を2本目に追記

        /// 棋譜自動保存（対局終了時に呼び出し）
        std::function<void(const QString& saveDir, PlayMode playMode,
                           const QString& humanName1, const QString& humanName2,
                           const QString& engineName1, const QString& engineName2)> autoSaveKifu;
    };

    /// 依存オブジェクト
    struct Deps {
        ShogiGameController* gc = nullptr;         ///< ゲームコントローラ（非所有）
        ShogiClock*          clock = nullptr;       ///< 将棋時計（非所有）
        ShogiView*           view = nullptr;        ///< 盤面ビュー（非所有）
        Usi*                 usi1 = nullptr;         ///< エンジン1（非所有）
        Usi*                 usi2 = nullptr;         ///< エンジン2（非所有）
        UsiCommLogModel*           comm1 = nullptr;  ///< エンジン1通信ログ（非所有）
        ShogiEngineThinkingModel*  think1 = nullptr; ///< エンジン1思考情報（非所有）
        UsiCommLogModel*           comm2 = nullptr;  ///< エンジン2通信ログ（非所有）
        ShogiEngineThinkingModel*  think2 = nullptr; ///< エンジン2思考情報（非所有）
        Hooks                hooks;                  ///< UIコールバック群
        QStringList* sfenRecord = nullptr;           ///< SFEN履歴（非所有）
        QVector<ShogiMove>* gameMoves = nullptr;     ///< 指し手リスト（非所有、外部共有用）
    };

    // --- 対局開始オプション ---

    /// 対局開始のための設定パラメータ
    struct StartOptions {
        PlayMode mode = PlayMode::NotStarted;  ///< 対局モード
        QString  sfenStart;                    ///< 開始SFEN

        QString engineName1;                   ///< エンジン1表示名
        QString enginePath1;                   ///< エンジン1実行パス
        QString engineName2;                   ///< エンジン2表示名
        QString enginePath2;                   ///< エンジン2実行パス

        bool engineIsP1 = false;               ///< エンジンが先手か（HvE）
        bool engineIsP2 = false;               ///< エンジンが後手か（HvE）

        int maxMoves = 0;                      ///< 最大手数（0=無制限）

        bool autoSaveKifu = false;             ///< 棋譜自動保存フラグ
        QString kifuSaveDir;                   ///< 棋譜保存ディレクトリ

        QString humanName1;                    ///< 先手の人間対局者名
        QString humanName2;                    ///< 後手の人間対局者名
    };

    explicit MatchCoordinator(const Deps& deps, QObject* parent=nullptr);
    ~MatchCoordinator() override;

    // --- 外部API ---

    /// 人間の投了を処理する
    void handleResign();

    /// エンジン投了通知を処理する（idx: 1 or 2）
    void handleEngineResign(int idx);

    /// エンジン入玉宣言勝ち通知を処理する（idx: 1 or 2）
    void handleEngineWin(int idx);

    void flipBoard();

    /// エンジンポインタを差し替える（再生成時など）
    void updateUsiPtrs(Usi* e1, Usi* e2);

    /// 中断を処理する（UI→司令塔）
    void handleBreakOff();

    /// 入玉宣言を処理する
    void handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw);

    /// 対局中の指し手リストを取得する（CSA出力等で使用）
    const QVector<ShogiMove>& gameMoves() const { return m_gameMoves; }
    /// SFEN履歴への参照を取得する（UI共有用）
    QStringList* sfenRecordPtr() { return m_sfenHistory; }
    /// SFEN履歴への参照を取得する（const版）
    const QStringList* sfenRecordPtr() const { return m_sfenHistory; }

    // --- 時間管理 ---

    /// 時間制御の設定
    struct TimeControl {
        bool useByoyomi    = false;  ///< 秒読み使用フラグ
        int  byoyomiMs1    = 0;      ///< 先手秒読み（ms）
        int  byoyomiMs2    = 0;      ///< 後手秒読み（ms）
        int  incMs1        = 0;      ///< 先手加算（ms）
        int  incMs2        = 0;      ///< 後手加算（ms）
        bool loseOnTimeout = false;  ///< 時間切れ負けフラグ
    };

    void setTimeControlConfig(bool useByoyomi,
                              int byoyomiMs1, int byoyomiMs2,
                              int incMs1,     int incMs2,
                              bool loseOnTimeout);
    const TimeControl& timeControl() const;

    // --- ターン計測（KIF表示用） ---

    void   markTurnEpochNowFor(Player side, qint64 nowMs = -1);
    qint64 turnEpochFor(Player side) const;

    // --- HvH: ターン計測 ---

    void armTurnTimerIfNeeded();
    void finishTurnTimerAndSetConsiderationFor(Player mover);

    // --- HvE: 人間タイマー解除（Strategy委譲） ---

    void disarmHumanTimerIfNeeded();

    // --- USI時間計算 ---

    void computeGoTimesForUSI(qint64& outB, qint64& outW) const;
    void refreshGoTimes();

    // --- 対局開始フロー ---

    /// USI position文字列の初期化
    void initializePositionStringsForStart(const QString& sfenStart);

    /// 初手がエンジン手番なら go を発行する（Strategy委譲）
    void startInitialEngineMoveIfNeeded();

    // --- UNDO ---

    /// UNDO に必要な外部参照（Deps/既存メンバと重複しない UNDO 固有の参照のみ）
    struct UndoRefs {
        KifuRecordListModel*          recordModel      = nullptr;  ///< 棋譜モデル
        QStringList*                  positionStrList  = nullptr;  ///< position文字列リスト（NULLable）
        int*                          currentMoveIndex = nullptr;  ///< 現在手数インデックス
        BoardInteractionController*   boardCtl = nullptr;   ///< 盤面操作コントローラ
    };

    /// UNDO時のUIコールバック群
    struct UndoHooks {
        std::function<void(int /*ply*/)>              updateHighlightsForPly;         ///< ハイライト更新
        std::function<void()>                         updateTurnAndTimekeepingDisplay; ///< 手番/時計表示更新
        std::function<bool(ShogiGameController::Player)> isHumanSide;                 ///< 人間側判定（必須）
    };

    void setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks);

    /// 2手分のUNDOを実行する
    bool undoTwoPlies();

    // --- エンジン管理 ---

    /// エンジンの初期化と開始（side: P1 or P2）
    void initializeAndStartEngineFor(Player side,
                                     const QString& enginePathIn,
                                     const QString& engineNameIn);

    /// エンジンを破棄する（idx: 1 or 2）
    void destroyEngine(int idx, bool clearThinking = true);

    /// 全エンジンを破棄する
    void destroyEngines(bool clearModels = true);

    // --- 終局状態管理 ---

    const GameOverState& gameOverState() const { return m_gameOver; }

    /// 終局状態をクリアする（対局開始/局面編集開始時に呼ぶ）
    void clearGameOverState();

    /// 終局状態を設定する
    void setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce = true);

    /// 棋譜に終局手を一意追記後に通知する
    void markGameOverMoveAppended();

    // --- 検討API ---

    /// 検討オプション
    struct AnalysisOptions {
        QString  enginePath;                ///< 検討に使うエンジン実行ファイル
        QString  engineName;                ///< 表示用エンジン名
        QString  positionStr;               ///< "position sfen ... [moves ...]" の完全文字列
        int      byoyomiMs = 0;             ///< 0=無制限、>0=ms
        int      multiPV   = 1;             ///< MultiPV（候補手の数）
        PlayMode mode      = PlayMode::ConsiderationMode;  ///< 検討モード
        ShogiEngineThinkingModel* considerationModel = nullptr; ///< 検討タブ用モデル
        int      previousFileTo = 0;        ///< 前回の移動先の筋（1-9, 0=未設定）
        int      previousRankTo = 0;        ///< 前回の移動先の段（1-9, 0=未設定）
        QString  lastUsiMove;               ///< 開始局面に至った最後の指し手（USI形式）
    };

    /// 検討を開始する
    void startAnalysis(const AnalysisOptions& opt);

    /// 詰み探索・検討エンジンを終了する
    void stopAnalysisEngine();

    /// 検討中にMultiPVを変更する
    void updateConsiderationMultiPV(int multiPV);

    /**
     * @brief 検討中にポジションを変更する（棋譜欄の別の手を選択したとき）
     * @return true: 再開処理を開始した, false: 検討モードでないか同じポジションのため無視
     */
    bool updateConsiderationPosition(const QString& newPositionStr,
                                     int previousFileTo = 0, int previousRankTo = 0,
                                     const QString& lastUsiMove = QString());

    // --- エンジンアクセサ ---

    Usi* primaryEngine() const;
    Usi* secondaryEngine() const;

    // --- 対局開始一括API ---

    /// 対局開始フローを一元化する
    void configureAndStart(const StartOptions& opt);

    /// StartOptionsを構築する
    StartOptions buildStartOptions(PlayMode mode,
                                   const QString& startSfenStr,
                                   const QStringList* sfenRecord,
                                   const StartGameDialog* dlg) const;

    /// 人間を手前に配置する
    void ensureHumanAtBottomIfApplicable(const StartGameDialog* dlg, bool bottomIsP1);

    /// 準備→開始→必要なら初手goまでを一括実行する
    void prepareAndStartGame(PlayMode mode,
                             const QString& startSfenStr,
                             const QStringList* sfenRecord,
                             const StartGameDialog* dlg,
                             bool bottomIsP1);

    // --- 時間/手番・終局の処理 ---

    void handleTimeUpdated();
    void handlePlayerTimeOut(int player);
    void handleGameEnded();

    /// 開始直後のタイマー起動や初手go判定を1本化する
    void startMatchTimingAndMaybeInitialGo();

    // --- PlayMode ---

    PlayMode playMode() const;

    /// 終局1回だけの棋譜追記と重複防止を一括処理する
    void appendGameOverLineAndMark(Cause cause, Player loser);

    /// 人間着手後の後処理（Strategy へ委譲）
    void onHumanMove(const QPoint& from, const QPoint& to,
                     const QString& prettyMove);

    /// 現在の状況に応じて適切なエンジンへstopを送り、即時bestmoveを促す
    void forceImmediateMove();

    // --- USI送受 ---

    void sendGoToEngine(Usi* which, const GoTimes& t);
    void sendStopToEngine(Usi* which);
    void sendRawToEngine(Usi* which, const QString& cmd);

    /// 中断時の棋譜1行追記と二重追記ブロック確定
    void appendBreakOffLineAndMark();

    /// 持将棋（最大手数到達）時の棋譜1行追記と終局処理
    void handleMaxMovesJishogi();

    /// 千日手チェック（trueなら終局済み）
    bool checkAndHandleSennichite();

    /// 通常千日手の終局処理
    void handleSennichite();

    /// 連続王手千日手の終局処理
    void handleOuteSennichite(bool p1Loses);

    int maxMoves() const { return m_maxMoves; }

    // --- ShogiClockアクセサ ---

    ShogiClock*       clock();
    const ShogiClock* clock() const;

    /// 時計を差し替える（後からの配線用）
    void setClock(ShogiClock* clock);

    void setPlayMode(PlayMode m);

    /// EvE用のエンジン生成・配線・初期化
    void initEnginesForEvE(const QString& engineName1,
                           const QString& engineName2);

    /// エンジン手進行ロジック
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

    /// エンジン破棄中かどうかを返す
    bool isEngineShutdownInProgress() const { return m_engineShutdownInProgress; }

public slots:
    /// 即時に現在値でtimeUpdated(...)を発火する（UIをすぐ同期させたい時に使う）
    void pokeTimeUpdateNow();

signals:
    /// 盤面反転の通知（→ GameStartCoordinator::boardFlipped へ再送出）
    void boardFlipped(bool nowFlipped);

    /// 終局の通知（→ GameStartCoordinator::matchGameEnded へ再送出）
    void gameEnded(const MatchCoordinator::GameEndInfo& info);

    /**
     * @brief 時計更新通知
     * @param p1ms 先手残りms
     * @param p2ms 後手残りms
     * @param p1turn 現在手番が先手ならtrue
     * @param urgencyMs 盤の緊急カラー用ms（非緊急ならINT64_MAX）
     */
    void timeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);

    void gameOverStateChanged(const MatchCoordinator::GameOverState& st);
    void requestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    /// 検討モード終了時に発火する
    void considerationModeEnded();

    /// 詰み探索モード終了時に発火する
    void tsumeSearchModeEnded();

    /// 検討待機開始時に発火する（経過タイマー停止用）
    void considerationWaitingStarted();

private:
    // --- 内部ヘルパ ---

    bool tryRemoveLastItems(QObject* model, int n);
    void setGameInProgressActions(bool inProgress);
    void updateTurnDisplay(Player p);
    GoTimes computeGoTimes() const;
    void displayResultsAndUpdateGui(const GameEndInfo& info);
    void initPositionStringsFromSfen(const QString& sfenBase);
    void wireResignToArbiter(Usi* engine, bool asP1);
    void wireWinToArbiter(Usi* engine, bool asP1);
    void emitTimeUpdateFromClock();
    void recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const;
    void sendRawTo(Usi* which, const QString& cmd);

    // --- configureAndStart 分解ヘルパ ---

    /// 過去対局の履歴探索結果
    struct HistorySearchResult {
        QString bestBaseFull;      ///< 一致したゲームのフルposition文字列
        int bestGameIdx  = -1;     ///< 一致したゲームのインデックス
        int bestMatchPly = -1;     ///< 一致した手数
    };

    /// 既存履歴の同期と過去対局の探索
    HistorySearchResult syncAndSearchGameHistory(const QString& targetSfen);

    /// オプション適用・フック呼び出し・UI初期化
    void applyStartOptionsAndHooks(const StartOptions& opt);

    /// 過去対局履歴を用いたposition文字列の構築（フォールバック含む）
    void buildPositionStringsFromHistory(const StartOptions& opt,
                                         const QString& targetSfen,
                                         const HistorySearchResult& searchResult);

    /// PlayMode別のStrategy生成と起動
    void createAndStartModeStrategy(const StartOptions& opt);

    void wireClock();
    void unwireClock();

private slots:
    void onEngine1Resign();
    void onEngine2Resign();
    void onEngine1Win();
    void onEngine2Win();
    void onClockTick();
    void onUsiError(const QString& msg);

private:
    // --- コアオブジェクト（非所有、寿命はMainWindow側で管理） ---

    ShogiGameController* m_gc   = nullptr;   ///< ゲームコントローラ
    ShogiClock*          m_clock= nullptr;   ///< 将棋時計
    ShogiView*           m_view = nullptr;   ///< 盤面ビュー
    Usi*                 m_usi1 = nullptr;    ///< エンジン1
    Usi*                 m_usi2 = nullptr;    ///< エンジン2
    Hooks                m_hooks;             ///< UIコールバック群

    std::unique_ptr<StrategyContext>  m_strategyCtx; ///< Strategy用コンテキスト
    std::unique_ptr<GameModeStrategy> m_strategy;    ///< 対局モード別Strategy

    Player m_cur = P1;                        ///< 現在手番

    // --- モデル ---

    PlayMode                 m_playMode = PlayMode::NotStarted; ///< 対局モード
    UsiCommLogModel*         m_comm1    = nullptr;   ///< エンジン1通信ログ（非所有）
    ShogiEngineThinkingModel*m_think1   = nullptr;   ///< エンジン1思考情報（非所有）
    UsiCommLogModel*         m_comm2    = nullptr;   ///< エンジン2通信ログ（非所有）
    ShogiEngineThinkingModel*m_think2   = nullptr;   ///< エンジン2思考情報（非所有）

    // --- USI position文字列 ---

    QString m_positionStr1, m_positionPonder1;  ///< エンジン1用
    QString m_positionStr2, m_positionPonder2;  ///< エンジン2用

    // --- 棋譜/SFEN記録 ---

    int m_currentMoveIndex = 0;              ///< 現在の手数インデックス
    QStringList* m_sfenHistory = nullptr;     ///< SFEN履歴（共有ポインタ、非所有）
    QStringList m_sharedSfenRecord;          ///< 共有ポインタ未指定時の内部SFEN履歴
    QVector<ShogiMove> m_gameMoves;          ///< 対局中の指し手リスト
    QVector<ShogiMove>* m_externalGameMoves = nullptr; ///< 外部共有の指し手リスト（非所有、Deps経由）

    // 外部共有ポインタが設定されていればそれを使う（Deps経由で注入）
    QVector<ShogiMove>& gameMovesRef() {
        return m_externalGameMoves ? *m_externalGameMoves : m_gameMoves;
    }

    // --- ターン計測 ---

    qint64 m_turnEpochP1Ms = -1;             ///< P1の手開始エポック（ms）
    qint64 m_turnEpochP2Ms = -1;             ///< P2の手開始エポック（ms）
    // --- 時間ルール ---

    TimeControl m_tc;                         ///< 時間制御設定
    int m_maxMoves = 0;                       ///< 最大手数（0=無制限）

    // --- 棋譜自動保存 ---

    bool m_autoSaveKifu = false;              ///< 自動保存フラグ
    QString m_kifuSaveDir;                    ///< 保存ディレクトリ

    // --- 対局者名 ---

    QString m_humanName1;                     ///< 先手人間名
    QString m_humanName2;                     ///< 後手人間名
    QString m_engineNameForSave1;             ///< 先手エンジン名（保存用）
    QString m_engineNameForSave2;             ///< 後手エンジン名（保存用）

    // --- 時計接続管理 ---

    QMetaObject::Connection m_clockConn;      ///< 時計接続ハンドル

    // --- 終局状態 ---

    GameOverState m_gameOver;                 ///< 終局状態

    bool m_engineShutdownInProgress = false;  ///< エンジン破棄中フラグ（再入防止）

    // --- 検討・詰み探索セッション管理 ---

    std::unique_ptr<AnalysisSessionHandler> m_analysisSession; ///< 検討・詰み探索ハンドラ
    void ensureAnalysisSession();             ///< ハンドラの遅延生成

    // --- UNDO ---

    bool m_isUndoInProgress = false;          ///< UNDO処理中フラグ
    UndoRefs  u_;                             ///< UNDO用参照
    UndoHooks h_;                             ///< UNDO用コールバック

    // --- USI position履歴 ---

    QStringList m_positionStrHistory;          ///< position文字列の履歴

    // --- 対局履歴 ---

    static constexpr int kMaxGameHistories = 10;  ///< 最大対局履歴数
    QVector<QStringList> m_allGameHistories;  ///< 過去の対局履歴
};

#endif // MATCHCOORDINATOR_H
