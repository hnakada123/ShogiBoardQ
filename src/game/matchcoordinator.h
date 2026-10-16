#ifndef MATCHCOORDINATOR_H
#define MATCHCOORDINATOR_H

/// @file matchcoordinator.h
/// @brief ローカル対局（人間・エンジン）の司令塔

#include <QObject>
#include <QString>
#include <memory>
#include <optional>

#include "gamemodestrategy.h"
#include "interruptedgamestate.h"
#include "matchsessionstate.h"
#include "matchtypes.h"
#include "recordline.h"
#include "startoptions.h"
#include "usiscore.h"

class AppActivityState;
class EngineLifecycleManager;
class MatchRecordSink;
class ShogiClock;
class ShogiGameController;
class Usi;

/**
 * @brief ローカル対局（人間・エンジン）の司令塔
 *
 * 対局の手順（RecordLine）を持ち、一手の確定（時計・棋譜・千日手・最大手数）、終局の判定と記録、
 * 待った、中断と再開を受け持つ。手番ごとに誰が指すかは対局者の組（MatchPlayers）別の Strategy が決める。
 *
 * 棋譜の行は MatchRecordSink に書き込む（書くと表示中の手が最新の手になり、盤・手番の表示が合う）。
 * 時計・評価値・終局はシグナルで知らせる。アプリの動作状態（対局中・待機）はこのクラスが書く。
 * 通信対局（CSA）はこのクラスを使わない（CsaGameCoordinator）。
 */
class MatchCoordinator : public QObject, private MatchStrategyHost {
    Q_OBJECT

public:
    using Cause = MatchCause;
    using GameEndInfo = MatchGameEndInfo;
    using GameOverState = MatchGameOverState;
    using StartOptions = MatchStartOptions;

    struct Deps {
        ShogiGameController* gc = nullptr;          ///< 対局の盤（必須、非所有）
        ShogiClock* clock = nullptr;                ///< 対局の時計（必須、非所有）
        AppActivityState* activity = nullptr;       ///< アプリの動作状態（必須、非所有）
        EngineLifecycleManager* engines = nullptr;  ///< エンジンの枠 E1・E2（必須、非所有。検討と共有する）
    };

    explicit MatchCoordinator(const Deps& deps, QObject* parent = nullptr);
    ~MatchCoordinator() override;

    /// 棋譜の受け手（ライブ対局のセッション）。未設定なら棋譜には書かない
    void setRecordSink(MatchRecordSink* sink) { m_sink = sink; }

    // --- 対局の手順 ---

    /// 対局の手順（開始局面と、対局で指した手。終局の行は含めない）
    const RecordLine& record() const { return m_record; }
    int currentPly() const override { return m_record.moveCount(); }

    // --- 開始・再開 ---

    /**
     * @brief 対局を準備する（盤・手順・対局者・Strategy。時計はまだ動かさない）
     *
     * opt.recordLine の currentPly までを対局の手順にする（後続の手は捨てる）。
     */
    void start(const StartOptions& opt);
    /// 時計を動かし、初手がエンジンの手番なら思考を始める
    void beginPlay();

    const StartOptions& options() const { return m_session.options(); }
    /// 対局者（人間・エンジンの組と名前）
    const MatchPlayers& players() const { return m_session.players(); }
    bool isHumanSide(Turn side) const;

    /// 終局の知らせで対局者を下手・上手と呼ぶか（駒落ちで、対局情報の見出しが下手・上手のとき）
    void setHandicapNames(bool handicapNames) { m_handicapNames = handicapNames; }
    bool usesHandicapNames() const { return m_handicapNames; }

    const InterruptedGameState* interruptedGame() const { return m_interruptedGame.get(); }
    void discardInterruptedGame();
    /// 中断した対局を、中断した局面・時計から再開する（時計は呼び出し側が beginPlay で動かす）
    bool resumeInterruptedGame();

    // --- 着手 ---

    /// 人間の手を受け付けるか（終局していない・棋譜に書ける）
    bool canAcceptMove() const;
    /// 人間が盤で指した（ShogiGameController::validateAndMove が指した後）
    void onHumanMove(const RecordEntry& played);
    /// 手番のエンジンにすぐ指させる
    void forceImmediateMove();
    /// 自分の手と相手の手を1つずつ取り消す（人間の手番のときだけ）
    bool undoTwoPlies();

    // --- 終局 ---

    const GameOverState& gameOverState() const { return m_session.gameOver(); }
    bool isGameOver() const override { return m_session.gameOver().isOver; }
    void clearGameOverState();

