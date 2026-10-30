#ifndef MATCHCOORDINATOR_H
#define MATCHCOORDINATOR_H

/// @file matchcoordinator.h
/// @brief ローカル対局（人間・エンジン）の司令塔

#include <QObject>
#include <QString>
#include <memory>
#include <optional>

#include "appactivitystate.h"
#include "gamemodestrategy.h"
#include "matchtimesource.h"
#include "nyugyokujudgement.h"
#include "recordline.h"
#include "shogiclock.h"
#include "shogitypes.h"
#include "startoptions.h"
#include "usiscore.h"

class EngineSlots;
class KifuDocument;
class MatchRecordSink;
class GameBoard;
class Usi;
struct JosekiPick;

/// 終局原因（MatchCoordinator::Cause）
enum class MatchCause : int {
    Resignation    = 0,  ///< 投了
    Timeout        = 1,  ///< 時間切れ
    BreakOff       = 2,  ///< 中断
    Jishogi        = 3,  ///< 持将棋／最大手数到達
    NyugyokuWin    = 4,  ///< 入玉宣言勝ち
    IllegalMove    = 5,  ///< 反則負け（入玉宣言失敗など）
    Sennichite     = 6,  ///< 千日手（引き分け）
    OuteSennichite = 7   ///< 連続王手の千日手（反則負け）
};

/// 終局情報（MatchCoordinator::GameEndInfo）。対局者（先手・後手）は Turn で表す
struct MatchGameEndInfo {
    MatchCause  cause = MatchCause::Resignation;  ///< 終局原因
    Turn        loser = Turn::Black;              ///< 敗者
};

/// 中断したローカル対局の状態（アプリ起動中だけ保持する）。再開すると、この手順と時計から続ける
struct InterruptedGameState {
    MatchStartOptions options;
    ShogiClock::Snapshot clock;
    RecordLine record;           ///< 中断までの対局の手順（「同」表記の基準もここから導く）
    int terminalNodeId = -1;     ///< 棋譜（分岐ツリー）に書いた中断の行。再開でこの行を除く（書くまでは -1）
};

/// 対局の条件と終局の所有者。変更は対局上の操作として行い、内部フィールドを公開しない。
class MatchSessionState
{
public:
    /// 対局を始める。開始の手順（options.recordLine）は対局の手順として MatchCoordinator が持つので、ここには残さない
    void begin(const MatchStartOptions& options)
    {
        m_options = options;
        m_options.recordLine = RecordLine{};
        m_started = true;
        m_end.reset();
    }
    /// 一度でも対局を始めたか
    bool started() const { return m_started; }
    /// 対局の条件（対局者・エンジン・開始局面・最大手数・自動保存・定跡）
    const MatchStartOptions& options() const { return m_options; }
    const MatchPlayers& players() const { return m_options.players; }
    int maxMoves() const { return m_options.maxMoves; }
    /// 終局の情報（終局していなければ nullopt）
    const std::optional<MatchGameEndInfo>& end() const { return m_end; }
    void clearEnd() { m_end.reset(); }
    /// 終局する（終局済みなら何もせず false）
    bool finish(const MatchGameEndInfo& info)
    {
        if (m_end) return false;
        m_end = info;
        return true;
    }

private:
    MatchStartOptions m_options;
    bool m_started = false;
    std::optional<MatchGameEndInfo> m_end;
};

/**
 * @brief ローカル対局（人間・エンジン）の司令塔
 *
 * 対局の手順（RecordLine）を持ち、一手の確定（時計・棋譜・千日手・最大手数）、終局の判定と記録、
 * 待った、中断と再開を受け持つ。手番ごとに誰が指すかは対局者の組（MatchPlayers）別の Strategy が決める。
 *
 * 棋譜の行は MatchRecordSink に書き込み、終局の行の後で確定させる（書くと表示中の手が最新の手になり、
 * 盤・手番の表示が合う）。時計・評価値・終局はシグナルで知らせる。アプリの動作状態（対局中・待機）は
 * このクラスが書く。通信対局（CSA）はこのクラスを使わず、CsaGameCoordinator が同じ受け手と動作状態を使う。
 */
