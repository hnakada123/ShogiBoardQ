#ifndef MATCHCOORDINATOR_H
#define MATCHCOORDINATOR_H

/// @file matchcoordinator.h
/// @brief ローカル対局（人間・エンジン）の司令塔

#include <QObject>
#include <QPoint>
#include <QString>
#include <memory>

#include "gamemodestrategy.h"
#include "interruptedgamestate.h"
#include "matchsessionstate.h"
#include "matchtypes.h"
#include "recordline.h"
#include "startoptions.h"

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
 * 盤・手番・対局者名・時計の表示はシグナルで知らせ、棋譜の行は MatchRecordSink に書き込む。
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
    /// 一度でも対局を始めたか
    bool hasStarted() const { return m_session.started(); }
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

    ShogiClock* clock() const override { return m_clock; }
    /// いまの時計の値で timeUpdated を出す
    void pokeTimeUpdateNow();

    /// Strategy から呼ぶ操作（テストから一手を確定するときにも使う）
    MatchStrategyHost& strategyHost() { return *this; }

signals:
    // --- 対局の準備・盤・手番 ---
    /// 対局を準備した（盤は開始局面。対局者名・エンジン名の表示を合わせる）
    void gamePrepared(const MatchStartOptions& opt);
    /// 盤を描き直す
    void boardChanged();
    /// 手番が変わった（手番の表示・時計の表示）
    void turnChanged(Turn sideToMove);
    /// 指した手の移動元・移動先を示す（盤座標）
    void moveHighlightRequested(const QPoint& from, const QPoint& to);
    /// 待ったで ply 手目の局面に戻した（盤・ハイライト・手番と時計の表示・盤の入力を合わせる）
    void movesUndone(int ply);
    /// エンジンが指した（side のエンジンの評価値を ply 手目としてグラフに足す）
    void engineMovePlayed(Turn side, int ply);

    // --- 時計 ---
    /**
     * @brief 時計の値
     * @param urgencyMs 盤の緊急カラー用ms（非緊急ならINT64_MAX）
     */
    void timeUpdated(qint64 blackMs, qint64 whiteMs, Turn sideToMove, qint64 urgencyMs);

    // --- 終局 ---
    /// 終局状態が変わった（終局した・終局の行を書いた・解いた）
    void gameOverStateChanged(const MatchCoordinator::GameOverState& st);
    /// 終局した（終局の行は棋譜に書いた後）
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
    ShogiGameController* gameController() const override { return m_gc; }
    void refreshBoardAndTurn() override;
    void requestEngineMove(Turn side) override;
    void setEnginePonder(Turn side, const QString& ponder) override;
    bool applyEngineMove(const UsiMove& move, Turn mover, qint64 elapsedMs) override;
    bool completeHumanMove(const RecordEntry& played) override;

    void createStrategy(const StartOptions& opt);
    bool completeMove(RecordEntry played, bool engineMove);
    void emitTimeUpdate();
    void disarmTurnTimer();

    // --- 終局 ---
    void handleEngineResign(Turn side);
    void handleEngineWin(Turn side);
    void handleEngineIllegalMove(Turn side);
    void handleEngineError(const QString& message);
    bool checkAndHandleSennichite();
    void handleMaxMovesJishogi();
    /// 終局を記録する（状態・棋譜の終局の行・gameEnded）
    void finishGame(const GameEndInfo& info);
    void appendTerminalRecord(const GameEndInfo& info);
    /// 終局を知らせ、自動保存を頼んで gameEndProcessed を出す
    void announceResult(const GameEndInfo& info);
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
};

#endif // MATCHCOORDINATOR_H