    void handleResign();
    void handleBreakOff();
    void handlePlayerTimeOut(Turn player);
    /// 入玉宣言の判定結果で終局する（宣言の結果は呼び出し側のダイアログで知らせる）
    void handleNyugyokuDeclaration(Turn declarer, bool success, bool isDraw);

    // --- 時計 ---

    ShogiClock* clock() const { return m_clock; }
    /// いまの時計の値で timeUpdated を出す
    void pokeTimeUpdateNow();

    /// Strategy から呼ぶ操作（テストから一手を確定するときにも使う）
    MatchStrategyHost& strategyHost() { return *this; }

signals:
    // --- 盤 ---
    /// 待ったで ply 手目の局面に戻した（評価値グラフと盤の入力を合わせる）
    void movesUndone(int ply);
    /// エンジンが指した（score は指した側から見た最善の読み筋の評価値。グラフの ply 手目に足す）
    void engineMovePlayed(Turn side, int ply, const UsiScore& score);

    // --- 時計 ---
    /**
     * @brief 時計の値
     * @param urgencyMs 盤の緊急カラー用ms（非緊急ならINT64_MAX）
     */
    void timeUpdated(qint64 blackMs, qint64 whiteMs, Turn sideToMove, qint64 urgencyMs);

    // --- 終局 ---
    /// 終局した（終局の行は棋譜に書き、動作状態を待機にした後）
    void gameEnded(const MatchCoordinator::GameEndInfo& info);
    /// 終局の知らせ（ダイアログ）を出す
    void gameOverMessage(const QString& title, const QString& message);
    /// 棋譜の自動保存を頼む
    void autoSaveRequested(const QString& saveDir);
    /// 終局の後処理（知らせ・自動保存）を終えた（連続対局の次局はここから）
    void gameEndProcessed(const MatchCoordinator::GameEndInfo& info);
    /// 中断した（再開できる）
    void gameInterrupted();

private:
    // --- MatchStrategyHost ---
    EngineLifecycleManager& engines() override { return *m_engines; }
    Turn sideToMove() const override;
    void requestEngineMove(Turn side) override;
    void setWaitingForEngines(bool waiting) override;
    void recordConsiderationTime(Turn mover, qint64 elapsedMs) override;
    bool applyEngineMove(const UsiMatchMove& move) override;
    bool completeHumanMove(const RecordEntry& played) override;

    void createStrategy(const StartOptions& opt);
    /// 一手を確定する。engineScore はエンジンの手のとき（グラフに足す評価値）
    bool completeMove(RecordEntry played, std::optional<UsiScore> engineScore);
    void emitTimeUpdate();
    void disarmTurnTimer();

    // --- 終局 ---
    void handleEngineResign(Turn side);
    void handleEngineWin(Turn side);
    void handleEngineIllegalMove(Turn side);
    void handleEngineError(const QString& message);
    bool checkAndHandleSennichite();
    void handleMaxMovesJishogi();
    /// 終局の後の知らせ方
    enum class Notice {
        Announce,     ///< 結果のダイアログを出し、自動保存して gameEndProcessed
        Declaration,  ///< 入玉宣言（結果は宣言の判定で知らせる）。自動保存して gameEndProcessed
        Interrupt,    ///< 中断（再開できる）。gameInterrupted
    };
    /**
     * @brief 終局する（時計を止め、エンジンに gameover を送り、終局の行を書いて待機に戻す）
     * @param engineLoser エンジンに負けと知らせる側（引き分け・中断は nullopt）
     */
    void endGame(const GameEndInfo& info, std::optional<Turn> engineLoser, Notice notice);
    void appendTerminalRecord(const GameEndInfo& info);
    QString resultMessage(const GameEndInfo& info) const;

    // --- 待った・中断 ---
    void captureInterruptedGame();

    ShogiGameController* m_gc = nullptr;
    ShogiClock* m_clock = nullptr;
    AppActivityState* m_activity = nullptr;
    MatchRecordSink* m_sink = nullptr;
    EngineLifecycleManager* m_engines = nullptr;
    std::unique_ptr<GameModeStrategy> m_strategy;
    MatchSessionState m_session;
    RecordLine m_record;
    std::unique_ptr<InterruptedGameState> m_interruptedGame;
    bool m_handicapNames = false;
    bool m_playing = false;             ///< beginPlay から終局まで（時計を動かしてよい）
    bool m_waitingForEngines = false;   ///< エンジンの初期化を待っている（時計を止めておく）
};

#endif // MATCHCOORDINATOR_H