class MatchCoordinator : public QObject, private MatchStrategyHost {
    Q_OBJECT

public:
    using Cause = MatchCause;
    using GameEndInfo = MatchGameEndInfo;
    using StartOptions = MatchStartOptions;

    struct Deps {
        GameBoard* gameBoard = nullptr;             ///< 対局の盤（必須、非所有）
        ShogiClock* clock = nullptr;                ///< 対局の時計（必須、非所有）
        AppActivityState* activity = nullptr;       ///< アプリの動作状態（必須、非所有）
        EngineSlots* engines = nullptr;  ///< エンジンの枠 E1・E2（必須、非所有。検討と共有する）
        /// 棋譜の文書（終局の知らせ・入玉宣言で対局者を下手・上手と呼ぶかを対局情報から決める。非所有）
        const KifuDocument* document = nullptr;
    };

    explicit MatchCoordinator(const Deps& deps, QObject* parent = nullptr);
    ~MatchCoordinator() override;

    /// 棋譜の受け手（ライブ対局のセッション）。未設定なら棋譜には書かない
    void setRecordSink(MatchRecordSink* sink) { m_sink = sink; }

    // --- 対局の手順 ---

    /// 対局の手順（開始局面と、対局で指した手。終局の行は含めない）
    const RecordLine& record() const { return m_record; }
    int currentPly() const override { return m_record.moveCount(); }
    /// 手順の最後の手の移動先（次の手の「同」表記の基準。手が無ければ無効なマス）
    BoardSquare lastMoveDestination() const { return m_record.destinationOf(currentPly()); }

    // --- 開始・再開 ---

    /**
     * @brief 対局を準備する（盤・手順・対局者・時計の決まり・Strategy。時計はまだ動かさない）
     *
     * opt.recordLine の currentPly までを対局の手順にし（後続の手は捨てる）、その局面から始める。
     * 時計には opt.time を入れ、手番を開始の局面の手番にする。
     */
    void start(const StartOptions& opt);
    /// 時計を動かし、初手がエンジンの手番なら思考を始める
    void beginPlay();

    const StartOptions& options() const { return m_session.options(); }
    /// 対局者（人間・エンジンの組と名前）
    const MatchPlayers& players() const { return m_session.players(); }
    bool isHumanSide(Turn side) const;
    /// どちらかの手番を人間が指す（待った・投了・入玉宣言を出すか）
    bool hasHumanPlayer() const { return isHumanSide(Turn::Black) || isHumanSide(Turn::White); }
    /// 手番（盤の手番）
    Turn sideToMove() const override;

    /// 終局の知らせ・入玉宣言で対局者を下手・上手と呼ぶか（駒落ちで、対局情報の見出しが下手・上手のとき）
    bool usesHandicapNames() const;

    const InterruptedGameState* interruptedGame() const { return m_interruptedGame.get(); }
    /// 中断の行を棋譜に書いた（そのノードを覚え、再開できるようにする）
    void attachInterruptedRecord(int terminalNodeId);
    /// 中断した対局を再開できるか（中断の行を棋譜に書いてある）
    bool canResumeInterruptedGame() const { return m_interruptedGame && m_interruptedGame->terminalNodeId >= 0; }
    void discardInterruptedGame();
    /// 中断した対局を、中断した局面・時計から再開する（時計は呼び出し側が beginPlay で動かす）
    bool resumeInterruptedGame();

    // --- 着手 ---

    /// 人間の手を受け付けるか（終局していない・棋譜に書ける）
    bool canAcceptMove() const;
    /// 人間が盤で指した（盤のクリックの手を GameBoard が指した後）
    void onHumanMove(const RecordEntry& played);
    /// 手番のエンジンにすぐ指させる
    void forceImmediateMove();
    /// 自分の手と相手の手を1つずつ取り消す（人間の手番のときだけ）
    bool undoTwoPlies();

    // --- 終局 ---

    /// 終局の情報（原因と負けた側。終局していなければ nullopt）
    const std::optional<GameEndInfo>& gameEnd() const { return m_session.end(); }
    bool isGameOver() const override { return m_session.end().has_value(); }
    void clearGameEnd();

