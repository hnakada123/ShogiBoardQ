#ifndef P2PMATCH_H
#define P2PMATCH_H

/// @file p2pmatch.h
/// @brief 開始後の対局の状態機械（State0・着手の合意・時計の観測・再接続・結果の提案と署名。実装仕様 6〜8章）

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

#include "p2pcontrolchannel.h"
#include "p2pevidence.h"
#include "p2pframes.h"
#include "p2pgamerules.h"
#include "p2pmatchtimer.h"
#include "p2psettlement.h"
#include "p2psignature.h"
#include "p2psignedline.h"

namespace P2p {

/// 対局を始めるのに要るもの（交渉の結果）
struct MatchSetup {
    StartAgreement agreement;
    RulesTerms rules;   ///< 先手・後手の対局者の種別（開始合意の rulesHash の元の値）
    TimeControlTerms terms;
    bool iAmBlack = true;
    QByteArray blackStartSignature;
    QByteArray whiteStartSignature;
    QByteArray myConnectionRandom;
    QByteArray opponentConnectionRandom;
    /// この接続で次に送るフレームの requestId（交渉から続けて増やす）
    quint64 nextRequestId = 1;
    /// 相手が名乗った表示名（ジャーナルの開始の件に書く。証跡には入れない）
    QString opponentDisplayName;
};

/// 対局からの知らせ
struct MatchEvent {
    enum class Type {
        Ready,             ///< State0 に両方が署名し、READY を交わした（先手の初手の計時が始まる）
        MoveCommitted,     ///< 両方の署名がそろった着手（盤・棋譜・時計へ反映する）
        OfferReceived,     ///< 相手が合意引き分け・合意中断を申し出た（利用者に尋ねる）
        ProposalDeclined,  ///< 自分の提案（申し出・時間切れの主張）を相手が断った
        ResultAgreed,      ///< 結果が一致した（以後、最終結果への署名を交わす）
        Reconnecting,      ///< 接続が切れた・着手の確認が遅れた（再接続を始める）
        Resumed,           ///< 再接続して状態を照合した
        Finished,          ///< 対局が終わった（証跡を保存する）
    };
    Type type = Type::MoveCommitted;
    QByteArray moveUsi;    ///< MoveCommitted
    quint64 spentMs = 0;   ///< MoveCommitted: その手の消費時間（ミリ秒。指した側の申告）
    std::optional<FinalResult> result;   ///< OfferReceived・ResultAgreed・Finished（結果が一致していれば）
};

/**
 * @brief 開始後の1局
 *
 * フレームを順に渡し（receive）、利用者の操作（playMove・resign・offer）を渡すと、送るフレームと知らせができる。
 * 時刻は単調増加時計の関数で受け取り、tick() で期限を確かめる。ソケット・タイマー・UI に依存しない。
 * 送る前の提案と ACK の前の受理証明は persist に書いてから送る（書けなければ中断する）。
 *
 * 状態は段階 Phase（と、再接続の間の Resync）で持つ。双方が署名した手順と棋譜から決まる終局は SignedLine、
 * 制御メッセージの通し番号は ControlChannel、計時は MatchTimer が持つ（どれも規則だけの値）。
 */
class Match
{
public:
    using MonotonicClock = std::function<qint64()>;
    /// ジャーナルへの記録（書けなければ false）
    using Persist = std::function<bool(const CborValue& record)>;

    enum class Phase { Opening, Playing, Reconnecting, Finishing, Finished };
    enum class MoveError { None, NotMyTurn, Pending, Illegal, TimeUp, Finished };
    /// 合意引き分け・合意中断の申し出の結果
    enum class OfferResult {
        Sent,            ///< 相手へ送った
        AwaitingAnswer,  ///< 相手がまだ答えていないので、送り直さなかった
        Unavailable,     ///< 今は送れない（開始の手続き中・再接続中・自分の手の確認待ち・結果が決まった後）
    };

    Match(MatchSetup setup, SigningKey sessionKey, const SigningKey* playerKey, MonotonicClock clock, Persist persist);

    /// 開始する（開始合意と両方の開始署名をジャーナルに書いてから。先手は State0 を提案する）
    void begin();
    void receive(const Frame& frame);
    /// 時間に関わる確認（再送・復旧・切断の上限・時間切れ・署名の待ち）。定期的に呼ぶ
    void tick();

    /// 手番の利用者が指す（棋譜から終局が決まった局面では指せない。Finished）
    MoveError playMove(const UsiMove& move);
    /// 投了（自分の手番に限る）
    bool resign();
    /// 合意引き分け・合意中断を申し出る。相手が答えるまでの間はそれぞれ1回までで、答えを待っている申し出は
    /// 送り直さない（AwaitingAnswer）。相手が断った後は、同じ手数でももう一度申し出られる
    OfferResult offer(Cause cause);
    /// 相手の申し出に答える
    void answerOffer(bool accept);
    /// 対局をやめる（ABORT を送り、結果未確定で終える）
    void abort(ErrorCode reason = ErrorCode::Cancelled);

    /// 接続が切れた（Reconnecting にする。計時は続ける）
    void connectionLost();
    /// 新しい接続ができた（再接続の HELLO を送る）
    void connectionEstablished();

    std::vector<Frame> takeOutgoing();
    std::vector<MatchEvent> takeEvents();

    Phase phase() const { return m_phase; }
    /// 棋譜から終局が決まった（詰み・千日手・512手など。もう手を指さず、結果を交わすだけ）
    bool isDecidedByRecord() const { return m_line.terminal().has_value(); }
    bool iAmBlack() const { return m_setup.iAmBlack; }
    QByteArray gameId() const { return m_gameId; }
    const StartAgreement& agreement() const { return m_setup.agreement; }
    /// 先手・後手の対局者の種別
    const RulesTerms& rules() const { return m_setup.rules; }
    TimeControlTerms terms() const { return m_setup.terms; }
    Settlement settlement() const { return m_settlement; }
    std::optional<ErrorCode> endingError() const { return m_endingError; }
    /// 合意した結果
    const std::optional<FinalResult>& agreedResult() const { return m_result.agreed; }
    /// 自分の手番か（最新の両署名付きの状態の手番）
    bool isMyTurn() const;
    /// 確認待ちの自分の手があるか
    bool hasPendingMove() const { return m_pending.has_value(); }
    /// 自分の今の手番の消費時間（ミリ秒。手番が始まる前は 0。エンジンに渡す残り時間の計算に使う）
    qint64 myTurnSpentMs() const { return m_timer.mySpentMs(now()); }
    /// 相手の承認を待っている自分の状態（消費時間を引いた残り時間を持つ。無ければ nullptr）
    const GameState* pendingState() const { return m_pending ? &m_pending->state.state : nullptr; }
    /// 両方の署名がそろった最新の状態。開始局面（State0）の承認がそろう前は、手数 0 の空の状態を返す
    /// （開始の手続きの途中で対局が終わったとき・相手が最初に壊れた提案を送ったときにも呼ばれる。空の列の末尾を読まない）
    const GameState& lastState() const { return m_line.last(); }
    const std::vector<SignedState>& confirmedStates() const { return m_line.states(); }
    /// 1局の切断時間の合計（ミリ秒）
    qint64 reconnectTotalMs() const;
    /// 再接続の待ちの残り（ミリ秒。Reconnecting でなければ -1）
    qint64 reconnectRemainingMs() const;
    /// 相手の申し出に答える前か
    bool hasPendingOffer() const { return m_result.opponentOffer.has_value(); }
    /// 相手の答えを待っている自分の申し出（合意引き分けか合意中断。続けて申し出たときは、相手が尋ねられている最後の
    /// もの。相手が答えた・手数が進んだ・結果が決まった・対局が終わった後は nullopt）
    std::optional<Cause> offerAwaitingAnswer() const;
    /// 相手が同じ手数の異なる状態に署名した証明をジャーナルに書いたか（ジャーナルを残す）
    bool hasConflictProof() const { return m_conflictProof; }
    /// 証跡（いつでも作れる。結果がまだなければ空の結果）
    Evidence evidence() const;

private:
    /// 相手の承認を待っている自分の状態
    struct PendingMove {
        SignedState state;
        /// 着手の STATE_PROPOSE（再送は同じフレーム）。State0 は先手が begin で1度送り、READY を待つので持たない
        std::optional<Frame> frame;
        bool sent = false;    ///< 一度でも送ったか（相手の手番の観測は、最初に送った時点から）
        qint64 sentAt = 0;    ///< 最後に送った時刻（再送の間隔）
    };

    /// 再接続の照合の段階（Phase::Reconnecting の間だけ意味を持つ。実装仕様 7.2節）
    enum class Resync {
        AwaitingConnection,   ///< 新しい接続を待っている
        AwaitingHello,        ///< 再接続の HELLO を送り、相手の HELLO を待っている
        AwaitingResume,       ///< RESUME を送り、相手の RESUME を待っている
        AwaitingSyncRequest,  ///< 相手の手順が古いので、相手の SYNC_REQUEST を待っている（応えたら照合は終わり）
        AwaitingSyncChunks,   ///< 自分の手順が古いので、SYNC_CHUNK を待っている
    };

    /// 結果の提案と申し出（手数が進むと無効）、一致した結果と、最終結果への署名
    struct ResultExchange {
        std::optional<FinalResult> myProposal;      ///< 自分の提案（相手の答えを待つ。1つだけ持つ）
        std::optional<FinalResult> opponentOffer;   ///< 相手の申し出（利用者の答え待ち）
        /// 自分の申し出に相手がまだ答えていない（断られるか手数が進むまで、同じ申し出を送り直さない）
        bool drawOfferAwaitingAnswer = false;
        bool abortOfferAwaitingAnswer = false;
        std::optional<FinalResult> agreed;
        QByteArray mySignature;
        QByteArray opponentSignature;
        qint64 agreedAt = 0;
        /// 利用者に尋ねた相手の申し出の数（その手数で・利用者が最後に答えてから）と、その手数
        quint32 offersAskedPly = 0;
        int offersAsked = 0;
        int offersBeforeAnswer = 0;