    void handleResign();
    void handleBreakOff();
    void handlePlayerTimeOut(Turn player);
    /// 入玉宣言の判定結果で終局する（宣言の結果は呼び出し側のダイアログで知らせる）
    void handleNyugyokuDeclaration(Turn declarer, bool success, bool isDraw);
    /// 人間の入玉宣言を盤面から判定（rule は Rule24・Rule27）して終局し、判定結果を返す（知らせは呼び出し側）
    NyugyokuJudgement::Result declareNyugyoku(Turn declarer, JishogiCalculator::Rule rule);

    /// Strategy から呼ぶ操作（テストから一手を確定するときにも使う）
    MatchStrategyHost& strategyHost() { return *this; }

signals:
    // --- 盤 ---
    /// 待ったで ply 手目の局面に戻した（評価値グラフと盤の入力を合わせる）
    void movesUndone(int ply);
    /// エンジンが指した（score は指した側から見た最善の読み筋の評価値。グラフの ply 手目に足す）
    void engineMovePlayed(Turn side, int ply, const UsiScore& score);

    // --- 終局 ---
    /// 終局した（終局の行を書いた棋譜を確定し、動作状態を待機にした後）
    void gameEnded(const MatchCoordinator::GameEndInfo& info);
    /// 終局の知らせ（ダイアログ）を出す
    void gameOverMessage(const QString& title, const QString& message);
    /// 終局の後処理（知らせ）を終えた（連続対局の次局はここから）
    void gameEndProcessed(const MatchCoordinator::GameEndInfo& info);
    /// 中断した（再開できる）
    void gameInterrupted();
    /// 中断した対局を再開できるかが変わった（canResumeInterruptedGame）
    void resumeAvailabilityChanged();

private:
    // --- MatchStrategyHost ---
    EngineSlots& engines() override { return *m_engines; }
    MatchTimeSource* timeSource() override { return &m_clockTime; }
    void requestEngineMove(Turn side) override;
    void setWaitingForEngines(bool waiting) override;
    bool applyEngineMove(const UsiMatchMove& move) override;
    bool completeHumanMove(const RecordEntry& played) override;

    /// 時間の決まりを時計に入れ、時計の手番を盤の手番にする（時計はまだ動かさない）
    void applyTimeSettings(const MatchTimeSettings& settings);
    void createStrategy(const StartOptions& opt);
    /// side の手番を指すエンジン（人間の手番・対局していなければ nullptr）
    Usi* engineFor(Turn side) const;
    /// 情報欄の「探索手」に出す定跡手の表示（例「▲７六歩(77)（定跡 82.1%）」）
    static QString bookMoveLabel(const QString& sfen, const JosekiPick& pick);
    /// 一手を確定する。engineScore はエンジンの手のとき（グラフに足す評価値）
    bool completeMove(RecordEntry played, std::optional<UsiScore> engineScore);

    // --- 終局 ---
    void handleEngineResign(Turn side);
    void handleEngineWin(Turn side);
    void handleEngineIllegalMove(Turn side);
    void handleEngineError(const QString& message);
    bool checkAndHandleSennichite();
    void handleMaxMovesJishogi();
    /// 終局の後の知らせ方
    enum class Notice {
        Announce,     ///< 結果のダイアログを出して gameEndProcessed
        Declaration,  ///< 入玉宣言（結果は宣言の判定で知らせる）。gameEndProcessed
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

    GameBoard* m_gameBoard = nullptr;
    ShogiClock* m_clock = nullptr;
    AppActivityState* m_activity = nullptr;
    ActivityLease m_activityLease;   ///< 対局中の動作状態（開始・再開で受け取り、終局で手放す）
    MatchRecordSink* m_sink = nullptr;
    EngineSlots* m_engines = nullptr;
    ClockTimeSource m_clockTime;   ///< 対局のエンジンが持ち時間を読む対局時計
    const KifuDocument* m_document = nullptr;
    std::unique_ptr<GameModeStrategy> m_strategy;
    MatchSessionState m_session;
    RecordLine m_record;
    std::unique_ptr<InterruptedGameState> m_interruptedGame;
    bool m_playing = false;             ///< beginPlay から終局まで（時計を動かしてよい）
    bool m_waitingForEngines = false;   ///< エンジンの初期化を待っている（時計を止めておく）
};

#endif // MATCHCOORDINATOR_H