        /// 手数が進んだ・相手が断った（それまでの提案と申し出は無効）
        void clearProposals();
        /// ply の手数で相手の申し出を利用者に尋ねてよいか（よければ数える）
        bool countOfferAsked(quint32 ply);
    };

    // --- 共通 ---
    void send(FrameType type, const CborValue& payload);
    void emitEvent(MatchEvent event) { m_events.push_back(std::move(event)); }
    bool persist(const CborValue& record);
    void finish(Settlement ending, std::optional<ErrorCode> error);
    void failWithError(ErrorCode code, quint32 relatedPly);
    /// 自分の側と相手の側
    Turn me() const { return turnOf(m_setup.iAmBlack); }
    Turn them() const { return opponent(me()); }
    QByteArray opponentSessionKey() const { return m_setup.agreement.sessionKey(!m_setup.iAmBlack); }
    QByteArray opponentPlayerKey() const { return m_setup.agreement.playerKey(!m_setup.iAmBlack); }
    /// 自分の承認だけを付けた状態
    SignedState signedByMe(const GameState& state) const;
    /// 自分の承認（先後に合わせて、両署名の組から取り出す）
    const QByteArray& myApproval(const SignedState& state) const;
    /// side の一局用鍵による、state の承認か
    bool verifyApproval(const GameState& state, const QByteArray& signature, Turn side) const;
    /// 自分の手番の基本の持ち時間の残り（最新の状態の）
    quint64 myMainMs() const;
    quint64 opponentMainMs() const;
    qint64 now() const { return m_clock ? m_clock() : 0; }

    // --- State0 と着手 ---
    void handleStatePropose(const StateProposePayload& payload);
    void handleStateAccept(const StateAcceptPayload& payload);
    /// 両方の署名がそろった着手を手順に足す（それまでの提案は無効。棋譜から終局が決まれば提案する）
    void commit(const SignedState& state);
    void resendPending();

    // --- 制御メッセージ ---
    void sendControl(ControlKind kind, const CborValue& body);
    void handleControl(const ControlPayload& payload);
    void handleReady(const CborValue& body);
    /// 制御メッセージの中身の形が、種類に合うか
    static bool hasKnownShape(ControlKind kind, const CborValue& body);

    // --- 結果 ---
    void handleResultPropose(const CborValue& body, const QByteArray& bodyHash);
    void handleResultSignature(const ResultSignaturePayload& payload);
    bool proposeResult(Outcome outcome, Cause cause);
    /// 相手の提案（申し出）をそのまま自分の提案として返し、一致させる
    void acceptProposal(const FinalResult& result);
    void agree(const FinalResult& result);
    void decline(const QByteArray& resultHash);

    // --- 再接続 ---
    void handleReconnectHello(const ReconnectHello& hello);
    void handleResume(const CborValue& body);
    void handleSyncRequest(const SyncRequestPayload& request);
    void handleSyncChunk(const SyncChunkPayload& chunk);
    void finishResume();

    MatchSetup m_setup;
    SigningKey m_sessionKey;
    const SigningKey* m_playerKey = nullptr;   ///< 非所有
    MonotonicClock m_clock;
    Persist m_persist;
    QByteArray m_gameId;

    Phase m_phase = Phase::Opening;
    Settlement m_settlement = Settlement::None;
    std::optional<ErrorCode> m_endingError;
    bool m_conflictProof = false;
    std::vector<Frame> m_outgoing;
    std::vector<MatchEvent> m_events;
    quint64 m_requestId = 1;

    // 手順
    SignedLine m_line;
    std::optional<PendingMove> m_pending;

    // 接続と制御
    ControlChannel m_control;
    Resync m_resync = Resync::AwaitingConnection;
    QByteArray m_myRandom;   ///< 今の接続の自分の乱数（再接続の HELLO で送り、connectionId を求める）

    // 時計
    qint64 m_startedAt = 0;          ///< 対局の開始（1局の実時間の上限）
    qint64 m_openingDeadline = 0;
    MatchTimer m_timer;              ///< 自分の消費時間・相手の手番の観測・切断の時間
    /// 回線が切れたときに、承認のない自分の手を送ってから待っていた時間（照合の後に送り直すときに、切断の時間に数える）
    qint64 m_unacknowledgedMs = 0;
    quint32 m_claimedTimeoutPly = 0xFFFFFFFFu;
    bool m_myTimeoutDeclared = false;

    // 結果
    ResultExchange m_result;
};

} // namespace P2p

#endif // P2PMATCH_H
